package com.fteqw;

import javax.microedition.khronos.egl.EGLConfig;
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

public class FTEDroidActivity extends Activity
{
	private SensorManager sensorman;
	private Sensor sensoracc;
	private FTEView view;

	private class FTERenderer implements GLSurfaceView.Renderer 
	{
		private boolean inited;
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
			FTEDroidEngine.init(width, height);
			inited = true;
		}
		@Override
		public void onSurfaceCreated(GL10 gl, EGLConfig config)
		{
		}
	}
	private class FTEView extends GLSurfaceView implements SensorEventListener
	{
		private final FTERenderer rndr;
		
		public FTEView(Context context)
		{
			super(context);

			rndr = new FTERenderer();
			setRenderer(rndr);
			setFocusable(true);
			setFocusableInTouchMode(true);
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
