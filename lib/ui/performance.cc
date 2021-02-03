
#include <third_party/skia/include/core/SkGraphics.h>
#include "performance.h"
#include <chrono>

using namespace std;

namespace flutter {
static std::mutex mtx;

std::map<string, int> event_dict = {
        {"init_task", 0},
        {"native_init", 1},
        {"ber_shell_create", 2},
        {"log_icu_init", 3},
        {"dartvm_create", 4},
        {"platform_view_init", 5},
        {"vsync_waiter", 6},
        {"rasterizer_init", 7},
        {"shell_io_manger", 8},
        {"ui_animator_pre", 9},
        {"read_isolate_snapshort", 10},
        {"ui_animator_after", 11},
        {"shell_wait", 12},
        {"plugin_registry", 13},
        {"execute_dart_entry", 14},
};

Performance::Performance():
      dart_image_memory_usage(0),
      engine_launch_infos(30) {}

int64_t Performance::GetImageMemoryUsageKB() {
  int64_t sizeInKB = dart_image_memory_usage.load(std::memory_order_relaxed);
  return  sizeInKB > 0 ? sizeInKB : 0;
}

int64_t Performance::CurrentTimestamp() {
    return chrono::system_clock::now().time_since_epoch().count();
}

void Performance::TraceApmStartAndEnd(const string& event, int64_t start) {
    std::lock_guard<std::mutex> lck (mtx);
    if (event == "ui_animator") {
        int index = event_dict.find(event + "_pre")->second;
        engine_launch_infos[index * 2] = start;
        index = event_dict.find(event + "_after")->second;
        engine_launch_infos[index * 2 + 1] = CurrentTimestamp();
    } else {
        int index = event_dict.find(event)->second;
        engine_launch_infos[index * 2] = start;
        engine_launch_infos[index * 2 + 1] = CurrentTimestamp();
    }
}

vector<int64_t> Performance::GetEngineInitApmInfo() {
    std::lock_guard<std::mutex> lck (mtx);
    vector<int64_t> engine_init_apm_info(engine_launch_infos);
    engine_init_apm_info[4] = engine_launch_infos[3];  // ber_shell_create_start = native_init_end
    engine_init_apm_info[5] = engine_launch_infos[6];  // ber_shell_create_end = log_icu_init_start
    engine_init_apm_info[19] = engine_launch_infos[20];  // animator_pre_end = read_isolate_snapshort_start
    engine_init_apm_info[22] = engine_launch_infos[21];  // animator_after_start = read_isolate_snapshort_end
    return engine_init_apm_info;
}

void Performance::SubImageMemoryUsage(int64_t sizeInKB) {
  dart_image_memory_usage.fetch_sub(sizeInKB, std::memory_order_relaxed);
}

void Performance::AddImageMemoryUsage(int64_t sizeInKB) {
  dart_image_memory_usage.fetch_add(sizeInKB, std::memory_order_relaxed);
}

void Performance::GetSkGraphicMemUsageKB(int64_t* bitmapMem,
                                         int64_t* fontMem, int64_t* imageFilter, int64_t* mallocSize) {
  *bitmapMem = SkGraphics::GetResourceCacheTotalBytesUsed() >> 10;
  *fontMem = SkGraphics::GetFontCacheUsed() >> 10;
  *imageFilter = SkGraphics::GetImageFilterCacheUsed() >> 10;
  *mallocSize = SkGraphics::GetSKMallocMemSize();
  if (*mallocSize >= 0) {
    *mallocSize = *mallocSize >> 10;
  } else {
    *mallocSize = 0;
  }
}

void Performance::GetGpuCacheUsageKB(int64_t* grTotalMem,
                                     int64_t* grResMem, int64_t* grPurgeableMem) {
  if (rasterizer_ && rasterizer_.get()) {
    size_t totalBytes = 0;
    size_t resourceBytes = 0;
    size_t purgeableBytes = 0;

#if defined(OS_IOS) || defined(OS_ANDROID)
    rasterizer_->getResourceCacheBytes(&totalBytes, &resourceBytes, &purgeableBytes);
#endif
    *grTotalMem = totalBytes >> 10;
    *grResMem = resourceBytes >> 10;
    *grPurgeableMem = purgeableBytes >> 10;
  }
}

void Performance::GetIOGpuCacheUsageKB(int64_t* grTotalMem,
                                     int64_t* grResMem, int64_t* grPurgeableMem) {
#if defined(OS_IOS) || defined(OS_ANDROID)
  if (iOManager_ && iOManager_.get() && iOManager_->GetResourceContext()) {
    size_t totalBytes = 0;
    size_t resourceBytes = 0;
    size_t purgeableBytes = 0;
    iOManager_->GetResourceContext()->getResourceCacheBytes(&totalBytes, &resourceBytes, &purgeableBytes);
    *grTotalMem = totalBytes >> 10;
    *grResMem = resourceBytes >> 10;
    *grPurgeableMem = purgeableBytes >> 10;
  }
#endif
}

void Performance::SetRasterizerAndIOManager(fml::WeakPtr<flutter::Rasterizer> rasterizer,
  fml::WeakPtr<flutter::ShellIOManager> ioManager) {
#if FLUTTER_RUNTIME_MODE != FLUTTER_RUNTIME_MODE_RELEASE && FLUTTER_RUNTIME_MODE != FLUTTER_RUNTIME_MODE_JIT_RELEASE
  rasterizer_ = std::move(rasterizer);
  iOManager_ = std::move(ioManager);
  isExitApp_ = false;
#endif
}

void Performance::SetExitStatus(bool isExitApp) {
  isExitApp_ = isExitApp;
}

bool Performance::IsExitApp() {
  return isExitApp_;
}

void Performance::UpdateGpuCacheUsageKB(flutter::Rasterizer* rasterizer) {

  size_t totalBytes = 0;
  size_t resourceBytes = 0;
  size_t purgeableBytes = 0;

#if defined(OS_IOS) || defined(OS_ANDROID)
  rasterizer->getResourceCacheBytes(&totalBytes, &resourceBytes, &purgeableBytes);
#endif
  grTotalMem_ = totalBytes >> 10;
  grResMem_ = resourceBytes >> 10;
  grPurgeableMem_ = purgeableBytes >> 10;
}

void Performance::UpdateIOCacheUsageKB(fml::WeakPtr<GrContext> iOContext) {
#if defined(OS_IOS) || defined(OS_ANDROID)
  if (iOContext) {
    size_t totalBytes = 0;
    size_t resourceBytes = 0;
    size_t purgeableBytes = 0;
    iOContext->getResourceCacheBytes(&totalBytes, &resourceBytes, &purgeableBytes);

    iOGrTotalMem_ = totalBytes >> 10;
    iOGrResMem_ = resourceBytes >> 10;
    iOGrPurgeableMem_ = purgeableBytes >> 10;
  }
#endif
}

void Performance::UpdateSkGraphicMemUsageKB() {
  imageMem_ = Performance::GetImageMemoryUsageKB();
  bitmapMem_ = SkGraphics::GetResourceCacheTotalBytesUsed() >> 10;
  fontMem_ = SkGraphics::GetFontCacheUsed() >> 10;
  imageFilter_ = SkGraphics::GetImageFilterCacheUsed() >> 10;
  mallocSize_ = SkGraphics::GetSKMallocMemSize();
  if (mallocSize_ >= 0) {
    mallocSize_ = mallocSize_ >> 10;
  } else {
    mallocSize_ = 0;
  }
}

std::vector<int64_t> Performance::GetMemoryDetails(){
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
  std::vector<int64_t> mem(kMemoryDetailsLength);
  mem[0] = imageMem_;
  mem[1] = grTotalMem_;
  mem[2] = grResMem_;
  mem[3] = grPurgeableMem_;
  mem[4] = iOGrTotalMem_;
  mem[5] = iOGrResMem_;
  mem[6] = iOGrPurgeableMem_;
  mem[7] = bitmapMem_;
  mem[8] = fontMem_;
  mem[9] = imageFilter_;
  mem[10] = mallocSize_;
  return mem;
}

}
