
#ifndef FLUTTER_LIB_UI_PERFORMANCE_H_
#define FLUTTER_LIB_UI_PERFORMANCE_H_

#include <atomic>
#include <flutter/fml/memory/weak_ptr.h>
#include <flutter/shell/common/rasterizer.h>
#include <flutter/shell/common/shell_io_manager.h>
#include <vector>
#include <string>
#include <map>

using namespace std;

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
  void SetRasterizerAndIOManager(fml::WeakPtr<flutter::Rasterizer> rasterizer,
    fml::WeakPtr<flutter::ShellIOManager>);
  void GetSkGraphicMemUsageKB(int64_t* bitmapMem,
                              int64_t* fontMem, int64_t* imageFilter, int64_t* mallocSize);  // KB

  // APM
  void TraceApmStartAndEnd(const std::string& event, int64_t start);
  std::vector<int64_t> GetEngineInitApmInfo();
  static int64_t CurrentTimestamp();

  void SetExitStatus(bool isExitApp);
  bool IsExitApp();

  void UpdateGpuCacheUsageKB(flutter::Rasterizer* rasterizer);
  void UpdateIOCacheUsageKB(fml::WeakPtr<GrContext> iOContext);
  void UpdateSkGraphicMemUsageKB();
  std::vector<int64_t> GetMemoryDetails();

 private:
  Performance();

  std::atomic_int64_t dart_image_memory_usage;  // KB
  fml::WeakPtr<flutter::Rasterizer> rasterizer_;
  fml::WeakPtr<flutter::ShellIOManager> iOManager_;
  std::vector<int64_t> engine_launch_infos; // Map
  std::atomic_bool isExitApp_ = {false};

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

};

}
#endif  // FLUTTER_LIB_UI_PERFORMANCE_H_
