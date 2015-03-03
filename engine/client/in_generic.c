//Generic input code.
//mostly mouse support, but can also handle a few keyboard events.

#include "quakedef.h"

extern qboolean mouse_active;

static cvar_t m_filter = CVARF("m_filter", "0", CVAR_ARCHIVE);
static cvar_t m_accel = CVARF("m_accel", "0", CVAR_ARCHIVE);
static cvar_t m_forcewheel = CVARD("m_forcewheel", "1", "0: ignore mousewheels in apis where it is abiguous.\n1: Use mousewheel when it is treated as a third axis. Motion above a threshold is ignored, to avoid issues with an unknown threshold.\n2: Like 1, but excess motion is retained. The threshold specifies exact z-axis distance per notice.");
static cvar_t m_forcewheel_threshold = CVARD("m_forcewheel_threshold", "32", "Mousewheel graduations smaller than this will not trigger mousewheel deltas.");
static cvar_t m_strafeonright = CVARFD("m_strafeonright", "1", CVAR_ARCHIVE, "If 1, touching the right half of the touchscreen will strafe/move, while the left side will turn.");
static cvar_t m_fatpressthreshold = CVARFD("m_fatpressthreshold", "0.2", CVAR_ARCHIVE, "How fat your thumb has to be to register a fat press (touchscreens).");
static cvar_t m_touchmajoraxis = CVARFD("m_touchmajoraxis", "1", CVAR_ARCHIVE, "When using a touchscreen, use only the major axis for strafing.");
static cvar_t m_slidethreshold = CVARFD("m_slidethreshold", "10", CVAR_ARCHIVE, "How far your finger needs to move to be considered a slide event (touchscreens).");

extern cvar_t _windowed_mouse;


#define EVENTQUEUELENGTH 128
struct eventlist_s
{
	enum
	{
		IEV_KEYDOWN,
		IEV_KEYRELEASE,
		IEV_MOUSEABS,
		IEV_MOUSEDELTA,
		IEV_JOYAXIS
	} type;
	int devid;

	union
	{
		struct
		{
			float x, y, z;
			float tsize;	//the size of the touch
		} mouse;
		struct
		{
			int scancode, unicode;
		} keyboard;
		struct
		{
			int axis;
			float value;
		} joy;
	};
} eventlist[EVENTQUEUELENGTH];
volatile int events_avail; /*volatile to make sure the cc doesn't try leaving these cached in a register*/
volatile int events_used;

static struct eventlist_s *in_newevent(void)
{
	if (events_avail >= events_used + EVENTQUEUELENGTH)
		return NULL;
	return &eventlist[events_avail & (EVENTQUEUELENGTH-1)];
}

static void in_finishevent(void)
{
	events_avail++;
}

#define MAXPOINTERS 8
struct mouse_s
{
	enum
	{
		M_INVALID,
		M_MOUSE,	//using deltas
		M_TOUCH		//using absolutes
	} type;
	int qdeviceid;	//so we can just use pointers.
	vec2_t oldpos;
	vec2_t downpos;
	float moveddist;	//how far it has moved while held. this provides us with our emulated mouse1 when they release the press
	vec2_t delta;		//how far its moved recently
	vec2_t old_delta;	//how far its moved previously, for mouse smoothing
	float wheeldelta;
	int down;
} ptr[MAXPOINTERS];
int touchcursor;	//the cursor follows whichever finger was most recently pressed in preference to any mouse also on the same system

#define MAXJOYAXIS 6
#define MAXJOYSTICKS 4
struct joy_s
{
	int qdeviceid;
	int axis[MAXJOYAXIS];
} joy[MAXJOYSTICKS];

void IN_Shutdown(void)
{
	INS_Shutdown();
}

void IN_ReInit(void)
{
	int i;

	for (i = 0; i < MAXPOINTERS; i++)
	{
		ptr[i].type = M_INVALID;
		ptr[i].qdeviceid = i;
	}

	for (i = 0; i < MAXJOYSTICKS; i++)
	{
		joy[i].qdeviceid = i;
	}

	INS_ReInit();
}

void IN_Init(void)
{
	events_avail = 0;
	events_used = 0;

	Cvar_Register (&m_filter, "input controls");
	Cvar_Register (&m_accel, "input controls");
	Cvar_Register (&m_forcewheel, "Input Controls");
	Cvar_Register (&m_forcewheel_threshold, "Input Controls");
	Cvar_Register (&m_strafeonright, "input controls");
	Cvar_Register (&m_fatpressthreshold, "input controls");
	Cvar_Register (&m_slidethreshold, "input controls");
	Cvar_Register (&m_touchmajoraxis, "input controls");

	INS_Init();
}

//tells the keys.c code whether the cursor is currently active, causing mouse clicks instead of binds.
qboolean IN_MouseDevIsTouch(int devid)
{
	if (devid < MAXPOINTERS)
		return ptr[devid].type == M_TOUCH;
	return false;
}
//there was no ui to click on at least...
//translates MOUSE1 press events into begin-look-or-strafe events.
//translates to MOUSE2 accordingly
//returns 0 if it ate it completely.
int IN_TranslateMButtonPress(int devid)
{
	int ret;
	if (!ptr[devid].down)
	{
		//set the cursor-pressed state, so we begin to look/strafe around
		ptr[devid].down = 1;
		ptr[devid].moveddist = 0;
		ptr[devid].downpos[0] = ptr[devid].oldpos[0];
		ptr[devid].downpos[1] = ptr[devid].oldpos[1];
		ptr[devid].delta[0] = 0;
		ptr[devid].delta[1] = 0;
		ret = 0;	//eat it
	}
	else
	{
		//this is the key binding that the press should use
		ret = (m_strafeonright.ival && ptr[devid].downpos[0] > vid.pixelwidth/2)?K_MOUSE2:K_MOUSE1;
	}

	return ret;
}

/*a 'pointer' is either a multitouch pointer, or a separate device
note that mice use the keyboard button api, but separate devices*/
void IN_Commands(void)
{
	struct eventlist_s *ev;

	INS_Commands();

	while (events_used != events_avail)
	{
		ev = &eventlist[events_used & (EVENTQUEUELENGTH-1)];

		switch(ev->type)
		{
		case IEV_KEYDOWN:
		case IEV_KEYRELEASE:
			//on touchscreens, mouse1 is used as up/down state. we have to emulate actual mouse clicks based upon distance moved, so we can get movement events.
			if (ev->keyboard.scancode == K_MOUSE1 && ev->devid < MAXPOINTERS && (ptr[ev->devid].type == M_TOUCH))
			{
				if (ev->type == IEV_KEYDOWN)
				{
					float fl;
					touchcursor = ev->devid;
					fl = ptr[ev->devid].oldpos[0] * vid.width / vid.pixelwidth;
					mousecursor_x = bound(0, fl, vid.width-1);
					fl = ptr[ev->devid].oldpos[1] * vid.height / vid.pixelheight;
					mousecursor_y = bound(0, fl, vid.height-1);
				}
				else if (touchcursor == ev->devid)
					touchcursor = 0;	//revert it to the mouse, or whatever device was 0.

				if (Key_MouseShouldBeFree())
					ptr[ev->devid].down = 0;
				else if (ptr[ev->devid].down)
				{
					if (ev->type == IEV_KEYDOWN)
						Key_Event(ev->devid, ev->keyboard.scancode, ev->keyboard.unicode, ev->type == IEV_KEYDOWN); 
					else
					{
						if (ptr[ev->devid].down == 1 && ptr[ev->devid].moveddist < m_slidethreshold.value)
						{
							ptr[ev->devid].down = 2;
							Key_Event(ev->devid, K_MOUSE1, 0, true);
						}
						Key_Event(ev->devid, K_MOUSE1, 0, false);
						ptr[ev->devid].down = 0;
					}
					break;
				}
			}
			Key_Event(ev->devid, ev->keyboard.scancode, ev->keyboard.unicode, ev->type == IEV_KEYDOWN); 
			break;
		case IEV_JOYAXIS:
			if (ev->devid < MAXJOYSTICKS && ev->joy.axis < MAXJOYAXIS)
			{
#ifdef CSQC_DAT
				if (CSQC_JoystickAxis(ev->joy.axis, ev->joy.value, ev->devid))
					joy[ev->devid].axis[ev->joy.axis] = 0;
				else
#endif
					joy[ev->devid].axis[ev->joy.axis] = ev->joy.value;
			}
			break;
		case IEV_MOUSEDELTA:
			if (ev->devid < MAXPOINTERS)
			{
				if (ptr[ev->devid].type != M_MOUSE)
				{
					ptr[ev->devid].type = M_MOUSE;
				}
				ptr[ev->devid].delta[0] += ev->mouse.x;
				ptr[ev->devid].delta[1] += ev->mouse.y;

				if (m_forcewheel.value >= 2)
					ptr[ev->devid].wheeldelta -= ev->mouse.z;
				else if (m_forcewheel.value)
				{
					int mfwt = (int)m_forcewheel_threshold.value;

					if (ev->mouse.z > mfwt)
						ptr[ev->devid].wheeldelta -= mfwt;
					else if (ev->mouse.z < -mfwt)
						ptr[ev->devid].wheeldelta += mfwt;
				}
			}
			break;
		case IEV_MOUSEABS:
			/*mouse cursors only really work with one pointer*/
			if (ev->devid == touchcursor)
			{
				float fl;
				fl = ev->mouse.x * vid.width / vid.pixelwidth;
				mousecursor_x = bound(0, fl, vid.width-1);
				fl = ev->mouse.y * vid.height / vid.pixelheight;
				mousecursor_y = bound(0, fl, vid.height-1);
			}

			if (ev->devid < MAXPOINTERS)
			{
				if (ptr[ev->devid].type != M_TOUCH)
				{
					//if its now become an absolute device, clear stuff so we don't get confused.
					ptr[ev->devid].type = M_TOUCH;
					ptr[ev->devid].down = 0;
					ptr[ev->devid].moveddist = 0;
					ptr[ev->devid].oldpos[0] = ev->mouse.x;
					ptr[ev->devid].oldpos[1] = ev->mouse.y;
				}

				if (ptr[ev->devid].down)
				{
					ptr[ev->devid].delta[0] += ev->mouse.x - ptr[ev->devid].oldpos[0];
					ptr[ev->devid].delta[1] += ev->mouse.y - ptr[ev->devid].oldpos[1];
		
					ptr[ev->devid].moveddist += fabs(ev->mouse.x - ptr[ev->devid].oldpos[0]) + fabs(ev->mouse.y - ptr[ev->devid].oldpos[1]);
				}

				ptr[ev->devid].oldpos[0] = ev->mouse.x;
				ptr[ev->devid].oldpos[1] = ev->mouse.y;


				if (ptr[ev->devid].down > 1 && ev->mouse.tsize < m_fatpressthreshold.value)
				{
					ptr[ev->devid].down = 1;
					Key_Event(ev->devid, K_MOUSE1, 0, false);
				}
				if (ptr[ev->devid].down == 1 && ev->mouse.tsize > m_fatpressthreshold.value)
				{
					ptr[ev->devid].down = 2;
					Key_Event(ev->devid, K_MOUSE1, 0, true);
				}
			}
			break;
		}
		events_used++;
	}
}

void IN_MoveMouse(struct mouse_s *mouse, float *movements, int pnum)
{
	int mx, my;
	double mouse_x, mouse_y, mouse_deltadist;
	int mfwt;
	qboolean strafe_x, strafe_y;
	int wpnum;

	//small performance boost
	if (mouse->type == M_INVALID)
		return;

	/*each device will be processed when its player comes to be processed*/
	wpnum = cl.splitclients;
	if (wpnum < 1)
		wpnum = 1;
	if (cl_forceseat.ival)
		wpnum = (cl_forceseat.ival-1) % wpnum;
	else
		wpnum = mouse->qdeviceid % wpnum;
	if (wpnum != pnum)
		return;

	if (m_forcewheel.value)
	{
		mfwt = m_forcewheel_threshold.ival;
		if (mfwt)
		{
			while(mouse->wheeldelta <= -mfwt)
			{
				Key_Event (mouse->qdeviceid, K_MWHEELUP, 0, true);
				Key_Event (mouse->qdeviceid, K_MWHEELUP, 0, false);
				mouse->wheeldelta += mfwt;
			}

			while(mouse->wheeldelta >= mfwt)
			{
				Key_Event (mouse->qdeviceid, K_MWHEELDOWN, 0, true);
				Key_Event (mouse->qdeviceid, K_MWHEELDOWN, 0, false);
				mouse->wheeldelta -= mfwt;
			}
		}

		if (m_forcewheel.value < 2)
			mouse->wheeldelta = 0;
	}

	mx = mouse->delta[0];
	mouse->delta[0]=0;
	my = mouse->delta[1];
	mouse->delta[1]=0;


	if(in_xflip.value) mx *= -1;

	mousemove_x += mx;
	mousemove_y += my;

	if (Key_MouseShouldBeFree())
	{
		if ((mx || my) && mouse->type != M_TOUCH)
		{
			mousecursor_x += mx;
			mousecursor_y += my;

			if (mousecursor_y<0)
				mousecursor_y=0;
			if (mousecursor_x<0)
				mousecursor_x=0;

			if (mousecursor_x >= vid.width)
				mousecursor_x = vid.width - 1;

			if (mousecursor_y >= vid.height)
				mousecursor_y = vid.height - 1;
			mx=my=0;
		}
	}
	else
	{
		if (mouse->type != M_TOUCH)
		{
			mousecursor_x += mx;
			mousecursor_y += my;
		}
#ifdef VM_UI
		if (UI_MousePosition(mx, my))
		{
			mx = 0;
			my = 0;
		}
#endif
	}

	if (mouse->type == M_TOUCH)
	{
		if (m_strafeonright.ival && mouse->downpos[0] > vid.pixelwidth/2 && movements != NULL && !Key_Dest_Has(~kdm_game))
		{
			//if they're strafing, calculate the speed to move at based upon their displacement
			if (mouse->down)
			{
				mx = mouse->oldpos[0] - (vid.pixelwidth*3)/4;
				my = mouse->oldpos[1] - (vid.pixelheight*3)/4;

				//mx = (mouse->oldpos[0] - mouse->downpos[0])*0.1;
				//my = (mouse->oldpos[1] - mouse->downpos[1])*0.1;
			}
			else
			{
				mx = 0;
				my = 0;
			}

			if (m_touchmajoraxis.ival)
			{
				//major axis only
				if (fabs(mx) > fabs(my))
					my = 0;
				else
					mx = 0;
			}

			strafe_x = true;
			strafe_y = true;
		}
		else
		{
			strafe_x = false;
			strafe_y = false;

			//boost sensitivity so that the default works okay.
			mx *= 1.75;
			my *= 1.75;
		}
	}
	else
	{
		strafe_x = (in_strafe.state[pnum] & 1) || (lookstrafe.value && (in_mlook.state[pnum] & 1) );
		strafe_y = !((in_mlook.state[pnum] & 1) && !(in_strafe.state[pnum] & 1));
	}

#ifdef PEXT_CSQC
	if (mouse->type == M_TOUCH)
	{
		if (CSQC_MousePosition(mouse->oldpos[0], mouse->oldpos[1], mouse->qdeviceid))
		{
			mx = 0;
			my = 0;
		}
	}
	else
	{
		if (mx || my)
		if (CSQC_MouseMove(mx, my, mouse->qdeviceid))
		{
			mx = 0;
			my = 0;
		}

		//if game is not focused, kill any mouse look
		if (Key_Dest_Has(~kdm_game))
		{
			mx = 0;
			my = 0;
		}
	}
#endif

	if (m_filter.value)
	{
		double fraction = bound(0, m_filter.value, 2) * 0.5;
		mouse_x = (mx*(1-fraction) + mouse->old_delta[0]*fraction);
		mouse_y = (my*(1-fraction) + mouse->old_delta[1]*fraction);
	}
	else
	{
		mouse_x = mx;
		mouse_y = my;
	}

	mouse->old_delta[0] = mx;
	mouse->old_delta[1] = my;

	if (m_accel.value)
	{
		mouse_deltadist = sqrt(mx*mx + my*my);
		mouse_x *= (mouse_deltadist*m_accel.value + sensitivity.value*in_sensitivityscale);
		mouse_y *= (mouse_deltadist*m_accel.value + sensitivity.value*in_sensitivityscale);
	}
	else
	{
		mouse_x *= sensitivity.value*in_sensitivityscale;
		mouse_y *= sensitivity.value*in_sensitivityscale;
	}

	if (cl.playerview[pnum].stats[STAT_VIEWZOOM])
	{
		mouse_x *= cl.playerview[pnum].stats[STAT_VIEWZOOM]/255.0f;
		mouse_y *= cl.playerview[pnum].stats[STAT_VIEWZOOM]/255.0f;
	}

	if (!movements)
	{
		return;
	}

// add mouse X/Y movement to cmd
	if (strafe_x)
		movements[1] += m_side.value * mouse_x;
	else
	{
//		if ((int)((cl.viewangles[pnum][PITCH]+89.99)/180) & 1)
//			mouse_x *= -1;
		cl.playerview[pnum].viewanglechange[YAW] -= m_yaw.value * mouse_x;
	}

	if (in_mlook.state[pnum] & 1)
		V_StopPitchDrift (&cl.playerview[pnum]);

	if (!strafe_y)
	{
		cl.playerview[pnum].viewanglechange[PITCH] += m_pitch.value * mouse_y;
	}
	else
	{
		if ((in_strafe.state[pnum] & 1) && noclip_anglehack)
			movements[2] -= m_forward.value * mouse_y;
		else
			movements[0] -= m_forward.value * mouse_y;
	}
}

static float joydeadzone(float mag, float deadzone)
{
	if (mag > 1)	//erg?
		mag = 1;
	if (mag > deadzone)
	{
		mag -= deadzone;
		mag = mag / (1.f-deadzone);
	}
	else
		mag = 0;
	return mag;
}

void IN_MoveJoystick(struct joy_s *joy, float *movements, int pnum, float frametime)
{
	float mag;
	vec3_t jlook, jstrafe;

	int wpnum;

	/*each device will be processed when its player comes to be processed*/
	wpnum = cl.splitclients;
	if (wpnum < 1)
		wpnum = 1;
	if (cl_forceseat.ival)
		wpnum = (cl_forceseat.ival-1) % wpnum;
	else
		wpnum = joy->qdeviceid % wpnum;
	if (wpnum != pnum)
		return;

	jlook[0] = joy->axis[0];
	jlook[1] = joy->axis[1];
	jlook[2] = joy->axis[2];

	jstrafe[0] = joy->axis[3];
	jstrafe[1] = joy->axis[4];
	jstrafe[2] = joy->axis[5];

	//uses a radial deadzone for x+y axis, and separate out the z axis, just because most controllers are 2d affairs with any 3rd axis being a separate knob.
	//deadzone values are stolen from microsoft's xinput documentation. they seem quite large to me - I guess that means that xbox controllers are just dodgy imprecise crap with excessive amounts of friction and finger grease.
	mag = joydeadzone(sqrt(jlook[0]*jlook[0] + jlook[1]*jlook[1]), 0.239);
	mag = pow(mag, 2);
	jlook[0] *= mag;
	jlook[1] *= mag;
	mag = joydeadzone(fabs(jlook[2]), 0.00092);
	jlook[2] *= mag;

	mag = joydeadzone(sqrt(jstrafe[0]*jstrafe[0] + jstrafe[1]*jstrafe[1]), 0.265);
	mag = pow(mag, 2);
	jstrafe[0] *= mag;
	jstrafe[1] *= mag;
	mag = joydeadzone(fabs(jstrafe[2]), 0.00092);
	jstrafe[2] *= mag;

	if (Key_Dest_Has(~kdm_game))
	{
		VectorClear(jlook);
		VectorClear(jstrafe);
	}

	VectorScale(jlook, frametime, jlook);
	VectorScale(jstrafe, frametime, jstrafe);

	if (!movements)	//if this is null, gamecode should still get inputs, just no camera looking or anything.
		return;

	//angle changes
	cl.playerview[pnum].viewanglechange[YAW] -= m_yaw.value * jlook[0];
	cl.playerview[pnum].viewanglechange[PITCH] += m_pitch.value * jlook[1];
//	cl.playerview[pnum].viewanglechange[ROLL] += m_roll.value * jlook[2];	//this would be too weird.

	if (in_mlook.state[pnum] & 1)
		V_StopPitchDrift (&cl.playerview[pnum]);

	//movement
	movements[1] += m_side.value * jstrafe[0];		//x=right=1
	movements[0] -= m_forward.value * jstrafe[1];	//y=forward=0
	movements[2] += m_side.value * jstrafe[2];		//z=up=2
}

void IN_Move (float *movements, int pnum, float frametime)
{
	int i;
	INS_Move(movements, pnum);
	for (i = 0; i < MAXPOINTERS; i++)
		IN_MoveMouse(&ptr[i], movements, pnum);

	for (i = 0; i < MAXJOYSTICKS; i++)
		IN_MoveJoystick(&joy[i], movements, pnum, frametime);
}

void IN_JoystickAxisEvent(int devid, int axis, float value)
{
	struct eventlist_s *ev = in_newevent();
	if (!ev)	
		return;
	ev->type = IEV_JOYAXIS;
	ev->devid = devid;
	ev->joy.axis = axis;
	ev->joy.value = value;
	in_finishevent();
}

void IN_KeyEvent(int devid, int down, int keycode, int unicode)
{
	struct eventlist_s *ev = in_newevent();
	if (!ev)	
		return;
	ev->type = down?IEV_KEYDOWN:IEV_KEYRELEASE;
	ev->devid = devid;
	ev->keyboard.scancode = keycode;
	ev->keyboard.unicode = unicode;
	in_finishevent();
}

/*
devid is the mouse device id. generally idependant from keyboards.
for multitouch, devid might be the touch identifier, which will persist until released.
x is horizontal, y is vertical.
z is height... generally its used as a mousewheel instead, but there are some '3d' mice out there, so its provided in this api.
*/
void IN_MouseMove(int devid, int abs, float x, float y, float z, float size)
{
	struct eventlist_s *ev = in_newevent();
	if (!ev)
		return;
	ev->devid = devid;
	ev->type = abs?IEV_MOUSEABS:IEV_MOUSEDELTA;
	ev->mouse.x = x;
	ev->mouse.y = y;
	ev->mouse.z = z;
	ev->mouse.tsize = size;
	in_finishevent();
}
