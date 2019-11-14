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
}

/// The [Performance] singleton.
final Performance performance = Performance();
