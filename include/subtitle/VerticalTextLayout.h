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

#ifndef CORAL_SUBTITLE_VERTICAL_TEXT_LAYOUT_H
#define CORAL_SUBTITLE_VERTICAL_TEXT_LAYOUT_H

#include <cstdint>

namespace coral {

/**
 * The geometry of a base column of vertical text, after fitting it to a viewport.
 *
 * <p>Vertical columns are as wide as one line height. When the natural column count does not fit the
 * viewport, the base font is scaled down and the columns are recomputed; a smaller line height fits
 * more characters per column and therefore needs fewer columns.
 */
struct VerticalTextLayout {
  /** The scale to apply to the base font size, in (0, 1]. */
  float fontScale = 1.0f;
  /** The scaled line height, which is also the width of one base column. */
  float lineHeight = 0.0f;
  /** Characters per base column at the fitted size. */
  int rowsPerColumn = 1;
  /** The number of base columns. */
  int columns = 1;
  /** The fitted width of all base columns. */
  float width = 0.0f;
};

/**
 * Fits @p characterCount vertical base characters into @p availableWidth by @p availableHeight.
 *
 * @param characterCount Number of base characters that need a cell.
 * @param availableWidth Width left for the vertical columns.
 * @param availableHeight Height left for one column.
 * @param initialLineHeight Line height at the cue's normal font size.
 *
 * @return The fitted font scale and column geometry. At least one character is always placed per
 *         column, and the scale never grows above one.
 */
VerticalTextLayout fitVerticalTextLayout(int characterCount, float availableWidth,
                                         float availableHeight, float initialLineHeight);

} // namespace coral

#endif //CORAL_SUBTITLE_VERTICAL_TEXT_LAYOUT_H
