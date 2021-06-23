// BD ADD

#ifndef FLUTTER_LIB_UI_PERFORMANCE_H_
#define FLUTTER_LIB_UI_PERFORMANCE_H_

#include <atomic>
#include <flutter/fml/memory/weak_ptr.h>
#include <flutter/shell/common/rasterizer.h>
#include <flutter/shell/common/shell_io_manager.h>
#include <vector>
#include <string>
#include <map>
#include <third_party/skia/include/core/SkGraphics.h>
#include <third_party/libcxx/include/string>

#if FLUTTER_RUNTIME_MODE == FLUTTER_RUNTIME_MODE_RELEASE || FLUTTER_RUNTIME_MODE == FLUTTER_RUNTIME_MODE_JIT_RELEASE || !defined(NDEBUG)
#define NO_REALTIME_MEM 1
#endif

namespace tonic {
class DartLibraryNatives;
}  // namespace tonic

namespace flutter {
  static constexpr int kMemoryDetailsLength(11);

class Performance {
 public:
  static Performance* GetInstance() {
    static Performance instance;
    return &instance;
  }
  void AddImageMemoryUsage(int64_t sizeInKB);
  void SubImageMemoryUsage(int64_t sizeInKB);

  int64_t GetImageMemoryUsageKB();  // KB

  void GetGpuCacheUsageKB(int64_t* grTotalMem,
                          int64_t* grResMem, int64_t* grPurgeableMem);
  void GetIOGpuCacheUsageKB(int64_t* grTotalMem,
                            int64_t* grResMem, int64_t* grPurgeableMem);
  void SetRasterizerAndIOManager(fml::TaskRunnerAffineWeakPtr<flutter::Rasterizer> rasterizer,
                                 fml::WeakPtr<flutter::ShellIOManager>);
  void GetSkGraphicMemUsageKB(int64_t* bitmapMem,
                              int64_t* fontMem, int64_t* imageFilter, int64_t* mallocSize);  // KB

  // APM
  void TraceApmStartAndEnd(const std::string& event, int64_t start);
  std::vector<int64_t> GetEngineInitApmInfo();
  static int64_t CurrentTimestamp();

  void SetExitStatus(bool isExitApp);
  bool IsExitApp();

  void DisableMips(bool disable);
  bool IsDisableMips();

  static void RegisterNatives(tonic::DartLibraryNatives* natives);

  void WarmUpZeroSizeOnce(bool warmUpOnce);
  bool PerformWarmUpZeroSize();

  void UpdateGpuCacheUsageKB(flutter::Rasterizer* rasterizer);
  void UpdateIOCacheUsageKB(fml::WeakPtr<GrDirectContext> iOContext);
  void UpdateSkGraphicMemUsageKB();
  std::vector<int64_t> GetMemoryDetails();
  void RecordLastLayoutTime();
  int64_t GetRecordLastLayoutTime();
  void ClearHugeFontCache();
  void ClearAllFontCache();
  void ClearLayoutCache();
  void EnableBoostVSync(bool enable);
  bool isEnableBoostVSync();
  void setDevicePixelRatio(float ratio);

 private:
  Performance();

  std::atomic_int64_t dart_image_memory_usage;  // KB

  std::atomic_bool disable_mipmaps_;

  fml::TaskRunnerAffineWeakPtr<flutter::Rasterizer> rasterizer_;
  fml::WeakPtr<flutter::ShellIOManager> iOManager_;
  std::vector<int64_t> engine_launch_infos; // Map
  std::atomic_bool isExitApp_ = {false};
  std::atomic_bool warmUpOnce_;

  // 0 -> imageMem
  // 1 -> grTotalMem
  // 2 -> grResMem
  // 3 -> grPurgeableMem
  // 4 -> iOGrTotalMem
  // 5 -> iOGrResMem
  // 6 -> iOGrPurgeableMem
  // 7 -> bitmapMem
  // 8 -> fontMem
  // 9 -> imageFilter
  // 10 -> mallocSize
  std::atomic_int64_t imageMem_;
  std::atomic_int64_t grTotalMem_;
  std::atomic_int64_t grResMem_;
  std::atomic_int64_t grPurgeableMem_;
  std::atomic_int64_t iOGrTotalMem_;
  std::atomic_int64_t iOGrResMem_;
  std::atomic_int64_t iOGrPurgeableMem_;
  std::atomic_int64_t bitmapMem_;
  std::atomic_int64_t fontMem_;
  std::atomic_int64_t imageFilter_;
  std::atomic_int64_t mallocSize_;

  // record last layout time
  std::atomic_int64_t lastLayoutTime_;

  // boost VSync
  // request VSync immediately, when begin frame
  std::atomic_bool boostVSync;
};

}
#endif  // FLUTTER_LIB_UI_PERFORMANCE_H_
