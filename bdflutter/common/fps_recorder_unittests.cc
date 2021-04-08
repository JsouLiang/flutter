//
//  fps_recorder_unittests.cpp
//  sources
//
//  Created by 邱鑫玥 on 2020/3/25.
//
// BD ADD: START
#include <flutter/fml/time/time_delta.h>
#include "fps_recorder_test.h"
#include "gtest/gtest.h"

namespace flutter {
namespace testing {
TEST_F(FpsRecorderTest, ObtainFpsData) {
  fps_recorder_->StartRecordFps("test");
  fps_recorder_->AddFrameCount(1, fml::TimeDelta::FromMilliseconds(16));
  fps_recorder_->AddFrameCount(1, fml::TimeDelta::FromMilliseconds(16));
  fps_recorder_->AddDrawCount(fml::TimeDelta::FromMilliseconds(16));
  std::vector<double> fps = fps_recorder_->ObtainFpsData("test", true);
  ASSERT_EQ(fps, std::vector<double>({30, 16, 16}));
}
}  // namespace testing
}  // namespace flutter
// END
