package com.fteqw;

import javax.microedition.khronos.egl.EGLConfig;
//import javax.microedition.khronos.egl.EGL10;
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

public class FTEDroidActivity extends Activity
{
	private SensorManager sensorman;
	private Sensor sensoracc;
	private FTEView view;
	float acc_x, acc_y, acc_z; /*might be some minor race condition on these*/

	private class FTERenderer implements GLSurfaceView.Renderer 
	{
		private boolean inited;
		private String basedir, userdir;
		FTEDroidActivity act;
		
		FTERenderer(Context ctx, FTEDroidActivity parent)
		{
			act = parent;
			try
			{
			   android.content.pm.PackageInfo info = ctx.getPackageManager().getPackageInfo("com.fteqw", 0);
			   basedir = info.applicationInfo.sourceDir;
			}
			catch(android.content.pm.PackageManager.NameNotFoundException e)
			{
				/*oh well, can just use the homedir instead*/
			}
//			try
//			{
				userdir = Environment.getExternalStorageDirectory().getPath();
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
				FTEDroidEngine.frame(act.acc_x, act.acc_y, act.acc_z);
			}
		}
		@Override
		public void onSurfaceChanged(GL10 gl, int width, int height)
		{
			android.util.Log.i("FTEDroid", "Surface changed, now " + width + " by " + height + ".");
			FTEDroidEngine.init(width, height, basedir, userdir);
			inited = true;
		}
		@Override
		public void onSurfaceCreated(GL10 gl, EGLConfig config)
		{
		}
	}
/*
	private class FTEEGLConfig implements GLSurfaceView.EGLConfigChooser
	{
		@Override
		public EGLConfig chooseConfig (javax.microedition.khronos.egl.EGL10 egl, javax.microedition.khronos.egl.EGLDisplay display)
		{
			int EGL_CONTEXT_CLIENT_VERSION = 0x3098;
			EGLConfig[] cfg = new EGLConfig[1];
			int[] num_configs = {0};
			int[] attribs =
				{
					EGL_CONTEXT_CLIENT_VERSION,	2,
					egl.EGL_SURFACE_TYPE,		egl.EGL_WINDOW_BIT,
					egl.EGL_BLUE_SIZE,		5,
					egl.EGL_GREEN_SIZE,		6,
					egl.EGL_RED_SIZE,			5,
					egl.EGL_DEPTH_SIZE,		16,
					egl.EGL_STENCIL_SIZE, 		8,
					egl.EGL_NONE,			egl.EGL_NONE
				};

			egl.eglChooseConfig(display, attribs, cfg, 1, num_configs);
			return cfg[0];
		}
	}
*/
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
					FTEDroidEngine.motion(0, event.getPointerId(i), event.getX(i), event.getY(i));
			}
			
			public boolean go(MotionEvent event)
			{
				int id;
				float x, y;
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
					FTEDroidEngine.motion(1, id, x, y);
					break;
				case MotionEvent.ACTION_UP:
				case MotionEvent.ACTION_POINTER_UP:
					id = ((act&event.ACTION_POINTER_ID_MASK) >> event.ACTION_POINTER_ID_SHIFT);
					x = event.getX(id);
					y = event.getY(id);
					id = event.getPointerId(id);
					FTEDroidEngine.motion(2, id, x, y);
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

				FTEDroidEngine.motion(0, 0, x, y);

				switch(act)
				{
				case MotionEvent.ACTION_DOWN:
					FTEDroidEngine.motion(1, 0, x, y);
					break;
				case MotionEvent.ACTION_UP:
					FTEDroidEngine.motion(2, 0, x, y);
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

			rndr = new FTERenderer(context, context);
//			setEGLConfigChooser(new FTEEGLConfig());
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
		private static final int K_UPARROW = 132;
		private static final int K_DOWNARROW = 133;
		private static final int K_LEFTARROW = 134;
		private static final int K_RIGHTARROW = 135;
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
				return '\r';
			case KeyEvent.KEYCODE_BACK:
				return 27;
			case KeyEvent.KEYCODE_MENU:
				return 241;
			case KeyEvent.KEYCODE_DEL:
				return 127;
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
			android.util.Log.i("FTEDroid", "emulator detected - skipping sensors to avoid emulator hangs");
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
