
#ifndef FLUTTER_LIB_UI_PERFORMANCE_H_
#define FLUTTER_LIB_UI_PERFORMANCE_H_

#include <atomic>
#include <flutter/fml/memory/weak_ptr.h>
#include <flutter/shell/common/rasterizer.h>

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
  void SetRasterizer(fml::WeakPtr<flutter::Rasterizer> rasterizer);
  void GetSkGraphicMemUsageKB(int64_t* bitmapMem,
                              int64_t* fontMem, int64_t* imageFilter);  // KB

 private:
  Performance();

  std::atomic_int64_t dart_image_memory_usage;  // KB
  fml::WeakPtr<flutter::Rasterizer> rasterizer_;

};

}
#endif  // FLUTTER_LIB_UI_PERFORMANCE_H_
