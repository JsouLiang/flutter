// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.util;

import android.content.Context;
import android.os.Build;
// BD ADD: START
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
// END

public final class PathUtils {
  public static String getFilesDir(Context applicationContext) {
    return applicationContext.getFilesDir().getPath();
  }

  public static String getDataDirectory(Context applicationContext) {
    return applicationContext.getDir("flutter", Context.MODE_PRIVATE).getPath();
  }

  public static String getCacheDirectory(Context applicationContext) {
    // BD MOD:START
    // if (Build.VERSION.SDK_INT >= 21) {
    if (Build.VERSION.SDK_INT >= 21 && applicationContext.getCodeCacheDir() != null) {
    // END
      return applicationContext.getCodeCacheDir().getPath();
    } else {
      return applicationContext.getCacheDir().getPath();
    }
  }

  /**
   * BD ADD
   */
  public static boolean copyFile(Context context, String origFilePath, String destFilePath, boolean deleteOriginFile) {
      boolean copyIsFinish = false;
      try {
          File destDir = new File(destFilePath.substring(0, destFilePath.lastIndexOf("/")));
          if (!destDir.exists()) {
              destDir.mkdirs();
          }
          File origFile = new File(origFilePath);
          FileInputStream is = new FileInputStream(origFile);
          File destFile = new File(destFilePath);
          if (destFile.exists()) {
              destFile.delete();
          }
          destFile.createNewFile();
          FileOutputStream fos = new FileOutputStream(destFile);
          byte[] temp = new byte[1024];
          int i = 0;
          while ((i = is.read(temp)) > 0) {
              fos.write(temp, 0, i);
          }
          fos.close();
          is.close();
          if (deleteOriginFile) {
              origFile.delete();
          }
          copyIsFinish = true;
      } catch (Exception e) {
          e.printStackTrace();
      }
      return copyIsFinish;
  }
}
