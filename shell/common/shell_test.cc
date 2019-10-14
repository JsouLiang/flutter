// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define FML_USED_ON_EMBEDDER

#include "flutter/shell/common/shell_test.h"

#include "flutter/fml/mapping.h"
#include "flutter/runtime/dart_vm.h"
#include "flutter/testing/testing.h"

namespace flutter {
namespace testing {

ShellTest::ShellTest()
    : native_resolver_(std::make_shared<::testing::TestDartNativeResolver>()) {}

ShellTest::~ShellTest() = default;

static std::unique_ptr<fml::Mapping> GetMapping(const fml::UniqueFD& directory,
                                                const char* path,
                                                bool executable) {
  fml::UniqueFD file = fml::OpenFile(directory, path, false /* create */,
                                     fml::FilePermission::kRead);
  if (!file.is_valid()) {
    return nullptr;
  }

  using Prot = fml::FileMapping::Protection;
  std::unique_ptr<fml::FileMapping> mapping;
  if (executable) {
    mapping = std::make_unique<fml::FileMapping>(
        file, std::initializer_list<Prot>{Prot::kRead, Prot::kExecute});
  } else {
    mapping = std::make_unique<fml::FileMapping>(
        file, std::initializer_list<Prot>{Prot::kRead});
  }

  if (mapping->GetSize() == 0 || mapping->GetMapping() == nullptr) {
    return nullptr;
  }

  return mapping;
}

// BD DEL: START
// Cherry-pick Reland Skia Caching improvements (#10434)
// （commid-id:13df65fd29f133bf9b9ca97c0bb8bd6735caf956） 官方新增了一些test
// case，但是test依赖于一些前置cid引入的依赖，这里暂时关闭
// void ShellTest::SendEnginePlatformMessage(
//    Shell* shell,
//    fml::RefPtr<PlatformMessage> message) {
//  fml::AutoResetWaitableEvent latch;
//  fml::TaskRunner::RunNowOrPostTask(
//      shell->GetTaskRunners().GetPlatformTaskRunner(),
//      [shell, &latch, message = std::move(message)]() {
//        if (auto engine = shell->weak_engine_) {
//          engine->HandlePlatformMessage(std::move(message));
//        }
//        latch.Signal();
//      });
//  latch.Wait();
//}
// END

void ShellTest::SetSnapshotsAndAssets(Settings& settings) {
  if (!assets_dir_.is_valid()) {
    return;
  }

  settings.assets_dir = assets_dir_.get();

  // In JIT execution, all snapshots are present within the binary itself and
  // don't need to be explicitly suppiled by the embedder.
  if (DartVM::IsRunningPrecompiledCode()) {
    settings.vm_snapshot_data = [this]() {
      return GetMapping(assets_dir_, "vm_snapshot_data", false);
    };

    settings.isolate_snapshot_data = [this]() {
      return GetMapping(assets_dir_, "isolate_snapshot_data", false);
    };

    if (DartVM::IsRunningPrecompiledCode()) {
      settings.vm_snapshot_instr = [this]() {
        return GetMapping(assets_dir_, "vm_snapshot_instr", true);
      };

      settings.isolate_snapshot_instr = [this]() {
        return GetMapping(assets_dir_, "isolate_snapshot_instr", true);
      };
    }
  } else {
    settings.application_kernels = [this]() {
      std::vector<std::unique_ptr<const fml::Mapping>> kernel_mappings;
      kernel_mappings.emplace_back(
          GetMapping(assets_dir_, "kernel_blob.bin", false));
      return kernel_mappings;
    };
  }
}

Settings ShellTest::CreateSettingsForFixture() {
  Settings settings;
  settings.leak_vm = false;
  settings.task_observer_add = [](intptr_t key, fml::closure handler) {
    fml::MessageLoop::GetCurrent().AddTaskObserver(key, handler);
  };
  settings.task_observer_remove = [](intptr_t key) {
    fml::MessageLoop::GetCurrent().RemoveTaskObserver(key);
  };
  settings.root_isolate_create_callback = [this]() {
    native_resolver_->SetNativeResolverForIsolate();
  };
  SetSnapshotsAndAssets(settings);
  return settings;
}

TaskRunners ShellTest::GetTaskRunnersForFixture() {
  return {
      "test",
      thread_host_->platform_thread->GetTaskRunner(),  // platform
      thread_host_->gpu_thread->GetTaskRunner(),       // gpu
      thread_host_->ui_thread->GetTaskRunner(),        // ui
      thread_host_->io_thread->GetTaskRunner()         // io
  };
}

// |testing::ThreadTest|
void ShellTest::SetUp() {
  ThreadTest::SetUp();
  assets_dir_ = fml::OpenDirectory(::testing::GetFixturesPath(), false,
                                   fml::FilePermission::kRead);
  thread_host_ = std::make_unique<ThreadHost>(
      "io.flutter.test." + ::testing::GetCurrentTestName() + ".",
      ThreadHost::Type::Platform | ThreadHost::Type::IO | ThreadHost::Type::UI |
          ThreadHost::Type::GPU);
}

// |testing::ThreadTest|
void ShellTest::TearDown() {
  ThreadTest::TearDown();
  assets_dir_.reset();
  thread_host_.reset();
}

void ShellTest::AddNativeCallback(std::string name,
                                  Dart_NativeFunction callback) {
  native_resolver_->AddNativeCallback(std::move(name), callback);
}

}  // namespace testing
}  // namespace flutter
