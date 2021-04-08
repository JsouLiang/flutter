// BD ADD:
// Created by sunkun01 on 2019-11-06.
//

#include <flutter/fml/logging.h>
#include "fps_recorder.h"

namespace flutter {

static const std::string kAvg = "ApplicationAverage";

// TODO: Support non-60 refresh rate
static const double kFrameCountOneSecond = 60.0;

FpsRecorder::FpsRecorder() : is_drawn(false) {
  fps_data_[kAvg] = std::pair<size_t, size_t>(0, 0);
  ui_time_[kAvg] = std::pair<size_t, int64_t>(0, 0);
  gpu_time_[kAvg] = std::pair<size_t, int64_t>(0, 0);
}

FpsRecorder::~FpsRecorder() = default;

void FpsRecorder::AddFrameCount(int count, const fml::TimeDelta &timeDelta) {
  draw_lock.lock();
  if (count > 0) {
    for (auto &it : fps_data_) {
      it.second.first += count; // add FrameCount
    }
  }
  if (timeDelta == fml::TimeDelta::Zero()) {
    is_drawn = false;
  } else { // add CostTime
    for (auto &it : ui_time_) {
      it.second.first += 1;
      it.second.second += timeDelta.ToMicroseconds();
    }
  }
  draw_lock.unlock();
}

void FpsRecorder::AddDrawCount(const fml::TimeDelta &timeDelta) {
  draw_lock.lock();
  if (!is_drawn) { // is valid draw, so drawCount++
    for (auto &it : fps_data_) {
      if (it.second.first == 0) { // has no frameCount,abandon this drawCount
        continue;
      }
      it.second.second += 1;
    }
    is_drawn = true;
  }
  for (auto &it : gpu_time_) { // add costTime
    if (ui_time_[it.first].first == 0) { // has no frameCost,abandon this
      continue;
    }
    it.second.first += 1;
    it.second.second += timeDelta.ToMicroseconds();
  }
  draw_lock.unlock();
}

void FpsRecorder::StartRecordFps(const std::string &key) {
  draw_lock.lock();
  fps_data_[key] = std::make_pair(0, 0);
  ui_time_[key] = std::make_pair(0, 0);
  gpu_time_[key] = std::make_pair(0, 0);
  draw_lock.unlock();
}

std::vector<double> FpsRecorder::ObtainFpsData(const std::string &key, bool stopRecord) {
  draw_lock.lock();
  std::vector<double> result;
  result.resize(3);
  auto it = fps_data_.find(key);
  if (it != fps_data_.end()) {
    auto fps_value = it->second;
    if (fps_value.first == 0) { // frame count is 0
      result[0] = -1;
      result[1] = 0;
      result[2] = 0;
    } else {
      result[0] =
          (fps_value.second + (is_drawn ? 0 : 1)) * kFrameCountOneSecond / fps_value.first; // Fps value
      if (ui_time_[key].first == 0) { // count == 0
        result[1] = 0;
      } else {
        result[1] = ui_time_[key].second * 1.0 / ui_time_[key].first / 1000.0; // convert to millSeconds
      }
      if (gpu_time_[key].first == 0) { // count == 0
        result[2] = 0;
      } else {
        result[2] = gpu_time_[key].second * 1.0 / gpu_time_[key].first / 1000.0; // convert to millSeconds
      }
    }
    if (stopRecord) {
      fps_data_.erase(key);
      ui_time_.erase(key);
      gpu_time_.erase(key);
    }
  } else { // do not find the data for this key
    result[0] = -2;
    result[1] = 0;
    result[2] = 0;
  }
  draw_lock.unlock();
  return result;
}

} // namespace flutter
