# CoralSubtitle examples

The framework's standalone demo. `subtitle_render_demo` shows the whole layer in one program:

```
subtitle document -> extractor parser -> AVCueGroup -> SubtitleRenderer -> RGBA overlay -> frame
```

It builds a small SubRip document (or reads a subtitle file), parses it through
`createDefaultSubtitleParserFactory()` — the same factory a player uses — prints the parsed cue table,
rasterises the cues that are active at one time into a transparent overlay, and composites that
overlay over a synthetic "video" frame. The frame is presented in a Cocoa window on macOS and written
as a PNG everywhere. The renderer and the compositor are platform independent; only the window is
macOS code.

## Running

As part of the framework:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
./build/examples/subtitle_render_demo
```

On its own, which is what an IDE opens when working on the demo:

```sh
cmake -S examples -B build/examples -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build/examples -j8
./build/examples/subtitle_render_demo
```

The standalone configure resolves CoralCore, CoralMedia and CoralGraphics from `external/`, so the
dependencies have to be fetched first (`./scripts/bootstrap.sh`, or `.\scripts\bootstrap.ps1`); see the
header of the repository README.

On Windows the executable is `build\examples\Release\subtitle_render_demo.exe` when the framework
root adds the project, and `build\Release\subtitle_render_demo.exe` when `examples/` is configured on
its own.

## What it prints

```
CoralSubtitle render demo
  document : the built-in SubRip sample
  format   : application/x-subrip
  frame    : 1280x720
  cues     : 3

  #   start           end             spans    text
  1   00:00:00.500   00:00:03.200    bold     The Coral subtitle renderer turns a cue into a styled overlay.
  2   00:00:03.500   00:00:06.500    #FFD166, italic
  3   00:00:07.000   00:00:10.000    -        Three lines in one cue are laid out\nbottom-up, ...

  frame at : 00:00:01.850 (1 active cue)
  overlay  : 50400 subtitle pixels, rows 636..685
  frame    : wrote subtitle_frame.png (33479 bytes)
```

The `spans` column is the summary of the cue's styled ranges (`bold`, `italic`, `underline`, a
`#RRGGBB` colour, `ruby`, …), which is what tells whether the parser produced the model the renderer
draws. The `overlay` line counts the non-transparent pixels and their row range: a subtitle that
parsed but did not draw shows up as `no subtitle pixel at this time`.

## Options

| Option | Meaning |
| --- | --- |
| `--file <path>` | Parse a subtitle file instead of the built-in sample. |
| `--format <srt\|vtt\|ssa\|ttml\|tx3g>` | Override the format of `--file`. Without it the format comes from the file suffix (`.srt`, `.vtt`, `.ssa`/`.ass`, `.ttml`/`.dfxp`/`.xml`, `.tx3g`). |
| `--time <seconds>` | Time of the frame. The default is the middle of the first cue. |
| `--width <pixels>`, `--height <pixels>` | Frame size. The default is 1280x720. |
| `--font <family>` | Font family of the cues. The default is the system font. |
| `--font-size <pixels>` | Cue font size. The default is 5.33% of the frame height. |
| `--style <plain\|window>` | `window` draws a translucent window behind the text (the default), `plain` draws the outline only. |
| `--write-document <path>` | Write the subtitle document that is parsed to this path — the round trip of the format the demo builds. |
| `--output <path>` | Write the composited frame as a PNG. |
| `--no-window` | Never open the window. |
| `--window-seconds <s>` | Close the window after `s` seconds; 0 (the default) waits for the user. |
| `--verify` | Headless check: render, then exit 1 when the frame holds no subtitle pixel. Implies `--no-window` and writes nothing unless `--output` is given. |
| `--help` | Print the usage. |

Without `--output`, macOS opens the window and the other platforms write `subtitle_frame.png` in the
current directory. `--verify` writes no frame unless `--output` names one.

## The window

The macOS window is a plain AppKit window (`platform/mac/SubtitleDemoWindow.mm`), not a GPU surface:
the renderer is a CPU canvas, and a demo that shows one frame does not need an RHI device or a Metal
layer. Esc, Q, the red button and Cmd+Q end the run loop. The frame keeps its aspect ratio and is
scaled down to fit the screen when it is larger than the display.

## Verifying a build

```sh
./subtitle_render_demo --verify
```

This mode needs no display, which is what a build machine runs; the framework's CTest test
`subtitle_render_demo_verify` is exactly this command.

## The sample document

The built-in document is compiled into the demo (`kSampleDocument` in
`subtitle_render_demo.cpp`), so the demo needs no asset and runs from any directory. It exercises the
default look, a colour span, bold and italic spans, a cue that wraps over two lines and one that is
already three lines; `--write-document sample.srt` writes it out if you want to edit and parse it back
with `--file sample.srt --time 4.5`.
