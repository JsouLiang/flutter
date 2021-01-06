//
//  https://jira.bytedance.com/browse/FLUTTER-61
//  Created by wangying on 2019/8/6.
//

#include "boost.h"
#include <thread>
#include "third_party/skia/include/core/SkCanvas.h"
#include "flutter/lib/ui/text/font_collection.h"
#include "flutter/lib/ui/window/window.h"
#include "minikin/FontLanguageListCache.h"

namespace flutter {

static const int64_t kDefaultMaxTime = 60000;

static const int64_t kMaxSkipGCTime = 10000;

Boost::Boost()
    : boost_flags_(0),
      gc_deadline_(0),
      aa_deadline_(0),
      delay_future_deadline_(0),
      delay_platform_message_deadline_(0),
      ui_message_athead_deadline_(0),
      wait_swap_buffer_deadline_(0),
      vsync_received_(false),
      dart_frame_deadline_(TimePoint::Now()),
      extend_buffer_deadline_(0),
      extend_count_(0),
      extend_semaphore_(4),
      notify_idle_deadline_(0) {}

Boost::~Boost() = default;

void Boost::StartUp(uint16_t flags, int millis) {
  int64_t now = Dart_TimelineGetMicros();
  int64_t deadline = now + millis * 1000;
  int64_t limited_deadline =
      millis < kDefaultMaxTime ? deadline : (now + kDefaultMaxTime * 1000);

  if (flags & Flags::kDisableGC) {
    gc_deadline_ =
        millis < kMaxSkipGCTime ? deadline : (now + kMaxSkipGCTime * 1000);
#if defined(DART_PERFORMANCE_EXTENSION)
    Dart_SkipGCFromNow(millis < kMaxSkipGCTime ? millis : kMaxSkipGCTime);
#endif
  }
  if (flags & Flags::kDisableAA) {
    aa_deadline_ = deadline;
  }
  if (flags & Flags::kDelayFuture) {
    delay_future_deadline_ = limited_deadline;
    PostBarrierDelayedTask(millis, Flags::kDelayFuture);
  }
  if (flags & Flags::kDelayPlatformMessage) {
    delay_platform_message_deadline_ = limited_deadline;
    PostBarrierDelayedTask(millis, Flags::kDelayPlatformMessage);
  }
  if (flags & Flags::kUiMessageAtHead) {
    ui_message_athead_deadline_ = deadline;
  }
  if (flags & Flags::kEnableWaitSwapBuffer) {
    wait_swap_buffer_deadline_ = limited_deadline;
  }
  if (flags & Flags::kEnableExtendBuffer) {
    extend_buffer_deadline_ = deadline;
  }
  if (flags & Flags::kCanNotifyIdle) {
    notify_idle_deadline_ = deadline;
  }

  boost_flags_ |= flags;
}

void Boost::PostBarrierDelayedTask(int millis, uint16_t flag) {
  UIDartState* dart_state = UIDartState::Current();
  auto& task_runners = dart_state->GetTaskRunners();
  auto task_runner = flag == Flags::kDelayFuture
                         ? task_runners.GetUITaskRunner()
                         : task_runners.GetPlatformTaskRunner();
  task_runner->PostBarrier(true);

  TimeDelta delay = TimeDelta::FromMilliseconds(
      (millis < kDefaultMaxTime ? millis : kDefaultMaxTime) + 1);
  task_runners.GetUITaskRunner()->PostDelayedTask(
      [dart_state]() {
        if (!dart_state) {
          return;
        }
        tonic::DartState::Scope scope(dart_state);
        Boost::Current()->CheckFinished();
      },
      delay);
}

void Boost::CheckFinished() {
  if (boost_flags_ == 0) {
    return;
  }
  int64_t current_micros = Dart_TimelineGetMicros();
  uint16_t finish_flags = 0;
  if (gc_deadline_ > 0 && gc_deadline_ < current_micros) {
    finish_flags |= kDisableGC;
  }
  if (aa_deadline_ > 0 && aa_deadline_ < current_micros) {
    finish_flags |= kDisableAA;
  }
  if (wait_swap_buffer_deadline_ > 0 &&
      wait_swap_buffer_deadline_ < current_micros) {
    finish_flags |= kEnableWaitSwapBuffer;
  }
  if (extend_buffer_deadline_ > 0 && extend_buffer_deadline_ < current_micros) {
    finish_flags |= kEnableExtendBuffer;
  }
  if (delay_future_deadline_ > 0 && delay_future_deadline_ < current_micros) {
    finish_flags |= kDelayFuture;
  }
  if (delay_platform_message_deadline_ > 0 &&
      delay_platform_message_deadline_ < current_micros) {
    finish_flags |= kDelayPlatformMessage;
  }
  if (ui_message_athead_deadline_ > 0 &&
      ui_message_athead_deadline_ < current_micros) {
    finish_flags |= kUiMessageAtHead;
  }
  if (notify_idle_deadline_ > 0 && notify_idle_deadline_ < current_micros) {
    finish_flags |= kCanNotifyIdle;
  }
  Finish(finish_flags);
}

void Boost::Finish(uint16_t flags) {
  if (flags == 0) {
    return;
  }
  boost_flags_ &= ~flags;

  if (flags & kDisableGC) {
    gc_deadline_ = 0;
#if defined(DART_PERFORMANCE_EXTENSION)
    Dart_SkipGCFromNow(0);
#endif
  }
  if (flags & kDisableAA) {
    aa_deadline_ = 0;
  }
  if (flags & kEnableWaitSwapBuffer) {
    wait_swap_buffer_deadline_ = 0;
  }
  if (flags & Flags::kEnableExtendBuffer) {
    extend_buffer_deadline_ = 0;
  }
  if (flags & Flags::kDelayFuture) {
    delay_future_deadline_ = 0;
    auto task_runner =
        UIDartState::Current()->GetTaskRunners().GetUITaskRunner();
    task_runner->PostBarrier(false);
  }
  if (flags & Flags::kDelayPlatformMessage) {
    delay_platform_message_deadline_ = 0;
    auto task_runner =
        UIDartState::Current()->GetTaskRunners().GetPlatformTaskRunner();
    task_runner->PostBarrier(false);
  }
  if (flags & Flags::kUiMessageAtHead) {
    ui_message_athead_deadline_ = 0;
  }
}

bool Boost::IsAADisabled() {
#if defined(SKIA_PERFORMANCE_EXTENSION)
  return boost_flags_ & kDisableAA || gSkDisableAntiAlias;
#else
  return boost_flags_ & kDisableAA;
#endif
}

bool Boost::IsGCDisabled() {
  return boost_flags_ & kDisableGC;
}

bool Boost::IsDelayFuture() {
  return boost_flags_ & kDelayFuture;
}

bool Boost::IsDelayPlatformMessage() {
  return boost_flags_ & kDelayPlatformMessage;
}

bool Boost::IsUiMessageAtHead() {
  return boost_flags_ & kUiMessageAtHead;
}

bool Boost::IsEnableExtendBuffer() {
  return boost_flags_ & kEnableExtendBuffer;
}

bool Boost::IsValidExtension() {
  return IsEnableExtendBuffer() && extend_semaphore_.IsValid();
}

bool Boost::TryWaitExtension() {
  if (IsEnableExtendBuffer() && extend_semaphore_.TryWait()) {
    extend_count_++;
    return true;
  }
  return false;
}

bool Boost::SignalExtension() {
  if (extend_count_ <= 0) {
    return false;
  }
  extend_count_--;
  extend_semaphore_.Signal();
  return true;
}

void Boost::UpdateVsync(bool received, TimePoint frame_target_time) {
  vsync_received_ = received;
  if (received) {
    dart_frame_deadline_ = frame_target_time;
  }
}

// If haven't received the vsync signal, wait util the frame dead line.
void Boost::WaitSwapBufferIfNeed() {
  if (vsync_received_ || !(boost_flags_ & kEnableWaitSwapBuffer)) {
    return;
  }
  double next_frame_time =
      (dart_frame_deadline_ - TimePoint::Now()).ToMillisecondsF() + 0.5;
  if (next_frame_time < 0) {
    return;
  }
  if (next_frame_time > 16) {
    next_frame_time = 16;
  }
  AutoResetWaitableEvent swap_buffer_wait;
  swap_buffer_wait.WaitWithTimeout(
      TimeDelta::FromMilliseconds(next_frame_time));
}

bool Boost::CanNotifyIdle() {
  return boost_flags_ & Flags::kCanNotifyIdle;
}

void Boost::PreloadFontFamilies(const std::vector<std::string>& font_families,
                                const std::string& locale) {
  FontCollection& collection =
      UIDartState::Current()->window()->client()->GetFontCollection();
  std::string minikin_locale;
  if (!locale.empty()) {
    uint32_t language_list_id =
        minikin::FontStyle::registerLanguageList(locale);
    const minikin::FontLanguages& langs =
        minikin::FontLanguageListCache::getById(language_list_id);
    if (langs.size()) {
      minikin_locale = langs[0].getString();
    }
  }
  collection.GetFontCollection()->GetMinikinFontCollectionForFamilies(
      font_families, minikin_locale);
}

void Boost::ForceGC() {
#if defined(DART_PERFORMANCE_EXTENSION)
  Dart_ForceGC();
#endif
}


void Boost::DisableMips(bool disable) {
  disable_mipmaps_ = disable;
}

bool Boost::IsDisableMips(){
  return disable_mipmaps_ ;
}

void Boost::WarmUpZeroSizeOnce(bool warmUpOnce) {
  warmUpOnce_ = warmUpOnce;
}

bool Boost::PerformWarmUpZeroSize() {
  bool result = warmUpOnce_;
  warmUpOnce_ = false;
  return result;
}
}  // namespace flutter
