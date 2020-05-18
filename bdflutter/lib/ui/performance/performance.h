// BD ADD

#ifndef FLUTTER_LIB_UI_PERFORMANCE_H_
#define FLUTTER_LIB_UI_PERFORMANCE_H_

#include <atomic>

namespace tonic {
class DartLibraryNatives;
}  // namespace tonic

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

  void DisableMips(bool disable);
  bool IsDisableMips();

  static void RegisterNatives(tonic::DartLibraryNatives* natives);

 private:
  Performance();

  std::atomic_int64_t dart_image_memory_usage;  // KB

  std::atomic_bool disable_mipmaps_;

};

}
#endif  // FLUTTER_LIB_UI_PERFORMANCE_H_
