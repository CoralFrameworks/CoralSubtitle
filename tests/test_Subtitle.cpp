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

// =============================================================
//  tests/test_Subtitle.cpp
//
//  The renderer and the layout helpers. The suite covers the layer a consumer sees: a cue group is
//  rasterised into a transparent overlay with the default look, the overlay is cached for an
//  unchanged group, and the two layout helpers wrap and fit text without a canvas.
// =============================================================

#include <subtitle/HorizontalTextLayout.h>
#include <subtitle/SubtitleRenderer.h>
#include <subtitle/SubtitleStyle.h>
#include <subtitle/VerticalTextLayout.h>

#include <graphics/Bitmap.h>
#include <graphics/Font.h>
#include <graphics/FontManager.h>
#include <graphics/Pixmap.h>

#include <extractor/text/Cue.h>
#include <extractor/text/CueGroup.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

using coral::AVCue;
using coral::AVCueGroup;
using coral::HorizontalTextLayout;
using coral::SubtitleRenderer;
using coral::SubtitleStyle;
using coral::TextDisplayOptions;
using coral::VerticalTextLayout;

AVCueGroup makeGroup(std::string text, int64_t presentationTimeUs = 0) {
  AVCue cue;
  cue.text = std::move(text);
  std::vector<AVCue> cues;
  cues.push_back(std::move(cue));
  return AVCueGroup(std::move(cues), presentationTimeUs);
}

struct Pixel {
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
  uint8_t a = 0;
};

Pixel pixelAt(const graphics::Bitmap& image, const int x, const int y) {
  const auto pixels = image.makePixmap();
  const auto* bytes = static_cast<const uint8_t*>(pixels.addr(x, y));
  if (bytes == nullptr) {
    return Pixel{};
  }
  return Pixel{bytes[0], bytes[1], bytes[2], bytes[3]};
}

/** @return the number of pixels whose alpha is not zero. */
int countOpaquePixels(const graphics::Bitmap& image) {
  int count = 0;
  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      if (pixelAt(image, x, y).a != 0) {
        ++count;
      }
    }
  }
  return count;
}

/** @return the topmost row that holds a drawn pixel, or -1 when the image is empty. */
int firstOpaqueRow(const graphics::Bitmap& image) {
  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      if (pixelAt(image, x, y).a != 0) {
        return y;
      }
    }
  }
  return -1;
}

std::shared_ptr<graphics::Font> makeFont(const float size) {
  const auto typeface = graphics::FontManager::shared().defaultTypeface();
  if (typeface == nullptr) {
    return nullptr;
  }
  return graphics::Font::create(typeface, size);
}

} // namespace

TEST(SubtitleRendererTest, RendersTheDefaultLookIntoATransparentOverlay) {
  SubtitleRenderer renderer;
  const auto image = renderer.render(makeGroup("Hello"), 320, 240, SubtitleStyle{},
    TextDisplayOptions{});
  ASSERT_NE(nullptr, image);
  EXPECT_EQ(320, image->width());
  EXPECT_EQ(240, image->height());
  EXPECT_EQ(0, pixelAt(*image, 0, 0).a) << "the corner of an overlay stays transparent";
  EXPECT_GT(countOpaquePixels(*image), 0);
  EXPECT_GT(firstOpaqueRow(*image), 120) << "the default look draws at the bottom of the frame";
}

TEST(SubtitleRendererTest, ReturnsNoImageForAnEmptyGroup) {
  SubtitleRenderer renderer;
  EXPECT_EQ(nullptr, renderer.render(AVCueGroup{}, 320, 240, SubtitleStyle{},
                                     TextDisplayOptions{}));
}

TEST(SubtitleRendererTest, ReturnsTheCachedImageForAnUnchangedGroup) {
  SubtitleRenderer renderer;
  const AVCueGroup group = makeGroup("Cached");
  const auto first = renderer.render(group, 320, 240, SubtitleStyle{}, TextDisplayOptions{});
  const auto second = renderer.render(group, 320, 240, SubtitleStyle{}, TextDisplayOptions{});
  ASSERT_NE(nullptr, first);
  ASSERT_NE(nullptr, second);
  EXPECT_EQ(first.get(), second.get()) << "an unchanged group must not be rasterised again";
}

TEST(SubtitleRendererTest, InvalidatingDropsTheCachedImage) {
  SubtitleRenderer renderer;
  const AVCueGroup group = makeGroup("Cached");
  const auto first = renderer.render(group, 320, 240, SubtitleStyle{}, TextDisplayOptions{});
  renderer.invalidate();
  const auto second = renderer.render(group, 320, 240, SubtitleStyle{}, TextDisplayOptions{});
  ASSERT_NE(nullptr, first);
  ASSERT_NE(nullptr, second);
  EXPECT_NE(first.get(), second.get());
}

TEST(SubtitleRendererTest, RendersABitmapCueWithoutAFont) {
  SubtitleRenderer renderer;
  AVCue cue;
  cue.bitmap.width = 2;
  cue.bitmap.height = 2;
  cue.bitmap.format = coral::PixelFormat::kFormatRGBA;
  cue.bitmap.premultiplied = true;
  cue.bitmap.rowBytes = 8;
  cue.bitmap.pixels = std::vector<uint8_t>(16, 0xFF);
  std::vector<AVCue> cues;
  cues.push_back(std::move(cue));
  const auto image = renderer.render(AVCueGroup(std::move(cues), 0), 320, 240, SubtitleStyle{},
    TextDisplayOptions{});
  ASSERT_NE(nullptr, image);
  EXPECT_GT(countOpaquePixels(*image), 0);
}

TEST(HorizontalTextLayoutTest, WrapsTextToTheAvailableWidth) {
  const auto font = makeFont(20.0f);
  if (font == nullptr) {
    GTEST_SKIP() << "this build has no font stack";
  }
  const HorizontalTextLayout layout = coral::layoutHorizontalText(
    *font, "the quick brown fox jumps over the lazy dog", 120.0f);
  ASSERT_GT(layout.lines.size(), 1u);
  for (const auto& line: layout.lines) {
    if (layout.lines.size() > 1) {
      EXPECT_LE(font->measureWidth(line), 120.0f + font->measureWidth(" "));
    }
  }
  EXPECT_GT(layout.width, 0.0f);
}

TEST(HorizontalTextLayoutTest, KeepsHardLineBreaks) {
  const auto font = makeFont(20.0f);
  if (font == nullptr) {
    GTEST_SKIP() << "this build has no font stack";
  }
  const HorizontalTextLayout layout =
    coral::layoutHorizontalText(*font, "first\nsecond", 400.0f);
  ASSERT_EQ(2u, layout.lines.size());
  EXPECT_EQ("first", layout.lines[0]);
  EXPECT_EQ("second", layout.lines[1]);
}

TEST(VerticalTextLayoutTest, FitsTheNaturalColumnsWhenTheyFit) {
  const VerticalTextLayout layout = coral::fitVerticalTextLayout(20, 400.0f, 300.0f, 20.0f);
  EXPECT_FLOAT_EQ(1.0f, layout.fontScale);
  EXPECT_FLOAT_EQ(20.0f, layout.lineHeight);
  EXPECT_EQ(15, layout.rowsPerColumn);
  EXPECT_EQ(2, layout.columns);
  EXPECT_FLOAT_EQ(40.0f, layout.width);
}

TEST(VerticalTextLayoutTest, ScalesDownWhenTheColumnsDoNotFit) {
  // Twenty characters at a 10 px line height need two 10 px columns at the natural size. A 20 px
  // viewport leaves room for one, so the fitter halves the line height and the text becomes a single
  // column of 30 rows -- still inside the available height.
  const VerticalTextLayout layout = coral::fitVerticalTextLayout(20, 20.0f, 300.0f, 20.0f);
  EXPECT_LT(layout.fontScale, 1.0f);
  EXPECT_LE(layout.width, 20.0f + 0.01f);
  EXPECT_EQ(1, layout.columns);
  EXPECT_GE(layout.rowsPerColumn, 20);
}

TEST(VerticalTextLayoutTest, KeepsOneColumnForNoCharacter) {
  const VerticalTextLayout layout = coral::fitVerticalTextLayout(0, 100.0f, 100.0f, 10.0f);
  EXPECT_EQ(1, layout.columns);
  EXPECT_GE(layout.lineHeight, 1.0f);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
