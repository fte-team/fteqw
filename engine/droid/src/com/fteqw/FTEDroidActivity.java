package com.fteqw;

import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.opengles.GL10;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.view.Window;
import android.view.WindowManager;

import android.opengl.GLSurfaceView;
import android.view.KeyEvent;
import android.view.MotionEvent;

import android.hardware.SensorManager;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;

import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.Environment;

import android.view.inputmethod.InputMethodManager;

import android.os.Vibrator;

public class FTEDroidActivity extends Activity
{
	private static final int USE_GLES_VERSION = 1;  //valid values: 1 or 2. If set to 2, it'll still fall back to 1 if gles2 isn't supported on this device.

	private SensorManager sensorman;
	private Sensor sensoracc;
	private FTEView view;
	float acc_x, acc_y, acc_z; /*might be some minor race condition on these*/

	private class FTERenderer implements GLSurfaceView.Renderer 
	{
		private boolean inited;
		public int glesversion;
		private String basedir, userdir;
		FTEDroidActivity act;
		FTEView theview;
		int notifiedflags;
		
		FTERenderer(FTEView view, FTEDroidActivity parent)
		{
			act = parent;
			theview = view;
			try
			{
			   android.content.pm.PackageInfo info = parent.getPackageManager().getPackageInfo("com.fteqw", 0);
			   basedir = info.applicationInfo.sourceDir;
			}
			catch(android.content.pm.PackageManager.NameNotFoundException e)
			{
				/*oh well, can just use the homedir instead*/
			}
//			try
//			{
				userdir = Environment.getExternalStorageDirectory().getPath() + "/fte";
//			}
//			catch(foo)
//			{
//			}
			
			android.util.Log.i("FTEDroid", "Base dir is \"" + basedir + "\".");
			android.util.Log.i("FTEDroid", "User dir is \"" + userdir + "\".");
		}
		
		@Override
		public void onDrawFrame(GL10 gl)
		{
			if (inited == true)
			{
				int flags;
				flags = FTEDroidEngine.frame(act.acc_x, act.acc_y, act.acc_z);
				if (flags != notifiedflags)
				{
					if (((flags ^ notifiedflags) & 1) != 0)
					{
						final int fl = flags;
						Runnable r = new Runnable() 
						{	//doing this on the ui thread because android sucks.
							public void run()
							{
								InputMethodManager im = (InputMethodManager) act.getSystemService(Context.INPUT_METHOD_SERVICE);
								if (im != null)
								{
									if ((fl & 1) != 0)
									{
		//								getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE);
										im.showSoftInput(theview, 0);//InputMethodManager.SHOW_FORCED);
									}
									else
									{
		//								getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_HIDDEN);
										im.hideSoftInputFromWindow(theview.getWindowToken(), 0);
									}
								}
								else
									android.util.Log.i("FTEDroid", "IMM failed");
							}
						};
						act.runOnUiThread(r);
					}
					if (((flags ^ notifiedflags) & 2) != 0)
					{
						int dur = FTEDroidEngine.getvibrateduration();
						flags &= ~2;
						Vibrator vib = (Vibrator) act.getSystemService(Context.VIBRATOR_SERVICE);
						if (vib != null)
						{
							android.util.Log.i("FTEDroid", "Vibrate " + dur + "ms");
							vib.vibrate(dur);
						}
						else
							android.util.Log.i("FTEDroid", "No vibrator device");
					}
					if (((flags ^ notifiedflags) & 4) != 0)
					{
						final int fl = flags;
						Runnable r = new Runnable() 
						{
							public void run()
							{
								if ((fl & 4) != 0)
									act.getWindow().setFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON, WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
								else
									act.getWindow().setFlags(0, WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
							}
						};
						act.runOnUiThread(r);
					}

					//clear anything which is an impulse
					notifiedflags = flags;
				}
			}
		}
		@Override
		public void onSurfaceChanged(GL10 gl, int width, int height)
		{
			android.util.Log.i("FTEDroid", "Surface changed, now " + width + " by " + height + ".");
			FTEDroidEngine.init(width, height, glesversion, basedir, userdir);
			inited = true;
		}
		@Override
		public void onSurfaceCreated(GL10 gl, EGLConfig config)
		{
		}
	}

	private class FTEEGLConfig implements GLSurfaceView.EGLConfigChooser
	{
		public void setversion(FTEView view, int version)
		{
			view.setEGLContextClientVersion(version);
		}
		public boolean CheckGLES2Support()
		{
			EGL10 egl = (EGL10) EGLContext.getEGL();       
			EGLDisplay display = egl.eglGetDisplay(EGL10.EGL_DEFAULT_DISPLAY);
			EGLConfig cfg;
			
			int[] version = new int[2];
			egl.eglInitialize(display, version);

			cfg = chooseConfig(egl, display);
			
			int[] value = {0};
			egl.eglGetConfigAttrib(display, cfg, EGL10.EGL_RENDERABLE_TYPE, value);
			egl.eglTerminate(display);
			return ((value[0] & 4) == 4);
		}
		
		@Override
		public EGLConfig chooseConfig (EGL10 egl, EGLDisplay display)
		{
			int EGL_CONTEXT_CLIENT_VERSION = 0x3098;
			EGLConfig[] cfg = new EGLConfig[64];
			int[] num_configs = {0};
			int[] value = {0};
			int i;
			int[] attribs =
				{
					egl.EGL_RENDERABLE_TYPE,	4/*egl.EGL_OPENGL_ES2_BIT*/,
//					egl.EGL_SURFACE_TYPE,		egl.EGL_WINDOW_BIT,
					egl.EGL_BLUE_SIZE,		5,
					egl.EGL_GREEN_SIZE,		6,
					egl.EGL_RED_SIZE,			5,
					egl.EGL_DEPTH_SIZE,		16,
//					egl.EGL_STENCIL_SIZE, 		8,
					egl.EGL_NONE,			egl.EGL_NONE
				};

			if (!egl.eglChooseConfig(display, attribs, cfg, 64, num_configs))
				throw new IllegalArgumentException("eglChooseConfig failed");
				
			if (num_configs[0] == 0)
			{
				attribs[1] = 1;	//egl.EGL_RENDERABLE_TYPE,	1/*egl.EGL_OPENGL_ES_BIT*/,
				if (!egl.eglChooseConfig(display, attribs, cfg, 64, num_configs))
					throw new IllegalArgumentException("eglChooseConfig failed");
					
				if (num_configs[0] == 0)
				{
					throw new IllegalArgumentException("eglChooseConfig didn't report any valid configs");
				}
			}
				
			android.util.Log.i("FTEDroid", "Found " + num_configs[0] + " EGL configs.");
			
			for (i = 0; i < num_configs[0]; i++)
			{
				android.util.Log.i("FTEDroid", "Config " + i + ":");
				egl.eglGetConfigAttrib(display, cfg[i], egl.EGL_RED_SIZE, value);
				android.util.Log.i("FTEDroid", "EGL_RED_SIZE " + value[0]);
				egl.eglGetConfigAttrib(display, cfg[i], egl.EGL_GREEN_SIZE, value);
				android.util.Log.i("FTEDroid", "EGL_GREEN_SIZE " + value[0]);
				egl.eglGetConfigAttrib(display, cfg[i], egl.EGL_BLUE_SIZE, value);
				android.util.Log.i("FTEDroid", "EGL_BLUE_SIZE " + value[0]);
				egl.eglGetConfigAttrib(display, cfg[i], egl.EGL_DEPTH_SIZE, value);
				android.util.Log.i("FTEDroid", "EGL_DEPTH_SIZE " + value[0]);
				egl.eglGetConfigAttrib(display, cfg[i], egl.EGL_STENCIL_SIZE, value);
				android.util.Log.i("FTEDroid", "EGL_STENCIL_SIZE " + value[0]);
				
				egl.eglGetConfigAttrib(display, cfg[i], egl.EGL_RENDERABLE_TYPE, value);
				android.util.Log.i("FTEDroid", "EGL_RENDERABLE_TYPE " + value[0]);
				
				if ((value[0] & 4) == 4)
				{
					android.util.Log.i("FTEDroid", "Found a GLES2 context!");
					return cfg[i];
				}
			}
			return cfg[0];
		}
	}

	private class FTEView extends GLSurfaceView implements SensorEventListener
	{
		private final FTERenderer rndr;

		private audiothreadclass audiothread;
		private class audiothreadclass extends Thread
		{
			boolean timetodie;
			@Override
			public void run()
			{
				byte[] audbuf = new byte[2048];
				int avail;
				
				int sspeed = 11025;
				int speakers = 1;
				int sz = 2*AudioTrack.getMinBufferSize(sspeed, ((speakers==2)?AudioFormat.CHANNEL_CONFIGURATION_STEREO:AudioFormat.CHANNEL_CONFIGURATION_MONO), AudioFormat.ENCODING_PCM_16BIT);

//				if (sz < sspeed * 0.05)
//					sz = sspeed * 0.05;

				AudioTrack at = new AudioTrack(AudioManager.STREAM_MUSIC, sspeed, ((speakers==2)?AudioFormat.CHANNEL_CONFIGURATION_STEREO:AudioFormat.CHANNEL_CONFIGURATION_MONO), AudioFormat.ENCODING_PCM_16BIT, sz, AudioTrack.MODE_STREAM);

				at.setStereoVolume(1, 1);
				at.play();
			
			
				while(!timetodie)
				{
					avail = FTEDroidEngine.paintaudio(audbuf, audbuf.length);
					at.write(audbuf, 0, avail);
				}
				
				at.stop();
			}
			public void killoff()
			{
				timetodie = true;
				try
				{
					join();
				}
				catch(InterruptedException e)
				{
				}
			}
		};

		private void audioInit()
		{
			if (audiothread == null)
			{
				audiothread = new audiothreadclass();
				audiothread.start();
			}
		}
		public void audioStop()
		{
			if (audiothread != null)
			{
				audiothread.killoff();
				audiothread = null;
			}
		}
		public void audioResume()
		{
			audioStop();
			audioInit();
		}
		
		private FTELegacyInputEvent inputevent;
		class FTEMultiTouchInputEvent extends FTELegacyInputEvent
		{
			/*Requires API level 5+ (android 2.0+)*/
			private void domove(MotionEvent event)
			{
				final int pointerCount = event.getPointerCount();
				int i;
				for (i = 0; i < pointerCount; i++)
					FTEDroidEngine.motion(0, event.getPointerId(i), event.getX(i), event.getY(i), event.getSize(i));
			}
			
			public boolean go(MotionEvent event)
			{
				int id;
				float x, y, size;
				final int act = event.getAction();
				
				domove(event);
				
				switch(act & event.ACTION_MASK)
				{
				case MotionEvent.ACTION_DOWN:
				case MotionEvent.ACTION_POINTER_DOWN:
					id = ((act&event.ACTION_POINTER_ID_MASK) >> event.ACTION_POINTER_ID_SHIFT);
					x = event.getX(id);
					y = event.getY(id);
					id = event.getPointerId(id);
					size = event.getSize(id);
					FTEDroidEngine.motion(1, id, x, y, size);
					break;
				case MotionEvent.ACTION_UP:
				case MotionEvent.ACTION_POINTER_UP:
					id = ((act&event.ACTION_POINTER_ID_MASK) >> event.ACTION_POINTER_ID_SHIFT);
					x = event.getX(id);
					y = event.getY(id);
					id = event.getPointerId(id);
					size = event.getSize(id);
					FTEDroidEngine.motion(2, id, x, y, size);
					break;
				case MotionEvent.ACTION_MOVE:
					break;
				default:
					return false;
				}
				return true;
			}
		}
		class FTELegacyInputEvent
		{
			public boolean go(MotionEvent event)
			{
				final int act = event.getAction();
				final float x = event.getX();
				final float y = event.getY();
				final float size = event.getSize();

				FTEDroidEngine.motion(0, 0, x, y, size);

				switch(act)
				{
				case MotionEvent.ACTION_DOWN:
					FTEDroidEngine.motion(1, 0, x, y, size);
					break;
				case MotionEvent.ACTION_UP:
					FTEDroidEngine.motion(2, 0, x, y, size);
					break;
				case MotionEvent.ACTION_MOVE:
					break;
				default:
					return false;
				}
				return true;
			}
		}

		public FTEView(FTEDroidActivity context)
		{
			super(context);
			
			if (android.os.Build.VERSION.SDK_INT >= 5)
				inputevent = new FTEMultiTouchInputEvent();
			else
				inputevent = new FTELegacyInputEvent();

			rndr = new FTERenderer(this, context);
			
			if (USE_GLES_VERSION < 2)
			{
				android.util.Log.i("FTEDroid", "GLES2 disabled at game compile time");
				rndr.glesversion = 1;
			}
			else if (android.os.Build.VERSION.SDK_INT >= 8)	//could be 5 with setEGLContextFactory instead of setEGLContextClientVersion
			{
				FTEEGLConfig cfgchooser = new FTEEGLConfig();
				setEGLConfigChooser(cfgchooser);
				
				if (cfgchooser.CheckGLES2Support())
				{
					android.util.Log.i("FTEDroid", "Support for GLES2 detected");
					rndr.glesversion = 2;
					cfgchooser.setversion(this, rndr.glesversion);
				}
				else
				{
					android.util.Log.i("FTEDroid", "GLES2 not supported. Using GLES1.");
					rndr.glesversion = 1;
				}
			}
			else
			{
				android.util.Log.i("FTEDroid", "GLES2 requires android 2.2+");
				rndr.glesversion = 1;
			}
				
			setRenderer(rndr);
			setFocusable(true);
			setFocusableInTouchMode(true);

			android.util.Log.i("FTEDroid", "starting audio");
			audioInit();
			android.util.Log.i("FTEDroid", "audio running");
		}
		
		private void sendKey(final boolean presseddown, final int qcode, final int unicode)
		{
			FTEDroidEngine.keypress(presseddown?1:0, qcode, unicode);
		}
		@Override
		public boolean onTouchEvent(MotionEvent event)
		{
			return inputevent.go(event);
		}
		/*
		@Override
		public boolean onTrackballEvent(MotionEvent event)
		{
			int act = event.getAction();
			float x = event.getX();
			float y = event.getY();
		}
		*/
		private static final int K_ENTER		= 13;
		private static final int K_ESCAPE		= 27;
		private static final int K_DEL			= 127;
		private static final int K_POWER		= 130;
		private static final int K_UPARROW		= 132;
		private static final int K_DOWNARROW	= 133;
		private static final int K_LEFTARROW	= 134;
		private static final int K_RIGHTARROW	= 135;
		private static final int K_APP			= 241;
		private static final int K_SEARCH		= 242;
		private static final int K_VOLUP		= 243;
		private static final int K_VOLDOWN		= 244;
		private int mapKey(int acode, int unicode)
		{
			switch(acode)
			{
			case KeyEvent.KEYCODE_DPAD_UP:
				return K_UPARROW;
			case KeyEvent.KEYCODE_DPAD_DOWN:
				return K_DOWNARROW;
			case KeyEvent.KEYCODE_DPAD_LEFT:
				return K_LEFTARROW;
			case KeyEvent.KEYCODE_DPAD_RIGHT:
				return K_RIGHTARROW;
			case KeyEvent.KEYCODE_DPAD_CENTER:
			case KeyEvent.KEYCODE_ENTER:
				return K_ENTER;
			case KeyEvent.KEYCODE_BACK:
				return K_ESCAPE;	//escape, cannot be rebound
			case KeyEvent.KEYCODE_MENU:
				return K_APP;		//"app"
			case KeyEvent.KEYCODE_DEL:
				return K_DEL;		//"del"
			case KeyEvent.KEYCODE_SEARCH:
				return K_SEARCH;	//"search"
			case KeyEvent.KEYCODE_POWER:
				return K_POWER;		//"power"
			case KeyEvent.KEYCODE_VOLUME_DOWN:
				return K_VOLDOWN;	//"voldown"
			case KeyEvent.KEYCODE_VOLUME_UP:
				return K_VOLUP;		//"volup"
			default:
				if (unicode < 128)
					return Character.toLowerCase(unicode);
			}
			return 0;
		}
		
		@Override
		public boolean onKeyDown(int keyCode, KeyEvent event)
		{
			int uc = event.getUnicodeChar();
			sendKey(true, mapKey(keyCode, uc), uc);
			return true;
		}

		@Override
		public boolean onKeyUp(int keyCode, KeyEvent event)
		{
			int uc = event.getUnicodeChar();
			sendKey(false, mapKey(keyCode, uc), uc);
			return true;
		}

		public void onAccuracyChanged(Sensor sensor, int accuracy)
		{
		}

		public void onSensorChanged(final SensorEvent event)
		{
			acc_x = event.values[0];
			acc_y = event.values[1];
			acc_z = event.values[2];
		}
	}
	
	private boolean runningintheemulator()
	{
		android.util.Log.i("FTEDroid", "model: " + android.os.Build.MODEL + " product: " + android.os.Build.PRODUCT + " device: " + android.os.Build.DEVICE);
		return android.os.Build.MODEL.equals("sdk") && android.os.Build.PRODUCT.equals("sdk") && android.os.Build.DEVICE.equals("generic");
	}
	
	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		android.util.Log.i("FTEDroid", "onCreate");
		//go full-screen		
		getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN);    	
		requestWindowFeature(Window.FEATURE_NO_TITLE);
		
		super.onCreate(savedInstanceState);
		
		android.util.Log.i("FTEDroid", "create view");
		view = new FTEView(this);
		setContentView(view);
	//	setContentView(R.layout.main);
				

		if (runningintheemulator())
		{
			android.util.Log.i("FTEDroid", "emulator detected - skipping sensors to avoid emulator bugs");
			sensorman = null;
		}
		else
		{
			android.util.Log.i("FTEDroid", "init sensor manager");
			sensorman = (SensorManager)getSystemService(SENSOR_SERVICE);
		}
		if (sensorman != null)
		{
			android.util.Log.i("FTEDroid", "init accelerometer");
			sensoracc = sensorman.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);
		}
		android.util.Log.i("FTEDroid", "done");
	}

	@Override
	protected void onResume()
	{
		super.onResume();
		if (sensorman != null && sensoracc != null)
			sensorman.registerListener((SensorEventListener)view, sensoracc, SensorManager.SENSOR_DELAY_GAME);

		view.audioResume();
	}

	@Override
	protected void onStop()
	{
		if (sensorman != null && sensoracc != null)
			sensorman.unregisterListener(view);
		view.audioStop();
		super.onStop();
	}

	@Override
	protected void onPause()
	{
		if (sensorman != null && sensoracc != null)
			sensorman.unregisterListener(view);
		view.audioStop();
		super.onPause();
	}
}
