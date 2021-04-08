package io.flutter.view;

import android.graphics.Bitmap;
import android.support.annotation.Keep;

/**
 * BD ADD: Interface for image load callback
 */
@Keep
public class NativeLoadCallback {

    public native void nativeSuccessCallback(String key, Bitmap bitmap);

    public native void nativeCodecSuccessCallback(String key, NativeCodec codec);

    public native void nativeGetNextFrameSuccessCallback(String key, Bitmap bitmap);

    public native void nativeFailCallback(String key);

    public native void nativeCodecFailCallback(String key);

    public native void nativeGetNextFrameFailCallback(String key);

    public void onLoadSuccess(String key, Bitmap bitmap) {
        nativeSuccessCallback(key, bitmap);
    }

    public void onLoadCodecSuccess(String key, NativeCodec codec) {
        nativeCodecSuccessCallback(key, codec);
    }

    public void onGetNextFrameSuccess(String key, Bitmap bitmap) {
        nativeGetNextFrameSuccessCallback(key, bitmap);
    }

    public void onLoadSuccess(String key, NativeCodec codec) {
        nativeCodecSuccessCallback(key, codec);
    }

    public void onLoadFail(String key) {
        nativeFailCallback(key);
    }

    public void onCodecLoadFail(String key) {
        nativeCodecFailCallback(key);
    }

    public void onGetNextFrameFail(String key) {
        nativeGetNextFrameFailCallback(key);
    }
}