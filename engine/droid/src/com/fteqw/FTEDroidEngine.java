package com.fteqw;

public class FTEDroidEngine
{
	public static native void init(int w, int h, float dpix, float dpiy, int gles2, String apkpath, String usrpath); /* init/reinit */
	public static native int frame(float ax, float ay, float az, float gx, float gy, float gz);
	public static native int openfile(String filename);
	public static native int getvibrateduration();	//in ms
	public static native int keypress(int down, int qkey, int unicode);
	public static native void motion(int act, int pointerid, float x, float y, float size);
	public static native int  paintaudio(byte[] stream, int len);
	public static native int audioinfo(int arg);
	public static native String geterrormessage();
	public static native String getpreferedorientation();
	public static native int getpreferedglesversion();
	public static native void killglcontext();
	public static native void newglcontext();

	static
	{
			System.loadLibrary("ftedroid");
	}
}
