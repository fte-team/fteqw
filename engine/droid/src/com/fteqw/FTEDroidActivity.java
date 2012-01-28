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

	private class FTERenderer implements GLSurfaceView.Renderer 
	{
		private boolean inited;
		private String basedir;
		
		FTERenderer(Context ctx)
		{
			try
			{
			   android.content.pm.PackageInfo info = ctx.getPackageManager().getPackageInfo("com.fteqw", 0);
			   basedir = info.applicationInfo.sourceDir;
			}
			catch(android.content.pm.PackageManager.NameNotFoundException e)
			{
				/*oh well, can just use the homedir instead*/
			}
		}
		
		@Override
		public void onDrawFrame(GL10 gl)
		{
			if (inited == true)
			{
				FTEDroidEngine.frame();
			}
		}
		@Override
		public void onSurfaceChanged(GL10 gl, int width, int height)
		{
			FTEDroidEngine.init(width, height, basedir);
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
		
		private byte[] audbuf;
		private AudioTrack at;

		private void audioInit()
		{
			final int notifframes = 2048;
			if (at != null)
				at.stop();
			int sspeed = 11025;
			int speakers = 1;
			int sz = 4*AudioTrack.getMinBufferSize(sspeed, ((speakers==2)?AudioFormat.CHANNEL_CONFIGURATION_STEREO:AudioFormat.CHANNEL_CONFIGURATION_MONO), AudioFormat.ENCODING_PCM_16BIT);
			if (sz < notifframes*2)
				sz = notifframes*2;

			at = new AudioTrack(AudioManager.STREAM_MUSIC, sspeed, ((speakers==2)?AudioFormat.CHANNEL_CONFIGURATION_STEREO:AudioFormat.CHANNEL_CONFIGURATION_MONO), AudioFormat.ENCODING_PCM_16BIT, sz, AudioTrack.MODE_STREAM);
			final int framesz = 2; /*mono 16bit*/
			audbuf = new byte[notifframes*framesz];

			at.setPlaybackPositionUpdateListener(new AudioTrack.OnPlaybackPositionUpdateListener()
			{
				@Override
				public void onMarkerReached(AudioTrack track)
				{
				}
				@Override
				public void onPeriodicNotification(AudioTrack track)
				{
					int avail = FTEDroidEngine.paintaudio(audbuf, audbuf.length);
					at.write(audbuf, 0, notifframes*framesz);
				}
			});
			at.setPositionNotificationPeriod(notifframes);

			at.setStereoVolume(1, 1);
			
			at.play();
			/*buffer needs to be completely full before it'll start playing*/
			while(sz > 0)
			{
				at.write(audbuf, 0, notifframes*framesz);
				sz -= notifframes;
			}
		}
		public void resume()
		{
			/*poke audio into submission*/
			if (at != null)
				at.play();
		}

		public FTEView(Context context)
		{
			super(context);

			rndr = new FTERenderer(getContext());
//			setEGLConfigChooser(new FTEEGLConfig());
			setRenderer(rndr);
			setFocusable(true);
			setFocusableInTouchMode(true);

			audioInit();
		}
		
		private void sendKey(final boolean presseddown, final int qcode, final int unicode)
		{
			queueEvent(new Runnable()
			{
				public void run()
				{
					FTEDroidEngine.keypress(presseddown?1:0, qcode, unicode);
				}
			});
		}
		private void sendAccelerometer(final float x, final float y, final float z)
		{
			queueEvent(new Runnable()
			{
				public void run()
				{
					FTEDroidEngine.accelerometer(x, y, z);
				}
			});
		}
		@Override
		public boolean onTouchEvent(MotionEvent event)
		{
			final int act = event.getAction();
			final float x = event.getX();
			final float y = event.getY();
			//float p = event.getPressure();
			
			queueEvent(new Runnable()
			{
				public void run()
				{
					switch(act)
					{
					case MotionEvent.ACTION_DOWN:
						FTEDroidEngine.motion(1, x, y);
						break;
					case MotionEvent.ACTION_UP:
						FTEDroidEngine.motion(2, x, y);
						break;
					case MotionEvent.ACTION_MOVE:
						FTEDroidEngine.motion(0, x, y);
						break;
					}
				}
			});
			return true;
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

		private float gx,gy,gz;
		public void onSensorChanged(final SensorEvent event)
		{
			// alpha is calculated as t / (t + dT)
			// with t, the low-pass filter's time-constant
			// and dT, the event delivery rate

			final float alpha = 0.8f;

			gx = alpha * gx + (1 - alpha) * event.values[0];
			gy = alpha * gy + (1 - alpha) * event.values[1];
			gz = alpha * gz + (1 - alpha) * event.values[2];

			sendAccelerometer(event.values[0] - gx, event.values[1] - gy, event.values[2] - gz);

		}
	}
	
	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		//go full-screen
		getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN);    	
		requestWindowFeature(Window.FEATURE_NO_TITLE);
		
		super.onCreate(savedInstanceState);

		view = new FTEView(this);
		setContentView(view);
	//	setContentView(R.layout.main);
				

		sensorman = (SensorManager)getSystemService(SENSOR_SERVICE);
		sensoracc = sensorman.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);
	}

	@Override
	protected void onResume()
	{
		super.onResume();
		sensorman.registerListener((SensorEventListener)view, sensoracc, SensorManager.SENSOR_DELAY_GAME);

		view.resume();
	}

	@Override
	protected void onStop()
	{
		sensorman.unregisterListener(view);
		super.onStop();
	}

	@Override
	protected void onPause()
	{
		sensorman.unregisterListener(view);
		super.onPause();
	}
}
