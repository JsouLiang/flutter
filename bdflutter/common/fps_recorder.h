// BD ADD:
// Created by sunkun01 on 2019-11-06.
//

#ifndef FLUTTER_FPS_RECORDER_H
#define FLUTTER_FPS_RECORDER_H

#include <vector>
#include <string>
#include <map>
#include "flutter/fml/time/time_delta.h"
#include "flutter/fml/synchronization/waitable_event.h"

namespace flutter {

class FpsRecorder {
 public:
  static FpsRecorder *Current() {
    static std::once_flag flag;
    static FpsRecorder *instance;
    std::call_once(flag, [] { instance = new FpsRecorder(); });
    return instance;
  }

  /**
   * frame count++ when vsync arrive
   * add missing count and cost time when UI Thread finished
   * @param count: value is 1 when vsync arrive,value is missCount when UI Thread finished
   * @param timeDelta: has data when UI Thread finished
   */
  void AddFrameCount(int count, const fml::TimeDelta &timeDelta = fml::TimeDelta::Zero());

  void AddDrawCount(const fml::TimeDelta &timeDelta);

  void StartRecordFps(const std::string &key);

  /**
   *
   * @param key
   * @param stopRecord :true :Clear Data
   * @return
   */
  std::vector<double> ObtainFpsData(const std::string &key, bool stopRecord);

  FpsRecorder();

  ~FpsRecorder();

 private:
  bool is_drawn;
  std::mutex draw_lock;
  /// key:FpsKey for record, Value is pair,fist is frameCount, second is DrawCount
  std::map<std::string, std::pair<size_t, size_t >> fps_data_;
  /// key:FpsKey for record, Value is pair,fist is count, second is total Time
  std::map<std::string, std::pair<size_t, int64_t >> ui_time_;
  /// key:FpsKey for record, Value is pair,fist is count, second is total Time
  std::map<std::string, std::pair<size_t, int64_t >> gpu_time_;
};

} // namespace flutter
#endif //FLUTTER_FPS_RECORDER_H
