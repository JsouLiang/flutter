// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/lib/ui/window/platform_message.h"

#include <utility>

namespace flutter {

PlatformMessage::PlatformMessage(std::string channel,
                                 std::vector<uint8_t> data,
                                 fml::RefPtr<PlatformMessageResponse> response)
    : channel_(std::move(channel)),
      data_(std::move(data)),
      hasData_(true),
      response_(std::move(response)) {
  // BD ADD: START
  if (channel_.compare(0, 5, "(bg-)") == 0) {
    channel_.erase(0, 5);
    runInChannelThread_ = true;
  } else if (channel_.compare(0, 3, "bg-") == 0) {
    runInChannelThread_ = true;
  } else if (channel_.compare(0, 5, "(fg-)") == 0) {
    channel_.erase(0, 5);
    runInUiThread_ = true;
  } else if (channel_.compare(0, 3, "fg-") == 0) {
    runInUiThread_ = true;
  }
  // END
}

PlatformMessage::PlatformMessage(std::string channel,
                                 fml::RefPtr<PlatformMessageResponse> response)
    : channel_(std::move(channel)),
      data_(),
      hasData_(false),
      response_(std::move(response)) {
  // BD ADD: START
  if (channel_.compare(0, 5, "(bg-)") == 0) {
    channel_.erase(0, 5);
    runInChannelThread_ = true;
  } else if (channel_.compare(0, 3, "bg-") == 0) {
    runInChannelThread_ = true;
  } else if (channel_.compare(0, 5, "(fg-)") == 0) {
    channel_.erase(0, 5);
    runInUiThread_ = true;
  } else if (channel_.compare(0, 3, "fg-") == 0) {
    runInUiThread_ = true;
  }
  // END
}

PlatformMessage::~PlatformMessage() = default;

}  // namespace flutter
