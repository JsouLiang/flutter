package io.flutter.view;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.util.AttributeSet;
import android.view.TextureView;
import android.view.WindowInsets;

import java.nio.ByteBuffer;

import io.flutter.app.FlutterPluginRegistry;
import io.flutter.plugin.common.BinaryMessenger;

@Deprecated
public class FlutterTextureView extends TextureView implements BinaryMessenger {

    @Deprecated
    public FlutterTextureView(Context context) {
        this(context, null);
    }

    @Deprecated
    public FlutterTextureView(Context context, AttributeSet attrs) {
        this(context, attrs, null);
    }

    @Deprecated
    public FlutterTextureView(Context context, AttributeSet attrs, FlutterNativeView nativeView) {
        super(context.getApplicationContext(), attrs);
        throw new UnsupportedOperationException("io.flutter.view.FlutterTextureView is not supported from version 1.22");
    }

    public void attachActivity(Activity activity) {

    }

    public void detachActivity() {

    }

    public FlutterTextureView.ViewportMetrics getViewPortMetrics() {
        return null;
    }

    public void updateViewportMetrics(FlutterTextureView.ViewportMetrics viewportMetrics) {

    }

    public FlutterNativeView getFlutterNativeView() {
        return null;
    }

    public FlutterPluginRegistry getPluginRegistry() {
        return null;
    }

    public void onStart() {

    }

    public void onPause() {

    }

    public void onPostResume() {

    }

    public void onStop() {

    }

    public void onMemoryPressure() {

    }

    public void addFirstFrameListener(FlutterTextureView.FirstFrameListener listener) {

    }

    public void removeFirstFrameListener(FlutterTextureView.FirstFrameListener listener) {

    }

    public void setInitialRoute(String route) {

    }

    public void pushRoute(String route) {

    }

    public void popRoute() {

    }

    public void destroy() {

    }

    public final WindowInsets onApplyWindowInsets(WindowInsets insets) {
        return null;
    }

    public void runFromBundle(FlutterRunArguments args) {

    }

    public Bitmap getBitmap() {
        return null;
    }

    public void onFirstFrame() {
    }

    @Override
    public void send(String s, ByteBuffer byteBuffer) {

    }

    @Override
    public void send(String s, ByteBuffer byteBuffer, BinaryReply binaryReply) {

    }

    @Override
    public void setMessageHandler(String s, BinaryMessageHandler binaryMessageHandler) {

    }

    public interface FirstFrameListener {
        void onFirstFrame();
    }

    public static final class ViewportMetrics {

    }

    public interface Provider {
        FlutterTextureView getFlutterView();
    }
}