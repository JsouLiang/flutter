// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/runtime/isolate_configuration.h"

#include "flutter/fml/make_copyable.h"
#include "flutter/runtime/dart_vm.h"
// BD: ADD
#include "flutter/assets/directory_asset_bundle.h"
#include "flutter/bdflutter/assets/zip_asset_store.h"
#include "flutter/fml/logging.h"

namespace flutter {

IsolateConfiguration::IsolateConfiguration() = default;

IsolateConfiguration::~IsolateConfiguration() = default;

// BD ADD: START
class DynamicartIsolateConfiguration : public IsolateConfiguration {
public:
    DynamicartIsolateConfiguration(std::vector<std::future<std::unique_ptr<const fml::Mapping>>>
                                   kernel_pieces, std::string package_preload_libs)
            : kernel_piece_futures_(std::move(kernel_pieces)), package_preload_libs(package_preload_libs) {}

    // |IsolateConfiguration|
    bool DoPrepareIsolate(DartIsolate& isolate) override {
      FML_LOG(ERROR)<<"kernel_pieces size:"<<kernel_piece_futures_.size()<<std::endl;
      ResolveKernelPiecesIfNecessary();
      for (size_t i = 0; i < resolved_kernel_pieces_.size(); i++) {
            bool last_piece = i + 1 == resolved_kernel_pieces_.size();

            if (!isolate.PrepareForRunningFromDynamicartKernel(std::move(resolved_kernel_pieces_[i]), package_preload_libs,
                                                               last_piece)) {
                return false;
            }
        }

        return true;
    }

    // |IsolateConfiguration|
    bool IsNullSafetyEnabled(const DartSnapshot& snapshot) override {
      ResolveKernelPiecesIfNecessary();
      const auto kernel = resolved_kernel_pieces_.empty()
                          ? nullptr
                          : resolved_kernel_pieces_.front().get();
      return snapshot.IsNullSafetyEnabled(kernel);
    }

  void ResolveKernelPiecesIfNecessary() {
    if (resolved_kernel_pieces_.size() == kernel_piece_futures_.size()) {
      return;
    }

    resolved_kernel_pieces_.clear();
    for (auto& piece : kernel_piece_futures_) {
      // The get() call will xfer the unique pointer out and leave an empty
      // future in the original vector.
      resolved_kernel_pieces_.emplace_back(piece.get());
    }
  }
private:
    std::vector<std::future<std::unique_ptr<const fml::Mapping>>>
        kernel_piece_futures_;
    std::vector<std::unique_ptr<const fml::Mapping>> resolved_kernel_pieces_;
    std::string package_preload_libs;

    FML_DISALLOW_COPY_AND_ASSIGN(DynamicartIsolateConfiguration);
};
// END

bool IsolateConfiguration::PrepareIsolate(DartIsolate& isolate) {
  if (isolate.GetPhase() != DartIsolate::Phase::LibrariesSetup) {
    FML_DLOG(ERROR)
        << "Isolate was in incorrect phase to be prepared for running.";
    return false;
  }

  return DoPrepareIsolate(isolate);
}

class AppSnapshotIsolateConfiguration final : public IsolateConfiguration {
 public:
  AppSnapshotIsolateConfiguration() = default;

  // |IsolateConfiguration|
  bool DoPrepareIsolate(DartIsolate& isolate) override {
    return isolate.PrepareForRunningFromPrecompiledCode();
  }

  // |IsolateConfiguration|
  bool IsNullSafetyEnabled(const DartSnapshot& snapshot) override {
    return snapshot.IsNullSafetyEnabled(nullptr);
  }

 private:
  FML_DISALLOW_COPY_AND_ASSIGN(AppSnapshotIsolateConfiguration);
};

class KernelIsolateConfiguration : public IsolateConfiguration {
 public:
  KernelIsolateConfiguration(std::unique_ptr<const fml::Mapping> kernel)
      : kernel_(std::move(kernel)) {}

  // |IsolateConfiguration|
  bool DoPrepareIsolate(DartIsolate& isolate) override {
    if (DartVM::IsRunningPrecompiledCode()) {
      return false;
    }
    return isolate.PrepareForRunningFromKernel(std::move(kernel_));
  }

  // |IsolateConfiguration|
  bool IsNullSafetyEnabled(const DartSnapshot& snapshot) override {
    return snapshot.IsNullSafetyEnabled(kernel_.get());
  }

 private:
  std::unique_ptr<const fml::Mapping> kernel_;

  FML_DISALLOW_COPY_AND_ASSIGN(KernelIsolateConfiguration);
};

class KernelListIsolateConfiguration final : public IsolateConfiguration {
 public:
  KernelListIsolateConfiguration(
      std::vector<std::future<std::unique_ptr<const fml::Mapping>>>
          kernel_pieces)
      : kernel_piece_futures_(std::move(kernel_pieces)) {
    if (kernel_piece_futures_.empty()) {
      FML_LOG(ERROR) << "Attempted to create kernel list configuration without "
                        "any kernel blobs.";
    }
  }

  // |IsolateConfiguration|
  bool DoPrepareIsolate(DartIsolate& isolate) override {
    if (DartVM::IsRunningPrecompiledCode()) {
      return false;
    }

    ResolveKernelPiecesIfNecessary();

    if (resolved_kernel_pieces_.empty()) {
      FML_DLOG(ERROR) << "No kernel pieces provided to prepare this isolate.";
      return false;
    }

    for (size_t i = 0; i < resolved_kernel_pieces_.size(); i++) {
      if (!resolved_kernel_pieces_[i]) {
        FML_DLOG(ERROR) << "This kernel list isolate configuration was already "
                           "used to prepare an isolate.";
        return false;
      }
      const bool last_piece = i + 1 == resolved_kernel_pieces_.size();
      if (!isolate.PrepareForRunningFromKernel(
              std::move(resolved_kernel_pieces_[i]), last_piece)) {
        return false;
      }
    }

    return true;
  }

  // |IsolateConfiguration|
  bool IsNullSafetyEnabled(const DartSnapshot& snapshot) override {
    ResolveKernelPiecesIfNecessary();
    const auto kernel = resolved_kernel_pieces_.empty()
                            ? nullptr
                            : resolved_kernel_pieces_.front().get();
    return snapshot.IsNullSafetyEnabled(kernel);
  }

  // This must be call as late as possible before accessing any of the kernel
  // pieces. This will delay blocking on the futures for as long as possible. So
  // far, only Fuchsia depends on this optimization and only on the non-AOT
  // configs.
  void ResolveKernelPiecesIfNecessary() {
    if (resolved_kernel_pieces_.size() == kernel_piece_futures_.size()) {
      return;
    }

    resolved_kernel_pieces_.clear();
    for (auto& piece : kernel_piece_futures_) {
      // The get() call will xfer the unique pointer out and leave an empty
      // future in the original vector.
      resolved_kernel_pieces_.emplace_back(piece.get());
    }
  }

 private:
  std::vector<std::future<std::unique_ptr<const fml::Mapping>>>
      kernel_piece_futures_;
  std::vector<std::unique_ptr<const fml::Mapping>> resolved_kernel_pieces_;

  FML_DISALLOW_COPY_AND_ASSIGN(KernelListIsolateConfiguration);
};

static std::vector<std::string> ParseKernelListPaths(
    std::unique_ptr<fml::Mapping> kernel_list) {
  FML_DCHECK(kernel_list);

  std::vector<std::string> kernel_pieces_paths;

  const char* kernel_list_str =
      reinterpret_cast<const char*>(kernel_list->GetMapping());
  size_t kernel_list_size = kernel_list->GetSize();

  size_t piece_path_start = 0;
  while (piece_path_start < kernel_list_size) {
    size_t piece_path_end = piece_path_start;
    while ((piece_path_end < kernel_list_size) &&
           (kernel_list_str[piece_path_end] != '\n')) {
      piece_path_end++;
    }
    std::string piece_path(&kernel_list_str[piece_path_start],
                           piece_path_end - piece_path_start);
    kernel_pieces_paths.emplace_back(std::move(piece_path));

    piece_path_start = piece_path_end + 1;
  }

  return kernel_pieces_paths;
}

static std::vector<std::future<std::unique_ptr<const fml::Mapping>>>
PrepareKernelMappings(std::vector<std::string> kernel_pieces_paths,
                      std::shared_ptr<AssetManager> asset_manager,
                      fml::RefPtr<fml::TaskRunner> io_worker) {
  FML_DCHECK(asset_manager);
  std::vector<std::future<std::unique_ptr<const fml::Mapping>>> fetch_futures;

  for (const auto& kernel_pieces_path : kernel_pieces_paths) {
    std::promise<std::unique_ptr<const fml::Mapping>> fetch_promise;
    fetch_futures.push_back(fetch_promise.get_future());
    auto fetch_task =
        fml::MakeCopyable([asset_manager, kernel_pieces_path,
                           fetch_promise = std::move(fetch_promise)]() mutable {
          fetch_promise.set_value(
              asset_manager->GetAsMapping(kernel_pieces_path));
        });
    // Fulfill the promise on the worker if one is available or the current
    // thread if one is not.
    if (io_worker) {
      io_worker->PostTask(fetch_task);
    } else {
      fetch_task();
    }
  }

  return fetch_futures;
}

std::unique_ptr<IsolateConfiguration> IsolateConfiguration::InferFromSettings(
    const Settings& settings,
    std::shared_ptr<AssetManager> asset_manager,
    fml::RefPtr<fml::TaskRunner> io_worker) {

  // BD ADD:
  // Running in Dynamicart mode. 注意：仅iOS调用
  if (!settings.package_dill_path.empty()) {
      return CreateForDynamicart(settings, *asset_manager);
  }
  // END

  // Running in AOT mode.
  if (DartVM::IsRunningPrecompiledCode()) {
    return CreateForAppSnapshot();
  }

  if (settings.application_kernels) {
    return CreateForKernelList(settings.application_kernels());
  }

  if (settings.application_kernel_asset.empty() &&
      settings.application_kernel_list_asset.empty()) {
    FML_DLOG(ERROR) << "application_kernel_asset or "
                       "application_kernel_list_asset must be set";
    return nullptr;
  }

  if (!asset_manager) {
    FML_DLOG(ERROR) << "No asset manager specified when attempting to create "
                       "isolate configuration.";
    return nullptr;
  }

  // Running from kernel snapshot. Requires asset manager.
  {
    std::unique_ptr<fml::Mapping> kernel =
        asset_manager->GetAsMapping(settings.application_kernel_asset);
    if (kernel) {
      return CreateForKernel(std::move(kernel));
    }
  }

  // Running from kernel divided into several pieces (for sharing). Requires
  // asset manager and io worker.

  if (!io_worker) {
    FML_DLOG(ERROR) << "No IO worker specified to load kernel pieces.";
    return nullptr;
  }

  {
    std::unique_ptr<fml::Mapping> kernel_list =
        asset_manager->GetAsMapping(settings.application_kernel_list_asset);
    if (!kernel_list) {
      FML_LOG(ERROR) << "Failed to load: "
                     << settings.application_kernel_list_asset;
      return nullptr;
    }
    auto kernel_pieces_paths = ParseKernelListPaths(std::move(kernel_list));
    auto kernel_mappings = PrepareKernelMappings(std::move(kernel_pieces_paths),
                                                 asset_manager, io_worker);
    return CreateForKernelList(std::move(kernel_mappings));
  }

  return nullptr;
}

std::unique_ptr<IsolateConfiguration>
IsolateConfiguration::CreateForAppSnapshot() {
  return std::make_unique<AppSnapshotIsolateConfiguration>();
}

std::unique_ptr<IsolateConfiguration> IsolateConfiguration::CreateForKernel(
    std::unique_ptr<const fml::Mapping> kernel) {
  return std::make_unique<KernelIsolateConfiguration>(std::move(kernel));
}

std::unique_ptr<IsolateConfiguration> IsolateConfiguration::CreateForKernelList(
    std::vector<std::unique_ptr<const fml::Mapping>> kernel_pieces) {
  std::vector<std::future<std::unique_ptr<const fml::Mapping>>> pieces;
  for (auto& piece : kernel_pieces) {
    if (!piece) {
      FML_DLOG(ERROR) << "Invalid kernel piece.";
      continue;
    }
    std::promise<std::unique_ptr<const fml::Mapping>> promise;
    pieces.push_back(promise.get_future());
    promise.set_value(std::move(piece));
  }
  return CreateForKernelList(std::move(pieces));
}

// BD ADD: START
std::unique_ptr<IsolateConfiguration> IsolateConfiguration::CreateForDynamicart(
        const Settings& settings, AssetManager& asset_manager) {
    // Running in Dynamicart mode.
    if (!settings.package_dill_path.empty()) {
        // 如果是动态模式，把动态包资源也加入asset_manager的查找范围中，且放在最前面，优先级最高。
        // 根据package_dill_path的后缀判断有不同的处理逻辑：
        // 如果.zip结尾就作为ZipAssetStore处理
        // 如果不是那就作为DirectoryAssetBundle处理
        size_t file_ext_index = settings.package_dill_path.rfind('.');
        if (file_ext_index == std::string::npos ||
            settings.package_dill_path.substr(file_ext_index) != ".zip") {
            asset_manager.PushFront(std::make_unique<DirectoryAssetBundle>(
                    fml::OpenDirectory(settings.package_dill_path.c_str(), false,
                                       fml::FilePermission::kRead), true));
        } else {
            asset_manager.PushFront(std::make_unique<ZipAssetStore>(
                    settings.package_dill_path.c_str(), "flutter_assets"));
        }

        // 然后，从资源中找出kernel文件, 由此生成IsolateConfiguration
        // 后续的逻辑会根据IsolateConfiguration创建isolate
        // isolate.PrepareForRunningFromDynamicartKernel()中加载该kernel文件
        std::vector<std::unique_ptr<const fml::Mapping>> pieces;

        std::unique_ptr<fml::Mapping> kernel_blob =
                fml::FileMapping::CreateReadOnly(settings.application_kernel_asset);
        if (kernel_blob != nullptr && kernel_blob->GetSize() > 0) {
            TT_LOG() << "Created IsolateConfiguration load kernel_blob.bin";
            pieces.push_back(std::move(kernel_blob));
        }

        std::unique_ptr<fml::Mapping> kernel =
                asset_manager.GetAsMapping("kb.origin");
        if (kernel != nullptr && kernel->GetSize() > 0) {
            TT_LOG() << "Created IsolateConfiguration For Dyart.";
            pieces.push_back(std::move(kernel));
        } else {
            kernel = asset_manager.GetAsMapping("kb");
            if (kernel != nullptr && kernel->GetSize() > 0) {
                std::unique_ptr<fml::Mapping> encrypt = asset_manager.GetAsMapping("encrypt.txt");
                if (encrypt != nullptr) {
                    const uint8_t *encodeData = kernel->GetMapping();
                    size_t encodeSize = kernel->GetSize();
                    std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()
                    );


                    long start = ms.count();
                    TT_LOG() << "begin decode kb:" << encodeSize << std::endl;
                    std::vector<uint8_t> decodeData;
                    const size_t space = 8;
                    size_t spaceCount = encodeSize / space;
                    for (size_t i = 0; i < spaceCount; i++) {
                        for (size_t j = 0; j < space; j++) {
                            decodeData.push_back(encodeData[i * space + j] ^ (i % space));
                        }
                    }
                    for (size_t i = (spaceCount * space); i < encodeSize; i++) {
                        decodeData.push_back(encodeData[i] ^ 2);
                    }
                    std::unique_ptr<fml::Mapping> decodeKernel(new fml::DataMapping(decodeData));
                    ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()
                    );
                    long end = ms.count();
                    TT_LOG() << "finish decode kb:" << decodeKernel->GetSize() << ",time:" << (end - start) << std::endl;
                    TT_LOG() << "Created IsolateConfiguration For Dyart.";
                    pieces.push_back(std::move(decodeKernel));
                } else {
                    TT_LOG() << "Created IsolateConfiguration For Dyart.";
                    pieces.push_back(std::move(kernel));
                }
            } else {
                TT_LOG() << "No kb file in package_dill_path "
                         << settings.package_dill_path.c_str();
            }
        }
        return IsolateConfiguration::CreateForDynamicartKernel(std::move(pieces), settings.package_preload_libs);
    }
    return nullptr;
}

std::unique_ptr<IsolateConfiguration>
IsolateConfiguration::CreateForDynamicartKernel(
        std::vector<std::unique_ptr<const fml::Mapping>> kernel_pieces, std::string package_preload_libs) {
    std::vector<std::future<std::unique_ptr<const fml::Mapping>>> pieces;
    for (auto& piece : kernel_pieces) {
        std::promise<std::unique_ptr<const fml::Mapping>> promise;
        pieces.push_back(promise.get_future());
        promise.set_value(std::move(piece));
    }
    return std::make_unique<DynamicartIsolateConfiguration>(std::move(pieces), package_preload_libs);
}
// END

std::unique_ptr<IsolateConfiguration> IsolateConfiguration::CreateForKernelList(
    std::vector<std::future<std::unique_ptr<const fml::Mapping>>>
        kernel_pieces) {
  return std::make_unique<KernelListIsolateConfiguration>(
      std::move(kernel_pieces));
}

}  // namespace flutter
