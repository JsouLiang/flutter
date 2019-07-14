// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iomanip>
#include <iostream>
#include <string>

#include "flutter/flow/layers/performance_overlay_layer.h"
#include "third_party/skia/include/core/SkFont.h"

namespace flutter {
namespace {

void DrawStatisticsText(SkCanvas& canvas,
                        const std::string& string,
                        int x,
                        int y,
                        const std::string& font_path) {
  SkFont font;
  if (font_path != "") {
    font = SkFont(SkTypeface::MakeFromFile(font_path.c_str()));
  }
  font.setSize(15);
  SkPaint paint;
  // BD MOD:
  // paint.setColor(SK_ColorGRAY);
  paint.setColor(SK_ColorRED);
  canvas.drawSimpleText(string.c_str(), string.size(), kUTF8_SkTextEncoding, x,
                        y, font, paint);
}

void VisualizeStopWatch(SkCanvas& canvas,
                        const Stopwatch& stopwatch,
                        SkScalar x,
                        SkScalar y,
                        SkScalar width,
                        SkScalar height,
                        bool show_graph,
                        bool show_labels,
                        const std::string& label_prefix,
                        const std::string& font_path) {
  const int label_x = 8;  // distance from x
  // BD MOD:
  // const int label_y = -10;  // distance from y+height
  const int label_y = -25;  // distance from y+height

  if (show_graph) {
    SkRect visualization_rect = SkRect::MakeXYWH(x, y, width, height);
    stopwatch.Visualize(canvas, visualization_rect);
  }

  if (show_labels) {
    double max_ms_per_frame = stopwatch.MaxDelta().ToMillisecondsF();
    double average_ms_per_frame = stopwatch.AverageDelta().ToMillisecondsF();
    // BD ADD:
    double fps = stopwatch.GetFps();
    std::stringstream stream;
    stream.setf(std::ios::fixed | std::ios::showpoint);
    stream << std::setprecision(1);
    stream << label_prefix
           << "  "
           // BD MOD: START
           //       << "max " << max_ms_per_frame << " ms/frame, "
           //       << "avg " << average_ms_per_frame << " ms/frame";
           << "max=" << max_ms_per_frame << " avg=" << average_ms_per_frame
           << " fps=" << fps;
    // END
    DrawStatisticsText(canvas, stream.str(), x + label_x, y + height + label_y,
                       font_path);
  }
}

}  // namespace

PerformanceOverlayLayer::PerformanceOverlayLayer(uint64_t options,
                                                 const char* font_path)
    : options_(options) {
  if (font_path != nullptr) {
    font_path_ = font_path;
  }
}

void PerformanceOverlayLayer::Paint(PaintContext& context) const {
  const int padding = 8;

  if (!options_)
    return;

  TRACE_EVENT0("flutter", "PerformanceOverlayLayer::Paint");
  SkScalar x = paint_bounds().x() + padding;
  SkScalar y = paint_bounds().y() + padding;
  SkScalar width = paint_bounds().width() - (padding * 2);
  SkScalar height = paint_bounds().height() / 2;
  SkAutoCanvasRestore save(context.leaf_nodes_canvas, true);

  VisualizeStopWatch(
      *context.leaf_nodes_canvas, context.frame_time, x, y, width,
      height - padding, options_ & kVisualizeRasterizerStatistics,
      options_ & kDisplayRasterizerStatistics, "GPU", font_path_);

  VisualizeStopWatch(*context.leaf_nodes_canvas, context.engine_time, x,
                     y + height, width, height - padding,
                     options_ & kVisualizeEngineStatistics,
                     options_ & kDisplayEngineStatistics, "UI", font_path_);
}

}  // namespace flutter
