package io.flutter.view;

import android.graphics.Bitmap;
import android.support.annotation.Keep;

@Keep
public class NativeCodec {
  public int frameCount;
  public int width;
  public int height;
  public int repeatCount;
  public int[] frameDurations;
  public Object codec;
}