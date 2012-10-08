package com.fteqw;

public class FTEDroidEngine
{
	public static native void init(int w, int h, int gles2, String apkpath, String usrpath); /* init/reinit */
	public static native int frame(float ax, float ay, float az);
	public static native int getvibrateduration();	//in ms
	public static native void keypress(int down, int qkey, int unicode);
	public static native void motion(int act, int pointerid, float x, float y, float size);
	public static native int  paintaudio(byte[] stream, int len);
	public static native int audioinfo(int arg);
	public static native String geterrormessage();
	public static native String getpreferedorientation();
	public static native void newglcontext();

	static
	{
			System.loadLibrary("ftedroid");
	}
}
