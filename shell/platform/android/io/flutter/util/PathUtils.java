// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.util;

import android.content.Context;
import android.os.Build;
// BD ADD
import java.io.File;

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
}
