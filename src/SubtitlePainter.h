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

#ifndef CORAL_SUBTITLE_SUBTITLE_PAINTER_H
#define CORAL_SUBTITLE_SUBTITLE_PAINTER_H

#include <subtitle/SubtitleStyle.h>

#include <extractor/text/Cue.h>

#include <graphics/Color.h>

#include <cstdint>
#include <memory>
#include <string>

namespace graphics {
class Canvas;
class Font;
class TextBlob;
class Typeface;
} // namespace graphics

namespace coral {
namespace detail {

/** The width of the black outline that the default text look draws around a glyph. */
constexpr float kOutlineWidth = 2.0f;

/**
 * What every cue painter of one frame shares: the selected fonts and the measurements of the
 * viewport. The per-cue parts (the cue, its layout, its window) stay parameters of the painters.
 */
struct PainterContext final {
  std::shared_ptr<graphics::Typeface> typeface;
  std::shared_ptr<graphics::Font> font;
  std::shared_ptr<graphics::Font> rubyFont;
  /** The cue font size in pixels; also the base that relative span sizes are a fraction of. */
  float textSize = 0.0f;
  uint32_t width = 0;
  uint32_t height = 0;
  float horizontalMargin = 0.0f;
  float bottomMargin = 0.0f;
  float lineHeight = 0.0f;
  const SubtitleStyle* style = nullptr;
  /** The base family and face style of `font`, used to resolve span-level attributes. */
  std::string fontFamily;
  bool fontBold = false;
  bool fontItalic = false;
};

/** @return @p argb as a premultiplied graphics colour. */
graphics::Color colorFromArgb(uint32_t argb);

/** @return the start of a box of @p size that is anchored at @p anchor. */
float anchoredStart(float anchor, AVCue::AnchorType type, float size);

/**
 * @return the pixel offset of `cue.line`.
 *
 * <p>A fraction is relative to the viewport height, while a line number is relative to the resolved
 * cue line height. The two units must not be mixed, which is what a WebVTT `line:3` cue expects.
 */
float lineOffsetFor(const AVCue& cue, float viewportHeight, float lineHeight);

/** @return the pixel offset of `cue.position`, using its fraction/absolute unit. */
float positionOffsetFor(const AVCue& cue, float viewportWidth);

/**
 * @return -1 for a subscript, 1 for a superscript and 0 for normal text at @p offset.
 *
 * <p>The renderers lower or raise the baseline by half an ascent, which is the shift the Android
 * `SubscriptSpan` applies to a text paint.
 */
int verticalAlignAt(const AVCue& cue, size_t offset);

/**
 * @return the text size in pixels of the run that contains @p offset, taking the span's text size
 *     relative to @p baseSize (or absolute) into account.
 */
float textSizeAt(const AVCue& cue, size_t offset, float baseSize);

/** @return the emphasis span that covers @p offset, or {@code nullptr}. */
const AVCueSpan::Emphasis* emphasisAt(const AVCue& cue, size_t offset);

/** @return the text-style span that covers @p offset, or {@code nullptr}. */
const AVCueSpan::TextStyle* textStyleAt(const AVCue& cue, size_t offset);

/** @return the background colour of @p offset, or 0 when no span sets one. */
uint32_t backgroundColorAt(const AVCue& cue, size_t offset);

/** Draws one emphasis mark centred at (@p centreX, @p markY). */
void drawEmphasisMark(graphics::Canvas& canvas, float centreX, float markY,
  const AVCueSpan::Emphasis& emphasis, uint32_t fallbackColor);

/** Draws @p blob as black outline plus a fill of @p fillColor, the subtitle's default text look. */
void drawOutlinedBlob(graphics::Canvas& canvas, const graphics::TextBlob& blob, float x,
  float baseline, const graphics::Font& font, uint32_t fillColor);

} // namespace detail
} // namespace coral

#endif // CORAL_SUBTITLE_SUBTITLE_PAINTER_H
