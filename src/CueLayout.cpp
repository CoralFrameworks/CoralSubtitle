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

#include "CueLayout.h"

#include <subtitle/TextLayout.h>

#include <graphics/Font.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace coral {

uint32_t foregroundColorOf(const AVCue& cue, const SubtitleStyle& style) {
  static_cast<void>(cue);
  return style.foregroundColor;
}

uint32_t foregroundColorAt(const AVCue& cue, const size_t offset, const uint32_t fallback) {
  for (const auto& span: cue.spans) {
    if (span.type == AVCueSpan::Type::kForegroundColor && span.start <= offset &&
        offset < span.end) {
      return span.color;
    }
  }
  return fallback;
}

float resolveCueTextSize(const AVCue& cue, const float defaultTextSize,
    const uint32_t viewportHeight, const float verticalPadding) {
  if (cue.textSize == AVCue::kDimenUnset ||
      cue.textSizeType == AVCue::TextSizeType::kUnset) {
    return defaultTextSize;
  }
  float size = defaultTextSize;
  switch (cue.textSizeType) {
    case AVCue::TextSizeType::kAbsolute:
      size = cue.textSize;
      break;
    case AVCue::TextSizeType::kFractional:
      size = cue.textSize * std::max(1.0f,
        static_cast<float>(viewportHeight) - verticalPadding * 2.0f);
      break;
    case AVCue::TextSizeType::kFractionalIgnorePadding:
      size = cue.textSize * static_cast<float>(viewportHeight);
      break;
    case AVCue::TextSizeType::kUnset:
      break;
  }
  return std::clamp(size, 1.0f, std::max(1.0f, defaultTextSize) * 8.0f);
}

std::vector<CueLayout> measureCues(const AVCueGroup& cues,
  const std::shared_ptr<graphics::Font>& font,
  const std::shared_ptr<graphics::Typeface>& typeface, const float maxWidth,
  const uint32_t viewportWidth, const uint32_t viewportHeight, const float verticalPadding,
  const float textSize, const SubtitleStyle& style) {
  std::vector<CueLayout> layouts;
  for (const auto& cue: cues.getCues()) {
    if (cue.isBitmap() || cue.text.empty()) {
      continue;
    }
    const float cueTextSize = resolveCueTextSize(cue, textSize, viewportHeight, verticalPadding);
    std::shared_ptr<graphics::Font> cueFont = font;
    if (std::abs(cueTextSize - textSize) > 0.01f && typeface != nullptr) {
      auto sized = graphics::Font::create(typeface, cueTextSize);
      if (sized != nullptr) cueFont = std::move(sized);
    }
    if (cueFont == nullptr || font == nullptr) {
      continue;
    }
    CueLayout layout;
    layout.cue = &cue;
    layout.textColor = foregroundColorOf(cue, style);
    layout.textSize = cueTextSize;
    layout.font = cueFont;
    layout.lineHeight = cueFont->metrics().ascent + cueFont->metrics().descent +
                        cueFont->metrics().lineGap;
    if (typeface != nullptr) {
      layout.rubyFont = graphics::Font::create(typeface, cueTextSize * 0.6f);
    }
    for (const auto& span: cue.spans) {
      if (span.type != AVCueSpan::Type::kRuby || span.ruby.type != AVCueSpan::RubyType::kBase ||
          span.end <= span.start || span.end > cue.text.size()) {
        continue;
      }
      RubyRun run;
      run.start = span.start;
      run.end = span.end;
      run.base = cue.text.substr(span.start, span.end - span.start);
      run.annotation = span.ruby.text;
      run.position = span.ruby.position;
      uint32_t foregroundColor = 0;
      uint32_t styleColor = 0;
      uint32_t emphasisColor = 0;
      for (const auto& styleSpan: cue.spans) {
        if (styleSpan.start > span.start || styleSpan.end < span.end) continue;
        if (styleSpan.type == AVCueSpan::Type::kTextStyle &&
            styleSpan.style.textSize != AVCueSpan::kTextSizeUnset) {
          run.fontScale = styleSpan.style.textSizeAbsolute
                            ? styleSpan.style.textSize / cueTextSize
                            : styleSpan.style.textSize;
        } else if (styleSpan.type == AVCueSpan::Type::kForegroundColor &&
                   styleSpan.color != 0) {
          foregroundColor = styleSpan.color;
        } else if (styleSpan.type == AVCueSpan::Type::kTextStyle &&
                   styleSpan.color != 0) {
          styleColor = styleSpan.color;
        } else if (styleSpan.type == AVCueSpan::Type::kTextEmphasis &&
                   styleSpan.emphasis.fillColor != 0) {
          emphasisColor = styleSpan.emphasis.fillColor;
        }
      }
      if (emphasisColor != 0) run.color = emphasisColor;
      else if (styleColor != 0) run.color = styleColor;
      else if (foregroundColor != 0) run.color = foregroundColor;
      if (!run.base.empty() && !run.annotation.empty()) layout.rubies.push_back(std::move(run));
    }
    const float cueMaxWidth = cue.size > 0.0f && cue.size != AVCue::kDimenUnset
                                ? std::min(maxWidth, cue.size * static_cast<float>(viewportWidth))
                                : maxWidth;
    // A paragraph inset narrows the wrapping width; the painter shifts the box right by the same
    // amount, so the right edge of the cue stays where it was.
    const float requestedIndent =
      std::clamp(cue.paragraphIndent, 0.0f, 0.5f) * static_cast<float>(viewportWidth);
    layout.paragraphIndent = std::min(requestedIndent, std::max(0.0f, cueMaxWidth - 1.0f));
    const auto textLayout = layoutCueText(TextLayoutDirection::kHorizontal, *cueFont, cue.text,
      std::max(1.0f, cueMaxWidth - layout.paragraphIndent), 0.0f, 0.0f);
    layout.width = textLayout.width;
    layout.lines = textLayout.lines;
    if (layout.lines.empty()) {
      continue;
    }
    // WebVTT REGION `lines` caps the visible region height. Scroll animation is not modelled yet;
    // without it the region keeps its top lines and clips the overflow.
    if (cue.regionLines > 0 && layout.lines.size() > static_cast<size_t>(cue.regionLines)) {
      layout.lines.resize(static_cast<size_t>(cue.regionLines));
    }
    layouts.push_back(std::move(layout));
  }
  return layouts;
}

} // namespace coral
