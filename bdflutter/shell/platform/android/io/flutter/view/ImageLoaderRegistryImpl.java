package io.flutter.view;

import io.flutter.view.AndroidImageLoader.RealImageLoader;
import io.flutter.view.FlutterNativeView;
import io.flutter.view.ImageLoaderRegistry;

import io.flutter.embedding.engine.FlutterJNI;

/**
 * BD ADD:
 * ImageLoaderRegistryImpl
 */
public class ImageLoaderRegistryImpl implements ImageLoaderRegistry {

    private AndroidImageLoader mAndroidImageLoader;
    private FlutterNativeView mNativeView;
    private FlutterJNI mFlutterJNI;

    public ImageLoaderRegistryImpl(FlutterJNI flutterJNI) {
      mFlutterJNI = flutterJNI;
    }

    public ImageLoaderRegistryImpl(FlutterNativeView nativeView) {
      mNativeView = nativeView;
    }

    /**
     * register android image loader
     */
    @Override
    public void registerImageLoader(RealImageLoader realImageLoader) {
      ensureAndroidImageLoaderAttached();
      mAndroidImageLoader.registerImageLoader(realImageLoader);
    }
    /**
     * unregister android image loader
     */
    @Override
    public void unRegisterImageLoader() {
      mAndroidImageLoader.unRegisterImageLoader();
    }
    /**
     * initialize android image loader
     */
    private void ensureAndroidImageLoaderAttached() {
      if (mAndroidImageLoader != null) {
        return;
      }

      mAndroidImageLoader = new AndroidImageLoader();
      registerAndroidImageLoader(mAndroidImageLoader);
    }
    /**
     * register android image loader
     */
    private void registerAndroidImageLoader(AndroidImageLoader androidImageLoader) {
        if (mFlutterJNI == null) {
            mNativeView.getFlutterJNI().registerAndroidImageLoader(androidImageLoader);
        } else {
            mFlutterJNI.registerAndroidImageLoader(androidImageLoader);
        }
    }
    /**
     * unregister android image loader
     */
    private void unRegisterAndroidImageLoader() {
        if (mFlutterJNI == null) {
            mNativeView.getFlutterJNI().unRegisterAndroidImageLoader();
        } else {
            mFlutterJNI.unRegisterAndroidImageLoader();
        }
    }


}