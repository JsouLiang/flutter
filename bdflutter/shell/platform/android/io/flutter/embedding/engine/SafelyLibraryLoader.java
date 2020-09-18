/**
 * *********************************************************************** > File Name:
 * SafelyLibraryLoader.java > Author: zhoutongwei > Mail: zhoutongwei@bytedance.com > Created Time:
 * 17:49:43 2021 **********************************************************************
 */
package io.flutter.embedding.engine;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.os.Build;
import io.flutter.Log;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

/**
 * The class provides helper functions to extract native libraries from APK, and load libraries from
 * there. The class should be package-visible only, but made public for testing purpose.
 */
public class SafelyLibraryLoader {

  private static final String LIB_DIR = "libso";
  private static List<String> sLoadedLibs = new ArrayList<>();

  @SuppressLint("UnsafeDynamicallyLoadedCode")
  public static synchronized String loadLibrary(Context context, String library) {
    if (sLoadedLibs.contains(library)) {
      return "";
    }
    try {
      System.loadLibrary(library);
      sLoadedLibs.add(library);
    } catch (UnsatisfiedLinkError e) {
      File output = getLibraryFile(context, library);
      if (output == null) {
        return "";
      }
      if (output.exists()) {
        output.delete();
      }
      String error = unpackLibrary(context, library, output);
      if (error != null) {
        Log.e("loadLibrary", e.getMessage() + "[" + error + "]");
        return null;
      } else {
        try {
          Log.e("loadLibrary", "output.getAbsolutePath:" + output.getAbsolutePath());
          System.load(output.getAbsolutePath());
          sLoadedLibs.add(library);
        } catch (Throwable t) {
          Log.e("loadLibrary", "Throwable: " + t.getMessage());
        } finally {
          return output.getAbsolutePath();
        }
      }
    } catch (Throwable t) {
      // ignore
      return null;
    }
    return null;
  }

  private static File getLibraryFolder(Context context) {
    if (context == null || context.getFilesDir() == null) {
      return null;
    }
    File file = new File(context.getFilesDir(), LIB_DIR);
    if (!file.exists()) {
      IoUtil.mkdir(file.getAbsolutePath());
    }
    return file;
  }

  private static File getLibraryFile(Context context, String library) {
    String libName = System.mapLibraryName(library);
    File libraryFolder = getLibraryFolder(context);
    if (libraryFolder != null) {
      return new File(libraryFolder, libName);
    } else {
      return null;
    }
  }

  /**
   * Unpack native libraries from the APK file.
   *
   * @param context
   * @param library
   * @param output
   * @return
   */
  public static String unpackLibrary(Context context, String library, File output) {
    ZipFile file = null;
    InputStream is = null;
    FileOutputStream os = null;
    try {
      ApplicationInfo appInfo = context.getApplicationInfo();
      file = new ZipFile(new File(appInfo.sourceDir), ZipFile.OPEN_READ);
      String jniNameInApk = "lib/" + Build.CPU_ABI + "/" + System.mapLibraryName(library);
      ZipEntry entry = file.getEntry(jniNameInApk);
      if (entry == null) {
        final int lineCharIndex = Build.CPU_ABI.indexOf('-');
        jniNameInApk =
            "lib/"
                + Build.CPU_ABI.substring(
                    0, lineCharIndex > 0 ? lineCharIndex : Build.CPU_ABI.length())
                + "/"
                + System.mapLibraryName(library);

        entry = file.getEntry(jniNameInApk);
        if (entry == null) {
          return "Library entry not found:" + jniNameInApk;
        }
      }
      output.createNewFile();
      is = file.getInputStream(entry);
      os = new FileOutputStream(output);
      int count;
      byte[] buffer = new byte[4 * 1024];
      while ((count = is.read(buffer)) > 0) {
        os.write(buffer, 0, count);
      }
      IoUtil.setPermissions(output.getAbsolutePath(), 0755);
      return null;
    } catch (Throwable th) {
      //            Ensure.getInstance().ensureNotReachHereForce(EnsureImpl.NPTH_CATCH, th);
      return th.getMessage();
    } finally {
      IoUtil.safeClose(os);
      IoUtil.safeClose(is);
      IoUtil.safeClose(file);
    }
  }
}
