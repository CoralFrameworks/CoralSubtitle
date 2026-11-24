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

#ifndef CORAL_SUBTITLE_HORIZONTAL_CUE_PAINTER_H
#define CORAL_SUBTITLE_HORIZONTAL_CUE_PAINTER_H

#include "CueLayout.h"
#include "SubtitlePainter.h"

namespace coral {
namespace detail {

/**
 * Draws one horizontal cue, wrapping already resolved by {@link measureCues}.
 *
 * @param boxLeft The left edge of the cue box.
 * @param top The top of the cue box; the first baseline is one ascent below it.
 * @param windowSet Whether the cue asks for a window behind its lines.
 * @param windowColor The window colour, as ARGB.
 */
void paintHorizontalCue(graphics::Canvas& canvas, const AVCue& cue, const CueLayout& layout,
  float boxLeft, float top, bool windowSet, uint32_t windowColor,
  const PainterContext& context);

} // namespace detail
} // namespace coral

#endif // CORAL_SUBTITLE_HORIZONTAL_CUE_PAINTER_H
