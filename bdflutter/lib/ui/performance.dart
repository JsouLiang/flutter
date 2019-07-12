/// BD ADD

// @dart = 2.9
part of dart.ui;

typedef TimeToFirstFrameMicrosCallback = void Function(int frameworkInitTime, int firstFrameTime);

class Performance {

  /// Memory usage of decoded image in dart heap external, in KB
  int getImageMemoryUsage() native 'Performance_imageMemoryUsage';

  int getEngineMainEnterMicros() native 'Performance_getEngineMainEnterMicros';

  TimeToFirstFrameMicrosCallback? onTimeToFirstFrameMicros;
  int timeToFrameworkInitMicros = 0;
  int timeToFirstFrameMicros = 0;

  void addNextFrameCallback(VoidCallback callback) native 'Performance_addNextFrameCallback';

  VoidCallback? get exitApp => _exitApp;
  VoidCallback? _exitApp;
  Zone _exitAppZone = Zone.root;
  set exitApp(VoidCallback? callback) {
    _exitApp = callback;
    _exitAppZone = Zone.current;
  }

  /**
   *  BD ADD:
   *
   *  [threadType]
   *     kUiThreadType = 1, get fps in ui thread
   *     kGpuThreadType = 2, get fps in gpu thread
   *
   *  [fpsType]
   *    kAvgFpsType = 1, get the average fps in the buffer
   *    kWorstFpsType = 2, get the worst fps in the buffer
   *
   *  [doClear]
   *    if true, will clear fps buffer after get fps
   *
   *  [result]
   *    result is a list,
   *    index [0] represents the fps value
   *    index [1] represents average time (or worst time in fpsType is kWorstFpsType)
   *    index [2] represents number of frames (or 0 in kWorstFpsType mode)
   *    index [3] represents number of dropped frames (or 0 in kWorstFpsType mode)
   */
  List getFps(int threadType, int fpsType, bool doClear) native 'performance_getFps';
  int getFpsMaxSamples() native 'Performance_getMaxSamples';
  void startRecordFps(String key) native 'Performance_startRecordFps';
  List obtainFps(String key, bool stopRecord) native 'Performance_obtainFps';
}

/// The [Performance] singleton.
final Performance performance = Performance();
