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

#ifndef CORAL_SUBTITLE_HORIZONTAL_TEXT_LAYOUT_H
#define CORAL_SUBTITLE_HORIZONTAL_TEXT_LAYOUT_H

#include <string>
#include <string_view>
#include <vector>

namespace graphics {
class Font;
} // namespace graphics

namespace coral {

/** The wrapped lines and measured width of a horizontal cue. */
struct HorizontalTextLayout {
  std::vector<std::string> lines;
  float width = 0.0f;
};

/**
 * Splits @p text at hard newlines and wraps each logical line to @p maxWidth using @p font.
 *
 * <p>This is the layout half of the horizontal renderer: it does not know about cue colours, ruby or
 * positioning, which keeps it reusable by other text consumers.
 */
HorizontalTextLayout layoutHorizontalText(const graphics::Font& font, std::string_view text,
                                          float maxWidth);

} // namespace coral

#endif //CORAL_SUBTITLE_HORIZONTAL_TEXT_LAYOUT_H
