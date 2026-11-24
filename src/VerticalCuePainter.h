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

#ifndef CORAL_SUBTITLE_VERTICAL_CUE_PAINTER_H
#define CORAL_SUBTITLE_VERTICAL_CUE_PAINTER_H

#include "CueLayout.h"
#include "SubtitlePainter.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace coral {
namespace detail {

/** Everything {@link measureVerticalRubies} needs to place the annotations of one vertical cue. */
struct VerticalRubyInput final {
  const std::vector<RubyRun>* rubies = nullptr;
  const std::vector<size_t>* characterOffsets = nullptr;
  int rowsPerColumn = 1;
  int columns = 1;
  float columnWidth = 0.0f;
  /** The cue font size in pixels. */
  float textSize = 0.0f;
  /** The scale the base font was fitted to; 1 when the cue fits at its natural size. */
  float fittedScale = 1.0f;
  float availableWidth = 0.0f;
  float availableHeight = 0.0f;
  bool leftToRight = false;
  std::shared_ptr<graphics::Font> rubyFont;
  std::shared_ptr<graphics::Typeface> typeface;
};

/**
 * The geometry of the ruby columns of one vertical cue.
 *
 * <p>It is a pure result: the annotation padding, which base columns carry a ruby before or after
 * them, the extra columns needed by annotations longer than their base, the box width, and the x of
 * every base and ruby column. The painter only adds {@link #boxLeft} and draws, which is what makes
 * the arithmetic testable without a canvas.
 */
struct VerticalRubyMetrics final {
  float pad = 0.0f;
  int overflowBefore = 0;
  int overflowAfter = 0;
  float boxWidth = 0.0f;
  std::vector<bool> before;
  std::vector<bool> after;
  std::vector<float> baseX;
  std::vector<float> beforeX;
  std::vector<float> afterX;
  std::vector<float> overflowBeforeX;
  std::vector<float> overflowAfterX;
};

/** @return the ruby geometry of one vertical cue, without drawing it. */
VerticalRubyMetrics measureVerticalRubies(const VerticalRubyInput& input);

/**
 * Draws one vertical cue and advances @p y past its box.
 *
 * @param y The running top of the cue boxes; the painter leaves it below the box it drew, which is
 *          where the next cue without an explicit line starts.
 * @param windowSet Whether the cue asks for a window behind its characters.
 * @param windowColor The window colour, as ARGB.
 */
void paintVerticalCue(graphics::Canvas& canvas, const AVCue& cue, const CueLayout& layout, float& y,
  bool windowSet, uint32_t windowColor, const PainterContext& context);

} // namespace detail
} // namespace coral

#endif // CORAL_SUBTITLE_VERTICAL_CUE_PAINTER_H
