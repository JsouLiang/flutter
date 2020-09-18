/**
 * *********************************************************************** > File Name:
 * bdflutter/shell/platform/android/io/flutter/embedding/engine/AssetUtil.java > Author: zhoutongwei
 * > Mail: zhoutongwei@bytedance.com > Created Time: 10/ 9 16:57:48 2021
 * **********************************************************************
 */
package io.flutter.embedding.engine;

import android.content.res.AssetManager;
import android.os.Build;
import android.text.TextUtils;
import io.flutter.Log;
import java.lang.reflect.Array;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;

public final class AssetUtil {

  private static final String TAG = "ReflectUtils";

  private AssetUtil() {}

  public static AssetManager updateAssetManager(
      AssetManager newAssetManager, AssetManager originAssetManager) {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
      return originAssetManager;
    }

    List<String> newPathList = getAssetPaths(newAssetManager);
    List<String> oldPathList = getAssetPaths(originAssetManager);
    for (String oldPath : oldPathList) {
      if (!newPathList.contains(oldPath)) {
        updateAssetManager(newAssetManager, oldPath);
      }
    }
    return newAssetManager;
  }

  public static AssetManager updateAssetManager(AssetManager originAssetManager, String sourceDir) {
    AssetManager newAssetManager = null;
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
      if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP
          && Build.VERSION.SDK_INT <= Build.VERSION_CODES.N_MR1) {
        newAssetManager = appendAssetPathSafely(originAssetManager, sourceDir);
        if (!containsPath(newAssetManager, sourceDir)) {
          newAssetManager = appendAssetPath(originAssetManager, sourceDir);
        }
      } else {
        newAssetManager = appendAssetPath(originAssetManager, sourceDir);
      }
    }
    Log.e(TAG, " updateAssetManager, newAssetManager=" + newAssetManager);
    return newAssetManager;
  }

  public static Object readField(Object target, String fieldName) throws IllegalAccessException {
    try {
      Class<?> clazz = target.getClass();
      Field field = clazz.getDeclaredField(fieldName);
      field.setAccessible(true);
      return field.get(target);
    } catch (Exception e) {
      return null;
    }
  }

  public static void writeField(Object target, String fieldName, Object value)
      throws IllegalAccessException {
    try {
      Class<?> clazz = target.getClass();
      Field field = clazz.getDeclaredField(fieldName);
      if (!field.isAccessible()) {
        field.setAccessible(true);
      }
      field.set(target, value);
    } catch (Exception e) {
      Log.e(TAG, "writeField failed: " + e.getMessage());
    }
  }

  private static AssetManager appendAssetPathSafely(AssetManager assetManager, String sourceDir) {
    int tryCount = 3;
    while (tryCount-- >= 0) {
      String errorMsg = null;
      try {
        synchronized (assetManager) {
          // addAssetPathNative
          int cookie = 0;
          // retry 3
          for (int i = 0; i < 3; i++) {
            // private native final int addAssetPathNative(String path);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP
                && Build.VERSION.SDK_INT <= Build.VERSION_CODES.M) {
              cookie =
                  (int)
                      ReflectUtils.invokeMethod(
                          assetManager.getClass(),
                          "addAssetPathNative",
                          new Object[] {sourceDir},
                          new Class[] {String.class},
                          assetManager);
            }
            // private native final int addAssetPathNative(String path, boolean appAsLib);
            else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N
                && Build.VERSION.SDK_INT <= Build.VERSION_CODES.N_MR1) {
              cookie =
                  (int)
                      ReflectUtils.invokeMethod(
                          assetManager.getClass(),
                          "addAssetPathNative",
                          new Object[] {sourceDir, false},
                          new Class[] {String.class, boolean.class},
                          assetManager);
            }
            if (cookie != 0) {
              break;
            }
          }
          if (cookie == 0) {
            errorMsg = "cookie == 0";
            break;
          }

          Object seed = readField(assetManager, "mStringBlocks");
          final int seedNum = seed != null ? Array.getLength(seed) : 0;
          final int num =
              (int)
                  ReflectUtils.invokeMethod(
                      assetManager.getClass(), "getStringBlockCount", assetManager);
          Object newStringBlocks = Array.newInstance(seed.getClass().getComponentType(), num);
          for (int i = 0; i < num; i++) {
            if (i < seedNum) {
              // copy element
              Array.set(newStringBlocks, i, Array.get(seed, i));
            } else {
              // private native final long getNativeStringBlock(int block);
              long nativeStringBlockObj =
                  (long)
                      ReflectUtils.invokeMethod(
                          assetManager.getClass(),
                          "getNativeStringBlock",
                          new Object[] {i},
                          new Class[] {int.class},
                          assetManager);
              // new StringBlock(long obj, boolean useSparse);
              Object stri =
                  ReflectUtils.invokeConstructor(
                      seed.getClass().getComponentType(),
                      new Object[] {nativeStringBlockObj, true},
                      new Class[] {long.class, boolean.class});
              // new element
              Array.set(newStringBlocks, i, stri);
            }
          }
          // Note
          writeField(assetManager, "mStringBlocks", newStringBlocks);
        }
        Log.w(TAG, TAG + " appendAssetPathSafely success, sourceDir = " + sourceDir);
        break;
      } catch (Exception e) {
        Log.e(TAG, TAG + " appendAssetPathSafely failed, sourceDir = " + sourceDir, e);
      }
    }
    return assetManager;
  }

  private static AssetManager appendAssetPath(AssetManager assetManager, String path) {
    Method addAssetPathMethod =
        ReflectUtils.getMethod(
            AssetManager.class,
            "addAssetPath",
            new Class<?>[] {
              String.class,
            });
    if (null != addAssetPathMethod) {
      int tryCount = 3;
      int cookie = 0;

      while (tryCount-- >= 0) {
        try {
          cookie = (int) addAssetPathMethod.invoke(assetManager, path);
          if (cookie != 0) {
            Log.i(
                TAG,
                TAG
                    + " invoke AssetManager.addAssetPath() success, cookie = "
                    + cookie
                    + ", path = "
                    + path);
            break;
          }
          Log.e(TAG, TAG + " invoke AssetManager.addAssetPath() failed, cookie = " + cookie);
        } catch (Exception e) {
          Log.e(TAG, TAG + " invoke AssetManager.addAssetPath() failed.");
        }
      }
    } else {
      Log.e(TAG, TAG + " reflect AssetManager.addAssetPath() failed.");
    }
    return assetManager;
  }

  public static boolean containsPath(AssetManager assetManager, String path) {
    try {
      if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P
          || (Build.VERSION.SDK_INT == Build.VERSION_CODES.O_MR1
              && Build.VERSION.PREVIEW_SDK_INT > 0)) {
        Object[] assets =
            (Object[])
                ReflectUtils.invokeMethod(assetManager.getClass(), "getApkAssets", assetManager);
        if (assets != null && assets.length > 0) {
          for (Object asset : assets) {
            String assetPath =
                (String) ReflectUtils.invokeMethod(asset.getClass(), "getAssetPath", asset);
            if (TextUtils.equals(assetPath, path)) {
              return true;
            }
          }
        }
      } else {
        int assetsCount =
            (int)
                ReflectUtils.invokeMethod(
                    assetManager.getClass(), "getStringBlockCount", assetManager);
        for (int i = 0; i < assetsCount; i++) {
          String assetPath =
              (String)
                  ReflectUtils.invokeMethod(
                      assetManager.getClass(), "getCookieName", i + 1, assetManager);
          if (TextUtils.equals(assetPath, path)) {
            return true;
          }
        }
      }
    } catch (Throwable e) {
      Log.e(TAG, "containsPath error. ", e);
    }
    return false;
  }

  public static List<String> getAssetPaths(AssetManager assetManager) {

    List<String> assetPaths = new ArrayList<>();

    if (null == assetManager) {
      return assetPaths;
    }

    try {
      String assetPath = null;
      // Android P(9.0)(API_28)
      if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P
          || (Build.VERSION.SDK_INT == Build.VERSION_CODES.O_MR1
              && Build.VERSION.PREVIEW_SDK_INT > 0)) {
        Object[] assets =
            (Object[])
                ReflectUtils.invokeMethod(assetManager.getClass(), "getApkAssets", assetManager);
        if (null != assets && assets.length > 0) {
          for (Object asset : assets) {
            assetPaths.add(
                (String) ReflectUtils.invokeMethod(asset.getClass(), "getAssetPath", asset));
          }
        }
      } else {
        int assetsCount =
            (int)
                ReflectUtils.invokeMethod(
                    assetManager.getClass(), "getStringBlockCount", assetManager);
        for (int i = 0; i < assetsCount; i++) {
          assetPath =
              (String)
                  ReflectUtils.invokeMethod(
                      assetManager.getClass(), "getCookieName", i + 1, assetManager);
          if (!TextUtils.isEmpty(assetPath)) {
            assetPaths.add(assetPath);
          }
        }
      }

    } catch (Throwable e) {
      Log.e(TAG, "GetAssetsPaths error. ", e);
      e.printStackTrace();
    }

    return assetPaths;
  }
}
