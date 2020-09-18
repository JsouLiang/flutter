package io.flutter.embedding.engine;

import android.text.TextUtils;
import io.flutter.Log;
import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public final class ReflectUtils {

  private static final String TAG = "ReflectUtils";
  public static final Object[] EMPTY_OBJECT_ARRAY = new Object[0];
  public static final Class<?>[] EMPTY_CLASS_ARRAY = new Class[0];

  private ReflectUtils() {}

  public static Object[] nullToEmpty(final Object[] array) {
    if (array == null || array.length == 0) {
      return EMPTY_OBJECT_ARRAY;
    }
    return array;
  }

  public static Class<?>[] nullToEmpty(final Class<?>[] array) {
    if (array == null || array.length == 0) {
      return EMPTY_CLASS_ARRAY;
    }
    return array;
  }

  public static Method getMethod(Class<?> owner, String func, Class<?>[] params) {
    if (owner == null || TextUtils.isEmpty(func)) {
      return null;
    }
    Method method = null;
    try {
      Log.d(TAG, "thread id : " + Thread.currentThread().getName());
      method = owner.getMethod(func, params);
    } catch (Throwable e) {
      Log.d(
          TAG,
          "exception in getMethod, pkg : "
              + owner.getName()
              + ", function : "
              + func
              + ", "
              + e.toString());
      try {
        method = owner.getDeclaredMethod(func, params);
      } catch (Throwable t) {
        // ignore
      }
    }
    return method;
  }

  public static <T> T invokeConstructor(
      final Class<T> owner, Object[] args, Class<?>[] parameterTypes)
      throws NoSuchMethodException, IllegalAccessException, InvocationTargetException,
          InstantiationException {
    args = nullToEmpty(args);
    parameterTypes = nullToEmpty(parameterTypes);
    final Constructor<T> ctor = getConstructor(owner, parameterTypes);
    if (ctor == null) {
      throw new NoSuchMethodException(
          "No such accessible constructor on object: " + owner.getName());
    }
    return ctor.newInstance(args);
  }

  public static <T> Constructor<T> getConstructor(Class<T> owner, Class<?>[] params) {
    Method method = null;
    try {
      Log.d(TAG, "thread id : " + Thread.currentThread().getName());
      final Constructor<T> ctor = owner.getConstructor(params);
      ctor.setAccessible(true);
      return ctor;
    } catch (Throwable e) {
      Log.d(TAG, "exception in getConstructor, pkg : " + owner.getName() + e.toString());
      return null;
    }
  }

  public static Object invokeMethod(Class<?> owner, String func, Object... receiver) {
    return invokeMethod(owner, func, null, null, receiver);
  }

  public static Object invokeMethod(
      Class<?> owner, String func, Class<?>[] paramtype, Object[] params, Object... receiver) {
    if (owner == null || func.isEmpty()) {
      return null;
    }
    if (paramtype == null) {
      paramtype = new Class<?>[] {};
    }
    if (params == null) {
      params = new Object[] {};
    }
    try {
      Method method = getMethod(owner, func, paramtype);
      if (method == null) {
        return null;
      }
      method.setAccessible(true);
      if (receiver != null && receiver.length > 0) {
        return method.invoke(receiver[0], params);
      }
      return method.invoke(null, params);
    } catch (Throwable e) {
      Log.d(
          TAG,
          "exception in invokeMethod, pkg : "
              + owner.getName()
              + ", function : "
              + func
              + ", "
              + e.toString());
    }
    return null;
  }
}
