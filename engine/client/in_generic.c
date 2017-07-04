//Generic input code.
//mostly mouse support, but can also handle a few keyboard events.

#include "quakedef.h"

extern qboolean mouse_active;

static cvar_t m_filter = CVARF("m_filter", "0", CVAR_ARCHIVE);
static cvar_t m_forcewheel = CVARD("m_forcewheel", "1", "0: ignore mousewheels in apis where it is abiguous.\n1: Use mousewheel when it is treated as a third axis. Motion above a threshold is ignored, to avoid issues with an unknown threshold.\n2: Like 1, but excess motion is retained. The threshold specifies exact z-axis distance per notice.");
static cvar_t m_forcewheel_threshold = CVARD("m_forcewheel_threshold", "32", "Mousewheel graduations smaller than this will not trigger mousewheel deltas.");
static cvar_t m_strafeonright = CVARFD("m_strafeonright", "1", CVAR_ARCHIVE, "If 1, touching the right half of the touchscreen will strafe/move, while the left side will turn.");
static cvar_t m_fatpressthreshold = CVARFD("m_fatpressthreshold", "0.2", CVAR_ARCHIVE, "How fat your thumb has to be to register a fat press (touchscreens).");
static cvar_t m_touchmajoraxis = CVARFD("m_touchmajoraxis", "1", CVAR_ARCHIVE, "When using a touchscreen, use only the major axis for strafing.");
static cvar_t m_slidethreshold = CVARFD("m_slidethreshold", "10", CVAR_ARCHIVE, "How far your finger needs to move to be considered a slide event (touchscreens).");

static cvar_t m_accel			= CVARAFD("m_accel",		"0",	"cl_mouseAccel", CVAR_ARCHIVE, "Values >0 will amplify mouse movement proportional to velocity. Small values have great effect. A lot of good Quake Live players use around the 0.1-0.2 mark, but this depends on your mouse CPI and polling rate.");
static cvar_t m_accel_style		= CVARAD("m_accel_style",	"1",	"cl_mouseAccelStyle",	"1 = Quake Live mouse acceleration, 0 = Old style accelertion.");
static cvar_t m_accel_power		= CVARAD("m_accel_power",	"2",	"cl_mouseAccelPower",	"Used when m_accel_style is 1.\nValues 1 or below are dumb. 2 is linear and the default. 99% of accel users use this. Above 2 begins to amplify exponentially and you will get more acceleration at higher velocities. Great if you want low accel for slow movements, and high accel for fast movements. Good in combination with a sensitivity cap.");
static cvar_t m_accel_offset	= CVARAD("m_accel_offset",	"0",	"cl_mouseAccelOffset",	"Used when m_accel_style is 1.\nAcceleration will not be active until the mouse movement exceeds this speed (counts per millisecond). Negative values are supported, which has the effect of causing higher rates of acceleration to happen at lower velocities.");
static cvar_t m_accel_senscap	= CVARAD("m_accel_senscap",	"0",	"cl_mouseSensCap",		"Used when m_accel_style is 1.\nSets an upper limit on the amplified mouse movement. Great for tuning acceleration around lower velocities while still remaining in control of fast motion such as flicking.");

void QDECL joyaxiscallback(cvar_t *var, char *oldvalue)
{
	int sign;
	char *end;
	strtol(var->string, &end, 0);
	if (!*end)	//okay, its missing or an actual number.
		return;

	end = var->string;
	if (*end == '-')
	{
		end++;
		sign = -1;
	}
	else if (*end == '+')
	{
		end++;
		sign = 1;
	}
	else
		sign = 1;
	if (!Q_strcasecmp(end, "forward") || !Q_strcasecmp(end, "moveforward"))
		var->ival = 1*sign;
	else if (!Q_strcasecmp(end, "back") || !Q_strcasecmp(end, "moveback"))
		var->ival = 1*sign*-1;
	else if (!Q_strcasecmp(end, "lookup") || !Q_strcasecmp(end, "pitchup"))
		var->ival = 2*sign;
	else if (!Q_strcasecmp(end, "lookdown") || !Q_strcasecmp(end, "pitchdown"))
		var->ival = 2*sign*-1;
	else if (!Q_strcasecmp(end, "moveright"))
		var->ival = 3*sign;
	else if (!Q_strcasecmp(end, "moveleft"))
		var->ival = 3*sign*-1;
	else if (!Q_strcasecmp(end, "right") || !Q_strcasecmp(end, "turnright"))
		var->ival = 4*sign;
	else if (!Q_strcasecmp(end, "left") || !Q_strcasecmp(end, "turnleft"))
		var->ival = 4*sign*1;
	else if (!Q_strcasecmp(end, "up") || !Q_strcasecmp(end, "moveup"))
		var->ival = 5*sign;
	else if (!Q_strcasecmp(end, "down") || !Q_strcasecmp(end, "movedown"))
		var->ival = 5*sign*-1;
	else if (!Q_strcasecmp(end, "rollright"))
		var->ival = 6*sign;
	else if (!Q_strcasecmp(end, "rollleft"))
		var->ival = 6*sign*-1;
}

static cvar_t	joy_advaxis[6] =
{
#define ADVAXISDESC (const char *)"Provides a way to remap each joystick/controller axis.\nShould be set to one of: moveforward, moveback, lookup, lookdown, turnleft, turnright, moveleft, moveright, moveup, movedown, rollleft, rollright"
	CVARCD("joyadvaxisx", "turnright", joyaxiscallback, ADVAXISDESC),
	CVARCD("joyadvaxisy", "lookup", joyaxiscallback, ADVAXISDESC),
	CVARCD("joyadvaxisz", "moveup", joyaxiscallback, ADVAXISDESC),
	CVARCD("joyadvaxisr", "moveright", joyaxiscallback, ADVAXISDESC),
	CVARCD("joyadvaxisu", "moveforward", joyaxiscallback, ADVAXISDESC),
	CVARCD("joyadvaxisv", "rollright", joyaxiscallback, ADVAXISDESC)
};
static cvar_t	joy_advaxisscale[6] =
{
	CVARD("joyadvaxisx_scale", "1.0", "Because joyadvaxisx etc can be added together, this provides a way to rescale or invert an individual axis without affecting another with the same action."),
	CVAR("joyadvaxisy_scale", "1.0"),
	CVAR("joyadvaxisz_scale", "1.0"),
	CVAR("joyadvaxisr_scale", "1.0"),
	CVAR("joyadvaxisu_scale", "1.0"),
	CVAR("joyadvaxisv_scale", "1.0")
};
static cvar_t	joy_anglesens[3] =
{
	CVARD("joypitchsensitivity", "1.0", "Scaler value for the controller when it is at its most extreme value"),
	CVAR("joyyawsensitivity", "-1.0"),
	CVAR("joyrollsensitivity", "1.0")
};
static cvar_t	joy_movesens[3] =
{
	CVAR("joyforwardsensitivity", "-1.0"),
	CVAR("joysidesensitivity", "1.0"),
	CVAR("joyupsensitivity", "1.0")
};
//comments on threshholds comes from microsoft's xinput docs.
static cvar_t	joy_anglethreshold[3] =
{
	CVARD("joypitchthreshold", "0.19", "Values reported near the center of the analog joystick/controller are often erroneous and undesired.\nThe joystick threshholds are how much of the total values to ignore."),	//8689/32767 (right thumb)
	CVAR("joyyawthreshold", "0.19"),	//8689/32767 (right thumb)
	CVAR("joyrollthreshold", "0.118"),	//30/255	 (trigger)
};
static cvar_t	joy_movethreshold[3] =
{
	CVAR("joyforwardthreshold", "0.17"),//7849/32767 (left thumb)
	CVAR("joysidethreshold", "0.17"),	//7849/32767 (left thumb)
	CVAR("joyupthreshold", "0.118"),	//30/255	 (trigger)
};

static cvar_t joy_exponent = CVARD("joyexponent", "2", "Scales joystick/controller sensitivity non-linearly to increase precision in the center.\nA value of 1 is linear.");
static cvar_t joy_radialdeadzone = CVARD("joyradialdeadzone", "1", "Treat controller dead zones as a pair, rather than per-axis.");
extern cvar_t _windowed_mouse;


#define EVENTQUEUELENGTH 512
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
	unsigned int devid;

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
	unsigned int qdeviceid;	//so we can just use pointers.
	vec2_t oldpos;
	vec2_t downpos;
	float moveddist;	//how far it has moved while held. this provides us with our emulated mouse1 when they release the press
	vec2_t delta;		//how far its moved recently
	vec2_t old_delta;	//how far its moved previously, for mouse smoothing
	float wheeldelta;
	int down;
	unsigned int updates;	//tracks updates per second
} ptr[MAXPOINTERS];
int touchcursor;	//the cursor follows whichever finger was most recently pressed in preference to any mouse also on the same system

#define MAXJOYAXIS 6
#define MAXJOYSTICKS 8
struct joy_s
{
	unsigned int qdeviceid;
	float axis[MAXJOYAXIS];
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
		memset(&ptr[i], 0, sizeof(ptr[i]));
		ptr[i].type = M_INVALID;
		ptr[i].qdeviceid = i;
	}

	for (i = 0; i < MAXJOYSTICKS; i++)
	{
		memset(&joy[i], 0, sizeof(joy[i]));
		joy[i].qdeviceid = i;
	}

	INS_ReInit();
}

struct remapctx
{
	char *type;
	char *devicename;
	unsigned int newdevid;
	unsigned int found;
	unsigned int failed;
};
static void IN_DeviceIDs_DoRemap(void *vctx, const char *type, const char *devicename, unsigned int *qdevid)
{
	struct remapctx *ctx = vctx;

	if (!strcmp(ctx->type, type))
		if (!strcmp(ctx->devicename, devicename))
		{
			if (qdevid)
				*qdevid = ctx->newdevid;
			else
				ctx->failed = true;
			ctx->found++;
		}
}
void IN_DeviceIDs_Enumerate(void *ctx, const char *type, const char *devicename, unsigned int *qdevid)
{
	char buf[8192];
	devicename = COM_QuotedString(devicename, buf, sizeof(buf), false);
	if (!qdevid)
		Con_Printf("%s\t%s\t%s\n", type, "N/A", devicename);
	else if (*qdevid == DEVID_UNSET)
		Con_Printf("%s\t%s\t%s\n", type, "Unset", devicename);
	else
		Con_Printf("%s\t%u\t%s\n", type, *qdevid, devicename);
}

void IN_DeviceIDs_f(void)
{
	struct remapctx ctx;

	if (Cmd_Argc() > 3)
	{
		ctx.failed = false;
		ctx.found = 0;
		ctx.type = Cmd_Argv(1);
		ctx.newdevid = strtoul(Cmd_Argv(2), NULL, 0);
		ctx.devicename = Cmd_Argv(3);
		INS_EnumerateDevices(&ctx, IN_DeviceIDs_DoRemap);

		if (ctx.failed)
			Con_Printf("device cannot be remapped\n");
		else if (!ctx.found)
			Con_Printf("%s \"%s\" not known\n", ctx.type, ctx.devicename);
		else if (!cl_warncmd.ival)
			Con_Printf("device remapped\n");
	}
	else
	{
		Con_Printf("Type\tMapping\tName\n");
		INS_EnumerateDevices(NULL, IN_DeviceIDs_Enumerate);
	}
}

float IN_DetermineMouseRate(void)
{
	float time = Sys_DoubleTime();
	static float timer;
	static float last;
	if (fabs(time - timer) > 1)
	{
		timer = time;
		last = ptr[0].updates;
		ptr[0].updates = 0;
	}
	return last;
}

void IN_Init(void)
{
	int i;
	events_avail = 0;
	events_used = 0;

	Cvar_Register (&m_filter, "input controls");
	Cvar_Register (&m_forcewheel, "Input Controls");
	Cvar_Register (&m_forcewheel_threshold, "Input Controls");
	Cvar_Register (&m_strafeonright, "input controls");
	Cvar_Register (&m_fatpressthreshold, "input controls");
	Cvar_Register (&m_slidethreshold, "input controls");
	Cvar_Register (&m_touchmajoraxis, "input controls");
	Cvar_Register (&m_accel, "input controls");
	Cvar_Register (&m_accel_style, "input controls");
	Cvar_Register (&m_accel_power, "input controls");
	Cvar_Register (&m_accel_offset, "input controls");
	Cvar_Register (&m_accel_senscap, "input controls");

	for (i = 0; i < 6; i++)
	{
		Cvar_Register (&joy_advaxis[i], "input controls");
		Cvar_Register (&joy_advaxisscale[i], "input controls");

		Cvar_ForceCallback(&joy_advaxis[i]);
	}
	for (i = 0; i < 3; i++)
	{
		Cvar_Register (&joy_anglesens[i], "input controls");
		Cvar_Register (&joy_movesens[i], "input controls");
		Cvar_Register (&joy_anglethreshold[i], "input controls");
		Cvar_Register (&joy_movethreshold[i], "input controls");
	}
	Cvar_Register (&joy_exponent, "input controls");
	Cvar_Register (&joy_radialdeadzone, "input controls");

	Cmd_AddCommand ("in_deviceids", IN_DeviceIDs_f);

	INS_Init();
}

//tells the keys.c code whether the cursor is currently active, causing mouse clicks instead of binds.
qboolean IN_MouseDevIsTouch(unsigned int devid)
{
	if (devid < MAXPOINTERS)
		return ptr[devid].type == M_TOUCH;
	return false;
}
//there was no ui to click on at least...
//translates MOUSE1 press events into begin-look-or-strafe events.
//translates to MOUSE2 accordingly
//returns 0 if it ate it completely.
int IN_TranslateMButtonPress(unsigned int devid)
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
#ifdef MENU_DAT
				if (MP_JoystickAxis(ev->joy.axis, ev->joy.value, ev->devid))
					joy[ev->devid].axis[ev->joy.axis] = 0;
				else
#endif
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

				if (ev->mouse.x || ev->mouse.y)
					ptr[ev->devid].updates++;
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

				if (ptr[ev->devid].delta[0] != ev->mouse.x - ptr[ev->devid].oldpos[0] ||
					ptr[ev->devid].delta[1] != ev->mouse.y - ptr[ev->devid].oldpos[1])
					ptr[ev->devid].updates++;

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

void IN_MoveMouse(struct mouse_s *mouse, float *movements, int pnum, float frametime)
{
	int mx, my;
	double mouse_x, mouse_y, mouse_deltadist;
	int mfwt;
	qboolean strafe_x, strafe_y;
	int wpnum;
#ifdef PEXT_CSQC
#ifdef MULTITHREAD
	extern qboolean runningindepphys;
#else
	const qboolean runningindepphys = false;
#endif
#endif

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
				if (abs(mx) > abs(my))
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

	if (mouse->type == M_TOUCH)
	{
#ifdef MENU_DAT
		if (!runningindepphys && MP_MousePosition(mouse->oldpos[0], mouse->oldpos[1], mouse->qdeviceid))
		{
			mx = 0;
			my = 0;
		}
#endif
#ifdef PEXT_CSQC
		if (!runningindepphys && CSQC_MousePosition(mouse->oldpos[0], mouse->oldpos[1], mouse->qdeviceid))
		{
			mx = 0;
			my = 0;
		}
#endif
	}
	else
	{
#ifdef MENU_DAT
		if (Key_Dest_Has(kdm_gmenu))
		if (mx || my)
		if (!runningindepphys && MP_MouseMove(mx, my, mouse->qdeviceid))
		{
			mx = 0;
			my = 0;
		}
#endif

#ifdef PEXT_CSQC
		if (mx || my)
		if (!runningindepphys && CSQC_MouseMove(mx, my, mouse->qdeviceid))
		{
			mx = 0;
			my = 0;
		}
#endif

		//if game is not focused, kill any mouse look
		if (Key_Dest_Has(~kdm_game))
		{
			mx = 0;
			my = 0;
		}
	}

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
		if (m_accel_style.ival)
		{
			float accelsens = sensitivity.value*in_sensitivityscale;
			float mousespeed = (sqrt (mx * mx + my * my)) / (1000.0f * (float) frametime);
			mousespeed -= m_accel_offset.value;
			if (mousespeed > 0)
			{
				mousespeed *= m_accel.value;
				if (m_accel_power.value > 1)
					accelsens += exp((m_accel_power.value - 1) * log(mousespeed));
				else
					accelsens = 1;
			}
			if (m_accel_senscap.value > 0 && accelsens > m_accel_senscap.value)
				accelsens = m_accel_senscap.value;
			mouse_x *= accelsens;
			mouse_y *= accelsens;
		}
		else
		{
			mouse_deltadist = sqrt(mx*mx + my*my);
			mouse_x *= (mouse_deltadist*m_accel.value + sensitivity.value*in_sensitivityscale);
			mouse_y *= (mouse_deltadist*m_accel.value + sensitivity.value*in_sensitivityscale);
		}
	}
	else
	{
		mouse_x *= sensitivity.value*in_sensitivityscale;
		mouse_y *= sensitivity.value*in_sensitivityscale;
	}

#ifdef QUAKESTATS
	if (cl.playerview[pnum].statsf[STAT_VIEWZOOM])
	{
		mouse_x *= cl.playerview[pnum].statsf[STAT_VIEWZOOM]/255.0f;
		mouse_y *= cl.playerview[pnum].statsf[STAT_VIEWZOOM]/255.0f;
	}
#endif

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

//rescales threshold-1 down 0-1
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

	int wpnum, i;
	for (i = 0; i < MAXJOYAXIS; i++)
		if (joy->axis[i])
			break;
	if (i == MAXJOYAXIS)
		return;

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

	memset(jstrafe, 0, sizeof(jstrafe));
	memset(jlook, 0, sizeof(jlook));

	for (i = 0; i < 6; i++)
	{
		int ax = joy_advaxis[i].ival;
		switch(ax)
		{
		default:
		case 0:	//dead axis
			break;
		case 1:
		case 3:
		case 5:
			jstrafe[(ax-1)/2] += joy->axis[i] * joy_advaxisscale[i].value;
			break;
		case -1:
		case -3:
		case -5:
			jstrafe[(-ax-1)/2] -= joy->axis[i] * joy_advaxisscale[i].value;

		case 2:
		case 4:
		case 6:
			jlook[(ax-2)/2] += joy->axis[i] * joy_advaxisscale[i].value;
			break;
		case -2:
		case -4:
		case -6:
			jlook[(-ax-2)/2] -= joy->axis[i] * joy_advaxisscale[i].value;
			break;
		}
	}

	//uses a radial deadzone for x+y axis, and separate out the z axis, just because most controllers are 2d affairs with any 3rd axis being a separate knob.
	//deadzone values are stolen from microsoft's xinput documentation. they seem quite large to me - I guess that means that xbox controllers are just dodgy imprecise crap with excessive amounts of friction and finger grease.

	if (joy_radialdeadzone.ival)
	{
		mag = joydeadzone(sqrt(jlook[0]*jlook[0] + jlook[1]*jlook[1]), sqrt(joy_anglethreshold[0].value*joy_anglethreshold[0].value + joy_anglethreshold[1].value*joy_anglethreshold[1].value));
		mag = pow(mag, joy_exponent.value);
		jlook[0] *= mag;
		jlook[1] *= mag;
		mag = joydeadzone(fabs(jlook[2]), joy_anglethreshold[2].value);
		jlook[2] *= mag;

		mag = joydeadzone(sqrt(jstrafe[0]*jstrafe[0] + jstrafe[1]*jstrafe[1]), sqrt(joy_movethreshold[0].value*joy_movethreshold[0].value + joy_movethreshold[1].value*joy_movethreshold[1].value));
		mag = pow(mag, joy_exponent.value);
		jstrafe[0] *= mag;
		jstrafe[1] *= mag;
		mag = joydeadzone(fabs(jstrafe[2]), joy_movethreshold[2].value);
		jstrafe[2] *= mag;
	}
	else
	{
		for (i = 0; i < 3; i++)
		{
			mag = joydeadzone(fabs(jlook[i]), joy_anglethreshold[i].value);
			mag = pow(mag, joy_exponent.value);
			jlook[i] *= mag;

			mag = joydeadzone(fabs(jstrafe[i]), joy_movethreshold[i].value);
			mag = pow(mag, joy_exponent.value);
			jstrafe[i] *= mag;
		}
	}

	if (Key_Dest_Has(~kdm_game))
	{
		VectorClear(jlook);
		VectorClear(jstrafe);
	}

	if (in_speed.state[pnum] & 1)
	{
		VectorScale(jlook, 360*cl_movespeedkey.value, jlook);
		VectorScale(jstrafe, 360*cl_movespeedkey.value, jstrafe);
	}
	VectorScale(jlook, 360*frametime, jlook);

	if (!movements)	//if this is null, gamecode should still get inputs, just no camera looking or anything.
		return;

	//angle changes
	cl.playerview[pnum].viewanglechange[PITCH] += joy_anglesens[0].value * jlook[0];
	cl.playerview[pnum].viewanglechange[YAW] += joy_anglesens[1].value * jlook[1];
	cl.playerview[pnum].viewanglechange[ROLL] += joy_anglesens[2].value * jlook[2];

	if (in_mlook.state[pnum] & 1)
		V_StopPitchDrift (&cl.playerview[pnum]);

	//movement
	movements[0] += joy_movesens[0].value * cl_forwardspeed.value * jstrafe[0];
	movements[1] += joy_movesens[1].value * cl_sidespeed.value * jstrafe[1];
	movements[2] += joy_movesens[2].value * cl_upspeed.value * jstrafe[2];
}

void IN_Move (float *movements, int pnum, float frametime)
{
	int i;
	INS_Move(movements, pnum);
	for (i = 0; i < MAXPOINTERS; i++)
		IN_MoveMouse(&ptr[i], movements, pnum, frametime);

	for (i = 0; i < MAXJOYSTICKS; i++)
		IN_MoveJoystick(&joy[i], movements, pnum, frametime);
}

void IN_JoystickAxisEvent(unsigned int devid, int axis, float value)
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

void IN_KeyEvent(unsigned int devid, int down, int keycode, int unicode)
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
void IN_MouseMove(unsigned int devid, int abs, float x, float y, float z, float size)
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
