//
//  fps_recorder_test.hpp
//  sources
//
//  Created by 邱鑫玥 on 2020/3/25.
//
// BD ADD: START
#ifndef FLUTTER_COMMON_FPS_RECORDER_TEST_H_
#define FLUTTER_COMMON_FPS_RECORDER_TEST_H_

#include "fps_recorder.h"
#include "gtest/gtest.h"

namespace flutter {
namespace testing {
class FpsRecorderTest : public ::testing::Test {
 public:
  FpsRecorderTest();

  ~FpsRecorderTest();

 protected:
  // |testing::ThreadTest|
  void SetUp() override;

  // |testing::ThreadTest|
  void TearDown() override;

  std::unique_ptr<FpsRecorder> fps_recorder_;
};
}  // namespace testing
}  // namespace flutter

#endif /* FLUTTER_COMMON_FPS_RECORDER_TEST_H_ */
// END
