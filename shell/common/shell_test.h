// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_COMMON_SHELL_TEST_H_
#define FLUTTER_SHELL_COMMON_SHELL_TEST_H_

#include <memory>

#include "flutter/common/settings.h"
#include "flutter/fml/macros.h"
#include "flutter/lib/ui/window/platform_message.h"
#include "flutter/shell/common/run_configuration.h"
#include "flutter/shell/common/thread_host.h"
#include "flutter/testing/test_dart_native_resolver.h"
#include "flutter/testing/thread_test.h"

namespace flutter {
namespace testing {

class ShellTest : public ::testing::ThreadTest {
 public:
  ShellTest();

  ~ShellTest();

  Settings CreateSettingsForFixture();

  TaskRunners GetTaskRunnersForFixture();

  // BD DEL: START
  // Cherry-pick Reland Skia Caching improvements (#10434)
  // （commid-id:13df65fd29f133bf9b9ca97c0bb8bd6735caf956） 官方新增了一些test
  // case，但是test依赖于一些前置cid引入的依赖，这里暂时关闭 void
  // SendEnginePlatformMessage(Shell* shell,
  //                               fml::RefPtr<PlatformMessage> message);
  // END

  void AddNativeCallback(std::string name, Dart_NativeFunction callback);

 protected:
  // |testing::ThreadTest|
  void SetUp() override;

  // |testing::ThreadTest|
  void TearDown() override;

 private:
  fml::UniqueFD assets_dir_;
  std::shared_ptr<::testing::TestDartNativeResolver> native_resolver_;
  std::unique_ptr<ThreadHost> thread_host_;

  void SetSnapshotsAndAssets(Settings& settings);
};

}  // namespace testing
}  // namespace flutter

#endif  // FLUTTER_SHELL_COMMON_SHELL_TEST_H_
