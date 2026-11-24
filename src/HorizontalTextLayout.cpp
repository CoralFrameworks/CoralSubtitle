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

#include <subtitle/HorizontalTextLayout.h>

#include "Utf8.h"

#include <graphics/Font.h>

#include <algorithm>
#include <cstdint>

namespace coral {
namespace {

std::vector<std::string> wrapLine(const graphics::Font& font, const std::string& line,
                                  const float maxWidth) {
  std::vector<std::string> out;
  std::string current;
  size_t position = 0;
  while (position < line.size()) {
    size_t end = line.find(' ', position);
    if (end == std::string::npos) end = line.size();
    const std::string word = line.substr(position, end - position);
    const std::string candidate = current.empty() ? word : current + " " + word;
    if (font.measureWidth(candidate) <= maxWidth || current.empty()) {
      current = candidate;
    } else {
      out.push_back(current);
      current = word;
    }
    while (font.measureWidth(current) > maxWidth && current.size() > 1) {
      std::string piece;
      size_t byte = 0;
      while (byte < current.size()) {
        const size_t length = utf8GraphemeLength(current, byte);
        const std::string next = piece + current.substr(byte, length);
        if (font.measureWidth(next) > maxWidth && !piece.empty()) break;
        piece = next;
        byte += length;
      }
      if (piece.empty() || byte >= current.size()) break;
      out.push_back(piece);
      current = current.substr(byte);
    }
    position = end == line.size() ? end : end + 1;
  }
  if (!current.empty()) out.push_back(current);
  return out;
}

std::vector<std::string> splitLines(const std::string& text) {
  std::vector<std::string> lines;
  size_t start = 0;
  while (true) {
    const size_t end = text.find('\n', start);
    lines.push_back(text.substr(start, end == std::string::npos ? std::string::npos : end - start));
    if (end == std::string::npos) break;
    start = end + 1;
  }
  return lines;
}

} // namespace

HorizontalTextLayout layoutHorizontalText(const graphics::Font& font, const std::string_view text,
                                          const float maxWidth) {
  HorizontalTextLayout layout;
  const std::string document(text);
  for (const auto& logicalLine: splitLines(document)) {
    const auto wrapped = wrapLine(font, logicalLine, maxWidth);
    for (const auto& line: wrapped) {
      layout.width = std::max(layout.width, font.measureWidth(line));
      layout.lines.push_back(line);
    }
  }
  return layout;
}

} // namespace coral
