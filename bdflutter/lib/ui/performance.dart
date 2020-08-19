/// BD ADD

// @dart = 2.9
part of dart.ui;

class Performance {

  /// Memory usage of decoded image in dart heap external, in KB
  int getImageMemoryUsage() native 'Performance_imageMemoryUsage';
}

/// The [Performance] singleton.
final Performance performance = Performance();
