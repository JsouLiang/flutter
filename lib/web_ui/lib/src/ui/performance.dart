/// BD ADD

part of ui;

typedef TimeToFirstFrameMicrosCallback = void Function(int frameworkInitTime, int firstFrameTime);
typedef NotifyIdleCallback = void Function(Duration duration);

class Performance {

  /// Memory usage of decoded image in dart heap external, in KB
  int getImageMemoryUsage() { return -1; }

  int getEngineMainEnterMicros() { return -1; }

  TimeToFirstFrameMicrosCallback? onTimeToFirstFrameMicros;
  int timeToFrameworkInitMicros = 0;
  int timeToFirstFrameMicros = 0;

  void addNextFrameCallback(VoidCallback callback) { }

  VoidCallback? get exitApp => _exitApp;
  VoidCallback? _exitApp;
  Zone _exitAppZone = Zone.root;
  set exitApp(VoidCallback? callback) {
    _exitApp = callback;
    _exitAppZone = Zone.current;
  }

  final double imageResizeRatio = 1.189; // sqrt(sqrt(2))
  int _maxImageWidthByUser = -1;
  int _maxImageWidthByViewport = 0;
  int _screenWidth = 0;

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
  List getFps(int threadType, int fpsType, bool doClear) { return []; }
  int getFpsMaxSamples() { return -1; }
  void startRecordFps(String key) { }
  List obtainFps(String key, bool stopRecord) { return []; }

  void startBoost(int flags, int millis) {}
  void finishBoost(int flags) { }
  void forceGC() { }

  void disableMips(bool disable) { }

  void startStackTraceSamples() { }

  void stopStackTraceSamples() { }

  String getStackTraceSamples(int microseconds) { return ''; }

  bool requestHeapSnapshot(String outFilePath) { return false; }

  NotifyIdleCallback? get onNotifyIdle => _onNotifyIdle;
  NotifyIdleCallback? _onNotifyIdle;
  Zone? _onNotifyIdleZone;
  set onNotifyIdle(NotifyIdleCallback? callback) {
    _onNotifyIdle = callback;
    _onNotifyIdleZone = Zone.current;
  }

  /// Get bitmap cache image info
  List getSkGraphicCacheMemoryUsage() { return []; }

  /// Get Gpu cache memory.
  Future<List> getGrResourceCacheMemInfo() {
    return _futurize(
          (_Callback<List> callback) => _getGrResourceCacheMemInfo(callback),
    );
  }

  String _getGrResourceCacheMemInfo(_Callback<List> callback) { return ''; }

  /// Get total ext memory info, contains gpu bitmap image memory and so on.
  Future<List> getTotalExtMemInfo() {
    return _futurize(
          (_Callback<List> callback) => _getTotalExtMemInfo(callback),
    );
  }
  String _getTotalExtMemInfo(_Callback<List> callback) { return ''; }

  /// Get Engine Init APM info
  List<dynamic> getEngineInitApmInfo() { return []; }

  void warmUpZeroSizeOnce(bool enable) { }

  int getRecordLastLayoutTime() { return -1; }
  void clearHugeFontCache() { }
  void clearAllFontCache() { }
  void clearLayoutCache() { }

  void updateScreenWidth(int width) {
    if (width != _screenWidth) {
      _screenWidth = math.max(width, _screenWidth);
      _maxImageWidthByViewport = (_screenWidth * imageResizeRatio).toInt();
    }
  }

  /// @brief      If width is 0, resize by viewport width
  ///             If width is greater than 0, resize by width
  ///             If width is lower than 0, disable resize
  ///
  void setMaxImageWidth(int maxImageWidth) {
    _maxImageWidthByUser = maxImageWidth;
  }

  int getMaxImageWidth() {
    return _maxImageWidthByUser != 0 ? _maxImageWidthByUser : _screenWidth;
  }

  /// @brief      Disable resize default
  ///             If @maxImageWidthByUser_ lower than 0, disable resize
  ///             else if origin image width is getter than @maxImageWidthByUser_
  ///             or @maxImageWidthByViewport, enable resize
  ///
  /// @return     Returns if need resize image by @maxImageWidthByUser_ or
  ///             @viewportSize_ width
  ///
  bool shouldResizeImage(int width) {
    if (_maxImageWidthByUser < 0) {
      return false;
    }
    int limit = _maxImageWidthByUser != 0 ? _maxImageWidthByUser : _maxImageWidthByViewport;
    return limit > 0 && width > limit;
  }
}

/// The [Performance] singleton.
final Performance performance = Performance();
