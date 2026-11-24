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

#include <subtitle/VerticalTextLayout.h>

#include <algorithm>
#include <cmath>

namespace coral {
namespace {

constexpr int kMaxFitIterations = 8;

} // namespace

VerticalTextLayout fitVerticalTextLayout(const int characterCount, const float availableWidth,
                                         const float availableHeight,
                                         const float initialLineHeight) {
  VerticalTextLayout layout;
  if (characterCount <= 0 || availableHeight <= 0.0f || initialLineHeight <= 0.0f) {
    layout.lineHeight = std::max(1.0f, initialLineHeight);
    return layout;
  }

  layout.lineHeight = initialLineHeight;
  for (int iteration = 0; iteration < kMaxFitIterations; ++iteration) {
    layout.rowsPerColumn = std::max(1,
      static_cast<int>(std::floor(availableHeight / layout.lineHeight)));
    layout.columns = std::max(1,
      (characterCount + layout.rowsPerColumn - 1) / layout.rowsPerColumn);
    layout.width = layout.columns * layout.lineHeight;
    if (availableWidth <= 0.0f || layout.width <= availableWidth) break;

    const float fittedScale = availableWidth / layout.width;
    layout.fontScale = std::clamp(layout.fontScale * fittedScale, 0.01f, 1.0f);
    layout.lineHeight = std::max(1.0f, initialLineHeight * layout.fontScale);
  }
  return layout;
}

} // namespace coral
