package io.flutter.view;

import androidx.annotation.Keep;

/** BD ADD: android image loader */
@Keep
public class AndroidImageLoader {
  /** Interface for those objects that load a image */
  public interface RealImageLoader {
    /**
     * load the given url
     *
     * @param url the image url to load
     * @param callback the callback to notify load result
     */
    void load(
        String url, int width, int height, float scale, NativeLoadCallback callback, String key);

    /**
     * release android image resources
     *
     * @param key the key associated to image resource
     */
    void release(String key);
  }

  public abstract static class ImageLoader implements RealImageLoader {

    /**
     * load animated image next frame.
     *
     * @param currentFrame current frame.
     * @param codec
     * @param callback
     * @param key
     */
    public abstract void getNextFrame(
        int currentFrame, Object codec, NativeLoadCallback callback, String key);
  }

  private RealImageLoader realImageLoader;

  /**
   * load the given url
   *
   * @param url the image url to load
   * @param callback the callback to notify load result
   */
  void load(
      String url, int width, int height, float scale, NativeLoadCallback callback, String key) {
    if (realImageLoader == null) {
      callback.onLoadFail(key);
      return;
    }
    realImageLoader.load(url, width, height, scale, callback, key);
  }

  void getNextFrame(int currentFrame, Object codec, NativeLoadCallback callback, String key) {
    if (realImageLoader == null) {
      callback.onLoadFail(key);
      return;
    }
    if (realImageLoader instanceof ImageLoader) {
      ImageLoader imageLoader = (ImageLoader) realImageLoader;
      imageLoader.getNextFrame(currentFrame, codec, callback, key);
    }
  }

  /**
   * release android image resources
   *
   * @param key the key associated to image resource
   */
  void release(String key) {
    if (realImageLoader != null) {
      realImageLoader.release(key);
    }
  }

  public void registerImageLoader(RealImageLoader realImageLoader) {
    this.realImageLoader = realImageLoader;
  }

  public void unRegisterImageLoader() {
    this.realImageLoader = null;
  }
}
