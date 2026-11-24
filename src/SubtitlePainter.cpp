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

#include "SubtitlePainter.h"

#include <algorithm>

#include <graphics/Canvas.h>
#include <graphics/Color.h>
#include <graphics/Font.h>
#include <graphics/Paint.h>
#include <graphics/TextBlob.h>

namespace coral {
namespace detail {

graphics::Color colorFromArgb(const uint32_t argb) {
  return graphics::Color(static_cast<float>((argb >> 16) & 0xFF) / 255.0f,
    static_cast<float>((argb >> 8) & 0xFF) / 255.0f,
    static_cast<float>(argb & 0xFF) / 255.0f,
    static_cast<float>((argb >> 24) & 0xFF) / 255.0f);
}

float anchoredStart(const float anchor, const AVCue::AnchorType type, const float size) {
  switch (type) {
    case AVCue::AnchorType::kEnd:
      return anchor - size;
    case AVCue::AnchorType::kMiddle:
      return anchor - size * 0.5f;
    case AVCue::AnchorType::kStart:
    case AVCue::AnchorType::kUnset:
    default:
      return anchor;
  }
}

float lineOffsetFor(const AVCue& cue, const float viewportHeight, const float lineHeight) {
  if (cue.line == AVCue::kDimenUnset) {
    return 0.0f;
  }
  if (cue.lineType == AVCue::LineType::kNumber) {
    return cue.line * lineHeight;
  }
  return cue.line * viewportHeight;
}

float positionOffsetFor(const AVCue& cue, const float viewportWidth) {
  if (cue.position == AVCue::kDimenUnset) {
    return 0.0f;
  }
  return cue.positionType == AVCue::PositionType::kAbsolute
           ? cue.position
           : cue.position * viewportWidth;
}

int verticalAlignAt(const AVCue& cue, const size_t offset) {
  for (const auto& span: cue.spans) {
    if (span.start <= offset && offset < span.end) {
      if (span.type == AVCueSpan::Type::kSubscript) {
        return -1;
      }
      if (span.type == AVCueSpan::Type::kSuperscript) {
        return 1;
      }
    }
  }
  return 0;
}

float textSizeAt(const AVCue& cue, const size_t offset, const float baseSize) {
  float size = baseSize;
  for (const auto& span: cue.spans) {
    if (span.type != AVCueSpan::Type::kTextStyle || span.start > offset || offset >= span.end) {
      continue;
    }
    if (span.style.textSize == AVCueSpan::kTextSizeUnset) {
      continue;
    }
    size = span.style.textSizeAbsolute ? span.style.textSize : baseSize * span.style.textSize;
  }
  return std::clamp(size, 1.0f, baseSize * 8.0f);
}

const AVCueSpan::Emphasis* emphasisAt(const AVCue& cue, const size_t offset) {
  const AVCueSpan::Emphasis* result = nullptr;
  for (const auto& span: cue.spans) {
    if (span.type == AVCueSpan::Type::kTextEmphasis && span.start <= offset &&
        offset < span.end && span.emphasis.mark != AVCueSpan::TextEmphasisMark::kNone) {
      result = &span.emphasis;
    }
  }
  return result;
}

const AVCueSpan::TextStyle* textStyleAt(const AVCue& cue, const size_t offset) {
  const AVCueSpan::TextStyle* result = nullptr;
  for (const auto& span: cue.spans) {
    if (span.type == AVCueSpan::Type::kTextStyle && span.start <= offset && offset < span.end) {
      result = &span.style;
    }
  }
  return result;
}

uint32_t backgroundColorAt(const AVCue& cue, const size_t offset) {
  uint32_t result = 0;
  for (const auto& span: cue.spans) {
    if (span.type == AVCueSpan::Type::kBackgroundColor && span.start <= offset &&
        offset < span.end && span.color != 0) {
      result = span.color;
    }
  }
  return result;
}

void drawEmphasisMark(graphics::Canvas& canvas, const float centreX, const float markY,
    const AVCueSpan::Emphasis& emphasis, const uint32_t fallbackColor) {
  graphics::Paint paint;
  paint.antiAlias = true;
  paint.color = colorFromArgb(emphasis.fillColor != 0 ? emphasis.fillColor : fallbackColor);
  const float radius = 1.6f;
  switch (emphasis.mark) {
    case AVCueSpan::TextEmphasisMark::kDot:
      canvas.drawCircle(centreX, markY, radius * 0.7f, paint);
      break;
    case AVCueSpan::TextEmphasisMark::kCircle:
    case AVCueSpan::TextEmphasisMark::kDoubleCircle:
    case AVCueSpan::TextEmphasisMark::kTriangle:
    case AVCueSpan::TextEmphasisMark::kSesame:
    default: {
      paint.style = graphics::Paint::Style::kStroke;
      paint.strokeWidth = 1.0f;
      canvas.drawCircle(centreX, markY, radius, paint);
      if (emphasis.mark == AVCueSpan::TextEmphasisMark::kDoubleCircle) {
        canvas.drawCircle(centreX, markY, radius * 1.7f, paint);
      }
      break;
    }
  }
}

void drawOutlinedBlob(graphics::Canvas& canvas, const graphics::TextBlob& blob, const float x,
                      const float baseline, const graphics::Font& font,
                      const uint32_t fillColor) {
  graphics::Paint outline;
  outline.color = graphics::Color(0.0f, 0.0f, 0.0f, 1.0f);
  outline.antiAlias = true;
  canvas.drawTextBlob(blob, x - kOutlineWidth, baseline, font, outline);
  canvas.drawTextBlob(blob, x + kOutlineWidth, baseline, font, outline);
  canvas.drawTextBlob(blob, x, baseline - kOutlineWidth, font, outline);
  canvas.drawTextBlob(blob, x, baseline + kOutlineWidth, font, outline);
  graphics::Paint fill;
  fill.color = colorFromArgb(fillColor);
  fill.antiAlias = true;
  canvas.drawTextBlob(blob, x, baseline, font, fill);
}

} // namespace detail
} // namespace coral
