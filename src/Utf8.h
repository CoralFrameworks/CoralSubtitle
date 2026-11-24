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

#ifndef CORAL_SUBTITLE_UTF8_H
#define CORAL_SUBTITLE_UTF8_H

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace coral {

/**
 * @return the number of bytes of the UTF-8 code point that starts with @p firstByte.
 *
 * <p>Every layer that walks cue text needs this: the wrapper, the vertical rune splitter and the
 * ruby scanner all used to carry their own copy, which is the kind of duplication where one copy
 * eventually disagrees with the others. A byte that is not a valid lead byte is treated as a
 * one-byte code point, so a malformed string still advances.
 */
inline size_t utf8CodePointLength(const char firstByte) {
  const auto value = static_cast<uint8_t>(firstByte);
  if ((value & 0x80) == 0) {
    return 1;
  }
  if ((value & 0xE0) == 0xC0) {
    return 2;
  }
  if ((value & 0xF0) == 0xE0) {
    return 3;
  }
  if ((value & 0xF8) == 0xF0) {
    return 4;
  }
  return 1;
}

/** @return the code point and byte length that starts at @p position, or U+FFFD on a malformed run. */
inline uint32_t utf8CodePointAt(const std::string_view text, const size_t position,
    size_t& length) {
  if (position >= text.size()) {
    length = 0;
    return 0;
  }
  const auto lead = static_cast<uint8_t>(text[position]);
  length = utf8CodePointLength(text[position]);
  if (length == 1 || position + length > text.size()) {
    return lead;
  }
  uint32_t codePoint = lead & (length == 2 ? 0x1Fu : length == 3 ? 0x0Fu : 0x07u);
  for (size_t index = 1; index < length; ++index) {
    const auto continuation = static_cast<uint8_t>(text[position + index]);
    if ((continuation & 0xC0) != 0x80) {
      length = index;
      return codePoint;
    }
    codePoint = (codePoint << 6u) | (continuation & 0x3Fu);
  }
  return codePoint;
}

inline bool isCombiningMark(const uint32_t codePoint) {
  return (codePoint >= 0x0300u && codePoint <= 0x036Fu) ||
         (codePoint >= 0x1AB0u && codePoint <= 0x1AFFu) ||
         (codePoint >= 0x1DC0u && codePoint <= 0x1DFFu) ||
         (codePoint >= 0x20D0u && codePoint <= 0x20FFu) ||
         (codePoint >= 0xFE20u && codePoint <= 0xFE2Fu);
}

inline bool isVariationSelector(const uint32_t codePoint) {
  return (codePoint >= 0xFE00u && codePoint <= 0xFE0Fu) ||
         (codePoint >= 0xE0100u && codePoint <= 0xE01EFu);
}

inline bool isEmojiModifier(const uint32_t codePoint) {
  return codePoint >= 0x1F3FBu && codePoint <= 0x1F3FFu;
}

/**
 * @return the byte length of the grapheme cluster that starts at @p position.
 *
 * <p>This is deliberately a small, dependency-free approximation of the extended grapheme cluster:
 * combining marks, variation selectors, skin-tone modifiers, keycaps, tag characters and ZWJ
 * sequences stay with their base. It is enough to keep a ZWJ emoji or an accented character from
 * being split by the subtitle line/column layout.
 */
inline size_t utf8GraphemeLength(const std::string_view text, const size_t position) {
  if (position >= text.size()) {
    return 0;
  }
  size_t length = 0;
  const uint32_t first = utf8CodePointAt(text, position, length);
  size_t end = position + length;
  const bool regionalIndicator = first >= 0x1F1E6u && first <= 0x1F1FFu;
  bool sawRegionalPartner = false;
  while (end < text.size()) {
    size_t nextLength = 0;
    const uint32_t next = utf8CodePointAt(text, end, nextLength);
    if (nextLength == 0) {
      break;
    }
    if (isCombiningMark(next) || isVariationSelector(next) || isEmojiModifier(next) ||
        next == 0x20E3u || (next >= 0xE0020u && next <= 0xE007Fu)) {
      end += nextLength;
      continue;
    }
    if (next == 0x200Du) {  // ZWJ joins the following code point.
      end += nextLength;
      if (end < text.size()) {
        size_t joinedLength = 0;
        utf8CodePointAt(text, end, joinedLength);
        end += joinedLength;
      }
      continue;
    }
    if (regionalIndicator && !sawRegionalPartner && next >= 0x1F1E6u && next <= 0x1F1FFu) {
      end += nextLength;
      sawRegionalPartner = true;
      continue;
    }
    break;
  }
  return end - position;
}

} // namespace coral

#endif // CORAL_SUBTITLE_UTF8_H
