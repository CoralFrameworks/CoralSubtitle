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

#ifndef CORAL_SUBTITLE_TEXT_LAYOUT_H
#define CORAL_SUBTITLE_TEXT_LAYOUT_H

#include <subtitle/HorizontalTextLayout.h>
#include <subtitle/VerticalTextLayout.h>

#include <string>
#include <string_view>
#include <vector>

namespace graphics {
class Font;
}

namespace coral {

/** Which axis the text flows along. */
enum class TextLayoutDirection {
  kHorizontal,
  kVertical,
};

/**
 * The direction-neutral result of laying out one cue.
 *
 * <p>Horizontal results carry wrapped lines; vertical results carry the fitted base-column geometry
 * in @c vertical. Consumers switch on @c direction instead of calling two unrelated layout APIs.
 */
struct TextLayout {
  TextLayoutDirection direction = TextLayoutDirection::kHorizontal;
  std::vector<std::string> lines;
  float width = 0.0f;
  VerticalTextLayout vertical;
};

/** Lays out horizontal text into wrapped lines. */
TextLayout layoutHorizontalCue(const graphics::Font& font, std::string_view text, float maxWidth);

/** Lays out vertical text and fits the base columns into the viewport. */
TextLayout layoutVerticalCue(const graphics::Font& font, std::string_view text,
                             float availableWidth, float availableHeight,
                             float initialLineHeight);

/** Dispatches to the horizontal or vertical implementation. */
TextLayout layoutCueText(TextLayoutDirection direction, const graphics::Font& font,
                         std::string_view text, float maxWidth, float availableHeight,
                         float initialLineHeight);

} // namespace coral

#endif //CORAL_SUBTITLE_TEXT_LAYOUT_H
