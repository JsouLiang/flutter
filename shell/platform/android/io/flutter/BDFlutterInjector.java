
package io.flutter;

import android.support.annotation.NonNull;
import android.support.annotation.VisibleForTesting;

public final class BDFlutterInjector {

    private static BDFlutterInjector instance;

    @VisibleForTesting
    public static void setInstance(@NonNull BDFlutterInjector injector) {
        instance = injector;
    }

    public static BDFlutterInjector instance() {
        if (instance == null) {
            instance = new Builder().build();
        }
        return instance;
    }

    // This whole class is here to enable testing so to test the thing that lets you test, some degree
    // of hack is needed.
    @VisibleForTesting
    public static void reset() {
        instance = null;
    }

    private BDFlutterInjector(boolean shouldLoadNative) {
        this.shouldLoadNative = shouldLoadNative;
    }

    private final boolean shouldLoadNative;

    /**
     * Returns whether the Flutter Android engine embedding should load the native C++ engine.
     *
     * <p>Useful for testing since JVM tests via Robolectric can't load native libraries.
     */
    public boolean shouldLoadNative() {
        return shouldLoadNative;
    }

    /**
     * Builder used to supply a custom FlutterInjector instance to {@link
     * BDFlutterInjector#setInstance(BDFlutterInjector)}.
     *
     * <p>Non-overriden values have reasonable defaults.
     */
    public static final class Builder {

        // Default value should be true when normal runtime, false when running tests
        private boolean shouldLoadNative = true;

        public Builder setShouldLoadNative(boolean shouldLoadNative) {
            this.shouldLoadNative = shouldLoadNative;
            return this;
        }


        public BDFlutterInjector build() {
            System.out.println("should load native is " + shouldLoadNative);
            return new BDFlutterInjector(shouldLoadNative);
        }
    }
}
