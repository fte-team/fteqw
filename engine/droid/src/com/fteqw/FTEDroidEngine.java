package com.fteqw;

public class FTEDroidEngine
{
	public static native void init(int w, int h, String apkpath, String usrpath); /* init/reinit */
	public static native void frame(float ax, float ay, float az);
	public static native void keypress(int down, int qkey, int unicode);
	public static native void motion(int act, int pointerid, float x, float y);
	public static native int  paintaudio(byte[] stream, int len);

	static
	{
			System.loadLibrary("ftedroid");
	}
}
