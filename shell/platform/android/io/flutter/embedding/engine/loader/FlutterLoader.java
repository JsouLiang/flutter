// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.engine.loader;

import android.app.ActivityManager;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.text.TextUtils;
import android.view.WindowManager;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

// BD ADD:
import io.flutter.BDFlutterInjector;
import io.flutter.BuildConfig;
import io.flutter.Log;
import io.flutter.embedding.engine.FlutterJNI;
// BD ADD:
import io.flutter.embedding.engine.FlutterShellArgs;
import io.flutter.util.PathUtils;
import io.flutter.view.VsyncWaiter;
import java.io.File;
import java.util.*;
import java.util.concurrent.Callable;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;

/** Finds Flutter resources in an application APK and also loads Flutter's native library. */
public class FlutterLoader {
  private static final String TAG = "FlutterLoader";

  private static final String OLD_GEN_HEAP_SIZE_META_DATA_KEY =
      "io.flutter.embedding.android.OldGenHeapSize";
  private static final String ENABLE_SKPARAGRAPH_META_DATA_KEY =
      "io.flutter.embedding.android.EnableSkParagraph";

  // Must match values in flutter::switches
  static final String AOT_SHARED_LIBRARY_NAME = "aot-shared-library-name";
  static final String SNAPSHOT_ASSET_PATH_KEY = "snapshot-asset-path";
  static final String VM_SNAPSHOT_DATA_KEY = "vm-snapshot-data";
  static final String ISOLATE_SNAPSHOT_DATA_KEY = "isolate-snapshot-data";
  static final String FLUTTER_ASSETS_DIR_KEY = "flutter-assets-dir";

  // Resource names used for components of the precompiled snapshot.
  private static final String DEFAULT_LIBRARY = "libflutter.so";
  private static final String DEFAULT_KERNEL_BLOB = "kernel_blob.bin";

  // BD ADD
  private static final String DEBUG_FLUTTER_LIBS_DIR = "flutter_libs";

  private static final String DEFAULT_HOST_MANIFEST_JSON = "host_manifest.json";
  private static final String DEFAULT_OPTI_PROPS = "optimize.properties";
  private static final String DEFAULT_FLUTTER_ASSETS_DIR = "flutter_assets";
  private String flutterAssetsDir = DEFAULT_FLUTTER_ASSETS_DIR;


  private static FlutterLoader instance;

  /**
   * Returns a singleton {@code FlutterLoader} instance.
   *
   * <p>The returned instance loads Flutter native libraries in the standard way. A singleton object
   * is used instead of static methods to facilitate testing without actually running native library
   * linking.
   *
   * @deprecated Use the {@link io.flutter.FlutterInjector} instead.
   */
  @Deprecated
  @NonNull
  public static FlutterLoader getInstance() {
    if (instance == null) {
      instance = new FlutterLoader();
    }
    return instance;
  }

  /** Creates a {@code FlutterLoader} that uses a default constructed {@link FlutterJNI}. */
  public FlutterLoader() {
    this(new FlutterJNI());
  }

  /**
   * Creates a {@code FlutterLoader} with the specified {@link FlutterJNI}.
   *
   * @param flutterJNI The {@link FlutterJNI} instance to use for loading the libflutter.so C++
   *     library, setting up the font manager, and calling into C++ initalization.
   */
  public FlutterLoader(@NonNull FlutterJNI flutterJNI) {
    this.flutterJNI = flutterJNI;
  }

  private boolean initialized = false;
  @Nullable private Settings settings;

  // BD ADD START:
  public interface SoLoader {
    void loadLibrary(Context context, String libraryName);
  }

  public interface MonitorCallback {
    void onMonitor(String event, long cost);
  }
  // END

  private long initStartTimestampMillis;
  private FlutterApplicationInfo flutterApplicationInfo;
  private FlutterJNI flutterJNI;

  private static class InitResult {
    final String appStoragePath;
    final String engineCachesPath;
    final String dataDirPath;

    private InitResult(String appStoragePath, String engineCachesPath, String dataDirPath) {
      this.appStoragePath = appStoragePath;
      this.engineCachesPath = engineCachesPath;
      this.dataDirPath = dataDirPath;
    }
  }

  @Nullable Future<InitResult> initResultFuture;

  /**
   * Starts initialization of the native system.
   *
   * @param applicationContext The Android application context.
   */
  public void startInitialization(@NonNull Context applicationContext) {
    startInitialization(applicationContext, new Settings());
  }

  /**
   * Starts initialization of the native system.
   *
   * <p>This loads the Flutter engine's native library to enable subsequent JNI calls. This also
   * starts locating and unpacking Dart resources packaged in the app's APK.
   *
   * <p>Calling this method multiple times has no effect.
   *
   * @param applicationContext The Android application context.
   * @param settings Configuration settings.
   */
  public void startInitialization(@NonNull Context applicationContext, @NonNull Settings settings) {
    // Do not run startInitialization more than once.
    if (this.settings != null) {
      return;
    }
    if (Looper.myLooper() != Looper.getMainLooper()) {
      throw new IllegalStateException("startInitialization must be called on the main thread");
    }

    // Ensure that the context is actually the application context.
    final Context appContext = applicationContext.getApplicationContext();

    this.settings = settings;

    initStartTimestampMillis = SystemClock.uptimeMillis();
    flutterApplicationInfo = ApplicationInfoLoader.load(appContext);
    VsyncWaiter.getInstance((WindowManager) appContext.getSystemService(Context.WINDOW_SERVICE))
        .init();

    // Use a background thread for initialization tasks that require disk access.
    Callable<InitResult> initTask =
        new Callable<InitResult>() {
          @Override
          public InitResult call() {
            ResourceExtractor resourceExtractor = initResources(appContext);

            // BD ADD:
            long initTaskStartTimestamp = System.currentTimeMillis() * 1000;
            // BD MOD: START
            // flutterJNI.loadLibrary();
            if (settings != null && settings.isDebugModeEnable()) {
                copyDebugFiles(appContext);
                File libsDir = appContext.getDir(DEBUG_FLUTTER_LIBS_DIR, Context.MODE_PRIVATE);
                String soPath = libsDir.getAbsolutePath() + File.separator + DEFAULT_LIBRARY;
                System.load(soPath);
                Log.i(TAG, "DebugMode: use copied " + DEFAULT_LIBRARY);
            } else if (settings != null && settings.getSoLoader() != null) {
                settings.getSoLoader().loadLibrary(appContext, "flutter");
            } else {
                flutterJNI.loadLibrary();
            }
            if (BDFlutterInjector.instance().shouldLoadNative()) {
                FlutterJNI.nativeTraceEngineInitApmStartAndEnd("init_task", initTaskStartTimestamp);
            }
            // END

            // Prefetch the default font manager as soon as possible on a background thread.
            // It helps to reduce time cost of engine setup that blocks the platform thread.
            Executors.newSingleThreadExecutor()
                .execute(
                    new Runnable() {
                      @Override
                      public void run() {
                        flutterJNI.prefetchDefaultFontManager();
                      }
                    });

            if (resourceExtractor != null) {
              resourceExtractor.waitForCompletion();
              if (settings != null && settings.monitorCallback != null) {
                settings.monitorCallback.onMonitor("resourceExtractor.waitForCompletion", SystemClock.uptimeMillis() - initStartTimestampMillis);
              }

            }
            return new InitResult(
                PathUtils.getFilesDir(appContext),
                PathUtils.getCacheDirectory(appContext),
                PathUtils.getDataDirectory(appContext));
          }
        };
    initResultFuture = Executors.newSingleThreadExecutor().submit(initTask);
  }

  /**
   * Blocks until initialization of the native system has completed.
   *
   * <p>Calling this method multiple times has no effect.
   *
   * @param applicationContext The Android application context.
   * @param args Flags sent to the Flutter runtime.
   */
  public void ensureInitializationComplete(
      @NonNull Context applicationContext, @Nullable String[] args) {
    if (initialized) {
      return;
    }
    if (Looper.myLooper() != Looper.getMainLooper()) {
      throw new IllegalStateException(
          "ensureInitializationComplete must be called on the main thread");
    }
    if (settings == null) {
      throw new IllegalStateException(
          "ensureInitializationComplete must be called after startInitialization");
    }

    try {
      InitResult result = initResultFuture.get();

      List<String> shellArgs = new ArrayList<>();
      shellArgs.add("--icu-symbol-prefix=_binary_icudtl_dat");

      // BD ADD: START
      String nativeLibraryDir = settings.getNativeLibraryDir();
      if (nativeLibraryDir == null) {
        nativeLibraryDir = flutterApplicationInfo.nativeLibraryDir;
      }
      // END

      // BD MOD: START
      // shellArgs.add(
      //           "--icu-native-lib-path="
      //               flutterApplicationInfo.nativeLibraryDir
      //               + File.separator
      //               + DEFAULT_LIBRARY);
      if (!settings.isDebugModeEnable()) {
          shellArgs.add(
                    "--icu-native-lib-path="
                        + nativeLibraryDir
                        + File.separator
                        + DEFAULT_LIBRARY);
      }
      // END

      if (args != null) {
        Collections.addAll(shellArgs, args);
      }

      String kernelPath = null;
      // BD MOD: START
      // if (BuildConfig.DEBUG || BuildConfig.JIT_RELEASE)
      if (settings.isDebugModeEnable()) {
          File debugLibDir = applicationContext.getDir(DEBUG_FLUTTER_LIBS_DIR, Context.MODE_PRIVATE);
          shellArgs.add("--icu-native-lib-path=" + debugLibDir.getAbsolutePath() + File.separator + DEFAULT_LIBRARY);
          String[] files = debugLibDir.list();
          boolean hasAppSo = false;
          for (String file : files) {
              if (TextUtils.equals(file, flutterApplicationInfo.aotSharedLibraryName)) {
                  hasAppSo = true;
                  break;
              }
          }
          if (hasAppSo) {
              shellArgs.add("--" + AOT_SHARED_LIBRARY_NAME + "=" + debugLibDir.getAbsolutePath()
                      + File.separator + flutterApplicationInfo.aotSharedLibraryName);
              Log.i(TAG, "DebugMode: use copied " + flutterApplicationInfo.aotSharedLibraryName);
          } else {
              String snapshotAssetPath = debugLibDir.getAbsolutePath();
              kernelPath = snapshotAssetPath + File.separator + DEFAULT_KERNEL_BLOB;
              Log.i(TAG, "DebugMode: use copied " + DEFAULT_KERNEL_BLOB);
              shellArgs.add("--" + SNAPSHOT_ASSET_PATH_KEY + "=" + snapshotAssetPath);
              shellArgs.add("--" + VM_SNAPSHOT_DATA_KEY + "=" + flutterApplicationInfo.vmSnapshotData);
              shellArgs.add("--" + ISOLATE_SNAPSHOT_DATA_KEY + "=" + flutterApplicationInfo.isolateSnapshotData);
          }
      } else if (BuildConfig.DEBUG || BuildConfig.JIT_RELEASE) {
      // END
        String snapshotAssetPath =
            result.dataDirPath + File.separator + flutterApplicationInfo.flutterAssetsDir;
        kernelPath = snapshotAssetPath + File.separator + DEFAULT_KERNEL_BLOB;
        shellArgs.add("--" + SNAPSHOT_ASSET_PATH_KEY + "=" + snapshotAssetPath);
        shellArgs.add("--" + VM_SNAPSHOT_DATA_KEY + "=" + flutterApplicationInfo.vmSnapshotData);
        shellArgs.add(
            "--" + ISOLATE_SNAPSHOT_DATA_KEY + "=" + flutterApplicationInfo.isolateSnapshotData);
      } else {
        shellArgs.add(
            "--" + AOT_SHARED_LIBRARY_NAME + "=" + flutterApplicationInfo.aotSharedLibraryName);

        // Most devices can load the AOT shared library based on the library name
        // with no directory path.  Provide a fully qualified path to the library
        // as a workaround for devices where that fails.
        shellArgs.add(
            "--"
                + AOT_SHARED_LIBRARY_NAME
                + "="
                // BD MOD:
                //+ flutterApplicationInfo.nativeLibraryDir
                + nativeLibraryDir
                + File.separator
                + flutterApplicationInfo.aotSharedLibraryName);
      }

      String hostManifestJson = PathUtils.getDataDirectory(applicationContext) + File.separator + flutterAssetsDir + File.separator + DEFAULT_HOST_MANIFEST_JSON;
      if(new File(hostManifestJson).exists()) {
          shellArgs.add("--dynamicart-host");
      }

      shellArgs.add("--cache-dir-path=" + result.engineCachesPath);
      if (!flutterApplicationInfo.clearTextPermitted) {
        shellArgs.add("--disallow-insecure-connections");
      }
      if (flutterApplicationInfo.domainNetworkPolicy != null) {
        shellArgs.add("--domain-network-policy=" + flutterApplicationInfo.domainNetworkPolicy);
      }
      if (settings.getLogTag() != null) {
        shellArgs.add("--log-tag=" + settings.getLogTag());
      }
      // BD ADD:START
      if (settings.isDisableLeakVM()) {
          shellArgs.add("--disable-leak-vm");
      }
      // END

      ApplicationInfo applicationInfo =
          applicationContext
              .getPackageManager()
              .getApplicationInfo(
                  applicationContext.getPackageName(), PackageManager.GET_META_DATA);
      Bundle metaData = applicationInfo.metaData;
      int oldGenHeapSizeMegaBytes =
          metaData != null ? metaData.getInt(OLD_GEN_HEAP_SIZE_META_DATA_KEY) : 0;
      if (oldGenHeapSizeMegaBytes == 0) {
        // default to half of total memory.
        ActivityManager activityManager =
            (ActivityManager) applicationContext.getSystemService(Context.ACTIVITY_SERVICE);
        ActivityManager.MemoryInfo memInfo = new ActivityManager.MemoryInfo();
        activityManager.getMemoryInfo(memInfo);
        oldGenHeapSizeMegaBytes = (int) (memInfo.totalMem / 1e6 / 2);
      }

      shellArgs.add("--old-gen-heap-size=" + oldGenHeapSizeMegaBytes);


      if (metaData != null && metaData.getBoolean(ENABLE_SKPARAGRAPH_META_DATA_KEY)) {
        shellArgs.add("--enable-skparagraph");
      }
      // BD ADD: START
      if (ResourceExtractor.isX86Device() &&
              !shellArgs.contains(FlutterShellArgs.ARG_ENABLE_SOFTWARE_RENDERING)) {
        shellArgs.add(FlutterShellArgs.ARG_ENABLE_SOFTWARE_RENDERING);
      }
      // END

      // BD ADD:
      long nativeInitStartTimestamp = System.currentTimeMillis() * 1000;
      long initTimeMillis = SystemClock.uptimeMillis() - initStartTimestampMillis;

      flutterJNI.init(
          applicationContext,
          shellArgs.toArray(new String[0]),
          kernelPath,
          result.appStoragePath,
          result.engineCachesPath,
          initTimeMillis);

      // BD ADD: START
      if (BDFlutterInjector.instance().shouldLoadNative()) {
          FlutterJNI.nativeTraceEngineInitApmStartAndEnd("native_init", nativeInitStartTimestamp);
      }
	  // END

      initialized = true;
      // BD ADD:START
      if (settings.monitorCallback != null) {
        settings.monitorCallback.onMonitor("EnsureInitialized", SystemClock.uptimeMillis() - initStartTimestampMillis);
      }
      // END
    } catch (Exception e) {
      Log.e(TAG, "Flutter initialization failed.", e);
      throw new RuntimeException(e);
    }
  }

  /**
   * Same as {@link #ensureInitializationComplete(Context, String[])} but waiting on a background
   * thread, then invoking {@code callback} on the {@code callbackHandler}.
   */
  public void ensureInitializationCompleteAsync(
      @NonNull Context applicationContext,
      @Nullable String[] args,
      @NonNull Handler callbackHandler,
      @NonNull Runnable callback) {
    if (Looper.myLooper() != Looper.getMainLooper()) {
      throw new IllegalStateException(
          "ensureInitializationComplete must be called on the main thread");
    }
    if (settings == null) {
      throw new IllegalStateException(
          "ensureInitializationComplete must be called after startInitialization");
    }
    if (initialized) {
      callbackHandler.post(callback);
      return;
    }
    Executors.newSingleThreadExecutor()
        .execute(
            new Runnable() {
              @Override
              public void run() {
                InitResult result;
                try {
                  result = initResultFuture.get();
                } catch (Exception e) {
                  Log.e(TAG, "Flutter initialization failed.", e);
                  throw new RuntimeException(e);
                }
                new Handler(Looper.getMainLooper())
                    .post(
                        new Runnable() {
                          @Override
                          public void run() {
                            ensureInitializationComplete(
                                applicationContext.getApplicationContext(), args);
                            callbackHandler.post(callback);
                          }
                        });
              }
            });
  }

  /** Returns whether the FlutterLoader has finished loading the native library. */
  public boolean initialized() {
    return initialized;
  }

  /** Extract assets out of the APK that need to be cached as uncompressed files on disk. */
  private ResourceExtractor initResources(@NonNull Context applicationContext) {
    ResourceExtractor resourceExtractor = null;
    if (BuildConfig.DEBUG || BuildConfig.JIT_RELEASE) {
      final String dataDirPath = PathUtils.getDataDirectory(applicationContext);
      final String packageName = applicationContext.getPackageName();
      final PackageManager packageManager = applicationContext.getPackageManager();
      final AssetManager assetManager = applicationContext.getResources().getAssets();
      resourceExtractor = new ResourceExtractor(dataDirPath, packageName, packageManager, assetManager, settings);

      // In debug/JIT mode these assets will be written to disk and then
      // mapped into memory so they can be provided to the Dart VM.
      resourceExtractor
          .addResource(fullAssetPathFrom(flutterApplicationInfo.vmSnapshotData))
          .addResource(fullAssetPathFrom(flutterApplicationInfo.isolateSnapshotData))
          .addResource(fullAssetPathFrom(DEFAULT_KERNEL_BLOB))
          .addResource(fullAssetPathFrom(DEFAULT_HOST_MANIFEST_JSON))
          .addResource(fullAssetPathFrom2(DEFAULT_OPTI_PROPS));

      resourceExtractor.start();
    }
    return resourceExtractor;
  }

  @NonNull
  public String findAppBundlePath() {
    return flutterApplicationInfo.flutterAssetsDir;
  }

  /**
   * Returns the file name for the given asset. The returned file name can be used to access the
   * asset in the APK through the {@link android.content.res.AssetManager} API.
   *
   * @param asset the name of the asset. The name can be hierarchical
   * @return the filename to be used with {@link android.content.res.AssetManager}
   */
  @NonNull
  public String getLookupKeyForAsset(@NonNull String asset) {
    return fullAssetPathFrom(asset);
  }

  /**
   * Returns the file name for the given asset which originates from the specified packageName. The
   * returned file name can be used to access the asset in the APK through the {@link
   * android.content.res.AssetManager} API.
   *
   * @param asset the name of the asset. The name can be hierarchical
   * @param packageName the name of the package from which the asset originates
   * @return the file name to be used with {@link android.content.res.AssetManager}
   */
  @NonNull
  public String getLookupKeyForAsset(@NonNull String asset, @NonNull String packageName) {
    return getLookupKeyForAsset("packages" + File.separator + packageName + File.separator + asset);
  }

  @NonNull
  private String fullAssetPathFrom(@NonNull String filePath) {
    return flutterApplicationInfo.flutterAssetsDir + File.separator + filePath;
  }

  @NonNull
  private String fullAssetPathFrom2(@NonNull String filePath) {
    return filePath;
  }

  /**
   * BD ADD
   */
  private void copyDebugFiles(@NonNull Context applicationContext) {
      Log.i(TAG, "DebugMode: start copy files..");

      // Check source
      File sourceDir = new File(applicationContext.getExternalFilesDir(null).getAbsolutePath()
              + File.separator + "flutter_debug");
      if (!sourceDir.exists()) {
          sourceDir.mkdirs();
          Log.i(TAG, "DebugMode: " + sourceDir.getAbsolutePath() + " not exists");
          return;
      }
      File[] sourceFiles = sourceDir.listFiles();
      if (sourceFiles == null || sourceFiles.length == 0) {
          Log.i(TAG, "DebugMode: " + sourceDir.getAbsolutePath() + " has no content");
          return;
      }

      // Create dest dir
      File destDir = applicationContext.getDir(DEBUG_FLUTTER_LIBS_DIR, Context.MODE_PRIVATE);
      if (destDir.exists()) {
          File[] filesIndest = destDir.listFiles();
          if (filesIndest != null && filesIndest.length > 0) {
              for (File file : filesIndest) {
                  boolean success = file.delete();
                  Log.i(TAG, "DebugMode: delete old " + file.getName() + " " + success);
              }
          }
      } else {
          destDir.mkdirs();
      }

      // Do copy
      for (File file : sourceFiles) {
          boolean success = PathUtils.copyFile(applicationContext, file.getAbsolutePath(),
                  destDir.getAbsolutePath() + File.separator + file.getName(), true);
          if (!success) {
              throw new RuntimeException("Copy debug files Failed");
          } else {
              Log.i(TAG, "DebugMode: copy new" + file.getName() + " success");
          }
      }
  }

  public interface InitExceptionCallback {
      void onRetryException(Throwable t);
      void onInitException(Throwable t);
  }

  public static class Settings {
    private String logTag;
    // BD ADD START:
    private String nativeLibraryDir;
    private SoLoader soLoader;
    private MonitorCallback monitorCallback;
    private boolean disableLeakVM = false;
    private Runnable onInitResources;
    private InitExceptionCallback initExceptionCallback;
    private boolean enableDebugMode = false;


    public boolean isDisableLeakVM() {
        return disableLeakVM;
    }

    // 页面退出后，FlutterEngine默认是不销毁VM的，disableLeakVM设置在所有页面退出后销毁VM
    public void disableLeakVM() {
        disableLeakVM = true;
    }
    public Runnable getOnInitResourcesCallback() {
         return onInitResources;
     }

    public void setOnInitResourcesCallback(Runnable callback) {
         onInitResources = callback;
    }

     public InitExceptionCallback getInitExceptionCallback() {
         return initExceptionCallback;
    }

    public void setInitExceptionCallback(InitExceptionCallback iec) {
           initExceptionCallback = iec;

    }

    public String getNativeLibraryDir() {
      return nativeLibraryDir;
    }

    public SoLoader getSoLoader() {
      return soLoader;
    }

    public void setNativeLibraryDir(String dir) {
      nativeLibraryDir = dir;
    }

    public void setSoLoader(SoLoader loader) {
      soLoader = loader;
    }


    public void setMonitorCallback(MonitorCallback monitorCallback) {
      this.monitorCallback = monitorCallback;
    }

    public boolean isDebugModeEnable() {
        return enableDebugMode;
    }

    public void setEnableDebugMode(boolean enableDebugMode) {
        this.enableDebugMode = enableDebugMode;
    }
    // END


    @Nullable
    public String getLogTag() {
      return logTag;
    }

    /**
     * Set the tag associated with Flutter app log messages.
     *
     * @param tag Log tag.
     */
    public void setLogTag(String tag) {
      logTag = tag;
    }
  }

  // BD ADD: START
  private FlutterLoader.onBundleRunListener onBundleRunListener;

  public void onBundleRun() {
      if (onBundleRunListener != null) {
          onBundleRunListener.onBundleRun(FlutterJNI.nativeGetEngineInitInfo());
      }
  }

  public interface onBundleRunListener {
      void onBundleRun(long[] infos);
  }

  public void setOnBundleRunListener(FlutterLoader.onBundleRunListener onBundleRunListener) {
      this.onBundleRunListener = onBundleRunListener;
  }
  // END
}
