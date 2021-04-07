/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef MINIKIN_HBFONT_CACHE_H
#define MINIKIN_HBFONT_CACHE_H

// BD ADD: START
#include <cstddef>
#include <cstdint>
// END

struct hb_font_t;

namespace minikin {
// BD ADD: START
static const size_t HUGE_FONT_SIZE = 5 << 20;
class MinikinFont;

void purgeHbFontCacheLocked();
void purgeHbFontLocked(const MinikinFont* minikinFont);
hb_font_t* getHbFontLocked(const MinikinFont* minikinFont);
// BD ADD: START
void putHugeFontIdLocked(int32_t fontId, size_t fontSize);
void clearHugeFontCacheLocked();
// END

}  // namespace minikin
#endif  // MINIKIN_HBFONT_CACHE_H
