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

#include <subtitle/SubtitleResources.h>

#include <graphics/Data.h>
#include <graphics/Image.h>

#include <fstream>
#include <iterator>
#include <vector>

namespace coral {

std::shared_ptr<const graphics::Bitmap> loadSubtitlePattern(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return nullptr;
  }
  std::vector<uint8_t> encoded((std::istreambuf_iterator<char>(file)),
    std::istreambuf_iterator<char>());
  if (encoded.empty()) {
    return nullptr;
  }
  const graphics::Data data = graphics::Data::makeCopy(encoded.data(), encoded.size());
  const graphics::Image image = graphics::Image::decode(data);
  if (image.isEmpty()) {
    return nullptr;
  }
  return std::make_shared<const graphics::Bitmap>(image.makeBitmap());
}

} // namespace coral
