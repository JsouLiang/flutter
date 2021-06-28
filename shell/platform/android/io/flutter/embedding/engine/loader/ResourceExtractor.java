// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.engine.loader;

import static java.util.Arrays.asList;

import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.os.AsyncTask;
import android.os.Build;
import androidx.annotation.NonNull;
import androidx.annotation.WorkerThread;
import io.flutter.BuildConfig;
import io.flutter.Log;
import java.io.*;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashSet;
import java.util.concurrent.CancellationException;
import java.util.concurrent.ExecutionException;

/** A class to initialize the native code. */
class ResourceExtractor {
  private static final String TAG = "ResourceExtractor";
  private static final String TIMESTAMP_PREFIX = "res_timestamp-";
  private static final String[] SUPPORTED_ABIS = getSupportedAbis();
  private static final int MAX_COPY_RETRY_COUNT = 1;

  @SuppressWarnings("deprecation")
  static long getVersionCode(@NonNull PackageInfo packageInfo) {
    // Linter needs P (28) hardcoded or else it will fail these lines.
    if (Build.VERSION.SDK_INT >= 28) {
      return packageInfo.getLongVersionCode();
    } else {
      return packageInfo.versionCode;
    }
  }

  /**
     * BD ADD:
     */
  static boolean isX86Device() {
      boolean isX86Device = false;
      if (SUPPORTED_ABIS != null && SUPPORTED_ABIS.length > 0) {
          for (String abi : SUPPORTED_ABIS) {
              if (abi != null && abi.contains("86")) {
                  isX86Device = true;
                  break;
              }
          }
      }
      return isX86Device;
  }

  private static class ExtractTask extends AsyncTask<Void, Void, Void> {
    @NonNull private final String mDataDirPath;
    @NonNull private final HashSet<String> mResources;
    @NonNull private final AssetManager mAssetManager;
    @NonNull private final String mPackageName;
    @NonNull private final PackageManager mPackageManager;
    // BD ADD
    private FlutterLoader.Settings mSettings;

    ExtractTask(
        @NonNull String dataDirPath,
        @NonNull HashSet<String> resources,
        @NonNull String packageName,
        @NonNull PackageManager packageManager,
        @NonNull AssetManager assetManager,
        FlutterLoader.Settings settings) {
      mDataDirPath = dataDirPath;
      mResources = resources;
      mAssetManager = assetManager;
      mPackageName = packageName;
      mPackageManager = packageManager;
      mSettings = settings;
    }

    @Override
    protected Void doInBackground(Void... unused) {
      final File dataDir = new File(mDataDirPath);
      final String timestamp = checkTimestamp(dataDir, mPackageManager, mPackageName);
      if (timestamp == null) {
        return null;
      }

      deleteFiles(mDataDirPath, mResources);

      if (!extractAPK(dataDir)) {
        return null;
      }

      if (timestamp != null) {
        try {
          new File(dataDir, timestamp).createNewFile();
        } catch (IOException e) {
          Log.w(TAG, "Failed to write resource timestamp");
        }
      }

      //BD ADD: START
      if (mSettings!=null && mSettings.getOnInitResourcesCallback() != null) {
          mSettings.getOnInitResourcesCallback().run();
      }
      //END

      return null;
    }

    /// Returns true if successfully unpacked APK resources,
    /// otherwise deletes all resources and returns false.
    @WorkerThread
    private boolean extractAPK(@NonNull File dataDir) {
      for (String asset : mResources) {
        try {
          final String resource = "assets/" + asset;
          final File output = new File(dataDir, asset);
          if (output.exists()) {
            continue;
          }
          if (output.getParentFile() != null) {
            output.getParentFile().mkdirs();
          }

          int copyRetryCount = 0;
            boolean error = true;
            do {
                InputStream is = null;
                OutputStream os = null;
                try {
                    is = mAssetManager.open(asset);
                    os = new FileOutputStream(output);
                    copy(is, os);
                    error = false;
                }catch (FileNotFoundException fnfe) {
                    throw fnfe;
                } catch (IOException ioe) {
                    error = true;
                    if (copyRetryCount == MAX_COPY_RETRY_COUNT) {
                        throw ioe;
                    }
                    copyRetryCount++;
                    Log.w(TAG,"copy retry1:"+resource);
                    if (mSettings.getInitExceptionCallback() != null) {
                        mSettings.getInitExceptionCallback().onRetryException(ioe);
                    }
                } finally {
                    try {
                        if (is != null) {
                            is.close();
                        }
                        if (os != null) {
                            os.close();
                        }
                    } catch (IOException ioe) {
                        error = true;
                        if (copyRetryCount == MAX_COPY_RETRY_COUNT) {
                            throw ioe;
                        }
                        copyRetryCount++;
                        Log.w(TAG,"copy retry2:"+resource);
                        if (mSettings.getInitExceptionCallback() != null) {
                            mSettings.getInitExceptionCallback().onRetryException(ioe);
                        }
                    }
                }
            } while (error);

          if (BuildConfig.DEBUG) {
            Log.i(TAG, "Extracted baseline resource " + resource);
          }
        } catch (FileNotFoundException fnfe) {
          continue;

        } catch (IOException ioe) {
          Log.w(TAG, "Exception unpacking resources: " + ioe.getMessage());
          deleteFiles(mDataDirPath, mResources);
          return false;
        }
      }

      return true;
    }
  }

  @NonNull private final String mDataDirPath;
  @NonNull private final String mPackageName;
  @NonNull private final PackageManager mPackageManager;
  @NonNull private final AssetManager mAssetManager;
  @NonNull private final HashSet<String> mResources;
  private ExtractTask mExtractTask;
  // BD ADD
  private FlutterLoader.Settings mSettings;

  ResourceExtractor(
      @NonNull String dataDirPath,
      @NonNull String packageName,
      @NonNull PackageManager packageManager,
      @NonNull AssetManager assetManager,
      FlutterLoader.Settings settings) {
    mDataDirPath = dataDirPath;
    mPackageName = packageName;
    mPackageManager = packageManager;
    mAssetManager = assetManager;
    mResources = new HashSet<>();
  }

  ResourceExtractor addResource(@NonNull String resource) {
    mResources.add(resource);
    return this;
  }

  ResourceExtractor addResources(@NonNull Collection<String> resources) {
    mResources.addAll(resources);
    return this;
  }

  ResourceExtractor start() {
    if (BuildConfig.DEBUG && mExtractTask != null) {
      Log.e(
          TAG, "Attempted to start resource extraction while another extraction was in progress.");
    }
    mExtractTask = new ExtractTask(mDataDirPath, mResources, mPackageName, mPackageManager, mAssetManager, mSettings);
    mExtractTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    return this;
  }

  void waitForCompletion() {
    if (mExtractTask == null) {
      return;
    }

    try {
      mExtractTask.get();
    } catch (CancellationException | ExecutionException | InterruptedException e) {
      deleteFiles(mDataDirPath, mResources);
    }
  }

  private static String[] getExistingTimestamps(File dataDir) {
    return dataDir.list(
        new FilenameFilter() {
          @Override
          public boolean accept(File dir, String name) {
            return name.startsWith(TIMESTAMP_PREFIX);
          }
        });
  }

  private static void deleteFiles(@NonNull String dataDirPath, @NonNull HashSet<String> resources) {
    final File dataDir = new File(dataDirPath);
    for (String resource : resources) {
      final File file = new File(dataDir, resource);
      if (file.exists()) {
        file.delete();
      }
    }
    final String[] existingTimestamps = getExistingTimestamps(dataDir);
    if (existingTimestamps == null) {
      return;
    }
    for (String timestamp : existingTimestamps) {
      new File(dataDir, timestamp).delete();
    }
  }

  // Returns null if extracted resources are found and match the current APK version
  // and update version if any, otherwise returns the current APK and update version.
  private static String checkTimestamp(
      @NonNull File dataDir, @NonNull PackageManager packageManager, @NonNull String packageName) {
    PackageInfo packageInfo = null;

    try {
      packageInfo = packageManager.getPackageInfo(packageName, 0);
    } catch (PackageManager.NameNotFoundException e) {
      return TIMESTAMP_PREFIX;
    }

    if (packageInfo == null) {
      return TIMESTAMP_PREFIX;
    }

    String expectedTimestamp =
        TIMESTAMP_PREFIX + getVersionCode(packageInfo) + "-" + packageInfo.lastUpdateTime;

    final String[] existingTimestamps = getExistingTimestamps(dataDir);

    if (existingTimestamps == null) {
      if (BuildConfig.DEBUG) {
        Log.i(TAG, "No extracted resources found");
      }
      return expectedTimestamp;
    }

    if (existingTimestamps.length == 1) {
      if (BuildConfig.DEBUG) {
        Log.i(TAG, "Found extracted resources " + existingTimestamps[0]);
      }
    }

    if (existingTimestamps.length != 1 || !expectedTimestamp.equals(existingTimestamps[0])) {
      if (BuildConfig.DEBUG) {
        Log.i(TAG, "Resource version mismatch " + expectedTimestamp);
      }
      return expectedTimestamp;
    }

    return null;
  }

  private static void copy(@NonNull InputStream in, @NonNull OutputStream out) throws IOException {
    byte[] buf = new byte[16 * 1024];
    for (int i; (i = in.read(buf)) >= 0; ) {
      out.write(buf, 0, i);
    }
  }

  @SuppressWarnings("deprecation")
  private static String[] getSupportedAbis() {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
      return Build.SUPPORTED_ABIS;
    } else {
      ArrayList<String> cpuAbis = new ArrayList<String>(asList(Build.CPU_ABI, Build.CPU_ABI2));
      cpuAbis.removeAll(asList(null, ""));
      return cpuAbis.toArray(new String[0]);
    }
  }
}
