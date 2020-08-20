package io.flutter.view;

import io.flutter.view.AndroidImageLoader.RealImageLoader;

/** BD ADD: register image loader impl to flutter */
public interface ImageLoaderRegistry {
  /**
   * register image loader
   *
   * @param imageLoader the image loader impl
   */
  void registerImageLoader(RealImageLoader imageLoader);

  /** unregister image loader */
  void unRegisterImageLoader();
}
