#!/usr/bin/env sh
# ==================================================================================================
# CoralSubtitle dependency bootstrap
# ==================================================================================================
# Clone or update every repository declared in .gitmodules.
#
# A standalone CoralSubtitle checkout does not carry CoralCore, CoralMedia, CoralGraphics or the
# third-party repositories; only the URLs are recorded. This tool reads those URLs and:
#
#   * clones a missing repository from its default remote branch,
#   * updates an existing clone with a fast-forward pull, switching a detached submodule checkout
#     back to the remote default branch,
#   * initializes the nested repositories the dependency needs (Minilog for CoralCore, zlib for libz,
#     GoogleTest for libgtest, and CoralGraphics' own third-party tree).
#
# The `update = none` entries in the dependency repositories are deliberately skipped by their own
# recursive update: in this layout the parent project supplies those targets, so fetching duplicates
# would only waste time. The repositories are idempotent: rerunning the tool updates what is already
# there and stops instead of discarding local changes.
#
# The clone paths are the ones the CMake configure searches first, so `cmake -S . -B build` finds the
# same tree on every host:
#
#   external/CoralCore       utils, core, event and Minilog
#   external/CoralMedia      codec, media and the extractor cue model/parsers
#   external/CoralGraphics   the Canvas, fonts, image codecs and the GPU backend
#   third_party/libz         zlib, used by the CoralMedia extractor and the CoralGraphics codecs
#   third_party/libyuv       only linked with -DCORAL_SUPPORTS_FFMPEG=ON
#   third_party/libgtest     GoogleTest, for the CoralSubtitle test suite
#
# Usage:
#   ./scripts/bootstrap.sh
# ==================================================================================================

set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
GITMODULES="$ROOT_DIR/.gitmodules"

say() { printf '%s\n' "$*"; }
die() { printf 'error: %s\n' "$*" >&2; exit 1; }

[ -f "$GITMODULES" ] || die "no .gitmodules in $ROOT_DIR"
command -v git >/dev/null 2>&1 || die "git is required"

cd "$ROOT_DIR"

# A checked-out submodule stores .git as a file rather than a directory, so test for any entry.
is_git_checkout() {
  [ -e "$1/.git" ]
}

clone_repo() {
  path=$1
  url=$2
  say "Cloning $url"
  say "     -> $path"
  mkdir -p "$(dirname -- "$path")"
  # A failed clone (or an interrupted one) must not be mistaken for a usable checkout on the next
  # run, so the partial directory is removed instead of being left behind.
  if ! git clone "$url" "$path"; then
    rm -rf "$path"
    die "cloning $url failed; check the network and rerun"
  fi
}

update_repo() {
  path=$1
  url=$2

  if is_git_checkout "$path"; then
    say "Updating $path"
    git -C "$path" fetch --prune origin

    branch=$(git -C "$path" symbolic-ref --quiet --short HEAD 2>/dev/null || true)
    if [ -z "$branch" ]; then
      # A plain `git submodule update` leaves the submodule on a detached commit. Follow the remote
      # default branch instead, which is what this tool is for.
      git -C "$path" remote set-head origin --auto >/dev/null 2>&1 || true
      default_ref=$(git -C "$path" symbolic-ref --quiet --short refs/remotes/origin/HEAD 2>/dev/null || true)
      [ -n "$default_ref" ] || die "cannot determine the default branch of $url for $path"
      branch=${default_ref#origin/}
      say "  detached HEAD; switching to the remote default branch $branch"
      git -C "$path" checkout "$branch"
    fi

    git -C "$path" pull --ff-only origin "$branch"
  elif [ -d "$path" ] && [ -z "$(ls -A "$path" 2>/dev/null || true)" ]; then
    # An interrupted checkout can leave the directory behind but empty; cloning into it is safe.
    clone_repo "$path" "$url"
  elif [ -e "$path" ]; then
    die "$path exists but is not a Git checkout; move it away and rerun"
  else
    clone_repo "$path" "$url"
  fi

  # Nested repositories: Minilog for CoralCore, GoogleTest inside libgtest, zlib inside libz, and the
  # tree CoralGraphics bundles for its own standalone build. The dependency repositories mark the
  # copies the parent project provides with `update = none`, so this recursive update fetches only
  # what has to be there.
  git -C "$path" submodule update --init --recursive
}

submodules=$(git config -f "$GITMODULES" --get-regexp '^submodule\..*\.path$') ||
  die "no submodules are declared in $GITMODULES"

printf '%s\n' "$submodules" | while IFS=' ' read -r key path; do
  [ -n "$key" ] || continue

  # key is `submodule.<name>.path`; the matching URL uses the same name.
  name=${key#submodule.}
  name=${name%.path}
  url=$(git config -f "$GITMODULES" --get "submodule.$name.url") ||
    die "no URL for submodule $name"

  update_repo "$path" "$url"
done

say ""
say "Dependencies are ready."
say "  cmake -S . -B build -G \"Unix Makefiles\" -DCMAKE_BUILD_TYPE=Release"
say "  cmake --build build -j8"
say "  ctest --test-dir build -R \"test_Subtitle|subtitle_render_demo_verify\" --output-on-failure"
say ""
say "  ./build/examples/subtitle_render_demo                 # macOS: window; PNG elsewhere"
say "  ./build/examples/subtitle_render_demo --verify        # headless check"
