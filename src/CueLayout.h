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

#ifndef CORAL_SUBTITLE_CUE_LAYOUT_H
#define CORAL_SUBTITLE_CUE_LAYOUT_H

#include <subtitle/SubtitleStyle.h>

#include <extractor/text/Cue.h>
#include <extractor/text/CueGroup.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace graphics {
class Font;
class Typeface;
} // namespace graphics

namespace coral {

/** One ruby annotation and the range of the cue text it annotates. */
struct RubyRun final {
  size_t start = 0;
  size_t end = 0;
  std::string base;
  std::string annotation;
  AVCueSpan::RubyPosition position = AVCueSpan::RubyPosition::kUnknown;
  float fontScale = 0.6f;
  uint32_t color = 0;
};

/** The measured text of one cue: its wrapped lines, ruby runs and colours. */
struct CueLayout final {
  const AVCue* cue = nullptr;
  std::vector<std::string> lines;
  std::vector<RubyRun> rubies;
  float width = 0.0f;
  /** The resolved cue-level text size in pixels. */
  float textSize = 0.0f;
  /** The line height of the resolved cue font, in pixels. */
  float lineHeight = 0.0f;
  /** The font of this cue, at the cue-level text size. */
  std::shared_ptr<graphics::Font> font;
  /** The ruby font of this cue, at 60% of the cue-level text size. */
  std::shared_ptr<graphics::Font> rubyFont;
  /** The left inset of the cue, in pixels, already subtracted from the wrapping width. */
  float paragraphIndent = 0.0f;
  uint32_t textColor = 0xFFFFFFFFu;
};

/**
 * Resolves the cue-level text size.
 *
 * <p>`textSizeType` follows the media3 Cue semantics: fractional sizes are a fraction of the
 * viewport height, or of the height minus vertical padding for `kFractional`.
 */
float resolveCueTextSize(const AVCue& cue, float defaultTextSize, uint32_t viewportHeight,
  float verticalPadding);

/** @return the colour of the cue text before any span overrides it. */
uint32_t foregroundColorOf(const AVCue& cue, const SubtitleStyle& style);

/** @return the colour of the character at @p offset, or @p fallback when no span covers it. */
uint32_t foregroundColorAt(const AVCue& cue, size_t offset, uint32_t fallback);

/**
 * Wraps and measures every text cue of @p cues, in the order they are drawn.
 *
 * <p>Bitmap cues and cues without text are skipped. The ruby runs of a cue are collected here as
 * well, so both the horizontal and the vertical painter read the same measurement instead of
 * walking the spans again.
 *
 * @param font The cue font.
 * @param maxWidth The widest line the viewport allows, in pixels.
 * @param viewportWidth The viewport width, which {@link AVCue::size} is a fraction of.
 * @param textSize The cue font size in pixels, used to resolve absolute span sizes.
 */
std::vector<CueLayout> measureCues(const AVCueGroup& cues,
  const std::shared_ptr<graphics::Font>& font,
  const std::shared_ptr<graphics::Typeface>& typeface, float maxWidth, uint32_t viewportWidth,
  uint32_t viewportHeight, float verticalPadding, float textSize, const SubtitleStyle& style);

} // namespace coral

#endif // CORAL_SUBTITLE_CUE_LAYOUT_H
