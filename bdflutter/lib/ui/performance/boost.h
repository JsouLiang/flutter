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
    kDisableAA = 1 << 1,            // Removed
    kDelayFuture = 1 << 2,          // Desperate
    kDelayPlatformMessage = 1 << 3, // Desperate
    kUiMessageAtHead = 1 << 4,      // Desperate
    kEnableWaitSwapBuffer = 1 << 5, // Removed
    kEnableExtendBuffer = 1 << 6,   // Removed
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

  bool IsGCDisabled();
  bool IsUiMessageAtHead();

  bool CanNotifyIdle();

  void ForceGC();

 private:
  Boost();
  ~Boost();

  uint16_t boost_flags_;

  int64_t gc_deadline_;

  int64_t ui_message_athead_deadline_;

  int64_t notify_idle_deadline_;

  FML_DISALLOW_COPY_AND_ASSIGN(Boost);
};

}  // namespace flutter
#endif /* boost_h */
