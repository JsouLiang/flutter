/*************************************************************************
 > File Name: bdflutter/shell/platform/android/io/flutter/embedding/engine/AssetUtil.java
 > Author: zhoutongwei
 > Mail: zhoutongwei@bytedance.com
 > Created Time: 六 10/ 9 16:57:48 2021
 ************************************************************************/
package io.flutter.embedding.engine;

import android.content.res.AssetManager;
import android.os.Build;
import android.text.TextUtils;

import io.flutter.embedding.engine.ReflectUtils;

import java.lang.reflect.Array;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;

import io.flutter.Log;

public final class AssetUtil {

  private static final String TAG = "ReflectUtils";

  // 防止被继承
  private AssetUtil() {
  }

  public static AssetManager updateAssetManager(AssetManager newAssetManager, AssetManager originAssetManager) {
    //Android KK(4.x) (API_20)及其以下机型直接使用originAssetManager
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

  /**
   * 更新AssetManager
   * <p>
   * 1.addAssetPath()及其适配
   * 2.new AssetManager及其适配
   *
   * @param originAssetManager
   * @param sourceDir
   * @return
   */
  public static AssetManager updateAssetManager(AssetManager originAssetManager, String sourceDir) {
    AssetManager newAssetManager = null;
    // >= Android L(5.0) (API_21)
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
      // >= Android L(5.0) (API_21) && <= Android N_MR1(7.1/7.1.1)(API_25)
      if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP && Build.VERSION.SDK_INT <= Build.VERSION_CODES.N_MR1) {
        newAssetManager = appendAssetPathSafely(originAssetManager, sourceDir);
        // 某些厂商机型比如三星修改过源代码，上述办法不一定有效，这里验证是否添加成功，决定直接反射重新添加
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

  public static void writeField(Object target, String fieldName, Object value) throws IllegalAccessException {
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

  /**
   * Android L(5.0) (API_21) ~ Android N_MR1(7.1/7.1.1)(API_25)
   * 向 AssetManager 对象中添加资源（适配异步调用的崩溃问题），虽然没有从根本上保证原子性，
   * 但尽量向原子性靠拢和收敛，降低发生概率来解决，相比直接反射调用AssetManager.addAssetPath()概率小了很多
   * <p>
   * 反射修改AssetManager这段代码的实现
   * <p>
   * public final int addAssetPath(String path) {
   * synchronized (this) {
   * int res = addAssetPathNative(path);
   * makeStringBlocks(mStringBlocks);
   * return res;
   * }
   * }
   * <p>
   * final void makeStringBlocks(StringBlock[] seed) {
   * final int seedNum = (seed != null) ? seed.length : 0;
   * final int num = getStringBlockCount();
   * mStringBlocks = new StringBlock[num];
   * for (int i=0; i<num; i++) {
   * if (i < seedNum) {
   * mStringBlocks[i] = seed[i];
   * } else {
   * mStringBlocks[i] = new StringBlock(getNativeStringBlock(i), true);
   * }
   * }
   * }
   *
   * @param assetManager
   * @param sourceDir
   * @return
   */
  private static AssetManager appendAssetPathSafely(AssetManager assetManager, String sourceDir) {
    // 如果添加失败重试3次
    int tryCount = 3;
    while (tryCount-- >= 0) {
      String errorMsg = null;
      try {
        // 加锁：保持跟原方法一致
        synchronized (assetManager) {
          // addAssetPathNative
          int cookie = 0;
          // retry 3
          for (int i = 0; i < 3; i++) {
            // private native final int addAssetPathNative(String path);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP && Build.VERSION.SDK_INT <= Build.VERSION_CODES.M) {
              cookie = (int) ReflectUtils.invokeMethod(assetManager.getClass(), "addAssetPathNative", new Object[]{sourceDir}, new Class[]{String.class}, assetManager);
            }
            // private native final int addAssetPathNative(String path, boolean appAsLib);
            else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N && Build.VERSION.SDK_INT <= Build.VERSION_CODES.N_MR1) {
              cookie = (int) ReflectUtils.invokeMethod(assetManager.getClass(), "addAssetPathNative", new Object[]{sourceDir, false}, new Class[]{String.class, boolean.class}, assetManager);
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
          final int num = (int) ReflectUtils.invokeMethod(assetManager.getClass(), "getStringBlockCount", assetManager);
          Object newStringBlocks = Array.newInstance(seed.getClass().getComponentType(), num);
          for (int i = 0; i < num; i++) {
            if (i < seedNum) {
              // copy element
              Array.set(newStringBlocks, i, Array.get(seed, i));
            } else {
              // private native final long getNativeStringBlock(int block);
              long nativeStringBlockObj = (long) ReflectUtils.invokeMethod(assetManager.getClass(), "getNativeStringBlock", new Object[]{i}, new Class[]{int.class}, assetManager);
              // new StringBlock(long obj, boolean useSparse);
              Object stri = ReflectUtils.invokeConstructor(seed.getClass().getComponentType(), new Object[]{nativeStringBlockObj, true}, new Class[]{long.class, boolean.class});
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

  /**
   * Android O(8.0) (API_26)及其以上
   * 向 AssetManager 对象中添加资源
   * 方案：直接反射调用 AssetManager.addAssetPath()方法
   *
   * @param assetManager
   * @param path
   * @return
   */
  private static AssetManager appendAssetPath(AssetManager assetManager, String path) {
    Method addAssetPathMethod = ReflectUtils.getMethod(AssetManager.class, "addAssetPath", new Class<?>[]{String.class,});
    if (null != addAssetPathMethod) {
      int tryCount = 3;
      int cookie = 0;

      // 如果添加失败重试3次
      while (tryCount-- >= 0) {
        try {
          cookie = (int) addAssetPathMethod.invoke(assetManager, path);
          if (cookie != 0) {
            Log.i(TAG, TAG + " invoke AssetManager.addAssetPath() success, cookie = " + cookie + ", path = " + path);
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

  /**
   * 检测AssetManager中是否包含目标资源路径
   *
   * @param assetManager
   * @param path
   * @return
   */
  public static boolean containsPath(AssetManager assetManager, String path) {
    try {
      if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P || (Build.VERSION.SDK_INT == Build.VERSION_CODES.O_MR1 && Build.VERSION.PREVIEW_SDK_INT > 0)) {
        Object[] assets = (Object[]) ReflectUtils.invokeMethod(assetManager.getClass(), "getApkAssets", assetManager);
        if (assets != null && assets.length > 0) {
          for (Object asset : assets) {
            String assetPath = (String) ReflectUtils.invokeMethod(asset.getClass(), "getAssetPath", asset);
            if (TextUtils.equals(assetPath, path)) {
              return true;
            }
          }
        }
      } else {
        int assetsCount = (int) ReflectUtils.invokeMethod(assetManager.getClass(), "getStringBlockCount", assetManager);
        for (int i = 0; i < assetsCount; i++) {
          String assetPath = (String) ReflectUtils.invokeMethod(assetManager.getClass(), "getCookieName", i + 1, assetManager);
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

  /**
   * 获取 AssetManager 下 AssetsPaths 所有路径集合。
   *
   * @param assetManager
   * @return
   */
  public static List<String> getAssetPaths(AssetManager assetManager) {

    List<String> assetPaths = new ArrayList<>();

    if (null == assetManager) {
      return assetPaths;
    }

    try {
      String assetPath = null;
      // Android P(9.0)(API_28)
      if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P || (Build.VERSION.SDK_INT == Build.VERSION_CODES.O_MR1 && Build.VERSION.PREVIEW_SDK_INT > 0)) {
        Object[] assets = (Object[]) ReflectUtils.invokeMethod(assetManager.getClass(), "getApkAssets", assetManager);
        if (null != assets && assets.length > 0) {
          for (Object asset : assets) {
            assetPaths.add((String) ReflectUtils.invokeMethod(asset.getClass(), "getAssetPath", asset));
          }
        }
      } else {
        int assetsCount = (int) ReflectUtils.invokeMethod(assetManager.getClass(), "getStringBlockCount", assetManager);
        for (int i = 0; i < assetsCount; i++) {
          // Cookies map 计数从 1 开始
          assetPath = (String) ReflectUtils.invokeMethod(assetManager.getClass(), "getCookieName", new Class[] { int.class }, new Object[] { i + 1 }, assetManager);
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
