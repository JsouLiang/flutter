//
//  fps_recorder_test.cpp
//  sources
//
//  Created by 邱鑫玥 on 2020/3/25.
//
// BD ADD: START
#include "fps_recorder_test.h"

namespace flutter {
namespace testing {
FpsRecorderTest::FpsRecorderTest()
    : fps_recorder_(std::make_unique<FpsRecorder>()) {}

FpsRecorderTest::~FpsRecorderTest() = default;

void FpsRecorderTest::SetUp() {}

void FpsRecorderTest::TearDown() {}
}  // namespace testing
}  // namespace flutter
// END
