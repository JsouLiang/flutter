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
}

/// The [Performance] singleton.
final Performance performance = Performance();
