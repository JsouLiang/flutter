
#include <third_party/skia/include/core/SkGraphics.h>
#include "performance.h"
#include <chrono>

using namespace std;

namespace flutter {

Performance::Performance():
      dart_image_memory_usage(0),
      apm_map() {}


int64_t Performance::GetImageMemoryUsageKB() {
  int64_t sizeInKB = dart_image_memory_usage.load(std::memory_order_relaxed);
  return  sizeInKB > 0 ? sizeInKB : 0;
}

int64_t Performance::CurrentTimestamp() {
    return chrono::system_clock::now().time_since_epoch().count();
}

void Performance::TraceApmStartAndEnd(const string& event, int64_t start) {
    apm_map[event + "_start"] = start;
    apm_map[event + "_end"] = CurrentTimestamp();
}

vector<int64_t> Performance::GetEngineInitApmInfo() {
    vector<int64_t> engine_init_apm_info = {
            apm_map["init_task_start"],
            apm_map["init_task_end"],
            apm_map["native_init_start"],
            apm_map["native_init_end"],
            apm_map["native_init_end"], // ber_shell_create_start
            apm_map["log_icu_init_start"],  // ber_shell_create_end
            apm_map["log_icu_init_start"],
            apm_map["log_icu_init_end"],
            apm_map["dartvm_create_start"],
            apm_map["dartvm_create_end"],
            apm_map["platform_view_init_start"],
            apm_map["platform_view_init_end"],
            apm_map["vsync_waiter_start"],
            apm_map["vsync_waiter_end"],
            apm_map["rasterizer_init_start"],
            apm_map["rasterizer_init_end"],
            apm_map["shell_io_manger_start"],
            apm_map["shell_io_manger_end"],
            apm_map["ui_animator_start"],
            apm_map["read_isolate_snapshort_start"], // animator_pre_end
            apm_map["read_isolate_snapshort_start"],
            apm_map["read_isolate_snapshort_end"],
            apm_map["read_isolate_snapshort_end"], // animator_after_start
            apm_map["ui_animator_end"],
            apm_map["shell_wait_start"],
            apm_map["shell_wait_end"],
            apm_map["execute_dart_entry_start"],
            apm_map["execute_dart_entry_end"],
    };
    return engine_init_apm_info;
}

void Performance::SubImageMemoryUsage(int64_t sizeInKB) {
  dart_image_memory_usage.fetch_sub(sizeInKB, std::memory_order_relaxed);
}

void Performance::AddImageMemoryUsage(int64_t sizeInKB) {
  dart_image_memory_usage.fetch_add(sizeInKB, std::memory_order_relaxed);
}

void Performance::GetSkGraphicMemUsageKB(int64_t* bitmapMem,
                                         int64_t* fontMem, int64_t* imageFilter) {
  *bitmapMem = SkGraphics::GetResourceCacheTotalBytesUsed() >> 10;
  *fontMem = SkGraphics::GetFontCacheUsed() >> 10;
  *imageFilter = SkGraphics::GetImageFilterCacheUsed() >> 10;
}

void Performance::GetGpuCacheUsageKB(int64_t* grTotalMem,
                                     int64_t* grResMem, int64_t* grPurgeableMem) {
  if (rasterizer_ && rasterizer_.get()) {
    size_t totalBytes = 0;
    size_t resourceBytes = 0;
    size_t purgeableBytes = 0;
#ifndef OS_MACOSX
    rasterizer_->getResourceCacheBytes(&totalBytes, &resourceBytes, &purgeableBytes);
#endif
    *grTotalMem = totalBytes >> 10;
    *grResMem = resourceBytes >> 10;
    *grPurgeableMem = purgeableBytes >> 10;
  }
}

void Performance::SetRasterizer(fml::WeakPtr<flutter::Rasterizer> rasterizer) {
  rasterizer_ = std::move(rasterizer);
}
}
