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

#ifndef CORAL_SUBTITLE_RENDERER_H
#define CORAL_SUBTITLE_RENDERER_H

#include <subtitle/SubtitleStyle.h>

#include <extractor/text/Cue.h>
#include <extractor/text/CueGroup.h>

#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace graphics {
class Bitmap;
class Canvas;
class Font;
class Typeface;
} // namespace graphics

namespace coral {

/**
 * Rasterises the cues of one frame into an RGBA overlay with the canvas of the graphics module.
 *
 * <p>The counterpart of the {@code SubtitlePainter} of the platform players, built on the Canvas of
 * this repository instead of a platform text stack: the lines of every cue are wrapped, laid out in
 * the bottom of the frame (or where the cue says), and drawn with the colour, the window and the
 * weights that the cue and the {@link SubtitleStyle} of the item declare. The result is an image
 * whose pixels are blended over the composed frame, or over a decoded video frame by a player.
 *
 * <p>The image is cached: an unchanged cue group at the same size with the same style returns the
 * image of the previous frame, which is what keeps a still subtitle from costing a rasterisation per
 * frame.
 */
class SubtitleRenderer {
public:
  SubtitleRenderer();

  ~SubtitleRenderer();

  SubtitleRenderer(const SubtitleRenderer&) = delete;
  SubtitleRenderer& operator=(const SubtitleRenderer&) = delete;

  /**
   * Renders @p cues into a transparent image of @p width by @p height.
   *
   * @return The image, or null when the group is empty, the size is not usable, or the canvas could
   *         not be created.
   */
  std::shared_ptr<graphics::Bitmap> render(const AVCueGroup& cues, uint32_t width,
                                           uint32_t height, const SubtitleStyle& style,
                                           const TextDisplayOptions& options);

  /**
   * Paints the cues through @p canvas without allocating an image.
   *
   * The player that records its text for a GPU canvas calls this with its recorder canvas; the CPU
   * path calls it with the canvas over the bitmap it is about to cache.
   *
   * @return false when there is nothing to draw.
   */
  bool paint(graphics::Canvas& canvas, const AVCueGroup& cues, uint32_t width, uint32_t height,
    const SubtitleStyle& style, const TextDisplayOptions& options);

  /** Drops every cached image, e.g. when the style of the item changes. */
  void invalidate();

private:
  /** One rendered group, kept so that a pre-rendered group is a lookup when it is shown. */
  struct CachedImage {
    std::vector<AVCue> cues;
    uint32_t width = 0;
    uint32_t height = 0;
    SubtitleStyle style;
    TextDisplayOptions options;
    std::shared_ptr<graphics::Bitmap> image;
  };

  /** The most recently rendered groups, newest first. */
  std::deque<CachedImage> mCache;
  /** The font of the last render, kept because creating one re-scales and re-measures a typeface. */
  std::string mFontFamily;
  float mFontSize = 0.0f;
  bool mFontBold = false;
  bool mFontItalic = false;
  std::shared_ptr<graphics::Typeface> mTypeface;
  std::shared_ptr<graphics::Font> mFont;
  /** The smaller font used for ruby annotations. */
  std::shared_ptr<graphics::Font> mRubyFont;
};

} // namespace coral

#endif //CORAL_SUBTITLE_RENDERER_H
