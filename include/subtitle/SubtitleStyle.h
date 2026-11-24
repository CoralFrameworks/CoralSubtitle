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

#ifndef CORAL_SUBTITLE_STYLE_H
#define CORAL_SUBTITLE_STYLE_H

#include <graphics/Bitmap.h>

#include <memory>
#include <string>
#include <vector>

namespace coral {

/**
 * The style of the cues of a subtitle track.
 *
 * <p>The counterpart of the style of an {@code AVTextStyleRule} of AVFoundation. The values are
 * stored and reported to the application through {@link PlayerItem}; they are what the burn-in
 * renderer of S5 reads, and the application that receives the cue groups may apply them itself.
 *
 * <p>CSS level matching is not implemented: a rule applies to every cue, and a later rule replaces
 * an earlier one rather than cascading over it.
 */
struct SubtitleStyle {
  /** The font family of the text, or empty for the default one. */
  std::string fontFamily;
  /** The size of the text in pixels, or 0 for the default one. */
  float fontSize = 0.0f;
  /** The ARGB colour of the text. */
  uint32_t foregroundColor = 0xFFFFFFFF;
  /** The ARGB colour behind the text, or 0 for a transparent background. */
  uint32_t backgroundColor = 0x00000000;
  /** Whether the text is bold, italic, underlined or struck out. */
  bool bold = false;
  bool italic = false;
  bool underline = false;
  bool strikeOut = false;
  /** The alignment: -1 for the one the cue declares, 0 left, 1 centre and 2 right. */
  int textAlignment = -1;

  /**
   * Whether the glyphs are drawn through the signed-distance-field route where the backend supports it.
   *
   * The SDF route keeps the outline, the shadow and the glow as shader parameters of one glyph draw; the
   * coverage route draws four offset copies for the outline. The values below are in SDF distance units:
   * 1.0 is the field padding (six pixels in the current generator).
   */
  bool sdf = false;
  /** Width of the outline band outside the fill; 0 disables it. */
  float sdfOutlineWidth = 0.0f;
  /** Shadow offset in normalized texture units; (0, 0) disables the shadow. */
  float sdfShadowX = 0.0f;
  float sdfShadowY = 0.0f;
  /** Width of the glow band outside the outline; 0 disables it. */
  float sdfGlow = 0.0f;
  /** ARGB colours of the outline and the shadow. */
  uint32_t sdfOutlineColor = 0xFF000000u;
  uint32_t sdfShadowColor = 0xFF000000u;
  /** Procedural border decoration: 0 none, 1 stripes, 2 dots; frequency, angle, strength. */
  float sdfPattern = 0.0f;
  float sdfPatternFrequency = 8.0f;
  float sdfPatternAngle = 0.0f;
  float sdfPatternStrength = 0.0f;
  /** Opacity of the glyph fill; 0 makes the text interior transparent over the frame behind it. */
  float sdfFillAlpha = 1.0f;
  /** Optional pattern texture sampled by the SDF pattern program; the renderer uploads it per surface. */
  std::shared_ptr<const graphics::Bitmap> sdfPatternBitmap;
  /** Path of a pattern image to load when no bitmap is supplied; resolved by the player. */
  std::string sdfPatternPath;

  bool operator==(const SubtitleStyle& other) const {
    return fontFamily == other.fontFamily && fontSize == other.fontSize &&
           foregroundColor == other.foregroundColor && backgroundColor == other.backgroundColor &&
           bold == other.bold && italic == other.italic && underline == other.underline &&
           strikeOut == other.strikeOut && textAlignment == other.textAlignment &&
           sdf == other.sdf && sdfOutlineWidth == other.sdfOutlineWidth &&
           sdfShadowX == other.sdfShadowX && sdfShadowY == other.sdfShadowY &&
           sdfGlow == other.sdfGlow && sdfOutlineColor == other.sdfOutlineColor &&
           sdfShadowColor == other.sdfShadowColor && sdfPattern == other.sdfPattern &&
           sdfPatternFrequency == other.sdfPatternFrequency &&
           sdfPatternAngle == other.sdfPatternAngle &&
           sdfPatternStrength == other.sdfPatternStrength &&
           sdfFillAlpha == other.sdfFillAlpha && sdfPatternPath == other.sdfPatternPath;
  }

  bool operator!=(const SubtitleStyle& other) const { return !(*this == other); }
};

/** The list of styles of a player item, which is what {@code textStyleRules} reports. */
class TextStyleRules {
public:
  TextStyleRules() = default;

  explicit TextStyleRules(std::vector<SubtitleStyle> styles) : mStyles(std::move(styles)) {}

  /** Adds a rule; a later rule replaces the earlier ones, as AVTextStyleRule documents. */
  void addStyle(SubtitleStyle style) { mStyles.push_back(std::move(style)); }

  void setStyles(std::vector<SubtitleStyle> styles) { mStyles = std::move(styles); }

  [[nodiscard]] const std::vector<SubtitleStyle>& getStyles() const { return mStyles; }

  [[nodiscard]] bool isEmpty() const { return mStyles.empty(); }

  bool operator==(const TextStyleRules& other) const { return mStyles == other.mStyles; }

  bool operator!=(const TextStyleRules& other) const { return !(*this == other); }

private:
  std::vector<SubtitleStyle> mStyles;
};

/**
 * The display options of the subtitles of a player item.
 *
 * <p>The counterpart of {@code AVPlayerItem.textDisplayOptions}: a window behind the text, and how
 * far the text is kept from the edges of the picture. The options are stored and reported, and the
 * burn-in renderer of S5 applies them.
 */
struct TextDisplayOptions {
  /** Whether a window is drawn behind the text. */
  bool windowEnabled = false;
  /** The ARGB colour of the window, when it is enabled. */
  uint32_t windowColor = 0x00000000;
  /** The margin kept at the bottom of the picture, in pixels, or 0 for the default one. */
  int64_t bottomMargin = 0;
  /** The margin kept at the left and right edges of the picture, in pixels, or 0 for the default. */
  int64_t horizontalMargin = 0;

  bool operator==(const TextDisplayOptions& other) const {
    return windowEnabled == other.windowEnabled && windowColor == other.windowColor &&
           bottomMargin == other.bottomMargin && horizontalMargin == other.horizontalMargin;
  }

  bool operator!=(const TextDisplayOptions& other) const { return !(*this == other); }
};

} // namespace coral

#endif //CORAL_SUBTITLE_STYLE_H
