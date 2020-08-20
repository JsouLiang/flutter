package io.flutter.view;

import androidx.annotation.Keep;

@Keep
public class NativeCodec {
  public int frameCount;
  public int width;
  public int height;
  public int repeatCount;
  public int[] frameDurations;
  public Object codec;
}
