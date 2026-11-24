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

#include "HorizontalCuePainter.h"

#include <graphics/Canvas.h>
#include <graphics/Font.h>
#include <graphics/FontManager.h>
#include <graphics/FontStyle.h>
#include <graphics/Paint.h>
#include <graphics/TextBlob.h>

#include <geometry/Matrix.h>

#include "Utf8.h"

#include <cmath>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace coral {
namespace detail {

void paintHorizontalCue(graphics::Canvas& canvas, const AVCue& cue, const CueLayout& layout,
  const float boxLeft, const float top, const bool windowSet, const uint32_t windowColor,
  const PainterContext& context) {
  const auto& font = *context.font;
  const SubtitleStyle& style = *context.style;

  std::map<std::tuple<int, bool, bool, std::string>, std::shared_ptr<graphics::Font>> sizedFonts;
  struct SpanFont final {
    float size = 0.0f;
    bool bold = false;
    bool italic = false;
    std::string family;
    uint32_t background = 0;
  };
  const auto spanFontAt = [&](const size_t absolute) {
    SpanFont result;
    result.size = textSizeAt(cue, absolute, context.textSize);
    result.background = backgroundColorAt(cue, absolute);
    if (const AVCueSpan::TextStyle* textStyle = textStyleAt(cue, absolute);
        textStyle != nullptr) {
      result.bold = textStyle->bold;
      result.italic = textStyle->italic;
      result.family = textStyle->fontFamily;
    } else {
      result.bold = context.fontBold;
      result.italic = context.fontItalic;
      result.family = context.fontFamily;
    }
    return result;
  };
  const auto fontForStyle = [&](const SpanFont& span) -> const graphics::Font& {
    const std::string family = span.family.empty() ? context.fontFamily : span.family;
    const std::tuple<int, bool, bool, std::string> key{
      static_cast<int>(std::lround(span.size * 100.0f)), span.bold, span.italic, family};
    const auto found = sizedFonts.find(key);
    if (found != sizedFonts.end()) {
      return *found->second;
    }
    std::shared_ptr<graphics::Font> sized;
    if (span.bold == context.fontBold && span.italic == context.fontItalic &&
        std::abs(span.size - context.textSize) <= 0.01f && family == context.fontFamily) {
      sized = context.font;
    } else {
      auto typeface = graphics::FontManager::shared().matchFamilyStyle(family,
        graphics::FontStyle(span.bold ? graphics::FontWeight::kBold : graphics::FontWeight::kNormal,
          span.italic ? graphics::FontStyle::Slant::Italic : graphics::FontStyle::Slant::Upright));
      if (typeface == nullptr) {
        typeface = context.typeface;
      }
      if (typeface != nullptr) {
        sized = graphics::Font::create(typeface, span.size);
      }
    }
    if (sized == nullptr) {
      sized = context.font;
    }
    sizedFonts[key] = sized;
    return *sized;
  };
  const auto sameStyle = [](const SpanFont& lhs, const SpanFont& rhs) {
    return lhs.size == rhs.size && lhs.bold == rhs.bold && lhs.italic == rhs.italic &&
           lhs.family == rhs.family && lhs.background == rhs.background;
  };
  const auto measureLine = [&](const std::string& line, const size_t lineStart) {
    float width = 0.0f;
    size_t position = 0;
    while (position < line.size()) {
      const size_t absolute = lineStart == std::string::npos ? position : lineStart + position;
      const SpanFont style = spanFontAt(absolute);
      size_t end = position + 1;
      while (end < line.size()) {
        const size_t endAbsolute = lineStart == std::string::npos ? end : lineStart + end;
        if (!sameStyle(spanFontAt(endAbsolute), style)) {
          break;
        }
        ++end;
      }
      width += fontForStyle(style).measureWidth(line.substr(position, end - position));
      position = end;
    }
    return width;
  };

  graphics::Paint window;
  window.color = colorFromArgb(windowColor);
  window.antiAlias = true;

  float baseline = top + font.metrics().ascent;
  size_t lineSearchOffset = 0;
  for (const auto& line: layout.lines) {
    const size_t lineStart = cue.text.find(line, lineSearchOffset);
    if (lineStart != std::string::npos) lineSearchOffset = lineStart + line.size();
    const float lineWidth = measureLine(line, lineStart);
    float x = boxLeft;
    const AVCue::Alignment lineAlignment =
      cue.multiRowAlignment != AVCue::Alignment::kUnset ? cue.multiRowAlignment : cue.textAlignment;
    switch (lineAlignment) {
      case AVCue::Alignment::kLeft:
      case AVCue::Alignment::kStart:
        x = boxLeft;
        break;
      case AVCue::Alignment::kRight:
      case AVCue::Alignment::kEnd:
        x = boxLeft + layout.width - lineWidth;
        break;
      case AVCue::Alignment::kCenter:
      case AVCue::Alignment::kUnset:
      default:
        x = boxLeft + (layout.width - lineWidth) * 0.5f;
        break;
    }
    const bool sheared = std::abs(cue.shearDegrees) > 0.01f;
    if (sheared) {
      const float radians = cue.shearDegrees * 3.14159265358979323846f / 180.0f;
      canvas.save();
      canvas.translate(0.0f, baseline);
      canvas.concat(graphics::Matrix::MakeSkew(std::tan(radians), 0.0f));
      canvas.translate(0.0f, -baseline);
    }
    if (windowSet) {
      canvas.drawRect(graphics::Rect::MakeLTRB(x - 4.0f,
                       baseline - font.metrics().ascent - 2.0f, x + lineWidth + 4.0f,
                       baseline + font.metrics().descent + 2.0f),
        window);
    }
    for (const auto& ruby: layout.rubies) {
      if (lineStart == std::string::npos) continue;
      std::shared_ptr<graphics::Font> rubyFont = context.rubyFont;
      if (ruby.fontScale != 0.6f && context.typeface != nullptr) {
        rubyFont = graphics::Font::create(context.typeface, context.textSize * ruby.fontScale);
      }
      if (rubyFont == nullptr) continue;
      std::vector<std::string> annotations;
      for (size_t position = 0; position < ruby.annotation.size();) {
        const size_t length = utf8GraphemeLength(ruby.annotation, position);
        const std::string glyph = ruby.annotation.substr(position, length);
        if (glyph != "\n") annotations.push_back(glyph);
        position += length;
      }
      std::vector<std::pair<size_t, size_t>> bases;
      for (size_t position = ruby.start; position < ruby.end && position < cue.text.size();) {
        const size_t length = utf8GraphemeLength(cue.text, position);
        bases.emplace_back(position, length);
        position += length;
      }
      if (annotations.empty() || bases.empty()) continue;
      const size_t lineEnd = lineStart + line.size();
      const float annotationBaseline =
        ruby.position == AVCueSpan::RubyPosition::kAfter
          ? baseline + font.metrics().descent + rubyFont->metrics().ascent + 2.0f
          : baseline - font.metrics().ascent - rubyFont->metrics().descent - 2.0f;
      for (size_t index = 0; index < bases.size(); ++index) {
        const auto [baseOffset, baseLength] = bases[index];
        if (baseOffset < lineStart || baseOffset + baseLength > lineEnd) continue;
        const size_t firstAnnotation = index * annotations.size() / bases.size();
        const size_t lastAnnotation = (index + 1) * annotations.size() / bases.size();
        if (firstAnnotation >= lastAnnotation) continue;
        std::string annotation;
        for (size_t annotationIndex = firstAnnotation; annotationIndex < lastAnnotation;
             ++annotationIndex) {
          annotation += annotations[annotationIndex];
        }
        const float prefixWidth = font.measureWidth(
          line.substr(0, baseOffset - lineStart));
        const float baseWidth = font.measureWidth(cue.text.substr(baseOffset, baseLength));
        const float annotationWidth = rubyFont->measureWidth(annotation);
        const float annotationX = x + prefixWidth + (baseWidth - annotationWidth) * 0.5f;
        const auto rubyBlob = rubyFont->shape(annotation);
        drawOutlinedBlob(canvas, rubyBlob, annotationX, annotationBaseline, *rubyFont,
          ruby.color != 0 ? ruby.color : layout.textColor);
      }
    }
    // Split the line at foreground-colour boundaries and shape each run separately, so a colour
    // span colours only the characters it covers instead of the whole line.
    float segmentX = x;
    uint32_t lastSegmentColor = layout.textColor;
    size_t segmentStart = 0;
    while (segmentStart < line.size()) {
      const size_t absoluteOffset =
        lineStart == std::string::npos ? segmentStart : lineStart + segmentStart;
      const uint32_t segmentColor = foregroundColorAt(cue, absoluteOffset, layout.textColor);
      const int segmentAlign = verticalAlignAt(cue, absoluteOffset);
      const SpanFont segmentStyle = spanFontAt(absoluteOffset);
      size_t segmentEnd = segmentStart;
      while (segmentEnd < line.size()) {
        const size_t offset =
          lineStart == std::string::npos ? segmentEnd : lineStart + segmentEnd;
        if (foregroundColorAt(cue, offset, layout.textColor) != segmentColor) break;
        if (verticalAlignAt(cue, offset) != segmentAlign) break;
        if (!sameStyle(spanFontAt(offset), segmentStyle)) break;
        ++segmentEnd;
      }
      if (segmentEnd == segmentStart) ++segmentEnd;
      const std::string segment = line.substr(segmentStart, segmentEnd - segmentStart);
      const graphics::Font& segmentFont = fontForStyle(segmentStyle);
      const float segmentWidth = segmentFont.measureWidth(segment);
      if (segmentStyle.background != 0) {
        graphics::Paint background;
        background.color = colorFromArgb(segmentStyle.background);
        background.antiAlias = true;
        canvas.drawRect(graphics::Rect::MakeLTRB(segmentX,
                         baseline - segmentFont.metrics().ascent, segmentX + segmentWidth,
                         baseline + segmentFont.metrics().descent),
          background);
      }
      const auto blob = segmentFont.shape(segment);
      // A subscript is lowered and a superscript raised by half an ascent, the shift the Android
      // text paint applies for the corresponding spans.
      const float shift = segmentAlign < 0 ? segmentFont.metrics().ascent * 0.5f
                        : segmentAlign > 0 ? -segmentFont.metrics().ascent * 0.5f
                                            : 0.0f;
      const float segmentStartX = segmentX;
      drawOutlinedBlob(canvas, blob, segmentX, baseline + shift, segmentFont, segmentColor);
      for (size_t byte = 0; byte < segment.size();) {
        const size_t absolute = (lineStart == std::string::npos ? 0 : lineStart) +
                                segmentStart + byte;
        const size_t glyphLength = utf8GraphemeLength(segment, byte);
        const AVCueSpan::Emphasis* emphasis = emphasisAt(cue, absolute);
        if (emphasis != nullptr) {
          const std::string prefix = segment.substr(0, byte);
          const std::string glyph = segment.substr(byte, glyphLength);
          const float centre = segmentStartX + segmentFont.measureWidth(prefix) +
                               segmentFont.measureWidth(glyph) * 0.5f;
          const float markY = emphasis->mode == AVCueSpan::TextEmphasisMode::kBefore
                                ? baseline + shift - segmentFont.metrics().ascent - 2.0f
                                : baseline + shift + segmentFont.metrics().descent + 2.0f;
          drawEmphasisMark(canvas, centre, markY, *emphasis, segmentColor);
        }
        byte += glyphLength;
      }
      lastSegmentColor = segmentColor;
      segmentX += segmentWidth;
      segmentStart = segmentEnd;
    }
    if (style.underline || style.strikeOut) {
      graphics::Paint decoration;
      decoration.color = colorFromArgb(lastSegmentColor);
      if (style.underline) {
        const float offset = baseline + font.metrics().underlineOffset;
        canvas.drawRect(
          graphics::Rect::MakeLTRB(x, offset - 1.0f, x + lineWidth, offset + 1.0f), decoration);
      }
      if (style.strikeOut) {
        const float offset = baseline - font.metrics().strikeThroughOffset;
        canvas.drawRect(
          graphics::Rect::MakeLTRB(x, offset - 1.0f, x + lineWidth, offset + 1.0f), decoration);
      }
    }
    if (sheared) {
      canvas.restore();
    }
    baseline += context.lineHeight;
  }
}

} // namespace detail
} // namespace coral
