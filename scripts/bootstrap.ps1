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
#   .\scripts\bootstrap.ps1
# ==================================================================================================

[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

# PS 7.3+ can turn a native command's non-zero exit into a terminating error. The helpers below
# inspect $LASTEXITCODE themselves, so keep native failures as data.
if (Get-Variable -Name PSNativeCommandUseErrorActionPreference -ErrorAction SilentlyContinue) {
    $PSNativeCommandUseErrorActionPreference = $false
}

$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$GitModules = Join-Path $Root '.gitmodules'

function Write-Step([string]$Message) {
    Write-Host $Message
}

function Fail([string]$Message) {
    throw "error: $Message"
}

# Run git and fail the script when it returns non-zero. Use Get-GitOutput when the output matters.
function Invoke-Git {
    param([string[]]$Arguments)

    & git @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "error: git $($Arguments -join ' ') failed with exit code $LASTEXITCODE"
    }
}

function Get-GitOutput {
    param([string[]]$Arguments)

    $output = & git @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "error: git $($Arguments -join ' ') failed with exit code $LASTEXITCODE"
    }
    return $output
}

# A failed clone (or an interrupted one) must not be mistaken for a usable checkout on the next run,
# so the partial directory is removed instead of being left behind.
function Clone-Into {
    param(
        [string]$FullPath,
        [string]$Url
    )

    try {
        Invoke-Git -Arguments @('clone', $Url, $FullPath)
    }
    catch {
        if (Test-Path $FullPath) {
            Remove-Item -LiteralPath $FullPath -Recurse -Force
        }
        Fail "cloning $Url failed; check the network and rerun"
    }
}

function Sync-Repo {
    param(
        [string]$Path,
        [string]$Url
    )

    $fullPath = Join-Path $Root $Path
    $nestedGit = Join-Path $fullPath '.git'

    if (Test-Path $nestedGit) {
        Write-Step "Updating $Path"
        Invoke-Git -Arguments @('-C', $fullPath, 'fetch', '--prune', 'origin')

        $branch = & git -C $fullPath symbolic-ref --quiet --short HEAD 2>$null
        if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($branch)) {
            # A plain `git submodule update` leaves the submodule on a detached commit. Follow the
            # remote default branch instead, which is what this tool is for.
            & git -C $fullPath remote set-head origin --auto > $null 2>&1
            $defaultRef = & git -C $fullPath symbolic-ref --quiet --short refs/remotes/origin/HEAD 2>$null
            if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($defaultRef)) {
                Fail "cannot determine the default branch of $Url for $Path"
            }
            $branch = ($defaultRef.Trim() -replace '^origin/', '')
            Write-Step "  detached HEAD; switching to the remote default branch $branch"
            Invoke-Git -Arguments @('-C', $fullPath, 'checkout', $branch)
        }
        else {
            $branch = $branch.Trim()
        }

        Invoke-Git -Arguments @('-C', $fullPath, 'pull', '--ff-only', 'origin', $branch)
    }
    elseif (Test-Path $fullPath) {
        $hasFiles = Get-ChildItem -Force -LiteralPath $fullPath | Select-Object -First 1
        if ($hasFiles) {
            Fail "$Path exists but is not a Git checkout; move it away and rerun"
        }
        Write-Step "Cloning $Url"
        Write-Step "     -> $Path"
        Clone-Into -FullPath $fullPath -Url $Url
    }
    else {
        Write-Step "Cloning $Url"
        Write-Step "     -> $Path"
        $parent = Split-Path -Parent $fullPath
        if ($parent -and -not (Test-Path $parent)) {
            New-Item -ItemType Directory -Path $parent -Force | Out-Null
        }
        Clone-Into -FullPath $fullPath -Url $Url
    }

    # Nested repositories: Minilog for CoralCore, GoogleTest inside libgtest, zlib inside libz, and the
    # tree CoralGraphics bundles for its own standalone build. The dependency repositories mark the
    # copies the parent project provides with `update = none`, so this recursive update fetches only
    # what has to be there.
    Invoke-Git -Arguments @('-C', $fullPath, 'submodule', 'update', '--init', '--recursive')
}

if (-not (Test-Path $GitModules)) {
    Fail "no .gitmodules in $Root"
}

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    Fail 'git is required'
}

Push-Location $Root
try {
    $entries = Get-GitOutput -Arguments @('config', '-f', $GitModules, '--get-regexp', '^submodule\..*\.path$')

    foreach ($entry in @($entries)) {
        if ([string]::IsNullOrWhiteSpace($entry)) {
            continue
        }

        # entry is `submodule.<name>.path <relative path>`; split only at the first space.
        $parts = $entry -split ' ', 2
        if ($parts.Count -lt 2) {
            Fail "cannot parse .gitmodules entry: $entry"
        }

        $key = $parts[0]
        $path = $parts[1]
        $name = $key.Substring('submodule.'.Length)
        $name = $name.Substring(0, $name.Length - '.path'.Length)
        $url = Get-GitOutput -Arguments @('config', '-f', $GitModules, '--get', "submodule.$name.url")
        if ([string]::IsNullOrWhiteSpace($url)) {
            Fail "no URL for submodule $name"
        }

        Sync-Repo -Path $path -Url $url.Trim()
    }
}
finally {
    Pop-Location
}

Write-Step ''
Write-Step 'Dependencies are ready.'
Write-Step '  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release'
Write-Step '  cmake --build build --config Release'
Write-Step '  ctest --test-dir build -C Release -R "test_Subtitle|subtitle_render_demo_verify" --output-on-failure'
Write-Step ''
Write-Step '  .\build\examples\Release\subtitle_render_demo.exe --verify'
