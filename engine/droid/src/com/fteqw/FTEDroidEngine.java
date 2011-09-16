package com.fteqw;

public class FTEDroidEngine
{
	public static native void init(int w, int h); /* init/reinit */
	public static native void frame();
	public static native void keypress(int down, int qkey, int unicode);
	public static native void motion(int act, float x, float y);
	public static native void accelerometer(float x, float y, float z);
	static
	{
			System.loadLibrary("ftedroid");
	}
}
