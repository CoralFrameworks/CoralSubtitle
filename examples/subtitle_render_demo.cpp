/*
 * Copyright (C) 2026 The Coral Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// =============================================================
//  examples/subtitle_render_demo.cpp
//
//  The standalone demo of the framework, and the manual check that the whole layer works:
//
//      subtitle document -> extractor parser -> AVCueGroup -> SubtitleRenderer -> frame
//
//  The program builds a small SubRip document (or reads one named on the command line), parses it
//  through the same factory a player uses, rasterises the cues that are active at one time into an
//  RGBA overlay, and composites that overlay over a synthetic "video" frame. The frame is written as
//  a PNG and, on macOS, presented in a Cocoa window.
//
//  Usage:
//      subtitle_render_demo [options]
//
//  Options:
//      --file <path>            parse a subtitle file instead of the built-in sample
//      --format <srt|vtt|ssa|ttml|tx3g>
//                               override the format of --file (inferred from the suffix otherwise)
//      --time <seconds>         time of the frame (default: the middle of the first cue)
//      --width <pixels>         frame width (default 1280)
//      --height <pixels>        frame height (default 720)
//      --font <family>          font family of the cues (default: the system font)
//      --font-size <pixels>     cue font size (default: 5.33% of the frame height)
//      --style <plain|window>   text style; "window" draws a window behind the text (default)
//      --write-document <path>  write the subtitle document that is parsed to <path>
//      --output <path>          write the composited frame as a PNG
//      --no-window              never open the window
//      --window-seconds <s>     close the window after s seconds (0 waits for the user; default 0)
//      --verify                 headless check: exit 1 when the frame holds no subtitle pixel
//      --help                   print this help
//
//  On Windows and Linux the demo writes subtitle_frame.png when --output is not given; macOS opens
//  the window instead. --verify is what a build machine runs, because it needs no display.
// =============================================================

#include <subtitle/SubtitleRenderer.h>
#include <subtitle/SubtitleStyle.h>

#include <extractor/text/Cue.h>
#include <extractor/text/CueGroup.h>
#include <extractor/text/CuesWithTiming.h>
#include <extractor/text/DefaultSubtitleParserFactory.h>
#include <extractor/text/SubtitleParser.h>

#include <media/Definitions.h>
#include <media/Format.h>
#include <media/Types.h>

#include <graphics/Bitmap.h>
#include <graphics/Canvas.h>
#include <graphics/Color.h>
#include <graphics/Data.h>
#include <graphics/Image.h>
#include <graphics/ImageCodec.h>
#include <graphics/Paint.h>
#include <graphics/Pixmap.h>

#include <geometry/Rect.h>

#if defined(__APPLE__)
#include "platform/mac/SubtitleDemoWindow.h"
#endif

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

using coral::AVCue;
using coral::AVCueGroup;
using coral::AVCuesWithTiming;
using coral::AVSubtitleParser;
using coral::MediaFormat;
using coral::MimeTypes;
using coral::SubtitleRenderer;
using coral::SubtitleStyle;
using coral::TextDisplayOptions;

#if defined(__APPLE__)
constexpr bool kHasWindowSupport = true;
#else
constexpr bool kHasWindowSupport = false;
#endif

/** The document the demo parses when the command line does not name one. */
constexpr auto kSampleDocument = R"(1
00:00:00,500 --> 00:00:03,200
The Coral subtitle renderer turns a cue into a <b>styled</b> overlay.

2
00:00:03,500 --> 00:00:06,500
<font color="#FFD166">CoralSubtitle</font> wraps <i>each line</i> to the
frame and draws it with the Graphics Canvas.

3
00:00:07,000 --> 00:00:10,000
Three lines in one cue are laid out
bottom-up, one line height apart, and
kept inside the safe margins.
)";

/** One cue with the interval the parser gave it. */
struct TimedCue final {
  AVCue cue;
  int64_t startUs = 0;
  int64_t endUs = 0;
};

struct Options final {
  std::string file;
  std::string format;
  std::string output;
  std::string writeDocument;
  std::string fontFamily;
  float fontSize = 0.0f;
  double time = -1.0;
  int width = 1280;
  int height = 720;
  double windowSeconds = 0.0;
  bool windowStyle = true;
  bool showWindow = true;
  bool verify = false;
  bool help = false;
};

void printUsage() {
  std::cout <<
    "Usage: subtitle_render_demo [options]\n"
    "\n"
    "  --file <path>            parse a subtitle file instead of the built-in sample\n"
    "  --format <srt|vtt|ssa|ttml|tx3g>\n"
    "                           override the format of --file\n"
    "  --time <seconds>         time of the frame (default: the first cue)\n"
    "  --width <pixels>         frame width (default 1280)\n"
    "  --height <pixels>        frame height (default 720)\n"
    "  --font <family>          font family of the cues\n"
    "  --font-size <pixels>     cue font size\n"
    "  --style <plain|window>   text style (default window)\n"
    "  --write-document <path>  write the parsed subtitle document\n"
    "  --output <path>          write the composited frame as a PNG\n"
    "  --no-window              never open the window\n"
    "  --window-seconds <s>     close the window after s seconds (default 0: wait)\n"
    "  --verify                 headless check; exit 1 when no subtitle pixel was drawn\n"
    "  --help                   print this help\n";
}

bool parseArgument(const int argc, char** argv, Options& options, std::string& error) {
  const auto valueOf = [&](const int index) -> const char* {
    return index + 1 < argc ? argv[index + 1] : nullptr;
  };
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--help" || argument == "-h") {
      options.help = true;
    } else if (argument == "--file" && valueOf(index) != nullptr) {
      options.file = argv[++index];
    } else if (argument == "--format" && valueOf(index) != nullptr) {
      options.format = argv[++index];
    } else if (argument == "--time" && valueOf(index) != nullptr) {
      options.time = std::strtod(argv[++index], nullptr);
    } else if (argument == "--width" && valueOf(index) != nullptr) {
      options.width = std::max(1, std::atoi(argv[++index]));
    } else if (argument == "--height" && valueOf(index) != nullptr) {
      options.height = std::max(1, std::atoi(argv[++index]));
    } else if (argument == "--font" && valueOf(index) != nullptr) {
      options.fontFamily = argv[++index];
    } else if (argument == "--font-size" && valueOf(index) != nullptr) {
      options.fontSize = static_cast<float>(std::strtod(argv[++index], nullptr));
    } else if (argument == "--style" && valueOf(index) != nullptr) {
      const std::string style = argv[++index];
      if (style == "plain") {
        options.windowStyle = false;
      } else if (style == "window") {
        options.windowStyle = true;
      } else {
        error = "unknown --style value: " + style;
        return false;
      }
    } else if (argument == "--write-document" && valueOf(index) != nullptr) {
      options.writeDocument = argv[++index];
    } else if (argument == "--output" && valueOf(index) != nullptr) {
      options.output = argv[++index];
    } else if (argument == "--no-window") {
      options.showWindow = false;
    } else if (argument == "--window-seconds" && valueOf(index) != nullptr) {
      options.windowSeconds = std::strtod(argv[++index], nullptr);
    } else if (argument == "--verify") {
      options.verify = true;
      options.showWindow = false;
    } else {
      error = "unknown argument: " + argument;
      return false;
    }
  }
  return true;
}

std::string lowerCase(std::string text) {
  for (char& character: text) {
    character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
  }
  return text;
}

/** @return the MIME type of a subtitle file, judged by its suffix. */
std::string mimeTypeForName(const std::string& name) {
  const auto dot = name.find_last_of('.');
  const std::string extension =
    dot == std::string::npos ? std::string() : lowerCase(name.substr(dot + 1));
  if (extension == "vtt" || extension == "webvtt") {
    return MimeTypes::TEXT_VTT;
  }
  if (extension == "ssa" || extension == "ass") {
    return MimeTypes::TEXT_SSA;
  }
  if (extension == "ttml" || extension == "dfxp" || extension == "xml") {
    return MimeTypes::APPLICATION_TTML;
  }
  if (extension == "tx3g") {
    return MimeTypes::APPLICATION_TX3G;
  }
  return MimeTypes::APPLICATION_SUBRIP;
}

std::string mimeTypeForOption(const std::string& format) {
  const std::string name = lowerCase(format);
  if (name == "vtt" || name == "webvtt") {
    return MimeTypes::TEXT_VTT;
  }
  if (name == "ssa" || name == "ass") {
    return MimeTypes::TEXT_SSA;
  }
  if (name == "ttml" || name == "dfxp" || name == "xml") {
    return MimeTypes::APPLICATION_TTML;
  }
  if (name == "tx3g") {
    return MimeTypes::APPLICATION_TX3G;
  }
  return MimeTypes::APPLICATION_SUBRIP;
}

bool readFile(const std::string& path, std::string& out) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }
  std::ostringstream buffer;
  buffer << file.rdbuf();
  out = buffer.str();
  return true;
}

bool writeFile(const std::string& path, const uint8_t* bytes, const size_t size) {
  std::ofstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }
  file.write(reinterpret_cast<const char*>(bytes), static_cast<std::streamsize>(size));
  return static_cast<bool>(file);
}

std::string formatTime(const int64_t timeUs) {
  const int64_t totalMs = timeUs / 1000;
  const int64_t milliseconds = totalMs % 1000;
  const int64_t totalSeconds = totalMs / 1000;
  const int64_t seconds = totalSeconds % 60;
  const int64_t minutes = (totalSeconds / 60) % 60;
  const int64_t hours = totalSeconds / 3600;
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%02lld:%02lld:%02lld.%03lld",
    static_cast<long long>(hours), static_cast<long long>(minutes),
    static_cast<long long>(seconds), static_cast<long long>(milliseconds));
  return buffer;
}

/** @return the text with newlines escaped, so a cue stays on one line of the table. */
std::string oneLine(const std::string& text) {
  std::string result;
  result.reserve(text.size());
  for (const char character: text) {
    if (character == '\n') {
      result += "\\n";
    } else {
      result += character;
    }
  }
  return result;
}

/** @return a human readable summary of the spans of @p cue, for the parsed-cue table. */
std::string describeSpans(const AVCue& cue) {
  if (cue.spans.empty()) {
    return "-";
  }
  std::string summary;
  for (const auto& span: cue.spans) {
    const char* name = "style";
    switch (span.type) {
      case coral::AVCueSpan::Type::kTextStyle:
        name = span.style.bold ? "bold"
             : span.style.italic ? "italic"
             : span.style.underline ? "underline"
             : span.style.strikeout ? "strikeout"
             : "style";
        break;
      case coral::AVCueSpan::Type::kForegroundColor: {
        char color[16];
        std::snprintf(color, sizeof(color), "#%06X", span.color & 0xFFFFFFu);
        summary += summary.empty() ? color : std::string(", ") + color;
        continue;
      }
      case coral::AVCueSpan::Type::kBackgroundColor:
        name = "background";
        break;
      case coral::AVCueSpan::Type::kRuby:
        name = "ruby";
        break;
      case coral::AVCueSpan::Type::kVoice:
        name = "voice";
        break;
      case coral::AVCueSpan::Type::kLanguage:
        name = "language";
        break;
      case coral::AVCueSpan::Type::kTextEmphasis:
        name = "emphasis";
        break;
      case coral::AVCueSpan::Type::kHorizontalTextInVerticalContext:
        name = "horizontal";
        break;
      case coral::AVCueSpan::Type::kSubscript:
        name = "subscript";
        break;
      case coral::AVCueSpan::Type::kSuperscript:
        name = "superscript";
        break;
    }
    summary += summary.empty() ? name : std::string(", ") + name;
  }
  return summary;
}

/** @return every cue of the parse results with an interval, in document order. */
std::vector<TimedCue> flatten(const std::vector<AVCuesWithTiming>& results) {
  std::vector<TimedCue> cues;
  for (const auto& result: results) {
    const int64_t resultStart = result.startTimeUs == coral::Definitions::kTimeUnset
                                  ? 0
                                  : result.startTimeUs;
    for (size_t index = 0; index < result.cues.size(); ++index) {
      TimedCue timed;
      timed.cue = result.cues[index];
      const int64_t relative = index < result.cueTimesUs.size() ? result.cueTimesUs[index] : 0;
      timed.startUs = resultStart + relative;
      const int64_t duration = index < result.cueDurationsUs.size()
                                 ? result.cueDurationsUs[index]
                                 : result.durationUs;
      // A cue without a duration is shown for a default interval, which is what a still frame of a
      // never-ending sample needs; the player replaces it with the next group.
      timed.endUs = duration == coral::Definitions::kTimeUnset || duration <= 0
                      ? timed.startUs + 2000000
                      : timed.startUs + duration;
      cues.push_back(std::move(timed));
    }
  }
  return cues;
}

/** @return the cues of @p timed that are shown at @p timeUs. */
std::vector<AVCue> cuesAt(const std::vector<TimedCue>& timed, const int64_t timeUs) {
  std::vector<AVCue> active;
  for (const auto& cue: timed) {
    if (cue.startUs <= timeUs && timeUs < cue.endUs) {
      active.push_back(cue.cue);
    }
  }
  return active;
}

/** Draws a cheap synthetic video frame: a gradient, a moon and a couple of bars. */
void paintBackdrop(graphics::Canvas& canvas, const int width, const int height) {
  const auto top = graphics::Color::MakeRGBA8(12, 18, 38, 255);
  const auto bottom = graphics::Color::MakeRGBA8(52, 32, 66, 255);
  graphics::Paint paint;
  paint.antiAlias = false;
  const int bands = std::max(1, height);
  for (int row = 0; row < bands; ++row) {
    const float t = static_cast<float>(row) / static_cast<float>(std::max(1, bands - 1));
    paint.color = graphics::Color(
      top.red + (bottom.red - top.red) * t,
      top.green + (bottom.green - top.green) * t,
      top.blue + (bottom.blue - top.blue) * t, 1.0f);
    canvas.drawRect(graphics::Rect::MakeLTRB(0.0f, static_cast<float>(row),
                       static_cast<float>(width), static_cast<float>(row + 1)),
      paint);
  }

  graphics::Paint moon;
  moon.antiAlias = true;
  moon.color = graphics::Color::MakeRGBA8(240, 226, 198, 190);
  canvas.drawCircle(static_cast<float>(width) * 0.80f, static_cast<float>(height) * 0.22f,
    static_cast<float>(height) * 0.09f, moon);

  graphics::Paint skyline;
  skyline.antiAlias = false;
  skyline.color = graphics::Color::MakeRGBA8(8, 10, 20, 235);
  const float groundTop = static_cast<float>(height) * 0.70f;
  canvas.drawRect(graphics::Rect::MakeLTRB(0.0f, groundTop, static_cast<float>(width),
                     static_cast<float>(height)), skyline);
  for (int bar = 0; bar < 9; ++bar) {
    const float barWidth = static_cast<float>(width) * 0.055f;
    const float left = static_cast<float>(bar) * barWidth * 1.6f;
    const float barHeight = static_cast<float>(height) * (0.08f + 0.035f * static_cast<float>(bar % 4));
    skyline.color = graphics::Color::MakeRGBA8(16, 20, 34, 255);
    canvas.drawRect(graphics::Rect::MakeLTRB(left, groundTop - barHeight, left + barWidth,
                       groundTop), skyline);
  }
}

} // namespace anonymous

int main(const int argc, char** argv) {
  Options options;
  std::string error;
  if (!parseArgument(argc, argv, options, error)) {
    std::cerr << "error: " << error << "\n\n";
    printUsage();
    return 2;
  }
  if (options.help) {
    printUsage();
    return 0;
  }

  // ---------------------------------------------------------------- the subtitle document
  std::string document = kSampleDocument;
  std::string documentName = "the built-in SubRip sample";
  if (!options.file.empty()) {
    if (!readFile(options.file, document)) {
      std::cerr << "error: cannot read " << options.file << "\n";
      return 2;
    }
    documentName = options.file;
  }
  if (!options.writeDocument.empty() &&
      !writeFile(options.writeDocument, reinterpret_cast<const uint8_t*>(document.data()),
        document.size())) {
    std::cerr << "error: cannot write " << options.writeDocument << "\n";
    return 2;
  }

  const std::string mimeType = !options.format.empty()
                                 ? mimeTypeForOption(options.format)
                                 : options.file.empty()
                                     ? std::string(MimeTypes::APPLICATION_SUBRIP)
                                     : mimeTypeForName(options.file);
  const auto format = MediaFormat::CreateSubtitleFormat(mimeType);
  const auto factory = coral::createDefaultSubtitleParserFactory();
  if (format == nullptr || factory == nullptr || !factory->supportsFormat(*format)) {
    std::cerr << "error: no subtitle parser for " << mimeType << "\n";
    return 2;
  }
  auto parser = factory->create(*format);
  if (parser == nullptr) {
    std::cerr << "error: the factory did not create a parser for " << mimeType << "\n";
    return 2;
  }

  std::vector<AVCuesWithTiming> results;
  const bool parsed = parser->parse(reinterpret_cast<const uint8_t*>(document.data()),
    document.size(), AVSubtitleParser::OutputOptions::allCues(),
    [&results](const AVCuesWithTiming& cues) { results.push_back(cues); });
  if (!parsed) {
    std::cerr << "error: the document is malformed\n";
    return 2;
  }
  std::vector<TimedCue> timed = flatten(results);
  if (timed.empty()) {
    std::cerr << "error: the document holds no cue\n";
    return 2;
  }
  std::stable_sort(timed.begin(), timed.end(),
    [](const TimedCue& lhs, const TimedCue& rhs) { return lhs.startUs < rhs.startUs; });

  std::cout << "CoralSubtitle render demo\n"
            << "  document : " << documentName << "\n"
            << "  format   : " << mimeType << "\n"
            << "  frame    : " << options.width << "x" << options.height << "\n"
            << "  cues     : " << timed.size() << "\n\n";

  std::printf("  %-3s %-15s %-15s %-18s %s\n", "#", "start", "end", "spans", "text");
  for (size_t index = 0; index < timed.size(); ++index) {
    const TimedCue& cue = timed[index];
    std::printf("  %-3zu %-15s %-15s %-18s %s\n", index + 1,
      formatTime(cue.startUs).c_str(), formatTime(cue.endUs).c_str(),
      describeSpans(cue.cue).c_str(), oneLine(cue.cue.text).c_str());
  }
  std::cout << "\n";

  // ---------------------------------------------------------------- the frame to render
  const double time = options.time >= 0.0
                        ? options.time
                        : static_cast<double>(timed.front().startUs + timed.front().endUs) /
                            2000000.0;
  const int64_t timeUs = static_cast<int64_t>(time * 1000000.0);
  const std::vector<AVCue> active = cuesAt(timed, timeUs);
  std::cout << "  frame at : " << formatTime(timeUs) << " (" << active.size()
            << (active.size() == 1 ? " active cue" : " active cues") << ")\n";

  SubtitleStyle style;
  style.fontFamily = options.fontFamily;
  style.fontSize = options.fontSize;
  TextDisplayOptions display;
  display.windowEnabled = options.windowStyle;
  display.windowColor = 0xA0000000;

  const uint32_t width = static_cast<uint32_t>(options.width);
  const uint32_t height = static_cast<uint32_t>(options.height);
  SubtitleRenderer renderer;
  std::shared_ptr<graphics::Bitmap> overlay;
  if (!active.empty()) {
    overlay = renderer.render(AVCueGroup(active, timeUs), width, height, style, display);
  }

  int subtitlePixels = 0;
  int firstRow = -1;
  int lastRow = -1;
  if (overlay != nullptr) {
    const auto pixels = overlay->makePixmap();
    for (int y = 0; y < static_cast<int>(height); ++y) {
      for (int x = 0; x < static_cast<int>(width); ++x) {
        const auto* pixel = static_cast<const uint8_t*>(pixels.addr(x, y));
        if (pixel != nullptr && pixel[3] != 0) {
          ++subtitlePixels;
          if (firstRow < 0) {
            firstRow = y;
          }
          lastRow = y;
        }
      }
    }
  }

  // Composite the overlay over the synthetic frame, the way a player blends subtitles over its
  // decoded picture.
  graphics::Bitmap frame = graphics::Bitmap::allocate(static_cast<int>(width),
    static_cast<int>(height));
  auto canvas = graphics::Canvas::create(frame);
  if (frame.isEmpty() || canvas == nullptr) {
    std::cerr << "error: cannot allocate a " << width << "x" << height << " frame\n";
    return 2;
  }
  paintBackdrop(*canvas, static_cast<int>(width), static_cast<int>(height));
  if (overlay != nullptr) {
    graphics::Paint paint;
    paint.color = graphics::Color(1.0f, 1.0f, 1.0f, 1.0f);
    canvas->drawImage(graphics::Image::fromBitmap(*overlay), 0.0f, 0.0f, paint);
  }

  // ---------------------------------------------------------------- the result
  if (subtitlePixels > 0) {
    std::cout << "  overlay  : " << subtitlePixels << " subtitle pixels, rows " << firstRow
              << ".." << lastRow << "\n";
  } else {
    std::cout << "  overlay  : no subtitle pixel at this time\n";
  }

  std::string output = options.output;
  // A plain run shows the window on macOS and writes a PNG elsewhere; --verify only writes a file when
  // one is asked for, so a test run leaves nothing behind.
  if (output.empty() && !options.verify && (!options.showWindow || !kHasWindowSupport)) {
    output = "subtitle_frame.png";
  }
  if (!output.empty()) {
    std::string encodeError;
    const graphics::Data png =
      graphics::Image::fromBitmap(frame).encode(graphics::ImageFormat::PNG, 100, &encodeError);
    if (png.isEmpty() || !writeFile(output, png.bytes(), png.size())) {
      std::cerr << "error: cannot write " << output
                << (encodeError.empty() ? "" : ": " + encodeError) << "\n";
      return 2;
    }
    std::cout << "  frame    : wrote " << output << " (" << png.size() << " bytes)\n";
  }

  if (options.verify) {
    if (subtitlePixels == 0) {
      std::cerr << "verify failed: the frame holds no subtitle pixel\n";
      return 1;
    }
    std::cout << "  verify   : ok\n";
    return 0;
  }

#if defined(__APPLE__)
  if (options.showWindow) {
    const std::string title = documentName + "  " + formatTime(timeUs);
    if (!coral::demo::presentSubtitleFrame(frame, options.windowSeconds, title)) {
      std::cerr << "error: cannot open the demo window\n";
      return 2;
    }
  }
#endif
  return 0;
}
