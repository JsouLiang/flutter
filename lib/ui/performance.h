
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
                              int64_t* fontMem, int64_t* imageFilter);  // KB

  // APM
  void TraceApmStartAndEnd(const std::string& event, int64_t start);
  std::vector<int64_t> GetEngineInitApmInfo();
  static int64_t CurrentTimestamp();

 private:
  Performance();

  std::atomic_int64_t dart_image_memory_usage;  // KB
  fml::WeakPtr<flutter::Rasterizer> rasterizer_;
  fml::WeakPtr<flutter::ShellIOManager> iOManager_;
  std::map<string, int64_t> apm_map; // Map
};

}
#endif  // FLUTTER_LIB_UI_PERFORMANCE_H_
