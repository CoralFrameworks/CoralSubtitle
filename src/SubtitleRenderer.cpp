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

#include <subtitle/SubtitleRenderer.h>

#include "CueLayout.h"
#include "HorizontalCuePainter.h"
#include "SubtitlePainter.h"
#include "VerticalCuePainter.h"

#include <graphics/Bitmap.h>
#include <graphics/Canvas.h>
#include <graphics/Data.h>
#include <graphics/Font.h>
#include <graphics/FontManager.h>
#include <graphics/FontStyle.h>
#include <graphics/Image.h>
#include <graphics/Paint.h>
#include <graphics/Pixmap.h>

#include <minilog/log.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace coral {
namespace {

using graphics::Bitmap;
using graphics::Canvas;
using graphics::Font;
using graphics::FontManager;
using graphics::FontStyle;

constexpr float kDefaultTextFraction = 0.0533f;
constexpr float kDefaultBottomFraction = 0.05f;

} // namespace

SubtitleRenderer::SubtitleRenderer() = default;

SubtitleRenderer::~SubtitleRenderer() = default;

void SubtitleRenderer::invalidate() {
  mCache.clear();
}

bool SubtitleRenderer::paint(graphics::Canvas& canvas, const AVCueGroup& cues,
  const uint32_t width, const uint32_t height, const SubtitleStyle& style,
  const TextDisplayOptions& options) {
  FontStyle fontStyle(FontStyle::Normal());
  if (style.bold || style.italic) {
    fontStyle = FontStyle(style.bold ? 700 : 400,
      style.italic ? FontStyle::Slant::Italic : FontStyle::Slant::Upright);
  }
  const std::string& family = style.fontFamily;
  const float textSize = style.fontSize > 0.0f ? style.fontSize
                                               : std::max(12.0f, height * kDefaultTextFraction);
  if (mFont == nullptr || family != mFontFamily || textSize != mFontSize ||
      style.bold != mFontBold || style.italic != mFontItalic) {
    auto typeface = FontManager::shared().matchFamilyStyle(family, fontStyle);
    if (typeface == nullptr) {
      typeface = FontManager::shared().defaultTypeface();
    }
    if (typeface == nullptr) {
      MINILOG_WARN("the subtitle renderer found no typeface");
      return false;
    }
    mTypeface = typeface;
    mFont = Font::create(typeface, textSize);
    mRubyFont = Font::create(typeface, textSize * 0.6f);
    mFontFamily = family;
    mFontSize = textSize;
    mFontBold = style.bold;
    mFontItalic = style.italic;
  }
  const auto fontPtr = mFont;
  if (fontPtr == nullptr) {
    return false;
  }

  const float lineHeight = fontPtr->metrics().ascent + fontPtr->metrics().descent +
                           fontPtr->metrics().lineGap;
  const float horizontalMargin =
    options.horizontalMargin > 0
      ? static_cast<float>(options.horizontalMargin)
      : std::max(8.0f, width * 0.04f);
  const float maxWidth = std::max(1.0f, static_cast<float>(width) - horizontalMargin * 2.0f);
  const float bottomMargin = options.bottomMargin > 0
                               ? static_cast<float>(options.bottomMargin)
                               : height * kDefaultBottomFraction;

  // Bitmap cues are independent of the text layout. TTML carries encoded images that the graphics
  // codec decodes; PGS/VobSub/DVB carry RGBA pixels directly. Draw both at the position the cue
  // states, falling back to the full frame for a bitmap that has no position.
  bool drewBitmap = false;
  const auto drawPicture = [&](const AVCue& cue, const graphics::Image& image) {
    if (image.isEmpty() || image.width() <= 0 || image.height() <= 0) {
      return;
    }
    const bool rawPixels = !cue.bitmap.pixels.empty();
    const float pictureWidth = cue.size != AVCue::kDimenUnset
                                 ? cue.size * static_cast<float>(width)
                                 : static_cast<float>(image.width());
    const float pictureHeight = cue.bitmapHeight != AVCue::kDimenUnset
                                  ? cue.bitmapHeight * static_cast<float>(height)
                                  : static_cast<float>(image.height());
    const float x = cue.position != AVCue::kDimenUnset
                      ? detail::positionOffsetFor(cue, static_cast<float>(width))
                      : rawPixels ? 0.0f
                                  : (static_cast<float>(width) - pictureWidth) * 0.5f;
    const float y = cue.line != AVCue::kDimenUnset
                      ? detail::lineOffsetFor(cue, static_cast<float>(height), lineHeight)
                      : rawPixels ? 0.0f
                                  : (static_cast<float>(height) - pictureHeight) * 0.5f;
    graphics::Paint paint;
    canvas.save();
    canvas.translate(x, y);
    canvas.scale(pictureWidth / static_cast<float>(image.width()),
      pictureHeight / static_cast<float>(image.height()));
    canvas.drawImage(image, 0.0f, 0.0f, paint);
    canvas.restore();
    drewBitmap = true;
  };
  for (const auto& cue: cues.getCues()) {
    if (!cue.isBitmap()) {
      continue;
    }
    if (!cue.bitmap.encoded.empty()) {
      const auto data = graphics::Data::makeCopy(cue.bitmap.encoded);
      drawPicture(cue, graphics::Image::decode(data));
      continue;
    }
    if (cue.bitmap.pixels.empty() || cue.bitmap.width <= 0 || cue.bitmap.height <= 0) {
      continue;
    }
    const auto colorType = cue.bitmap.format == PixelFormat::kFormatBGRA
                             ? graphics::ColorType::kBGRA_8888
                             : graphics::ColorType::kRGBA_8888;
    const auto alphaType = cue.bitmap.premultiplied ? graphics::AlphaType::kPremultiplied
                                                    : graphics::AlphaType::kUnpremultiplied;
    const graphics::ImageInfo info(cue.bitmap.width, cue.bitmap.height, colorType, alphaType);
    const auto data = graphics::Data::makeCopy(cue.bitmap.pixels);
    drawPicture(cue, graphics::Image::fromData(info, data, cue.bitmap.rowBytes));
  }

  const std::vector<CueLayout> layouts =
    measureCues(cues, fontPtr, mTypeface, maxWidth, width, height, bottomMargin, textSize, style);
  if (layouts.empty()) {
    return drewBitmap;
  }
  float totalHeight = 0.0f;
  for (const auto& layout: layouts) {
    totalHeight += layout.lineHeight * static_cast<float>(layout.lines.size());
  }

  detail::PainterContext context;
  context.typeface = mTypeface;
  context.font = mFont;
  context.rubyFont = mRubyFont;
  context.textSize = textSize;
  context.width = width;
  context.height = height;
  context.horizontalMargin = horizontalMargin;
  context.bottomMargin = bottomMargin;
  context.lineHeight = lineHeight;
  context.style = &style;
  context.fontFamily = mFontFamily;
  context.fontBold = mFontBold;
  context.fontItalic = mFontItalic;

  float y = static_cast<float>(height) - bottomMargin - totalHeight;
  for (const auto& layout: layouts) {
    const AVCue& cue = *layout.cue;
    detail::PainterContext cueContext = context;
    cueContext.font = layout.font;
    cueContext.rubyFont = layout.rubyFont != nullptr ? layout.rubyFont : context.rubyFont;
    cueContext.textSize = layout.textSize;
    cueContext.lineHeight = layout.lineHeight;
    // `line` is a viewport fraction unless the parser marked it as a line number.
    if (cue.line != AVCue::kDimenUnset) {
      y = detail::anchoredStart(
        detail::lineOffsetFor(cue, static_cast<float>(height), layout.lineHeight),
        cue.lineAnchor, totalHeight);
    }
    float boxLeft = (static_cast<float>(width) - layout.width) * 0.5f;
    if (cue.position != AVCue::kDimenUnset) {
      boxLeft = detail::anchoredStart(detail::positionOffsetFor(cue, static_cast<float>(width)),
        cue.positionAnchor, layout.width);
    }
    boxLeft = std::clamp(boxLeft, horizontalMargin,
      std::max(horizontalMargin, static_cast<float>(width) - horizontalMargin - layout.width));
    // A blockquote inset shifts the text block right; the wrapping width was already narrowed in
    // measureCues(), so this cannot push the text past the right margin.
    boxLeft = std::clamp(boxLeft + layout.paragraphIndent, horizontalMargin,
      std::max(horizontalMargin, static_cast<float>(width) - horizontalMargin - layout.width));

    const bool windowSet = cue.windowColorSet || options.windowEnabled;
    const uint32_t windowColor = cue.windowColorSet ? cue.windowColor : options.windowColor;

    if (cue.verticalType != AVCue::VerticalType::kUnset) {
      detail::paintVerticalCue(canvas, cue, layout, y, windowSet, windowColor, cueContext);
      continue;
    }
    detail::paintHorizontalCue(canvas, cue, layout, boxLeft, y, windowSet, windowColor, cueContext);
    y += layout.lineHeight * static_cast<float>(layout.lines.size());
  }
  return true;
}

std::shared_ptr<graphics::Bitmap> SubtitleRenderer::render(const AVCueGroup& cues,
                                                            const uint32_t width,
                                                            const uint32_t height,
                                                            const SubtitleStyle& style,
                                                            const TextDisplayOptions& options) {
  if (cues.isEmpty() || width == 0 || height == 0) {
    return nullptr;
  }
  const auto& cueList = cues.getCues();
  for (auto it = mCache.begin(); it != mCache.end(); ++it) {
    if (it->width == width && it->height == height && it->style == style &&
        it->options == options && it->cues == cueList) {
      // A hit is moved to the front, so the group that is about to be shown is the last evicted.
      if (it != mCache.begin()) {
        CachedImage hit = std::move(*it);
        mCache.erase(it);
        mCache.push_front(std::move(hit));
      }
      return mCache.front().image;
    }
  }

  Bitmap bitmap = Bitmap::allocate(static_cast<int>(width), static_cast<int>(height));
  if (bitmap.isEmpty()) {
    return nullptr;
  }
  auto canvas = Canvas::create(bitmap);
  if (canvas == nullptr) {
    return nullptr;
  }

  // `paint` selects the font as well, so the cache and the bitmap path share one implementation.
  if (!paint(*canvas, cues, width, height, style, options)) {
    return nullptr;
  }

  auto pixels = bitmap.lockPixels();
  if (pixels.addr(0, 0) == nullptr) {
    return nullptr;
  }
  auto image = std::make_shared<Bitmap>(std::move(bitmap));

  CachedImage entry;
  entry.cues = cueList;
  entry.width = width;
  entry.height = height;
  entry.style = style;
  entry.options = options;
  entry.image = image;
  mCache.push_front(std::move(entry));
  // A full frame per cache entry is expensive at 4K (about 33 MB for RGBA). Keep the newest
  // frames but bound the total so a 4K player does not hold three full frames for a subtitle that
  // is mostly transparent.
  constexpr size_t kCacheBudgetBytes = 32u * 1024u * 1024u;
  const auto cacheBytes = [this] {
    size_t bytes = 0;
    for (const auto& cached: mCache) {
      bytes += static_cast<size_t>(cached.width) * static_cast<size_t>(cached.height) * 4u;
    }
    return bytes;
  };
  while (mCache.size() > 1 && cacheBytes() > kCacheBudgetBytes) {
    mCache.pop_back();
  }
  return image;
}

} // namespace coral
