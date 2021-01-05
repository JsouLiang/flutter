// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.engine;

import android.content.Context;
import android.content.res.AssetManager;
import android.graphics.Bitmap;
import android.graphics.SurfaceTexture;
import android.os.Build;
import android.os.Looper;
import android.view.Surface;
import android.view.SurfaceHolder;
import androidx.annotation.Keep;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.UiThread;
import androidx.annotation.VisibleForTesting;
import io.flutter.Log;
import io.flutter.embedding.engine.FlutterEngine.EngineLifecycleListener;
import io.flutter.embedding.engine.dart.PlatformMessageHandler;
import io.flutter.embedding.engine.mutatorsstack.FlutterMutatorsStack;
import io.flutter.embedding.engine.renderer.FlutterUiDisplayListener;
import io.flutter.embedding.engine.renderer.RenderSurface;
import io.flutter.plugin.common.StandardMessageCodec;
import io.flutter.plugin.localization.LocalizationPlugin;
import io.flutter.plugin.platform.PlatformViewsController;
import io.flutter.view.AccessibilityBridge;
import io.flutter.view.FlutterCallbackInformation;
// BD ADD: START
import io.flutter.view.AndroidImageLoader;
import java.nio.ByteBuffer;
import java.util.*;
import java.util.concurrent.CopyOnWriteArraySet;
// END

/**
 * Interface between Flutter embedding's Java code and Flutter engine's C/C++ code.
 *
 * <p>Flutter's engine is built with C/C++. The Android Flutter embedding is responsible for
 * coordinating Android OS events and app user interactions with the C/C++ engine. Such coordination
 * requires messaging from an Android app in Java code to the C/C++ engine code. This communication
 * requires a JNI (Java Native Interface) API to cross the Java/native boundary.
 *
 * <p>The entirety of Flutter's JNI API is codified in {@code FlutterJNI}. There are multiple
 * reasons that all such calls are centralized in one class. First, JNI calls are inherently static
 * and contain no Java implementation, therefore there is little reason to associate calls with
 * different classes. Second, every JNI call must be registered in C/C++ code and this registration
 * becomes more complicated with every additional Java class that contains JNI calls. Third, most
 * Android developers are not familiar with native development or JNI intricacies, therefore it is
 * in the interest of future maintenance to reduce the API surface that includes JNI declarations.
 * Thus, all Flutter JNI calls are centralized in {@code FlutterJNI}.
 *
 * <p>Despite the fact that individual JNI calls are inherently static, there is state that exists
 * within {@code FlutterJNI}. Most calls within {@code FlutterJNI} correspond to a specific
 * "platform view", of which there may be many. Therefore, each {@code FlutterJNI} instance holds
 * onto a "native platform view ID" after {@link #attachToNative(boolean)}, which is shared with the
 * native C/C++ engine code. That ID is passed to every platform-view-specific native method. ID
 * management is handled within {@code FlutterJNI} so that developers don't have to hold onto that
 * ID.
 *
 * <p>To connect part of an Android app to Flutter's C/C++ engine, instantiate a {@code FlutterJNI}
 * and then attach it to the native side:
 *
 * <pre>{@code
 * // Instantiate FlutterJNI and attach to the native side.
 * FlutterJNI flutterJNI = new FlutterJNI();
 * flutterJNI.attachToNative();
 *
 * // Use FlutterJNI as desired. flutterJNI.dispatchPointerDataPacket(...);
 *
 * // Destroy the connection to the native side and cleanup.
 * flutterJNI.detachFromNativeAndReleaseResources();
 * }</pre>
 *
 * <p>To provide a visual, interactive surface for Flutter rendering and touch events, register a
 * {@link RenderSurface} with {@link #setRenderSurface(RenderSurface)}
 *
 * <p>To receive callbacks for certain events that occur on the native side, register listeners:
 *
 * <ol>
 *   <li>{@link #addEngineLifecycleListener(EngineLifecycleListener)}
 *   <li>{@link #addIsDisplayingFlutterUiListener(FlutterUiDisplayListener)}
 * </ol>
 *
 * To facilitate platform messages between Java and Dart running in Flutter, register a handler:
 *
 * <p>{@link #setPlatformMessageHandler(PlatformMessageHandler)}
 *
 * <p>To invoke a native method that is not associated with a platform view, invoke it statically:
 *
 * <p>{@code bool enabled = FlutterJNI.getIsSoftwareRenderingEnabled(); }
 */
@Keep
public class FlutterJNI {
  private static final String TAG = "FlutterJNI";

  @Nullable private static AsyncWaitForVsyncDelegate asyncWaitForVsyncDelegate;
  // This should also be updated by FlutterView when it is attached to a Display.
  // The initial value of 0.0 indicates unknown refresh rate.
  private static float refreshRateFPS = 0.0f;

  // This is set from native code via JNI.
  @Nullable private static String observatoryUri;

  // TODO(mattcarroll): add javadocs
  public static native void nativeInit(
      @NonNull Context context,
      @NonNull String[] args,
      @Nullable String bundlePath,
      @NonNull String appStoragePath,
      @NonNull String engineCachesPath,
      long initTimeMillis);

  /**
   * Prefetch the default font manager provided by SkFontMgr::RefDefault() which is a process-wide
   * singleton owned by Skia. Note that, the first call to SkFontMgr::RefDefault() will take
   * noticeable time, but later calls will return a reference to the preexisting font manager.
   */
  public static native void nativePrefetchDefaultFontManager();

  private native boolean nativeGetIsSoftwareRenderingEnabled();

  @UiThread
  // TODO(mattcarroll): add javadocs
  public boolean getIsSoftwareRenderingEnabled() {
    return nativeGetIsSoftwareRenderingEnabled();
  }

  @Nullable
  // TODO(mattcarroll): add javadocs
  public static String getObservatoryUri() {
    return observatoryUri;
  }

  public static void setRefreshRateFPS(float refreshRateFPS) {
    FlutterJNI.refreshRateFPS = refreshRateFPS;
  }

  // TODO(mattcarroll): add javadocs
  public static void setAsyncWaitForVsyncDelegate(@Nullable AsyncWaitForVsyncDelegate delegate) {
    asyncWaitForVsyncDelegate = delegate;
  }

  // TODO(mattcarroll): add javadocs
  // Called by native.
  private static void asyncWaitForVsync(final long cookie) {
    if (asyncWaitForVsyncDelegate != null) {
      asyncWaitForVsyncDelegate.asyncWaitForVsync(cookie);
    } else {
      throw new IllegalStateException(
          "An AsyncWaitForVsyncDelegate must be registered with FlutterJNI before asyncWaitForVsync() is invoked.");
    }
  }
  

// BD ADD: START
  public static void loopForVsync(boolean initLooper) {
      if (initLooper) {
          if (Looper.myLooper() == null) {
              Looper.prepare();
          } else {
              Looper.myLooper().quit();
          }
      } else {
          Looper.loop();
      }
  }
// END
  // TODO(mattcarroll): add javadocs
  public static native void nativeOnVsync(
      long frameTimeNanos, long frameTargetTimeNanos, long cookie);

  // TODO(mattcarroll): add javadocs
  @NonNull
  public static native FlutterCallbackInformation nativeLookupCallbackInformation(long handle);

  // ----- Start FlutterTextUtils Methods ----

  public native boolean nativeFlutterTextUtilsIsEmoji(int codePoint);

  public native boolean nativeFlutterTextUtilsIsEmojiModifier(int codePoint);

  public native boolean nativeFlutterTextUtilsIsEmojiModifierBase(int codePoint);

  public native boolean nativeFlutterTextUtilsIsVariationSelector(int codePoint);

  public native boolean nativeFlutterTextUtilsIsRegionalIndicator(int codePoint);

  // ----- End Engine FlutterTextUtils Methods ----

  @Nullable private Long nativePlatformViewId;
  @Nullable private AccessibilityDelegate accessibilityDelegate;
  @Nullable private PlatformMessageHandler platformMessageHandler;
  @Nullable private LocalizationPlugin localizationPlugin;
  @Nullable private PlatformViewsController platformViewsController;

  @NonNull
  private final Set<EngineLifecycleListener> engineLifecycleListeners = new CopyOnWriteArraySet<>();

  @NonNull
  private final Set<FlutterUiDisplayListener> flutterUiDisplayListeners =
      new CopyOnWriteArraySet<>();

  @NonNull private final Looper mainLooper; // cached to avoid synchronization on repeat access.

  public FlutterJNI() {
    // We cache the main looper so that we can ensure calls are made on the main thread
    // without consistently paying the synchronization cost of getMainLooper().
    mainLooper = Looper.getMainLooper();
  }

  // ------ Start Native Attach/Detach Support ----
  /**
   * Returns true if this instance of {@code FlutterJNI} is connected to Flutter's native engine via
   * a Java Native Interface (JNI).
   */
  public boolean isAttached() {
    return nativePlatformViewId != null;
  }

  /**
   * Attaches this {@code FlutterJNI} instance to Flutter's native engine, which allows for
   * communication between Android code and Flutter's platform agnostic engine.
   *
   * <p>This method must not be invoked if {@code FlutterJNI} is already attached to native.
   */
  // BD ADD: START
  @UiThread
  public void attachToNative(boolean isBackgroundView) {
    attachToNative(isBackgroundView, false);
  }
  // END

  @UiThread
  // BD MOD:
  // public void attachToNative(boolean isBackgroundView) {
  public void attachToNative(boolean isBackgroundView, boolean isPreload) {
    ensureRunningOnMainThread();
    ensureNotAttachedToNative();
    // BD MOD:
    // nativePlatformViewId = performNativeAttach(this, isBackgroundView);
    nativePlatformViewId = performNativeAttach(this, isBackgroundView, isPreload);
  }

  @VisibleForTesting
  // BD MOD: START
  // public long performNativeAttach(@NonNull FlutterJNI flutterJNI, boolean isBackgroundView) {
  //  return nativeAttach(flutterJNI, isBackgroundView);
  // }
  public long performNativeAttach(@NonNull FlutterJNI flutterJNI, boolean isBackgroundView, boolean isPreload) {
    return nativeAttach(flutterJNI, isBackgroundView, isPreload);
  }
  // END

  // BD MOD:
  // private native long nativeAttach(@NonNull FlutterJNI flutterJNI, boolean isBackgroundView);
  private native long nativeAttach(@NonNull FlutterJNI flutterJNI, boolean isBackgroundView, boolean isPreload);

  /**
   * Detaches this {@code FlutterJNI} instance from Flutter's native engine, which precludes any
   * further communication between Android code and Flutter's platform agnostic engine.
   *
   * <p>This method must not be invoked if {@code FlutterJNI} is not already attached to native.
   *
   * <p>Invoking this method will result in the release of all native-side resources that were setup
   * during {@link #attachToNative(boolean)}, or accumulated thereafter.
   *
   * <p>It is permissable to re-attach this instance to native after detaching it from native.
   */
  @UiThread
  public void detachFromNativeAndReleaseResources() {
    ensureRunningOnMainThread();
    ensureAttachedToNative();
    nativeDestroy(nativePlatformViewId);
    nativePlatformViewId = null;
  }

  private native void nativeDestroy(long nativePlatformViewId);

  private void ensureNotAttachedToNative() {
    if (nativePlatformViewId != null) {
      throw new RuntimeException(
          "Cannot execute operation because FlutterJNI is attached to native.");
    }
  }

  private void ensureAttachedToNative() {
    if (nativePlatformViewId == null) {
      throw new RuntimeException(
          "Cannot execute operation because FlutterJNI is not attached to native.");
    }
  }
  // ------ End Native Attach/Detach Support ----

  // ----- Start Render Surface Support -----
  /**
   * Adds a {@link FlutterUiDisplayListener}, which receives a callback when Flutter's engine
   * notifies {@code FlutterJNI} that Flutter is painting pixels to the {@link Surface} that was
   * provided to Flutter.
   */
  @UiThread
  public void addIsDisplayingFlutterUiListener(@NonNull FlutterUiDisplayListener listener) {
    ensureRunningOnMainThread();
    flutterUiDisplayListeners.add(listener);
  }

  /**
   * Removes a {@link FlutterUiDisplayListener} that was added with {@link
   * #addIsDisplayingFlutterUiListener(FlutterUiDisplayListener)}.
   */
  @UiThread
  public void removeIsDisplayingFlutterUiListener(@NonNull FlutterUiDisplayListener listener) {
    ensureRunningOnMainThread();
    flutterUiDisplayListeners.remove(listener);
  }

  // Called by native to notify first Flutter frame rendered.
  @SuppressWarnings("unused")
  @VisibleForTesting
  @UiThread
  public void onFirstFrame() {
    ensureRunningOnMainThread();

    for (FlutterUiDisplayListener listener : flutterUiDisplayListeners) {
      listener.onFlutterUiDisplayed();
    }
  }

  // TODO(mattcarroll): get native to call this when rendering stops.
  @VisibleForTesting
  @UiThread
  void onRenderingStopped() {
    ensureRunningOnMainThread();

    for (FlutterUiDisplayListener listener : flutterUiDisplayListeners) {
      listener.onFlutterUiNoLongerDisplayed();
    }
  }

  /**
   * Call this method when a {@link Surface} has been created onto which you would like Flutter to
   * paint.
   *
   * <p>See {@link android.view.SurfaceHolder.Callback#surfaceCreated(SurfaceHolder)} for an example
   * of where this call might originate.
   */
  @UiThread
  public void onSurfaceCreated(@NonNull Surface surface) {
    ensureRunningOnMainThread();
    ensureAttachedToNative();
    nativeSurfaceCreated(nativePlatformViewId, surface);
  }

  private native void nativeSurfaceCreated(long nativePlatformViewId, @NonNull Surface surface);

  /**
   * In hybrid composition, call this method when the {@link Surface} has changed.
   *
   * <p>In hybrid composition, the root surfaces changes from {@link
   * android.view.SurfaceHolder#getSurface()} to {@link android.media.ImageReader#getSurface()} when
   * a platform view is in the current frame.
   */
  @UiThread
  public void onSurfaceWindowChanged(@NonNull Surface surface) {
    ensureRunningOnMainThread();
    ensureAttachedToNative();
    nativeSurfaceWindowChanged(nativePlatformViewId, surface);
  }

  private native void nativeSurfaceWindowChanged(
      long nativePlatformViewId, @NonNull Surface surface);

  /**
   * Call this method when the {@link Surface} changes that was previously registered with {@link
   * #onSurfaceCreated(Surface)}.
   *
   * <p>See {@link android.view.SurfaceHolder.Callback#surfaceChanged(SurfaceHolder, int, int, int)}
   * for an example of where this call might originate.
   */
  @UiThread
  public void onSurfaceChanged(int width, int height) {
    ensureRunningOnMainThread();
    ensureAttachedToNative();
    nativeSurfaceChanged(nativePlatformViewId, width, height);
  }

  private native void nativeSurfaceChanged(long nativePlatformViewId, int width, int height);

  /**
   * Call this method when the {@link Surface} is destroyed that was previously registered with
   * {@link #onSurfaceCreated(Surface)}.
   *
   * <p>See {@link android.view.SurfaceHolder.Callback#surfaceDestroyed(SurfaceHolder)} for an
   * example of where this call might originate.
   */
  @UiThread
  public void onSurfaceDestroyed() {
    ensureRunningOnMainThread();
    ensureAttachedToNative();
    onRenderingStopped();
    nativeSurfaceDestroyed(nativePlatformViewId);
  }

  private native void nativeSurfaceDestroyed(long nativePlatformViewId);

  /**
   * Call this method to notify Flutter of the current device viewport metrics that are applies to
   * the Flutter UI that is being rendered.
   *
   * <p>This method should be invoked with initial values upon attaching to native. Then, it should
   * be invoked any time those metrics change while {@code FlutterJNI} is attached to native.
   */
  @UiThread
  public void setViewportMetrics(
      float devicePixelRatio,
      int physicalWidth,
      int physicalHeight,
      int physicalPaddingTop,
      int physicalPaddingRight,
      int physicalPaddingBottom,
      int physicalPaddingLeft,
      int physicalViewInsetTop,
      int physicalViewInsetRight,
      int physicalViewInsetBottom,
      int physicalViewInsetLeft,
      int systemGestureInsetTop,
      int systemGestureInsetRight,
      int systemGestureInsetBottom,
      int systemGestureInsetLeft) {
    ensureRunningOnMainThread();
    ensureAttachedToNative();
    nativeSetViewportMetrics(
        nativePlatformViewId,
        devicePixelRatio,
        physicalWidth,
        physicalHeight,
        physicalPaddingTop,
        physicalPaddingRight,
        physicalPaddingBottom,
        physicalPaddingLeft,
        physicalViewInsetTop,
        physicalViewInsetRight,
        physicalViewInsetBottom,
        physicalViewInsetLeft,
        systemGestureInsetTop,
        systemGestureInsetRight,
        systemGestureInsetBottom,
        systemGestureInsetLeft);
  }

  private native void nativeSetViewportMetrics(
      long nativePlatformViewId,
      float devicePixelRatio,
      int physicalWidth,
      int physicalHeight,
      int physicalPaddingTop,
      int physicalPaddingRight,
      int physicalPaddingBottom,
      int physicalPaddingLeft,
      int physicalViewInsetTop,
      int physicalViewInsetRight,
      int physicalViewInsetBottom,
      int physicalViewInsetLeft,
      int systemGestureInsetTop,
      int systemGestureInsetRight,
      int systemGestureInsetBottom,
      int systemGestureInsetLeft);
  // ----- End Render Surface Support -----

  // ------ Start Touch Interaction Support ---
  /** Sends a packet of pointer data to Flutter's engine. */
  @UiThread
  public void dispatchPointerDataPacket(@NonNull ByteBuffer buffer, int position) {
    ensureRunningOnMainThread();
    ensureAttachedToNative();
    nativeDispatchPointerDataPacket(nativePlatformViewId, buffer, position);
  }

  private native void nativeDispatchPointerDataPacket(
      long nativePlatformViewId, @NonNull ByteBuffer buffer, int position);
  // ------ End Touch Interaction Support ---

  @UiThread
  public void setPlatformViewsController(@NonNull PlatformViewsController platformViewsController) {
    ensureRunningOnMainThread();
    this.platformViewsController = platformViewsController;
  }

  // ------ Start Accessibility Support -----
  /**
   * Sets the {@link AccessibilityDelegate} for the attached Flutter context.
   *
   * <p>The {@link AccessibilityDelegate} is responsible for maintaining an Android-side cache of
   * Flutter's semantics tree and custom accessibility actions. This cache should be hooked up to
   * Android's accessibility system.
   *
   * <p>See {@link AccessibilityBridge} for an example of an {@link AccessibilityDelegate} and the
   * surrounding responsibilities.
   */
  @UiThread
  public void setAccessibilityDelegate(@Nullable AccessibilityDelegate accessibilityDelegate) {
    ensureRunningOnMainThread();
    this.accessibilityDelegate = accessibilityDelegate;
  }

  /**
   * Invoked by native to send semantics tree updates from Flutter to Android.
   *
   * <p>The {@code buffer} and {@code strings} form a communication protocol that is implemented
   * here:
   * https://github.com/flutter/engine/blob/master/shell/platform/android/platform_view_android.cc#L207
   */
  @SuppressWarnings("unused")
  @UiThread
  private void updateSemantics(@NonNull ByteBuffer buffer, @NonNull String[] strings) {
    ensureRunningOnMainThread();
    if (accessibilityDelegate != null) {
      accessibilityDelegate.updateSemantics(buffer, strings);
    }
    // TODO(mattcarroll): log dropped messages when in debug mode
    // (https://github.com/flutter/flutter/issues/25391)
  }

  /**
   * Invoked by native to send new custom accessibility events from Flutter to Android.
   *
   * <p>The {@code buffer} and {@code strings} form a communication protocol that is implemented
   * here:
   * https://github.com/flutter/engine/blob/master/shell/platform/android/platform_view_android.cc#L207
   *
   * <p>// TODO(cbracken): expand these docs to include more actionable information.
   */
  @SuppressWarnings("unused")
  @UiThread
  private void updateCustomAccessibilityActions(
      @NonNull ByteBuffer buffer, @NonNull String[] strings) {
    ensureRunningOnMainThread();
    if (accessibilityDelegate != null) {
      accessibilityDelegate.updateCustomAccessibilityActions(buffer, strings);
    }
    // TODO(mattcarroll): log dropped messages when in debug mode
    // (https://github.com/flutter/flutter/issues/25391)
  }

  /** Sends a semantics action to Flutter's engine, without any additional arguments. */
  public void dispatchSemanticsAction(int id, @NonNull AccessibilityBridge.Action action) {
    dispatchSemanticsAction(id, action, null);
  }

  /** Sends a semantics action to Flutter's engine, with additional arguments. */
  public void dispatchSemanticsAction(
      int id, @NonNull AccessibilityBridge.Action action, @Nullable Object args) {
    ensureAttachedToNative();

    ByteBuffer encodedArgs = null;
    int position = 0;
    if (args != null) {
      encodedArgs = StandardMessageCodec.INSTANCE.encodeMessage(args);
      position = encodedArgs.position();
    }
    dispatchSemanticsAction(id, action.value, encodedArgs, position);
  }

  /**
   * Sends a semantics action to Flutter's engine, given arguments that are already encoded for the
   * engine.
   *
   * <p>To send a semantics action that has not already been encoded, see {@link
   * #dispatchSemanticsAction(int, AccessibilityBridge.Action)} and {@link
   * #dispatchSemanticsAction(int, AccessibilityBridge.Action, Object)}.
   */
  @UiThread
  public void dispatchSemanticsAction(
      int id, int action, @Nullable ByteBuffer args, int argsPosition) {
    ensureRunningOnMainThread();
    ensureAttachedToNative();
    nativeDispatchSemanticsAction(nativePlatformViewId, id, action, args, argsPosition);
  }

  private native void nativeDispatchSemanticsAction(
      long nativePlatformViewId, int id, int action, @Nullable ByteBuffer args, int argsPosition);

  /**
   * Instructs Flutter to enable/disable its semantics tree, which is used by Flutter to support
   * accessibility and related behaviors.
   */
  @UiThread
  public void setSemanticsEnabled(boolean enabled) {
    // BD ADD: START
    if (!isAttached()) {
      Log.e(TAG, "Cannot execute setSemanticsEnabled because FlutterJNI is not attached to native.");
      return;
    }
    // END
    ensureRunningOnMainThread();
    // BD DEL:
    // ensureAttachedToNative();
    nativeSetSemanticsEnabled(nativePlatformViewId, enabled);
  }

  private native void nativeSetSemanticsEnabled(long nativePlatformViewId, boolean enabled);

  // TODO(mattcarroll): figure out what flags are supported and add javadoc about when/why/where to
  // use this.
  @UiThread
  public void setAccessibilityFeatures(int flags) {
    ensureRunningOnMainThread();
    ensureAttachedToNative();
    nativeSetAccessibilityFeatures(nativePlatformViewId, flags);
  }

  private native void nativeSetAccessibilityFeatures(long nativePlatformViewId, int flags);
  //------ End Accessibility Support ----
  /**
   * BD ADD: register android image loader HuWeijie
   */
  @UiThread
  public void registerAndroidImageLoader(AndroidImageLoader androidImageLoader) {
    ensureAttachedToNative();
    nativeRegisterAndroidImageLoader(nativePlatformViewId, androidImageLoader);
  }
  /**
   * BD ADD: register android image loader HuWeijie
   */
  private native void nativeRegisterAndroidImageLoader(long nativePlatformViewId, AndroidImageLoader androidImageLoader);

  /**
   * BD ADD: unregister android image loader
   */
  @UiThread
  public void unRegisterAndroidImageLoader() {
    ensureAttachedToNative();
    nativeUnregisterAndroidImageLoader(nativePlatformViewId);
  }
  /**
   * BD ADD: unregister android image loader HuWeijie
   */
  private native void nativeUnregisterAndroidImageLoader(long nativePlatformViewId);

  // ------ Start Texture Registration Support -----
  /**
   * Gives control of a {@link SurfaceTexture} to Flutter so that Flutter can display that texture
   * within Flutter's UI.
   */
  @UiThread
  public void registerTexture(long textureId, @NonNull SurfaceTexture surfaceTexture) {
    ensureRunningOnMainThread();
    ensureAttachedToNative();
    nativeRegisterTexture(nativePlatformViewId, textureId, surfaceTexture);
  }

  private native void nativeRegisterTexture(
      long nativePlatformViewId, long textureId, @NonNull SurfaceTexture surfaceTexture);

  /**
   * Call this method to inform Flutter that a texture previously registered with {@link
   * #registerTexture(long, SurfaceTexture)} has a new frame available.
   *
   * <p>Invoking this method instructs Flutter to update its presentation of the given texture so
   * that the new frame is displayed.
   */
  @UiThread
  public void markTextureFrameAvailable(long textureId) {
    ensureRunningOnMainThread();
    ensureAttachedToNative();
    nativeMarkTextureFrameAvailable(nativePlatformViewId, textureId);
  }

  private native void nativeMarkTextureFrameAvailable(long nativePlatformViewId, long textureId);

  /**
   * Unregisters a texture that was registered with {@link #registerTexture(long, SurfaceTexture)}.
   */
  @UiThread
  public void unregisterTexture(long textureId) {
    ensureRunningOnMainThread();
    ensureAttachedToNative();
    nativeUnregisterTexture(nativePlatformViewId, textureId);
  }

  private native void nativeUnregisterTexture(long nativePlatformViewId, long textureId);
  // ------ Start Texture Registration Support -----

  // BD ADD: START
  public void scheduleBackgroundFrame() {
    ensureAttachedToNative();
    nativeScheduleBackgroundFrame(nativePlatformViewId);
  }

  private native void nativeScheduleBackgroundFrame(long nativePlatformViewId);

  @UiThread
  public void updateNative(String assetsPath) {
    ensureAttachedToNative();
    nativeUpdateSettings(nativePlatformViewId, assetsPath);
  }

  private native void nativeUpdateSettings(long nativePlatformViewId, String assetsPath);
  // END

  // ------ Start Dart Execution Support -------
  /**
   * Executes a Dart entrypoint.
   *
   * <p>This can only be done once per JNI attachment because a Dart isolate can only be entered
   * once.
   */
  @UiThread
  public void runBundleAndSnapshotFromLibrary(
      @NonNull String bundlePath,
      @Nullable String entrypointFunctionName,
      @Nullable String pathToEntrypointFunction,
      @NonNull AssetManager assetManager) {
    ensureRunningOnMainThread();
    ensureAttachedToNative();
    nativeRunBundleAndSnapshotFromLibrary(
        nativePlatformViewId,
        bundlePath,
        entrypointFunctionName,
        pathToEntrypointFunction,
        assetManager);
  }

  private native void nativeRunBundleAndSnapshotFromLibrary(
      long nativePlatformViewId,
      @NonNull String bundlePath,
      @Nullable String entrypointFunctionName,
      @Nullable String pathToEntrypointFunction,
      @NonNull AssetManager manager);
  // ------ End Dart Execution Support -------

  // --------- Start Platform Message Support ------
  /**
   * Sets the handler for all platform messages that come from the attached platform view to Java.
   *
   * <p>Communication between a specific Flutter context (Dart) and the host platform (Java) is
   * accomplished by passing messages. Messages can be sent from Java to Dart with the corresponding
   * {@code FlutterJNI} methods:
   *
   * <ul>
   *   <li>{@link #dispatchPlatformMessage(String, ByteBuffer, int, int)}
   *   <li>{@link #dispatchEmptyPlatformMessage(String, int)}
   * </ul>
   *
   * <p>{@code FlutterJNI} is also the recipient of all platform messages sent from its attached
   * Flutter context. {@code FlutterJNI} does not know what to do with these messages, so a handler
   * is exposed to allow these messages to be processed in whatever manner is desired:
   *
   * <p>{@code setPlatformMessageHandler(PlatformMessageHandler)}
   *
   * <p>If a message is received but no {@link PlatformMessageHandler} is registered, that message
   * will be dropped (ignored). Therefore, when using {@code FlutterJNI} to integrate a Flutter
   * context in an app, a {@link PlatformMessageHandler} must be registered for 2-way Java/Dart
   * communication to operate correctly. Moreover, the handler must be implemented such that
   * fundamental platform messages are handled as expected. See {@link FlutterNativeView} for an
   * example implementation.
   */
  @UiThread
  public void setPlatformMessageHandler(@Nullable PlatformMessageHandler platformMessageHandler) {
    ensureRunningOnMainThread();
    this.platformMessageHandler = platformMessageHandler;
  }

  // Called by native.
  // TODO(mattcarroll): determine if message is nonull or nullable
  @SuppressWarnings("unused")
  @VisibleForTesting
  public void handlePlatformMessage(
      @NonNull final String channel, byte[] message, final int replyId) {
    if (platformMessageHandler != null) {
      platformMessageHandler.handleMessageFromDart(channel, message, replyId);
    }
    // TODO(mattcarroll): log dropped messages when in debug mode
    // (https://github.com/flutter/flutter/issues/25391)
  }

  // Called by native to respond to a platform message that we sent.
  // TODO(mattcarroll): determine if reply is nonull or nullable
  @SuppressWarnings("unused")
  private void handlePlatformMessageResponse(int replyId, byte[] reply) {
    if (platformMessageHandler != null) {
      platformMessageHandler.handlePlatformMessageResponse(replyId, reply);
    }
    // TODO(mattcarroll): log dropped messages when in debug mode
    // (https://github.com/flutter/flutter/issues/25391)
  }

  /**
   * Sends an empty reply (identified by {@code responseId}) from Android to Flutter over the given
   * {@code channel}.
   */
  @UiThread
  public void dispatchEmptyPlatformMessage(@NonNull String channel, int responseId) {
    ensureRunningOnMainThread();
    if (isAttached()) {
      nativeDispatchEmptyPlatformMessage(nativePlatformViewId, channel, responseId);
    } else {
      Log.w(
          TAG,
          "Tried to send a platform message to Flutter, but FlutterJNI was detached from native C++. Could not send. Channel: "
              + channel
              + ". Response ID: "
              + responseId);
    }
  }

  // Send an empty platform message to Dart.
  private native void nativeDispatchEmptyPlatformMessage(
      long nativePlatformViewId, @NonNull String channel, int responseId);

  /** Sends a reply {@code message} from Android to Flutter over the given {@code channel}. */
  @UiThread
  public void dispatchPlatformMessage(
      @NonNull String channel, @Nullable ByteBuffer message, int position, int responseId) {
    ensureRunningOnMainThread();
    if (isAttached()) {
      nativeDispatchPlatformMessage(nativePlatformViewId, channel, message, position, responseId);
    } else {
      Log.w(
          TAG,
          "Tried to send a platform message to Flutter, but FlutterJNI was detached from native C++. Could not send. Channel: "
              + channel
              + ". Response ID: "
              + responseId);
    }
  }

  // Send a data-carrying platform message to Dart.
  private native void nativeDispatchPlatformMessage(
      long nativePlatformViewId,
      @NonNull String channel,
      @Nullable ByteBuffer message,
      int position,
      int responseId);

  // TODO(mattcarroll): differentiate between channel responses and platform responses.
  @UiThread
  public void invokePlatformMessageEmptyResponseCallback(int responseId) {
    ensureRunningOnMainThread();
    if (isAttached()) {
      nativeInvokePlatformMessageEmptyResponseCallback(nativePlatformViewId, responseId);
    } else {
      Log.w(
          TAG,
          "Tried to send a platform message response, but FlutterJNI was detached from native C++. Could not send. Response ID: "
              + responseId);
    }
  }

  // Send an empty response to a platform message received from Dart.
  private native void nativeInvokePlatformMessageEmptyResponseCallback(
      long nativePlatformViewId, int responseId);

  // TODO(mattcarroll): differentiate between channel responses and platform responses.
  @UiThread
  public void invokePlatformMessageResponseCallback(
      int responseId, @Nullable ByteBuffer message, int position) {
    ensureRunningOnMainThread();
    if (isAttached()) {
      nativeInvokePlatformMessageResponseCallback(
          nativePlatformViewId, responseId, message, position);
    } else {
      Log.w(
          TAG,
          "Tried to send a platform message response, but FlutterJNI was detached from native C++. Could not send. Response ID: "
              + responseId);
    }
  }

  // Send a data-carrying response to a platform message received from Dart.
  private native void nativeInvokePlatformMessageResponseCallback(
      long nativePlatformViewId, int responseId, @Nullable ByteBuffer message, int position);
  // ------- End Platform Message Support ----

  // ----- Start Engine Lifecycle Support ----
  /**
   * Adds the given {@code engineLifecycleListener} to be notified of Flutter engine lifecycle
   * events, e.g., {@link EngineLifecycleListener#onPreEngineRestart()}.
   */
  @UiThread
  public void addEngineLifecycleListener(@NonNull EngineLifecycleListener engineLifecycleListener) {
    ensureRunningOnMainThread();
    engineLifecycleListeners.add(engineLifecycleListener);
  }

  /**
   * Removes the given {@code engineLifecycleListener}, which was previously added using {@link
   * #addIsDisplayingFlutterUiListener(FlutterUiDisplayListener)}.
   */
  @UiThread
  public void removeEngineLifecycleListener(
      @NonNull EngineLifecycleListener engineLifecycleListener) {
    ensureRunningOnMainThread();
    engineLifecycleListeners.remove(engineLifecycleListener);
  }

  // Called by native.
  @SuppressWarnings("unused")
  private void onPreEngineRestart() {
    for (EngineLifecycleListener listener : engineLifecycleListeners) {
      listener.onPreEngineRestart();
    }
  }

  @SuppressWarnings("unused")
  @UiThread
  public void onDisplayOverlaySurface(int id, int x, int y, int width, int height) {
    ensureRunningOnMainThread();
    if (platformViewsController == null) {
      throw new RuntimeException(
          "platformViewsController must be set before attempting to position an overlay surface");
    }
    platformViewsController.onDisplayOverlaySurface(id, x, y, width, height);
  }

  @SuppressWarnings("unused")
  @UiThread
  public void onBeginFrame() {
    ensureRunningOnMainThread();
    if (platformViewsController == null) {
      throw new RuntimeException(
          "platformViewsController must be set before attempting to begin the frame");
    }
    platformViewsController.onBeginFrame();
  }

  @SuppressWarnings("unused")
  @UiThread
  public void onEndFrame() {
    ensureRunningOnMainThread();
    if (platformViewsController == null) {
      throw new RuntimeException(
          "platformViewsController must be set before attempting to end the frame");
    }
    platformViewsController.onEndFrame();
  }

  @SuppressWarnings("unused")
  @UiThread
  public FlutterOverlaySurface createOverlaySurface() {
    ensureRunningOnMainThread();
    if (platformViewsController == null) {
      throw new RuntimeException(
          "platformViewsController must be set before attempting to position an overlay surface");
    }
    return platformViewsController.createOverlaySurface();
  }

  @SuppressWarnings("unused")
  @UiThread
  public void destroyOverlaySurfaces() {
    ensureRunningOnMainThread();
    if (platformViewsController == null) {
      throw new RuntimeException(
          "platformViewsController must be set before attempting to destroy an overlay surface");
    }
    platformViewsController.destroyOverlaySurfaces();
  }
  // ----- End Engine Lifecycle Support ----

  // ----- Start Localization Support ----

  /** Sets the localization plugin that is used in various localization methods. */
  @UiThread
  public void setLocalizationPlugin(@Nullable LocalizationPlugin localizationPlugin) {
    ensureRunningOnMainThread();
    this.localizationPlugin = localizationPlugin;
  }

  /** Invoked by native to obtain the results of Android's locale resolution algorithm. */
  @SuppressWarnings("unused")
  @VisibleForTesting
  String[] computePlatformResolvedLocale(@NonNull String[] strings) {
    if (localizationPlugin == null) {
      return new String[0];
    }
    List<Locale> supportedLocales = new ArrayList<Locale>();
    final int localeDataLength = 3;
    for (int i = 0; i < strings.length; i += localeDataLength) {
      String languageCode = strings[i + 0];
      String countryCode = strings[i + 1];
      String scriptCode = strings[i + 2];
      // Convert to Locales via LocaleBuilder if available (API 24+) to include scriptCode.
      if (Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.LOLLIPOP) {
        Locale.Builder localeBuilder = new Locale.Builder();
        if (!languageCode.isEmpty()) {
          localeBuilder.setLanguage(languageCode);
        }
        if (!countryCode.isEmpty()) {
          localeBuilder.setRegion(countryCode);
        }
        if (!scriptCode.isEmpty()) {
          localeBuilder.setScript(scriptCode);
        }
        supportedLocales.add(localeBuilder.build());
      } else {
        // Pre-API 24, we fall back on scriptCode-less locales.
        supportedLocales.add(new Locale(languageCode, countryCode));
      }
    }

    Locale result = localizationPlugin.resolveNativeLocale(supportedLocales);

    if (result == null) {
      return new String[0];
    }
    String[] output = new String[localeDataLength];
    output[0] = result.getLanguage();
    output[1] = result.getCountry();
    if (Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.LOLLIPOP) {
      output[2] = result.getScript();
    } else {
      output[2] = "";
    }
    return output;
  }

  // ----- End Localization Support ----

  // @SuppressWarnings("unused")
  @UiThread
  public void onDisplayPlatformView(
      int viewId,
      int x,
      int y,
      int width,
      int height,
      int viewWidth,
      int viewHeight,
      FlutterMutatorsStack mutatorsStack) {
    ensureRunningOnMainThread();
    if (platformViewsController == null) {
      throw new RuntimeException(
          "platformViewsController must be set before attempting to position a platform view");
    }
    platformViewsController.onDisplayPlatformView(
        viewId, x, y, width, height, viewWidth, viewHeight, mutatorsStack);
  }

  // TODO(mattcarroll): determine if this is nonull or nullable
  @UiThread
  public Bitmap getBitmap() {
    ensureRunningOnMainThread();
    ensureAttachedToNative();
    return nativeGetBitmap(nativePlatformViewId);
  }

  // TODO(mattcarroll): determine if this is nonull or nullable
  private native Bitmap nativeGetBitmap(long nativePlatformViewId);

  /**
   * Notifies the Dart VM of a low memory event, or that the application is in a state such that now
   * is an appropriate time to free resources, such as going to the background.
   *
   * <p>This is distinct from sending a SystemChannel message about low memory, which only notifies
   * the running Flutter application.
   */
  @UiThread
  public void notifyLowMemoryWarning() {
    ensureRunningOnMainThread();
    ensureAttachedToNative();
    nativeNotifyLowMemoryWarning(nativePlatformViewId);
  }

  private native void nativeNotifyLowMemoryWarning(long nativePlatformViewId);

  private void ensureRunningOnMainThread() {
    if (Looper.myLooper() != mainLooper) {
      throw new RuntimeException(
          "Methods marked with @UiThread must be executed on the main thread. Current thread: "
              + Thread.currentThread().getName());
    }
  }

  /**
   * Delegate responsible for creating and updating Android-side caches of Flutter's semantics tree
   * and custom accessibility actions.
   *
   * <p>{@link AccessibilityBridge} is an example of an {@code AccessibilityDelegate}.
   */
  public interface AccessibilityDelegate {
    /**
     * Sends new custom accessibility actions from Flutter to Android.
     *
     * <p>Implementers are expected to maintain an Android-side cache of custom accessibility
     * actions. This method provides new actions to add to that cache.
     */
    void updateCustomAccessibilityActions(@NonNull ByteBuffer buffer, @NonNull String[] strings);

    /**
     * Sends new {@code SemanticsNode} information from Flutter to Android.
     *
     * <p>Implementers are expected to maintain an Android-side cache of Flutter's semantics tree.
     * This method provides updates from Flutter for the Android-side semantics tree cache.
     */
    void updateSemantics(@NonNull ByteBuffer buffer, @NonNull String[] strings);
  }

  public interface AsyncWaitForVsyncDelegate {
    void asyncWaitForVsync(final long cookie);
  }
}
