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

#include <subtitle/TextLayout.h>

#include "Utf8.h"

#include <graphics/Font.h>

#include <cstdint>

namespace coral {
namespace {

int countVerticalCharacters(const std::string_view text) {
  int count = 0;
  size_t position = 0;
  while (position < text.size()) {
    const size_t length = utf8GraphemeLength(text, position);
    if (length != 1 || text[position] != '\n') ++count;
    position += length;
  }
  return count;
}

} // namespace

TextLayout layoutHorizontalCue(const graphics::Font& font, const std::string_view text,
                               const float maxWidth) {
  const auto horizontal = layoutHorizontalText(font, text, maxWidth);
  TextLayout layout;
  layout.direction = TextLayoutDirection::kHorizontal;
  layout.lines = horizontal.lines;
  layout.width = horizontal.width;
  return layout;
}

TextLayout layoutVerticalCue(const graphics::Font& font, const std::string_view text,
                             const float availableWidth, const float availableHeight,
                             const float initialLineHeight) {
  (void) font;
  TextLayout layout;
  layout.direction = TextLayoutDirection::kVertical;
  layout.vertical = fitVerticalTextLayout(countVerticalCharacters(text), availableWidth,
    availableHeight, initialLineHeight);
  layout.width = layout.vertical.width;
  return layout;
}

TextLayout layoutCueText(const TextLayoutDirection direction, const graphics::Font& font,
                         const std::string_view text, const float maxWidth,
                         const float availableHeight, const float initialLineHeight) {
  if (direction == TextLayoutDirection::kVertical) {
    return layoutVerticalCue(font, text, maxWidth, availableHeight, initialLineHeight);
  }
  return layoutHorizontalCue(font, text, maxWidth);
}

} // namespace coral
