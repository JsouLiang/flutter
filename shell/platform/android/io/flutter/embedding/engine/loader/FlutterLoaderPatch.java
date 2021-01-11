package io.flutter.embedding.engine.loader;

import android.content.Context;


import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.charset.Charset;
import java.util.HashMap;

import com.bytedance.crash.Npth;

import io.flutter.Log;

/**
 * BD ADD:
 * For some customization of the initialization of flutter
 */
class FlutterLoaderPatch {
    private final static String TAG = "FlutterLoaderPatch";
    private final static String FLUTTER_VERSION_NAME = "flutter_version";
    private final FlutterLoader flutterLoader;
    private final Context context;
    private final String defaultFlutterAssetsDir;

    FlutterLoaderPatch(FlutterLoader flutterLoader, Context applicationContext, String defaultFlutterAssetsDir) {
        this.flutterLoader = flutterLoader;
        this.context = applicationContext;
        this.defaultFlutterAssetsDir = defaultFlutterAssetsDir;
    }

    public void onInitialize() {
        Log.d(TAG, "onInitialize");
        try {
            initializeVersionInfoForNpth();
        } catch (LibraryLinkageException e) {
            //noinspection SpellCheckingInspection
            Log.w(TAG, "Init flutter version info for Npth failed, ignored. " + e.toString());
        }
    }

    static class LibraryLinkageException extends Exception {
        LibraryLinkageException(String message, Throwable cause) {
            super(message, cause);
        }
    }

    @SuppressWarnings("SpellCheckingInspection")
    private void initializeVersionInfoForNpth() throws LibraryLinkageException {
        final String flutterVersion = readFlutterVersion();
        Log.i(TAG, "flutter version: " + flutterVersion);
        // Don't call Npth using reflection because apk may use proguard and reflection will fail.
        // Here puts Npth in classpath (compileOnly dependency) and calls Npth in bytecode directly,
        // and proguard can rename and handle the method calling correctly.
        try {
            String npthClassName = Npth.class.toString();
            Log.d(TAG, "found Npth class: " + npthClassName);
        } catch (NoClassDefFoundError e) {
            throw new LibraryLinkageException("Npth class not found at runtime", e);
        }
        HashMap<String, String> tags = new HashMap<>();
        tags.put("flutter_version_commit_id", flutterVersion);
        final int FLUTTER_ENGINE_SLARDAR_SID = 2180;
        final int FLUTTER_ENGINE_SLARDAR_NEW_SID = 4908;
        try {
            Npth.addTags(tags);
            Npth.registerSdk(FLUTTER_ENGINE_SLARDAR_SID, flutterVersion);
            Npth.registerSdk(FLUTTER_ENGINE_SLARDAR_NEW_SID, flutterVersion);
        } catch (LinkageError e) {
            throw new LibraryLinkageException("Npth method calling failed", e);
        }
    }

    private String readFlutterVersion() {
        String filePath = fullAssetPathFrom(defaultFlutterAssetsDir, FLUTTER_VERSION_NAME);
        try {
            InputStream is = context.getAssets().open(filePath);
            @SuppressWarnings("CharsetObjectCanBeUsed")
            BufferedReader br = new BufferedReader(new InputStreamReader(is, Charset.forName("UTF-8")));
            String str;
            StringBuilder sb = new StringBuilder();
            while ((str = br.readLine()) != null) {
                sb.append(str);
            }
            br.close();
            return sb.toString();
        } catch (IOException e) {
            Log.e(TAG, "cannot read flutter version from assets", e);
        }
        return "unknown";
    }

    private String fullAssetPathFrom(String flutterAssetsDir, String filePath) {
        return flutterAssetsDir + File.separator + filePath;
    }
}
