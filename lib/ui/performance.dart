/// BD ADD

// @dart = 2.12
part of dart.ui;

typedef TimeToFirstFrameMicrosCallback = void Function(int frameworkInitTime, int firstFrameTime);
typedef NotifyIdleCallback = void Function(Duration duration);

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

  void startBoost(int flags, int millis) native 'Performance_startBoost';
  void finishBoost(int flags) native 'Performance_finishBoost';
  void forceGC() native 'Performance_forceGC';

  void disableMips(bool disable) native 'Performance_disableMips';

  void startStackTraceSamples() native 'Performance_startStackTraceSamples';

  void stopStackTraceSamples() native 'Performance_stopStackTraceSamples';

  String getStackTraceSamples(int microseconds) native 'Performance_getStackTraceSamples';

  bool requestHeapSnapshot(String outFilePath) native 'Performance_requestHeapSnapshot';

  String getHeapInfo() native 'Performance_heapInfo';

  NotifyIdleCallback? get onNotifyIdle => _onNotifyIdle;
  NotifyIdleCallback? _onNotifyIdle;
  Zone? _onNotifyIdleZone;
  set onNotifyIdle(NotifyIdleCallback? callback) {
    _onNotifyIdle = callback;
    _onNotifyIdleZone = Zone.current;
  }

  /// Get bitmap cache image info
  List getSkGraphicCacheMemoryUsage() native 'Performance_skGraphicCacheMemoryUsage';

  /// Get Gpu cache memory.
  Future<List> getGrResourceCacheMemInfo() {
    return _futurize(
          (_Callback<List> callback) => _getGrResourceCacheMemInfo(callback),
    );
  }

  String _getGrResourceCacheMemInfo(_Callback<List> callback) native 'Performance_getGpuCacheUsageKBInfo';

  /// Get total ext memory info, contains gpu bitmap image memory and so on.
  Future<List> getTotalExtMemInfo() {
    return _futurize(
          (_Callback<List> callback) => _getTotalExtMemInfo(callback),
    );
  }
  String _getTotalExtMemInfo(_Callback<List> callback) native 'Performance_getTotalExtMemInfo';

  /// Get Engine Init APM info
  List<dynamic> getEngineInitApmInfo() native 'Performance_getEngineInitApmInfo';

  void warmUpZeroSizeOnce(bool enable) native 'Performance_warmUpZeroSizeOnce';
}

/// The [Performance] singleton.
final Performance performance = Performance();
