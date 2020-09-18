/**
 * *********************************************************************** > File Name:
 * bdflutter/shell/platform/android/io/flutter/embedding/engine/IoUtil.java > Author: zhoutongwei >
 * Mail: zhoutongwei@bytedance.com > Created Time: 15:48:21 2021
 * **********************************************************************
 */
package io.flutter.embedding.engine;

import io.flutter.Log;
import java.io.Closeable;
import java.io.File;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.HashMap;
import java.util.Map;
import java.util.zip.ZipFile;

public class IoUtil {

  private IoUtil() {}

  public static void safeClose(Closeable stream) {
    try {
      if (stream != null) {
        stream.close();
      }
    } catch (Throwable ignored) {

    }
  }

  public static void safeClose(ZipFile zipFile) {
    if (zipFile == null) {
      return;
    }
    try {
      zipFile.close();
    } catch (Throwable ignored) {
      // ignore
    }
  }

  public static boolean mkdir(String path) {
    File dir = new File(path);
    return dir.mkdirs();
  }

  @SuppressWarnings("unchecked")
  public static <T> T callStaticMethodOrThrow(
      final String className, String methodName, Object... args)
      throws SecurityException, NoSuchMethodException, IllegalArgumentException,
          IllegalAccessException, InvocationTargetException, ClassNotFoundException {
    Class<?> clazz = Class.forName(className);
    Method method = getDeclaredMethod(clazz, methodName, getParameterTypes(args));

    T result = (T) method.invoke(null, getParameters(args));
    return result;
  }

  @SuppressWarnings("unchecked")
  public static <T> T callStaticMethodOrThrow(
      final Class<?> clazz, String methodName, Object... args)
      throws SecurityException, NoSuchMethodException, IllegalArgumentException,
          IllegalAccessException, InvocationTargetException {
    Method method = getDeclaredMethod(clazz, methodName, getParameterTypes(args));

    T result = (T) method.invoke(null, getParameters(args));
    return result;
  }

  public static <T> T callStaticMethod(String className, String methodName, Object... args) {
    try {
      Class<?> clazz = Class.forName(className);
      return callStaticMethodOrThrow(clazz, methodName, args);
    } catch (Exception e) {
      Log.w(
          "Flutter.io", "Meet exception when call Method '" + methodName + "' in " + className, e);
      return null;
    }
  }

  private static Class<?>[] getParameterTypes(Object... args) {
    Class<?>[] parameterTypes = null;

    if (args != null && args.length > 0) {
      parameterTypes = new Class<?>[args.length];
      for (int i = 0; i < args.length; i++) {
        Object param = args[i];
        if (param != null && param instanceof JavaParam<?>) {
          parameterTypes[i] = ((JavaParam<?>) param).clazz;
        } else {
          parameterTypes[i] = param == null ? null : param.getClass();
        }
      }
    }
    return parameterTypes;
  }

  private static Object[] getParameters(Object... args) {
    Object[] parameters = null;

    if (args != null && args.length > 0) {
      parameters = new Object[args.length];
      for (int i = 0; i < args.length; i++) {
        Object param = args[i];
        if (param != null && param instanceof JavaParam<?>) {
          parameters[i] = ((JavaParam<?>) param).obj;
        } else {
          parameters[i] = param;
        }
      }
    }
    return parameters;
  }

  private static Method getDeclaredMethod(
      final Class<?> clazz, String name, Class<?>... parameterTypes)
      throws NoSuchMethodException, SecurityException {
    Method[] methods = clazz.getDeclaredMethods();
    Method method = findMethodByName(methods, name, parameterTypes);
    if (method == null) {
      if (clazz.getSuperclass() != null) {
        // find in parent
        return getDeclaredMethod(clazz.getSuperclass(), name, parameterTypes);
      } else {
        throw new NoSuchMethodException();
      }
    }
    method.setAccessible(true);
    return method;
  }

  private static Method findMethodByName(Method[] list, String name, Class<?>[] parameterTypes) {
    if (name == null) {
      throw new NullPointerException("Method name must not be null.");
    }

    for (Method method : list) {
      if (method.getName().equals(name)
          && compareClassLists(method.getParameterTypes(), parameterTypes)) {
        return method;
      }
    }
    return null;
  }

  private static final Map<Class<?>, Class<?>> PRIMITIVE_MAP = new HashMap<>();

  static {
    PRIMITIVE_MAP.put(Boolean.class, boolean.class);
    PRIMITIVE_MAP.put(Byte.class, byte.class);
    PRIMITIVE_MAP.put(Character.class, char.class);
    PRIMITIVE_MAP.put(Short.class, short.class);
    PRIMITIVE_MAP.put(Integer.class, int.class);
    PRIMITIVE_MAP.put(Float.class, float.class);
    PRIMITIVE_MAP.put(Long.class, long.class);
    PRIMITIVE_MAP.put(Double.class, double.class);
    PRIMITIVE_MAP.put(boolean.class, boolean.class);
    PRIMITIVE_MAP.put(byte.class, byte.class);
    PRIMITIVE_MAP.put(char.class, char.class);
    PRIMITIVE_MAP.put(short.class, short.class);
    PRIMITIVE_MAP.put(int.class, int.class);
    PRIMITIVE_MAP.put(float.class, float.class);
    PRIMITIVE_MAP.put(long.class, long.class);
    PRIMITIVE_MAP.put(double.class, double.class);
  }

  public static class JavaParam<T> {
    public final Class<? extends T> clazz;
    public final T obj;

    public JavaParam(Class<? extends T> clazz, T obj) {
      this.clazz = clazz;
      this.obj = obj;
    }
  }

  private static boolean compareClassLists(Class<?>[] a, Class<?>[] b) {
    if (a == null) {
      return (b == null) || (b.length == 0);
    }

    if (b == null) {
      return (a.length == 0);
    }

    if (a.length != b.length) {
      return false;
    }

    for (int i = 0; i < a.length; ++i) {
      // if a[i] and b[i] is not same, return false
      if (!(a[i].isAssignableFrom(b[i])
          || (PRIMITIVE_MAP.containsKey(a[i])
              && PRIMITIVE_MAP.get(a[i]).equals(PRIMITIVE_MAP.get(b[i]))))) {
        return false;
      }
    }

    return true;
  }

  public static void setPermissions(String path, int permissions) {
    callStaticMethod("android.os.FileUtils", "setPermissions", path, permissions, -1, -1);
  }
}
