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

#include "VerticalCuePainter.h"

#include "Utf8.h"

#include <subtitle/TextLayout.h>
#include <subtitle/VerticalTextLayout.h>

#include <graphics/Canvas.h>
#include <graphics/Font.h>
#include <graphics/FontManager.h>
#include <graphics/FontStyle.h>
#include <graphics/Paint.h>
#include <graphics/TextBlob.h>

#include <geometry/Matrix.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

namespace coral {
namespace detail {

VerticalRubyMetrics measureVerticalRubies(const VerticalRubyInput& input) {
  VerticalRubyMetrics metrics;
  if (input.rubies == nullptr || input.characterOffsets == nullptr) {
    return metrics;
  }
  const std::vector<RubyRun>& rubies = *input.rubies;
  const std::vector<size_t>& characterOffsets = *input.characterOffsets;
  const int totalCharacters = static_cast<int>(characterOffsets.size());
  const int rowsPerColumn = std::max(1, input.rowsPerColumn);
  const int columns = std::max(1, input.columns);
  const float textSize = input.textSize;
  const float fittedScale = input.fittedScale;

  metrics.pad = (textSize * fittedScale) * 0.75f;
  metrics.before.assign(static_cast<size_t>(columns), false);
  metrics.after.assign(static_cast<size_t>(columns), false);
  for (const auto& ruby: rubies) {
    metrics.pad = std::max(metrics.pad, (textSize * fittedScale) * ruby.fontScale * 1.2f);
    std::shared_ptr<graphics::Font> measureFont = input.rubyFont;
    if ((ruby.fontScale != 0.6f || fittedScale < 1.0f) && input.typeface != nullptr) {
      measureFont =
        graphics::Font::create(input.typeface, (textSize * fittedScale) * ruby.fontScale);
    }
    if (measureFont != nullptr) {
      for (size_t position = 0; position < ruby.annotation.size();) {
        const size_t length = utf8GraphemeLength(ruby.annotation, position);
        metrics.pad = std::max(metrics.pad,
          measureFont->measureWidth(ruby.annotation.substr(position, length)) + 4.0f);
        position += length;
      }
    }
    for (int index = 0; index < totalCharacters; ++index) {
      if (characterOffsets[static_cast<size_t>(index)] < ruby.start) continue;
      if (characterOffsets[static_cast<size_t>(index)] >= ruby.end) break;
      const int column = index / rowsPerColumn;
      if (ruby.position == AVCueSpan::RubyPosition::kAfter) {
        metrics.after[static_cast<size_t>(column)] = true;
      } else {
        metrics.before[static_cast<size_t>(column)] = true;
      }
    }
  }
  metrics.pad = std::min(metrics.pad, (textSize * fittedScale) * 2.0f);

  for (const auto& ruby: rubies) {
    size_t annotationCount = 0;
    for (size_t position = 0; position < ruby.annotation.size();) {
      const size_t length = utf8GraphemeLength(ruby.annotation, position);
      if (length != 1 || ruby.annotation[position] != '\n') ++annotationCount;
      position += length;
    }
    size_t baseCount = 0;
    for (size_t position = 0; position < ruby.base.size();) {
      position += utf8GraphemeLength(ruby.base, position);
      ++baseCount;
    }
    const int extra = static_cast<int>(annotationCount) - static_cast<int>(baseCount);
    if (extra <= 0) continue;
    std::shared_ptr<graphics::Font> layoutRubyFont = input.rubyFont;
    if ((ruby.fontScale != 0.6f || fittedScale < 1.0f) && input.typeface != nullptr) {
      layoutRubyFont =
        graphics::Font::create(input.typeface, (textSize * fittedScale) * ruby.fontScale);
    }
    int extraColumns = (extra + rowsPerColumn - 1) / rowsPerColumn;
    if (layoutRubyFont != nullptr) {
      std::vector<float> heights;
      for (size_t position = 0; position < ruby.annotation.size();) {
        const size_t length = utf8GraphemeLength(ruby.annotation, position);
        const auto blob = layoutRubyFont->shape(ruby.annotation.substr(position, length));
        const auto bounds = blob.bounds(layoutRubyFont->metrics().ascent,
          layoutRubyFont->metrics().descent);
        heights.push_back(std::max(1.0f, bounds.getHeight()));
        position += length;
      }
      const size_t firstExtra = heights.size() > static_cast<size_t>(extra)
                                  ? heights.size() - static_cast<size_t>(extra)
                                  : 0;
      const float availableHeight = input.availableHeight;
      int columnsNeeded = 0;
      float usedHeight = 0.0f;
      for (size_t index = firstExtra; index < heights.size(); ++index) {
        if (usedHeight > 0.0f && usedHeight + heights[index] > availableHeight) {
          ++columnsNeeded;
          usedHeight = 0.0f;
        }
        usedHeight += heights[index];
      }
      if (usedHeight > 0.0f) ++columnsNeeded;
      extraColumns = std::max(1, columnsNeeded);
    }
    if (ruby.position == AVCueSpan::RubyPosition::kAfter) {
      metrics.overflowAfter += extraColumns;
    } else {
      metrics.overflowBefore += extraColumns;
    }
  }

  int padColumns = metrics.overflowBefore + metrics.overflowAfter;
  for (int column = 0; column < columns; ++column) {
    if (metrics.before[static_cast<size_t>(column)]) ++padColumns;
    if (metrics.after[static_cast<size_t>(column)]) ++padColumns;
  }
  const float fixedWidth = columns * input.columnWidth;
  if (padColumns > 0 && fixedWidth + padColumns * metrics.pad > input.availableWidth) {
    const float fittedPad = (input.availableWidth - fixedWidth) / static_cast<float>(padColumns);
    if (fittedPad > 0.0f) metrics.pad = std::max(1.0f, fittedPad);
  }
  metrics.boxWidth = columns * input.columnWidth +
                     static_cast<float>(metrics.overflowBefore + metrics.overflowAfter) *
                       metrics.pad;
  for (int column = 0; column < columns; ++column) {
    if (metrics.before[static_cast<size_t>(column)]) metrics.boxWidth += metrics.pad;
    if (metrics.after[static_cast<size_t>(column)]) metrics.boxWidth += metrics.pad;
  }

  const float rubyPad = metrics.pad;
  const float columnWidth = input.columnWidth;
  metrics.baseX.assign(static_cast<size_t>(columns), 0.0f);
  metrics.beforeX.assign(static_cast<size_t>(columns), 0.0f);
  metrics.afterX.assign(static_cast<size_t>(columns), 0.0f);
  metrics.overflowBeforeX.assign(static_cast<size_t>(std::max(0, metrics.overflowBefore)), 0.0f);
  metrics.overflowAfterX.assign(static_cast<size_t>(std::max(0, metrics.overflowAfter)), 0.0f);
  if (input.leftToRight) {
    float cursor = 0.0f;
    for (int index = 0; index < metrics.overflowAfter; ++index) {
      metrics.overflowAfterX[static_cast<size_t>(index)] = cursor;
      cursor += rubyPad;
    }
    for (int column = 0; column < columns; ++column) {
      if (metrics.after[static_cast<size_t>(column)]) {
        metrics.afterX[static_cast<size_t>(column)] = cursor;
        cursor += rubyPad;
      }
      metrics.baseX[static_cast<size_t>(column)] = cursor;
      cursor += columnWidth;
      if (metrics.before[static_cast<size_t>(column)]) {
        metrics.beforeX[static_cast<size_t>(column)] = cursor;
        cursor += rubyPad;
      }
    }
    for (int index = 0; index < metrics.overflowBefore; ++index) {
      metrics.overflowBeforeX[static_cast<size_t>(index)] = cursor;
      cursor += rubyPad;
    }
  } else {
    float cursor = metrics.boxWidth;
    for (int index = 0; index < metrics.overflowBefore; ++index) {
      cursor -= rubyPad;
      metrics.overflowBeforeX[static_cast<size_t>(index)] = cursor;
    }
    for (int column = 0; column < columns; ++column) {
      if (metrics.before[static_cast<size_t>(column)]) {
        cursor -= rubyPad;
        metrics.beforeX[static_cast<size_t>(column)] = cursor;
      }
      metrics.baseX[static_cast<size_t>(column)] = cursor - columnWidth;
      cursor -= columnWidth;
      if (metrics.after[static_cast<size_t>(column)]) {
        cursor -= rubyPad;
        metrics.afterX[static_cast<size_t>(column)] = cursor;
      }
    }
    for (int index = 0; index < metrics.overflowAfter; ++index) {
      cursor -= rubyPad;
      metrics.overflowAfterX[static_cast<size_t>(index)] = cursor;
    }
  }
  return metrics;
}

void paintVerticalCue(graphics::Canvas& canvas, const AVCue& cue, const CueLayout& layout,
  float& y, const bool windowSet, const uint32_t windowColor, const PainterContext& context) {
  const graphics::Font& baseFont = *context.font;
  const SubtitleStyle& style = *context.style;
  auto verticalFont = context.font;
  const float availableWidth =
    static_cast<float>(context.width) - context.horizontalMargin * 2.0f;
  const float paragraphIndent =
    std::clamp(cue.paragraphIndent, 0.0f, 0.5f) * static_cast<float>(context.height);
  const float availableHeight =
    static_cast<float>(context.height) - context.bottomMargin * 2.0f - paragraphIndent;
  float verticalLineHeight =
    baseFont.metrics().ascent + baseFont.metrics().descent + baseFont.metrics().lineGap;
  float columnWidth = verticalLineHeight;
  int rowsPerColumn = std::max(1, static_cast<int>(availableHeight / verticalLineHeight));
  std::vector<std::string> characters;
  std::vector<size_t> characterOffsets;
  size_t textPosition = 0;
  while (textPosition < cue.text.size()) {
    const size_t length = utf8GraphemeLength(cue.text, textPosition);
    const std::string character = cue.text.substr(textPosition, length);
    if (character != "\n") {
      characters.push_back(character);
      characterOffsets.push_back(textPosition);
    }
    textPosition += length;
  }
  const int totalCharacters = static_cast<int>(characters.size());
  // A larger span must reserve a larger column/row metric, otherwise the glyph is drawn at its
  // bigger size inside a cell that was measured for the cue's base font.
  float layoutTextSize = context.textSize;
  for (const size_t offset: characterOffsets) {
    layoutTextSize = std::max(layoutTextSize, textSizeAt(cue, offset, context.textSize));
  }
  if (layoutTextSize > context.textSize + 0.01f && context.typeface != nullptr) {
    auto layoutFont = graphics::Font::create(context.typeface, layoutTextSize);
    if (layoutFont != nullptr) verticalFont = layoutFont;
  }
  const auto textLayout = layoutVerticalCue(*verticalFont, cue.text, availableWidth,
    availableHeight, verticalLineHeight);
  const auto& fitted = textLayout.vertical;
  const float fittedScale = fitted.fontScale;
  if (fittedScale < 1.0f && context.typeface != nullptr) {
    auto fittedFont = graphics::Font::create(context.typeface, layoutTextSize * fittedScale);
    if (fittedFont != nullptr) verticalFont = fittedFont;
  }
  verticalLineHeight = fitted.lineHeight;
  columnWidth = fitted.lineHeight;
  rowsPerColumn = fitted.rowsPerColumn;
  const int columns = fitted.columns;

  std::map<std::tuple<int, bool, bool, std::string>, std::shared_ptr<graphics::Font>> sizedFonts;
  const auto fontFor = [&](const size_t offset, const float size) -> const graphics::Font& {
    const AVCueSpan::TextStyle* spanStyle = textStyleAt(cue, offset);
    const bool bold = spanStyle != nullptr ? spanStyle->bold : context.fontBold;
    const bool italic = spanStyle != nullptr ? spanStyle->italic : context.fontItalic;
    const std::string family = spanStyle != nullptr && !spanStyle->fontFamily.empty()
                                 ? spanStyle->fontFamily
                                 : context.fontFamily;
    const std::tuple<int, bool, bool, std::string> key{
      static_cast<int>(std::lround(size * 100.0f)), bold, italic, family};
    const auto found = sizedFonts.find(key);
    if (found != sizedFonts.end()) return *found->second;
    std::shared_ptr<graphics::Font> sized;
    if (bold == context.fontBold && italic == context.fontItalic && family == context.fontFamily &&
        std::abs(size - layoutTextSize * fittedScale) <= 0.01f) {
      sized = verticalFont;
    } else {
      auto typeface = graphics::FontManager::shared().matchFamilyStyle(family,
        graphics::FontStyle(bold ? graphics::FontWeight::kBold : graphics::FontWeight::kNormal,
          italic ? graphics::FontStyle::Slant::Italic : graphics::FontStyle::Slant::Upright));
      if (typeface == nullptr) typeface = context.typeface;
      if (typeface != nullptr) sized = graphics::Font::create(typeface, size);
    }
    if (sized == nullptr) sized = verticalFont;
    sizedFonts[key] = sized;
    return *sized;
  };

  VerticalRubyInput rubyInput;
  rubyInput.rubies = &layout.rubies;
  rubyInput.characterOffsets = &characterOffsets;
  rubyInput.rowsPerColumn = rowsPerColumn;
  rubyInput.columns = columns;
  rubyInput.columnWidth = columnWidth;
  rubyInput.textSize = layoutTextSize;
  rubyInput.fittedScale = fittedScale;
  rubyInput.availableWidth = availableWidth;
  rubyInput.availableHeight = availableHeight;
  rubyInput.leftToRight = cue.verticalType == AVCue::VerticalType::kLeftToRight;
  rubyInput.rubyFont = context.rubyFont;
  rubyInput.typeface = context.typeface;
  const VerticalRubyMetrics metrics = measureVerticalRubies(rubyInput);

  const int visibleRows = std::min(totalCharacters, rowsPerColumn);
  const float boxHeight = visibleRows * verticalLineHeight;
  float boxTop = static_cast<float>(context.height) - context.bottomMargin - boxHeight;
  if (cue.line != AVCue::kDimenUnset) {
    boxTop =
      anchoredStart(cue.line * static_cast<float>(context.height), cue.lineAnchor, boxHeight);
  }
  const AVCue::Alignment columnAlignment =
    cue.multiRowAlignment != AVCue::Alignment::kUnset ? cue.multiRowAlignment
                                                      : cue.textAlignment;
  const auto columnTop = [&](const int column) {
    const int first = column * rowsPerColumn;
    const int count = std::clamp(totalCharacters - first, 0, rowsPerColumn);
    const float height = count * verticalLineHeight;
    switch (columnAlignment) {
      case AVCue::Alignment::kEnd:
      case AVCue::Alignment::kRight:
        return boxTop + boxHeight - height;
      case AVCue::Alignment::kCenter:
        return boxTop + (boxHeight - height) * 0.5f;
      case AVCue::Alignment::kStart:
      case AVCue::Alignment::kLeft:
      case AVCue::Alignment::kUnset:
      default:
        return boxTop;
    }
  };
  const float fitScale = std::min(1.0f, availableWidth / std::max(metrics.boxWidth, 1.0f));
  const float logicalWidth = static_cast<float>(context.width) / fitScale;
  float boxLeft = (logicalWidth - metrics.boxWidth) * 0.5f;
  if (cue.position != AVCue::kDimenUnset) {
    boxLeft = anchoredStart(positionOffsetFor(cue, static_cast<float>(context.width)),
      cue.positionAnchor, metrics.boxWidth);
  }
  const float logicalMargin = context.horizontalMargin / fitScale;
  boxLeft = std::clamp(boxLeft, logicalMargin,
    std::max(logicalMargin, logicalWidth - logicalMargin - metrics.boxWidth));

  graphics::Paint window;
  window.color = colorFromArgb(windowColor);
  window.antiAlias = true;

  canvas.save();
  if (fitScale < 1.0f) canvas.scale(fitScale, 1.0f);
  const bool sheared = std::abs(cue.shearDegrees) > 0.01f;
  if (sheared) {
    const float radians = cue.shearDegrees * 3.14159265358979323846f / 180.0f;
    const float anchorY = boxTop + boxHeight * 0.5f;
    canvas.save();
    canvas.translate(0.0f, anchorY);
    canvas.concat(graphics::Matrix::MakeSkew(std::tan(radians), 0.0f));
    canvas.translate(0.0f, -anchorY);
  }
  size_t combinedUntil = 0;
  for (int index = 0; index < totalCharacters; ++index) {
    const int characterColumn = index / rowsPerColumn;
    const int characterRow = index % rowsPerColumn;
    const float x = boxLeft + metrics.baseX[static_cast<size_t>(characterColumn)];
    const size_t characterOffset = characterOffsets[static_cast<size_t>(index)];
    if (characterOffset < combinedUntil) {
      continue;
    }
    const float characterSize = textSizeAt(cue, characterOffset, context.textSize) * fittedScale;
    const graphics::Font& characterFont = fontFor(characterOffset, characterSize);
    const float characterBaseline =
      columnTop(characterColumn) + characterFont.metrics().ascent +
      characterRow * verticalLineHeight;
    const AVCueSpan* combinedSpan = nullptr;
    for (const auto& span: cue.spans) {
      if (span.type == AVCueSpan::Type::kHorizontalTextInVerticalContext &&
          span.start <= characterOffset && characterOffset < span.end) {
        combinedSpan = &span;
        break;
      }
    }
    if (combinedSpan != nullptr) {
      std::string combined;
      for (size_t position = combinedSpan->start; position < combinedSpan->end;) {
        const size_t length = utf8GraphemeLength(cue.text, position);
        const std::string character = cue.text.substr(position, length);
        if (character != "\n") combined += character;
        position += length;
      }
      if (!combined.empty()) {
        const float width = characterFont.measureWidth(combined);
        std::shared_ptr<graphics::Font> combinedFont = context.font;
        if (context.typeface != nullptr && width > columnWidth) {
          combinedFont = graphics::Font::create(context.typeface,
            characterSize * columnWidth / std::max(width, 1.0f));
        }
        if (combinedFont != nullptr) {
          const auto blob = combinedFont->shape(combined);
          drawOutlinedBlob(canvas, blob, x, characterBaseline, *combinedFont,
            foregroundColorAt(cue, characterOffset, layout.textColor));
        }
      }
      combinedUntil = combinedSpan->end;
      continue;
    }
    if (windowSet) {
      canvas.drawRect(graphics::Rect::MakeLTRB(x,
                       characterBaseline - characterFont.metrics().ascent, x + columnWidth,
                       characterBaseline + characterFont.metrics().descent),
        window);
    }
    if (const uint32_t background = backgroundColorAt(cue, characterOffset); background != 0) {
      graphics::Paint paint;
      paint.color = colorFromArgb(background);
      paint.antiAlias = true;
      canvas.drawRect(graphics::Rect::MakeLTRB(x,
                       characterBaseline - characterFont.metrics().ascent, x + columnWidth,
                       characterBaseline + characterFont.metrics().descent),
        paint);
    }
    const auto blob = characterFont.shape(characters[static_cast<size_t>(index)]);
    const uint32_t characterColor = foregroundColorAt(cue, characterOffset, layout.textColor);
    const int align = verticalAlignAt(cue, characterOffset);
    const float shift = align < 0 ? characterFont.metrics().ascent * 0.5f
                      : align > 0 ? -characterFont.metrics().ascent * 0.5f
                                  : 0.0f;
    drawOutlinedBlob(canvas, blob, x, characterBaseline + shift, characterFont, characterColor);
    if (const AVCueSpan::Emphasis* emphasis = emphasisAt(cue, characterOffset);
        emphasis != nullptr) {
      const float markX = emphasis->mode == AVCueSpan::TextEmphasisMode::kBefore
                            ? x - 3.0f
                            : x + columnWidth + 3.0f;
      const float markY = characterBaseline - characterFont.metrics().ascent * 0.5f;
      drawEmphasisMark(canvas, markX, markY, *emphasis, characterColor);
    }
    bool underline = style.underline;
    bool strikeOut = style.strikeOut;
    const size_t decorationOffset = characterOffsets[static_cast<size_t>(index)];
    for (const auto& span: cue.spans) {
      if (span.type == AVCueSpan::Type::kTextStyle && span.start <= decorationOffset &&
          decorationOffset < span.end) {
        underline = span.style.underline;
        strikeOut = span.style.strikeout;
      }
    }
    if (underline || strikeOut) {
      graphics::Paint decoration;
      decoration.color = colorFromArgb(characterColor);
      if (underline) {
        canvas.drawRect(graphics::Rect::MakeLTRB(x - 2.0f,
                         characterBaseline - characterFont.metrics().ascent, x,
                         characterBaseline + characterFont.metrics().descent),
          decoration);
      }
      if (strikeOut) {
        const float mid = x + columnWidth * 0.5f;
        canvas.drawRect(graphics::Rect::MakeLTRB(mid - 1.0f,
                         characterBaseline - characterFont.metrics().ascent, mid + 1.0f,
                         characterBaseline + characterFont.metrics().descent),
          decoration);
      }
    }
  }

  int overflowBeforeUsed = 0;
  int overflowAfterUsed = 0;
  for (const auto& ruby: layout.rubies) {
    const auto found =
      std::lower_bound(characterOffsets.begin(), characterOffsets.end(), ruby.start);
    if (found == characterOffsets.end() || *found > ruby.start) continue;
    const size_t firstIndex = static_cast<size_t>(found - characterOffsets.begin());
    std::shared_ptr<graphics::Font> rubyFont = context.rubyFont;
    if ((ruby.fontScale != 0.6f || fittedScale < 1.0f) && context.typeface != nullptr) {
      rubyFont = graphics::Font::create(context.typeface,
        (layoutTextSize * fittedScale) * ruby.fontScale);
    }
    if (rubyFont == nullptr) continue;
    std::vector<std::string> annotationCharacters;
    size_t annotationPosition = 0;
    while (annotationPosition < ruby.annotation.size()) {
      const size_t length = utf8GraphemeLength(ruby.annotation, annotationPosition);
      const std::string character = ruby.annotation.substr(annotationPosition, length);
      if (character != "\n") annotationCharacters.push_back(character);
      annotationPosition += length;
    }
    const uint32_t rubyColor = ruby.color != 0 ? ruby.color : layout.textColor;
    auto drawRubyGlyph = [&](const std::string& character, const float x, const float baseline,
                             const uint32_t color) {
      std::shared_ptr<graphics::Font> glyphFont = rubyFont;
      if (context.typeface != nullptr && metrics.pad > 0.0f) {
        const float glyphWidth = rubyFont->measureWidth(character);
        if (glyphWidth > metrics.pad) {
          const float size = std::max(1.0f,
            (layoutTextSize * fittedScale) * ruby.fontScale * metrics.pad / glyphWidth);
          auto sized = graphics::Font::create(context.typeface, size);
          if (sized != nullptr) glyphFont = sized;
        }
      }
      if (glyphFont == nullptr) return;
      const auto blob = glyphFont->shape(character);
      drawOutlinedBlob(canvas, blob, x, baseline, *glyphFont, color);
    };
    const bool before = ruby.position != AVCueSpan::RubyPosition::kAfter;
    size_t baseIndex = firstIndex;
    size_t annotationIndex = 0;
    while (baseIndex < characterOffsets.size() &&
           characterOffsets[baseIndex] < ruby.end &&
           annotationIndex < annotationCharacters.size()) {
      const int baseColumn = static_cast<int>(baseIndex) / rowsPerColumn;
      const int baseRow = static_cast<int>(baseIndex) % rowsPerColumn;
      const float rubyX = boxLeft + (before
        ? metrics.beforeX[static_cast<size_t>(baseColumn)]
        : metrics.afterX[static_cast<size_t>(baseColumn)]);
      const float rubyBaseline =
        columnTop(baseColumn) + verticalFont->metrics().ascent +
        baseRow * verticalLineHeight;
      drawRubyGlyph(annotationCharacters[annotationIndex++], rubyX, rubyBaseline, rubyColor);
      ++baseIndex;
    }
    if (baseIndex > firstIndex && annotationIndex < annotationCharacters.size()) {
      const auto& positions = before ? metrics.overflowBeforeX : metrics.overflowAfterX;
      const int overflowStart = before ? overflowBeforeUsed : overflowAfterUsed;
      const int availableColumns = static_cast<int>(positions.size()) - overflowStart;
      if (availableColumns > 0) {
        int overflow = 0;
        float usedHeight = 0.0f;
        while (annotationIndex < annotationCharacters.size() &&
               overflow < availableColumns) {
          const auto blob = rubyFont->shape(annotationCharacters[annotationIndex]);
          const auto bounds =
            blob.bounds(rubyFont->metrics().ascent, rubyFont->metrics().descent);
          const float glyphHeight = std::max(1.0f, bounds.getHeight());
          if (usedHeight > 0.0f && usedHeight + glyphHeight > availableHeight) {
            ++overflow;
            usedHeight = 0.0f;
            continue;
          }
          const float rubyX =
            boxLeft + positions[static_cast<size_t>(overflowStart + overflow)];
          const float rubyBaseline = boxTop + rubyFont->metrics().ascent + usedHeight;
          drawRubyGlyph(annotationCharacters[annotationIndex], rubyX, rubyBaseline, rubyColor);
          usedHeight += glyphHeight;
          ++annotationIndex;
        }
        if (overflow > 0 || usedHeight > 0.0f) {
          const int columnsUsed = std::min(availableColumns, overflow + 1);
          if (before) overflowBeforeUsed += columnsUsed;
          else overflowAfterUsed += columnsUsed;
        }
      }
    }
  }
  if (sheared) {
    canvas.restore();
  }
  canvas.restore();
  y += boxHeight;
}

} // namespace detail
} // namespace coral
