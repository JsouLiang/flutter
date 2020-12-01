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
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
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
    public static boolean copyFile(String origFilePath, String destFilePath, boolean deleteOriginFile) {
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

    /**
     * BD ADD
     * 拷贝整个文件夹
     * @param src
     * @param dest
     */
    public static boolean copyFolder(File src, File dest) {
        if (src.isDirectory()) {
            if (!dest.exists()) {
                dest.mkdirs();
            }
            String files[] = src.list();
            for (String file : files) {
                File srcFile = new File(src, file);
                File destFile = new File(dest, file);
                // 递归复制
                if (!copyFolder(srcFile, destFile)) {
                    return false;
                }
            }
            return true;
        } else {
            InputStream in = null;
            OutputStream out = null;
            try {
                in = new FileInputStream(src);
                out = new FileOutputStream(dest);

                byte[] buffer = new byte[1024];

                int length;

                while ((length = in.read(buffer)) > 0) {
                    out.write(buffer, 0, length);
                }
                return true;
            } catch (IOException e) {
                return false;
            } finally {
                if (in != null) {
                    try {
                        in.close();
                    } catch (IOException e) {
                    }
                }
                if (out != null) {
                    try {
                        out.close();
                    } catch (IOException e) {
                    }
                }

            }
        }
    }

    /**
     * BD ADD
     * @param file
     * @return
     */
    public static boolean delete(File file) {
        if (file.isFile()) {
            return file.delete();
        }

        if (file.isDirectory()) {
            File[] childFiles = file.listFiles();
            if (childFiles == null || childFiles.length == 0) {
                return file.delete();
            }

            for (int i = 0; i < childFiles.length; i++) {
                if (!delete(childFiles[i])) {
                    return false;
                }
            }
            return file.delete();
        }
        return false;
    }
}
