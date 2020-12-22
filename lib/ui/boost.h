//
//  https://jira.bytedance.com/browse/FLUTTER-61
//  Created by wangying on 2019/8/6.
//

#ifndef BOOST_H
#define BOOST_H

#include <stdio.h>
#include <atomic>
#include "flutter/fml/synchronization/semaphore.h"
#include "flutter/fml/synchronization/waitable_event.h"
#include "flutter/fml/time/time_point.h"
#include "flutter/lib/ui/ui_dart_state.h"
#include "third_party/dart/runtime/include/dart_tools_api.h"

namespace flutter {
using namespace fml;
using namespace std;

class Boost {
 public:
  enum Flags {
    kDisableGC = 1 << 0,
    kDisableAA = 1 << 1,
    kDelayFuture = 1 << 2,
    kDelayPlatformMessage = 1 << 3,
    kUiMessageAtHead = 1 << 4,
    kEnableWaitSwapBuffer = 1 << 5,
    kEnableExtendBuffer = 1 << 6,
    kCanNotifyIdle = 1 << 7,
  };
    
  enum IdleTypes {
    kPageQuiet = 1 << 0,
    kVsyncIdle = 1 << 1,
  };

 public:
  static Boost* Current() {
    static Boost instance;
    return &instance;
  }

  void StartUp(uint16_t flags, int millis);
  void CheckFinished();
  void Finish(uint16_t flags);

  bool IsAADisabled();
  bool IsGCDisabled();
  bool IsDelayFuture();
  bool IsDelayPlatformMessage();
  bool IsUiMessageAtHead();

  void WaitSwapBufferIfNeed();
  void UpdateVsync(bool received,
                   TimePoint frame_target_time = TimePoint::Now());

  bool IsEnableExtendBuffer();
  bool IsValidExtension();
  bool TryWaitExtension();
  bool SignalExtension();

  void PreloadFontFamilies(const std::vector<std::string>& font_families,
                           const std::string& locale);
  
  bool CanNotifyIdle();

  void ForceGC();

  void DisableMips(bool disable);
  bool IsDisableMips();

  void WarmUpZeroSizeOnce(bool warmUpOnce);
  bool PerformWarmUpZeroSize();

 private:
  Boost();
  ~Boost();

  void PostBarrierDelayedTask(int millis, uint16_t flag);

  uint16_t boost_flags_;

  int64_t gc_deadline_;

  int64_t aa_deadline_;

  int64_t delay_future_deadline_;

  int64_t delay_platform_message_deadline_;

  int64_t ui_message_athead_deadline_;

  int64_t wait_swap_buffer_deadline_;
  atomic_bool vsync_received_;
  TimePoint dart_frame_deadline_;

  int64_t extend_buffer_deadline_;
  atomic_char extend_count_;
  fml::Semaphore extend_semaphore_;
  
  int64_t notify_idle_deadline_;

  atomic_bool disable_mipmaps_;
  atomic_bool warmUpOnce_;
  FML_DISALLOW_COPY_AND_ASSIGN(Boost);
};

}  // namespace flutter
#endif /* boost_h */
