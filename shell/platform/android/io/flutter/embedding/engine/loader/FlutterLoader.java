// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.engine.loader;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
// BD ADD
import android.text.TextUtils;
import android.util.Log;
import android.view.WindowManager;

import java.io.File;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.Executor;
import java.util.concurrent.FutureTask;

import io.flutter.BuildConfig;
import io.flutter.embedding.engine.FlutterJNI;
// BD ADD:
import io.flutter.embedding.engine.FlutterShellArgs;
import io.flutter.util.PathUtils;
import io.flutter.view.VsyncWaiter;

/**
 * Finds Flutter resources in an application APK and also loads Flutter's native library.
 */
public class FlutterLoader {
    private static final String TAG = "FlutterLoader";

    // Must match values in flutter::switches
    private static final String AOT_SHARED_LIBRARY_NAME = "aot-shared-library-name";
    private static final String SNAPSHOT_ASSET_PATH_KEY = "snapshot-asset-path";
    private static final String VM_SNAPSHOT_DATA_KEY = "vm-snapshot-data";
    private static final String ISOLATE_SNAPSHOT_DATA_KEY = "isolate-snapshot-data";
    private static final String FLUTTER_ASSETS_DIR_KEY = "flutter-assets-dir";

    // XML Attribute keys supported in AndroidManifest.xml
    private static final String PUBLIC_AOT_SHARED_LIBRARY_NAME =
        FlutterLoader.class.getName() + '.' + AOT_SHARED_LIBRARY_NAME;
    private static final String PUBLIC_VM_SNAPSHOT_DATA_KEY =
        FlutterLoader.class.getName() + '.' + VM_SNAPSHOT_DATA_KEY;
    private static final String PUBLIC_ISOLATE_SNAPSHOT_DATA_KEY =
        FlutterLoader.class.getName() + '.' + ISOLATE_SNAPSHOT_DATA_KEY;
    private static final String PUBLIC_FLUTTER_ASSETS_DIR_KEY =
        FlutterLoader.class.getName() + '.' + FLUTTER_ASSETS_DIR_KEY;

    // Resource names used for components of the precompiled snapshot.
    private static final String DEFAULT_AOT_SHARED_LIBRARY_NAME = "libapp.so";
    private static final String DEFAULT_VM_SNAPSHOT_DATA = "vm_snapshot_data";
    private static final String DEFAULT_ISOLATE_SNAPSHOT_DATA = "isolate_snapshot_data";
    private static final String DEFAULT_LIBRARY = "libflutter.so";
    private static final String DEFAULT_KERNEL_BLOB = "kernel_blob.bin";
    private static final String DEFAULT_HOST_MANIFEST_JSON = "host_manifest.json";
    private static final String DEFAULT_FLUTTER_ASSETS_DIR = "flutter_assets";

    // BD ADD: START
    private static final String DEBUG_FLUTTER_LIBS_DIR = "flutter_libs";
    private static final String DEBUG_FLUTTER_ASSET_DIR = "flutter_assets";
    // END

    // Mutable because default values can be overridden via config properties
    private String aotSharedLibraryName = DEFAULT_AOT_SHARED_LIBRARY_NAME;
    private String vmSnapshotData = DEFAULT_VM_SNAPSHOT_DATA;
    private String isolateSnapshotData = DEFAULT_ISOLATE_SNAPSHOT_DATA;
    private String flutterAssetsDir = DEFAULT_FLUTTER_ASSETS_DIR;

    private static FlutterLoader instance;

    /**
     * Returns a singleton {@code FlutterLoader} instance.
     * <p>
     * The returned instance loads Flutter native libraries in the standard way. A singleton object
     * is used instead of static methods to facilitate testing without actually running native
     * library linking.
     */
    @NonNull
    public static FlutterLoader getInstance() {
        if (instance == null) {
            instance = new FlutterLoader();
        }
        return instance;
    }

    private boolean initialized = false;
    @Nullable
    private ResourceExtractor resourceExtractor;
    @Nullable
    private Settings settings;

    // BD ADD START:
    public interface SoLoader {
        void loadLibrary(Context context, String libraryName);
    }

    private FutureTask<Void> sInitTask;

    private class InitTask implements Callable<Void> {
        private final Context context;

        public InitTask(Context applicationContext) {
            this.context = applicationContext;
        }

        @Override
        public Void call() {
            try {
                long initStartTimestampMillis = SystemClock.uptimeMillis();
                initConfig(context);
                initResources(context);
                // BD MOD: START
                // if (settings.getSoLoader() != null) {
                if (settings.isDebugModeEnable()) {
                    copyDebugFiles(context);
                    File libsDir = context.getDir(DEBUG_FLUTTER_LIBS_DIR, Context.MODE_PRIVATE);
                    String soPath = libsDir.getAbsolutePath() + File.separator + DEFAULT_LIBRARY;
                    System.load(soPath);
                    Log.i(TAG, "DebugMode: use copied " + DEFAULT_LIBRARY);
                } else if (settings.getSoLoader() != null) {
                // END
                    settings.getSoLoader().loadLibrary(context, "flutter");
                } else {
                    System.loadLibrary("flutter");
                }

                VsyncWaiter
                        .getInstance((WindowManager) context.getSystemService(Context.WINDOW_SERVICE))
                        .init();

                // We record the initialization time using SystemClock because at the start of the
                // initialization we have not yet loaded the native library to call into dart_tools_api.h.
                // To get Timeline timestamp of the start of initialization we simply subtract the delta
                // from the Timeline timestamp at the current moment (the assumption is that the overhead
                // of the JNI call is negligible).
                long initTimeMillis = SystemClock.uptimeMillis() - initStartTimestampMillis;
                try {
                  FlutterJNI.nativeRecordStartTimestamp(initTimeMillis);
                } catch (Exception e) {
                  Log.e(TAG, "Flutter initialization failed.", e);
                }
                if (settings.monitorCallback != null) {
                    settings.monitorCallback.onMonitor("InitTask", initTimeMillis);
                }
            } catch (Exception e){
                throw new RuntimeException("InitTask failed.", e);
            }
            return null;
        }
    }
    // END

    /**
     * Starts initialization of the native system.
     * @param applicationContext The Android application context.
     */
    public void startInitialization(@NonNull Context applicationContext) {
        startInitialization(applicationContext, new Settings());
    }

    /**
     * Starts initialization of the native system.
     * <p>
     * This loads the Flutter engine's native library to enable subsequent JNI calls. This also
     * starts locating and unpacking Dart resources packaged in the app's APK.
     * <p>
     * Calling this method multiple times has no effect.
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

        this.settings = settings;

        sInitTask = new FutureTask<>(new InitTask(applicationContext));
        new Thread(sInitTask).start();

        new Handler().postDelayed(new Runnable() {
            @Override
            public void run() {
                new ResourceCleaner(applicationContext).start();
            }
        }, 5000L);
    }

    /**
     * Blocks until initialization of the native system has completed.
     * <p>
     * Calling this method multiple times has no effect.
     *
     * @param applicationContext The Android application context.
     * @param args Flags sent to the Flutter runtime.
     */
    public void ensureInitializationComplete(@NonNull Context applicationContext, @Nullable String[] args) {
        if (initialized) {
            return;
        }
        if (Looper.myLooper() != Looper.getMainLooper()) {
          throw new IllegalStateException("ensureInitializationComplete must be called on the main thread");
        }
        if (settings == null || sInitTask == null) {
          throw new IllegalStateException("ensureInitializationComplete must be called after startInitialization");
        }
        try {
            long now = SystemClock.elapsedRealtime();
            // BD ADD:
            sInitTask.get();
            if (settings.monitorCallback != null) {
                settings.monitorCallback.onMonitor("InitTask.get", SystemClock.elapsedRealtime() - now);
            }
            if (resourceExtractor != null) {
                resourceExtractor.waitForCompletion();
            }
            if (settings.monitorCallback != null) {
                settings.monitorCallback.onMonitor("resourceExtractor.waitForCompletion", SystemClock.elapsedRealtime() - now);
            }

            List<String> shellArgs = new ArrayList<>();
            shellArgs.add("--icu-symbol-prefix=_binary_icudtl_dat");
            String nativeLibraryDir = settings.getNativeLibraryDir();
            if (nativeLibraryDir == null) {
                nativeLibraryDir = getApplicationInfo(applicationContext).nativeLibraryDir;
            }
            // BD MOD: START
            // shellArgs.add("--icu-native-lib-path=" + nativeLibraryDir + File.separator + DEFAULT_LIBRARY);
            if (!settings.isDebugModeEnable()) {
                shellArgs.add("--icu-native-lib-path=" + nativeLibraryDir + File.separator + DEFAULT_LIBRARY);
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
                    if (TextUtils.equals(file, DEFAULT_AOT_SHARED_LIBRARY_NAME)) {
                        hasAppSo = true;
                    }
                    if (TextUtils.equals(file, DEBUG_FLUTTER_ASSET_DIR)) {
                        shellArgs.add("--extra-asset-dir=" + debugLibDir.getAbsolutePath()
                                + File.separator + DEBUG_FLUTTER_ASSET_DIR);
                        Log.i(TAG, "DebugMode: use copied " + DEBUG_FLUTTER_ASSET_DIR);
                    }
                }
                if (hasAppSo) {
                    shellArgs.add("--" + AOT_SHARED_LIBRARY_NAME + "=" + debugLibDir.getAbsolutePath()
                            + File.separator + aotSharedLibraryName);
                    Log.i(TAG, "DebugMode: use copied " + aotSharedLibraryName);
                } else {
                    String snapshotAssetPath = debugLibDir.getAbsolutePath();
                    kernelPath = snapshotAssetPath + File.separator + DEFAULT_KERNEL_BLOB;
                    Log.i(TAG, "DebugMode: use copied " + DEFAULT_KERNEL_BLOB);
                    shellArgs.add("--" + SNAPSHOT_ASSET_PATH_KEY + "=" + snapshotAssetPath);
                    shellArgs.add("--" + VM_SNAPSHOT_DATA_KEY + "=" + vmSnapshotData);
                    shellArgs.add("--" + ISOLATE_SNAPSHOT_DATA_KEY + "=" + isolateSnapshotData);
                }
            } else if (BuildConfig.DEBUG || BuildConfig.JIT_RELEASE) {
            // END
                String snapshotAssetPath = PathUtils.getDataDirectory(applicationContext) + File.separator + flutterAssetsDir;
                kernelPath = snapshotAssetPath + File.separator + DEFAULT_KERNEL_BLOB;
                shellArgs.add("--" + SNAPSHOT_ASSET_PATH_KEY + "=" + snapshotAssetPath);
                shellArgs.add("--" + VM_SNAPSHOT_DATA_KEY + "=" + vmSnapshotData);
                shellArgs.add("--" + ISOLATE_SNAPSHOT_DATA_KEY + "=" + isolateSnapshotData);
            } else {
                shellArgs.add("--" + AOT_SHARED_LIBRARY_NAME + "=" + aotSharedLibraryName);

                // Most devices can load the AOT shared library based on the library name
                // with no directory path.  Provide a fully qualified path to the library
                // as a workaround for devices where that fails.
                shellArgs.add("--" + AOT_SHARED_LIBRARY_NAME + "=" + nativeLibraryDir + File.separator + aotSharedLibraryName);
            }

            String hostManifestJson = PathUtils.getDataDirectory(applicationContext) + File.separator + flutterAssetsDir + File.separator + DEFAULT_HOST_MANIFEST_JSON;
            if(new File(hostManifestJson).exists()){
                shellArgs.add("--dynamicart-host");
            }

            shellArgs.add("--cache-dir-path=" + PathUtils.getCacheDirectory(applicationContext));
            if (settings.getLogTag() != null) {
                shellArgs.add("--log-tag=" + settings.getLogTag());
            }
            // BD ADD:START
            if (settings.isDisableLeakVM()) {
                shellArgs.add("--disable-leak-vm");
            }
            if (ResourceExtractor.isX86Device() &&
                    !shellArgs.contains(FlutterShellArgs.ARG_ENABLE_SOFTWARE_RENDERING)) {
                shellArgs.add(FlutterShellArgs.ARG_ENABLE_SOFTWARE_RENDERING);
            }
            // END

            String appStoragePath = PathUtils.getFilesDir(applicationContext);
            String engineCachesPath = PathUtils.getCacheDirectory(applicationContext);
            FlutterJNI.nativeInit(applicationContext, shellArgs.toArray(new String[0]),
                kernelPath, appStoragePath, engineCachesPath);

            initialized = true;
            if (settings.monitorCallback != null) {
                settings.monitorCallback.onMonitor("EnsureInitialized", SystemClock.elapsedRealtime() - now);
            }
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
        @NonNull Runnable callback
    ) {
        if (Looper.myLooper() != Looper.getMainLooper()) {
            throw new IllegalStateException("ensureInitializationComplete must be called on the main thread");
        }
        if (settings == null) {
            throw new IllegalStateException("ensureInitializationComplete must be called after startInitialization");
        }
        if (initialized) {
            return;
        }
        new Thread(new Runnable() {
            @Override
            public void run() {
                //BD ADD: SunKun
                try {
                    sInitTask.get();
                } catch (Exception e) {
                    Log.e(TAG, "Flutter initialization failed.", e);
                    throw new RuntimeException(e);
                }
                //END
                if (resourceExtractor != null) {
                    resourceExtractor.waitForCompletion();
                }
                new Handler(Looper.getMainLooper()).post(new Runnable() {
                    @Override
                    public void run() {
                        ensureInitializationComplete(applicationContext.getApplicationContext(), args);
                        callbackHandler.post(callback);
                    }
                });
            }
        }).start();
    }

    @NonNull
    private ApplicationInfo getApplicationInfo(@NonNull Context applicationContext) {
        try {
            return applicationContext
                .getPackageManager()
                .getApplicationInfo(applicationContext.getPackageName(), PackageManager.GET_META_DATA);
        } catch (PackageManager.NameNotFoundException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Initialize our Flutter config values by obtaining them from the
     * manifest XML file, falling back to default values.
     */
    private void initConfig(@NonNull Context applicationContext) {
        Bundle metadata = getApplicationInfo(applicationContext).metaData;

        // There isn't a `<meta-data>` tag as a direct child of `<application>` in
        // `AndroidManifest.xml`.
        if (metadata == null) {
            return;
        }

        aotSharedLibraryName = metadata.getString(PUBLIC_AOT_SHARED_LIBRARY_NAME, DEFAULT_AOT_SHARED_LIBRARY_NAME);
        flutterAssetsDir = metadata.getString(PUBLIC_FLUTTER_ASSETS_DIR_KEY, DEFAULT_FLUTTER_ASSETS_DIR);

        vmSnapshotData = metadata.getString(PUBLIC_VM_SNAPSHOT_DATA_KEY, DEFAULT_VM_SNAPSHOT_DATA);
        isolateSnapshotData = metadata.getString(PUBLIC_ISOLATE_SNAPSHOT_DATA_KEY, DEFAULT_ISOLATE_SNAPSHOT_DATA);
    }

    /**
     * Extract assets out of the APK that need to be cached as uncompressed
     * files on disk.
     */
    private void initResources(@NonNull Context applicationContext) {
        // BD DEL:
        // new ResourceCleaner(applicationContext).start();

        if (BuildConfig.DEBUG || BuildConfig.JIT_RELEASE) {
            final String dataDirPath = PathUtils.getDataDirectory(applicationContext);
            final String packageName = applicationContext.getPackageName();
            final PackageManager packageManager = applicationContext.getPackageManager();
            final AssetManager assetManager = applicationContext.getResources().getAssets();
            resourceExtractor = new ResourceExtractor(dataDirPath, packageName, packageManager, assetManager, settings);

            // In debug/JIT mode these assets will be written to disk and then
            // mapped into memory so they can be provided to the Dart VM.
            resourceExtractor
                .addResource(fullAssetPathFrom(vmSnapshotData))
                .addResource(fullAssetPathFrom(isolateSnapshotData))
                .addResource(fullAssetPathFrom(DEFAULT_KERNEL_BLOB))
                .addResource(fullAssetPathFrom(DEFAULT_HOST_MANIFEST_JSON));

            resourceExtractor.start();
        }
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
                    boolean success = PathUtils.delete(file);
                    Log.i(TAG, "DebugMode: delete old " + file.getName() + " " + success);
                }
            }
        } else {
            destDir.mkdirs();
        }

        // Do copy
        for (File file : sourceFiles) {
            boolean success = false;
            if (file.isDirectory()) {
                success = PathUtils.copyFolder(file,
                        new File(destDir.getAbsolutePath() + File.separator
                                + DEBUG_FLUTTER_ASSET_DIR + File.separator + file.getName()));
                PathUtils.delete(file);
            } else {
                success = PathUtils.copyFile(file.getAbsolutePath(),
                        destDir.getAbsolutePath() + File.separator + file.getName(), true);
            }
            if (!success) {
                throw new RuntimeException("Copy debug files Failed");
            } else {
                Log.i(TAG, "DebugMode: copy " + file.getName() + " success");
            }
        }
    }

    @NonNull
    public String findAppBundlePath() {
        return flutterAssetsDir;
    }

    /**
     * Returns the file name for the given asset.
     * The returned file name can be used to access the asset in the APK
     * through the {@link android.content.res.AssetManager} API.
     *
     * @param asset the name of the asset. The name can be hierarchical
     * @return      the filename to be used with {@link android.content.res.AssetManager}
     */
    @NonNull
    public String getLookupKeyForAsset(@NonNull String asset) {
        return fullAssetPathFrom(asset);
    }

    /**
     * Returns the file name for the given asset which originates from the
     * specified packageName. The returned file name can be used to access
     * the asset in the APK through the {@link android.content.res.AssetManager} API.
     *
     * @param asset       the name of the asset. The name can be hierarchical
     * @param packageName the name of the package from which the asset originates
     * @return            the file name to be used with {@link android.content.res.AssetManager}
     */
    @NonNull
    public String getLookupKeyForAsset(@NonNull String asset, @NonNull String packageName) {
        return getLookupKeyForAsset(
            "packages" + File.separator + packageName + File.separator + asset);
    }

    @NonNull
    private String fullAssetPathFrom(@NonNull String filePath) {
        return flutterAssetsDir + File.separator + filePath;
    }

    public interface InitExceptionCallback {
        void onRetryException(Throwable t);
        void onInitException(Throwable t);
    }

    public interface MonitorCallback {
        void onMonitor(String event, long cost);
    }

    public static class Settings {
        private String logTag;
        // BD ADD START:
        private String nativeLibraryDir;
        private SoLoader soLoader;
        private boolean disableLeakVM = false;
        private Runnable onInitResources;
        private InitExceptionCallback initExceptionCallback;
        private MonitorCallback monitorCallback;
        private Executor executor;
        private boolean enableDebugMode = false;
        // END

        @Nullable
        public String getLogTag() {
            return logTag;
        }

        // BD ADD START:
        public String getNativeLibraryDir() {
            return nativeLibraryDir;
        }

        public SoLoader getSoLoader() {
            return soLoader;
        }

        public boolean isDisableLeakVM() {
            return disableLeakVM;
        }

        public boolean isDebugModeEnable() {
            return enableDebugMode;
        }
        // END

        /**
         * Set the tag associated with Flutter app log messages.
         * @param tag Log tag.
         */
        public void setLogTag(String tag) {
            logTag = tag;
        }

        // BD ADD START:
        public void setNativeLibraryDir(String dir) {
            nativeLibraryDir = dir;
        }

        public void setSoLoader(SoLoader loader) {
            soLoader = loader;
        }

        public void setExecutor(Executor executor) {
            this.executor = executor;
        }

        public void setMonitorCallback(MonitorCallback monitorCallback) {
            this.monitorCallback = monitorCallback;
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

        public void setEnableDebugMode(boolean enableDebugMode) {
            this.enableDebugMode = enableDebugMode;
        }
        // END
    }
}
