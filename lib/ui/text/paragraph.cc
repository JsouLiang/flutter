// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/lib/ui/text/paragraph.h"

#include "flutter/common/settings.h"
#include "flutter/common/task_runners.h"
#include "flutter/fml/logging.h"
#include "flutter/fml/task_runner.h"
#include "third_party/tonic/converter/dart_converter.h"
#include "third_party/tonic/dart_args.h"
#include "third_party/tonic/dart_binding_macros.h"
#include "third_party/tonic/dart_library_natives.h"
// BD ADD: START
#include <flutter/bdflutter/lib/ui/performance/performance.h>
#include "flutter/fml/make_copyable.h"
#include "third_party/tonic/logging/dart_invoke.h"
// END

using tonic::ToDart;

namespace flutter {

// BD ADD: START
double clamp(double num, double lowerLimit, double upperLimit) {
  return num < lowerLimit ? lowerLimit : (num > upperLimit ? upperLimit : num);
}
// END

IMPLEMENT_WRAPPERTYPEINFO(ui, Paragraph);

#define FOR_EACH_BINDING(V)             \
  V(Paragraph, width)                   \
  V(Paragraph, height)                  \
  V(Paragraph, longestLine)             \
  V(Paragraph, minIntrinsicWidth)       \
  V(Paragraph, maxIntrinsicWidth)       \
  V(Paragraph, alphabeticBaseline)      \
  V(Paragraph, ideographicBaseline)     \
  V(Paragraph, didExceedMaxLines)       \
  V(Paragraph, layout)                  \
  V(Paragraph, paint)                   \
  V(Paragraph, getWordBoundary)         \
  V(Paragraph, getLineBoundary)         \
  V(Paragraph, getRectsForRange)        \
  V(Paragraph, getRectsForPlaceholders) \
  V(Paragraph, getPositionForOffset)    \
  V(Paragraph, computeLineMetrics)

DART_BIND_ALL(Paragraph, FOR_EACH_BINDING)

Paragraph::Paragraph(std::unique_ptr<txt::Paragraph> paragraph)
    :  // BD MOD:
       //  m_paragraph(std::move(paragraph)) {}
      m_paragraph(std::move(paragraph)) {}

Paragraph::~Paragraph() = default;

size_t Paragraph::GetAllocationSize() const {
  // We don't have an accurate accounting of the paragraph's memory consumption,
  // so return a fixed size to indicate that its impact is more than the size
  // of the Paragraph class.
  return 2000;
}

double Paragraph::width() {
  return m_paragraph->GetMaxWidth();
}

double Paragraph::height() {
  return m_paragraph->GetHeight();
}

double Paragraph::longestLine() {
  return m_paragraph->GetLongestLine();
}

double Paragraph::minIntrinsicWidth() {
  return m_paragraph->GetMinIntrinsicWidth();
}

double Paragraph::maxIntrinsicWidth() {
  return m_paragraph->GetMaxIntrinsicWidth();
}

double Paragraph::alphabeticBaseline() {
  return m_paragraph->GetAlphabeticBaseline();
}

double Paragraph::ideographicBaseline() {
  return m_paragraph->GetIdeographicBaseline();
}

bool Paragraph::didExceedMaxLines() {
  return m_paragraph->DidExceedMaxLines();
}

// BD MOD: START
// void Paragraph::layout(double width) {
//   m_paragraph->layout(width);
// }
void Paragraph::layout(double maxWidth,
                       double minWidth,
                       bool async,
                       Dart_Handle callback_handle) {
  tonic::DartPersistentValue* callback = new tonic::DartPersistentValue(
      tonic::DartState::Current(), callback_handle);
  const auto& task_runners = UIDartState::Current()->GetTaskRunners();
  m_paragraph->SetAsyncMode(async);

  auto closure = [weak_paragraph = std::weak_ptr<txt::Paragraph>(m_paragraph),
                  ui_task_runner = task_runners.GetUITaskRunner(), maxWidth,
                  minWidth, callback]() {
    if (auto paragraph = weak_paragraph.lock()) {
      // BD ADD:
      Performance::GetInstance()->RecordLastLayoutTime();
      paragraph->Layout(maxWidth);

      if (minWidth >= 0 && minWidth != maxWidth) {
        double newWidth =
            clamp(ceil(paragraph->GetMaxIntrinsicWidth()), minWidth, maxWidth);
        if (newWidth != ceil(paragraph->GetMaxWidth())) {
          paragraph->Layout(newWidth);
        }
      }
    }

    fml::TaskRunner::RunNowOrPostTask(ui_task_runner, [callback]() {
      std::shared_ptr<tonic::DartState> dart_state =
          callback->dart_state().lock();
      if (!dart_state) {
        return;
      }
      tonic::DartState::Scope scope(dart_state);
      if (Dart_IsClosure(callback->value())) {
        tonic::DartInvoke(callback->value(), {});
      }
      delete callback;
    });
  };

  if (async) {
    task_runners.GetIOTaskRunner()->PostTask(closure);
  } else {
    closure();
  }
}
// end

void Paragraph::paint(Canvas* canvas, double x, double y) {
  SkCanvas* sk_canvas = canvas->canvas();
  if (!sk_canvas) {
    return;
  }
  m_paragraph->Paint(sk_canvas, x, y);
}

static tonic::Float32List EncodeTextBoxes(
    const std::vector<txt::Paragraph::TextBox>& boxes) {
  // Layout:
  // First value is the number of values.
  // Then there are boxes.size() groups of 5 which are LTRBD, where D is the
  // text direction index.
  tonic::Float32List result(
      Dart_NewTypedData(Dart_TypedData_kFloat32, boxes.size() * 5));
  uint64_t position = 0;
  for (uint64_t i = 0; i < boxes.size(); i++) {
    const txt::Paragraph::TextBox& box = boxes[i];
    result[position++] = box.rect.fLeft;
    result[position++] = box.rect.fTop;
    result[position++] = box.rect.fRight;
    result[position++] = box.rect.fBottom;
    result[position++] = static_cast<float>(box.direction);
  }
  return result;
}

tonic::Float32List Paragraph::getRectsForRange(unsigned start,
                                               unsigned end,
                                               unsigned boxHeightStyle,
                                               unsigned boxWidthStyle) {
  std::vector<txt::Paragraph::TextBox> boxes = m_paragraph->GetRectsForRange(
      start, end, static_cast<txt::Paragraph::RectHeightStyle>(boxHeightStyle),
      static_cast<txt::Paragraph::RectWidthStyle>(boxWidthStyle));
  return EncodeTextBoxes(boxes);
}

tonic::Float32List Paragraph::getRectsForPlaceholders() {
  std::vector<txt::Paragraph::TextBox> boxes =
      m_paragraph->GetRectsForPlaceholders();
  return EncodeTextBoxes(boxes);
}

Dart_Handle Paragraph::getPositionForOffset(double dx, double dy) {
  txt::Paragraph::PositionWithAffinity pos =
      m_paragraph->GetGlyphPositionAtCoordinate(dx, dy);
  std::vector<size_t> result = {
      pos.position,                      // size_t already
      static_cast<size_t>(pos.affinity)  // affinity (enum)
  };
  return tonic::DartConverter<decltype(result)>::ToDart(result);
}

Dart_Handle Paragraph::getWordBoundary(unsigned offset) {
  txt::Paragraph::Range<size_t> point = m_paragraph->GetWordBoundary(offset);
  std::vector<size_t> result = {point.start, point.end};
  return tonic::DartConverter<decltype(result)>::ToDart(result);
}

Dart_Handle Paragraph::getLineBoundary(unsigned offset) {
  std::vector<txt::LineMetrics> metrics = m_paragraph->GetLineMetrics();
  int line_start = -1;
  int line_end = -1;
  for (txt::LineMetrics& line : metrics) {
    if (offset >= line.start_index && offset <= line.end_index) {
      line_start = line.start_index;
      line_end = line.end_index;
      break;
    }
  }
  std::vector<int> result = {line_start, line_end};
  return tonic::DartConverter<decltype(result)>::ToDart(result);
}

tonic::Float64List Paragraph::computeLineMetrics() {
  std::vector<txt::LineMetrics> metrics = m_paragraph->GetLineMetrics();

  // Layout:
  // boxes.size() groups of 9 which are the line metrics
  // properties
  tonic::Float64List result(
      Dart_NewTypedData(Dart_TypedData_kFloat64, metrics.size() * 9));
  uint64_t position = 0;
  for (uint64_t i = 0; i < metrics.size(); i++) {
    const txt::LineMetrics& line = metrics[i];
    result[position++] = static_cast<double>(line.hard_break);
    result[position++] = line.ascent;
    result[position++] = line.descent;
    result[position++] = line.unscaled_ascent;
    // We add then round to get the height. The
    // definition of height here is different
    // than the one in LibTxt.
    result[position++] = round(line.ascent + line.descent);
    result[position++] = line.width;
    result[position++] = line.left;
    result[position++] = line.baseline;
    result[position++] = static_cast<double>(line.line_number);
  }

  return result;
}

}  // namespace flutter
