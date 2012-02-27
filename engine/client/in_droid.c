#include "quakedef.h"

#include <jni.h>

extern qboolean mouse_active;

cvar_t m_filter = CVARF("m_filter", "0", CVAR_ARCHIVE);
cvar_t m_strafeonright = CVARFD("m_strafeonright", "1", CVAR_ARCHIVE, "If 1, touching the right half of the touchscreen will strafe/move, while the left side will turn.");

extern cvar_t _windowed_mouse;

int mousecursor_x, mousecursor_y;	/*absolute position*/
extern int mousemove_x, mousemove_y;
static float mouse_x, mouse_y;
static float mousestrafe_x, mousestrafe_y;
static float old_mouse_x, old_mouse_y; /*for smoothing*/


#define EVENTQUEUELENGTH 128
struct eventlist_s
{
	enum
	{
		IEV_KEYDOWN,
		IEV_KEYRELEASE,
		IEV_MOUSEABS
	} type;
	int devid;

	union
	{
		struct
		{
			float x, y;
		} mouse;
		struct
		{
			int scancode, unicode;
		} keyboard;
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
struct
{
	vec2_t oldpos;
	vec2_t downpos;
	float movedist;
	vec2_t move;
	int down;
} ptr[MAXPOINTERS];



void IN_Shutdown(void)
{
}

void IN_ReInit()
{
	Cvar_Register (&m_filter, "input controls");
	Cvar_Register (&m_strafeonright, "input controls");
}

void IN_Init(void)
{
	IN_ReInit();
}

/*on android, each 'pointer' is a separate touch location*/
void IN_Commands(void)
{
	struct eventlist_s *ev;
	while (events_used != events_avail)
	{
		ev = &eventlist[events_used & (EVENTQUEUELENGTH-1)];
		switch(ev->type)
		{
		case IEV_KEYDOWN:
		case IEV_KEYRELEASE:
			if (ev->keyboard.scancode == K_MOUSE1 && ev->devid < MAXPOINTERS)
			{
				if (Key_MouseShouldBeFree())
					ptr[ev->devid].down = false;
				else
				{
					if (ev->type == IEV_KEYDOWN)
					{
						ptr[ev->devid].down = true;
						ptr[ev->devid].movedist = 0;
						ptr[ev->devid].downpos[0] = ptr[ev->devid].oldpos[0];
						ptr[ev->devid].downpos[1] = ptr[ev->devid].oldpos[1];
						ptr[ev->devid].move[0] = 0;
						ptr[ev->devid].move[1] = 0;
					}
					else
					{
						if (ptr[ev->devid].down)
						{
							if (ptr[ev->devid].movedist < 5)
							{
								/*if its on the right, make it a mouse2*/
								int key = (m_strafeonright.ival && ptr[ev->devid].downpos[0] > vid.pixelwidth/2)?K_MOUSE3:K_MOUSE1;
								Key_Event(ev->devid, key, 0, true);
								Key_Event(ev->devid, key, 0, false);
							}
						}
						ptr[ev->devid].down = false;
					}
					break;
				}
			}
			Key_Event(ev->devid, ev->keyboard.scancode, ev->keyboard.unicode, ev->type == IEV_KEYDOWN); 
			break;
		case IEV_MOUSEABS:
			/*mouse cursors only really work with one pointer*/
			if (ev->devid == 0)
			{
				mousecursor_x = bound(0, ev->mouse.x, vid.width - 1);
				mousecursor_y = bound(0, ev->mouse.y, vid.height - 1);
			}

			if (ev->devid < MAXPOINTERS)
			{
				ptr[ev->devid].move[0] += ev->mouse.x - ptr[ev->devid].oldpos[0];
				ptr[ev->devid].move[1] += ev->mouse.y - ptr[ev->devid].oldpos[1];
	
				ptr[ev->devid].movedist += fabs(ev->mouse.x - ptr[ev->devid].oldpos[0]) + fabs(ev->mouse.y - ptr[ev->devid].oldpos[1]);

				ptr[ev->devid].oldpos[0] = ev->mouse.x;
				ptr[ev->devid].oldpos[1] = ev->mouse.y;
			}
			break;
		}
		events_used++;
	}
}



static void IN_Update(qboolean ingame)
{
	int i;
	//strafing speed is absolute
	mousestrafe_x = 0;
	mousestrafe_y = 0;

	for (i = 0; i < MAXPOINTERS; i++)
	{
		/*ignore if no action, to avoid phantom csqc input events*/
		if (!ptr[i].down && !ptr[i].move[0] && !ptr[i].move[1])
			continue;

		if (!CSQC_MousePosition(ptr[i].oldpos[0], ptr[i].oldpos[1], i))
		{
			if (!CSQC_MouseMove(ptr[i].move[0], ptr[i].move[1], i))
			{
				if (ptr[i].down && m_strafeonright.ival && ptr[i].downpos[0] > vid.pixelwidth/2 && ingame)
				{
					mousestrafe_x += ptr[i].oldpos[0] - ptr[i].downpos[0];
					mousestrafe_y += ptr[i].oldpos[1] - ptr[i].downpos[1];
				}
				else
				{
					mouse_x += ptr[i].move[0];
					mouse_y += ptr[i].move[1];
				}
			}
		}
		ptr[i].move[0] = 0;
		ptr[i].move[1] = 0;
	}
}


void IN_Move (float *movements, int pnum)
{
	qboolean ingame;
	extern int mousecursor_x, mousecursor_y;

	if (pnum != 0)
		return;	//we're lazy today.

	ingame = movements != NULL && (key_dest == key_game);

	IN_Update(ingame);

	if (m_filter.value)
	{
		mouse_x = (mouse_x + old_mouse_x) * 0.5;
		mouse_y = (mouse_y + old_mouse_y) * 0.5;
	}
	old_mouse_x = mouse_x;
	old_mouse_y = mouse_y;

	if(in_xflip.value) mouse_x *= -1;

	mousemove_x += mouse_x;
	mousemove_y += mouse_y;

   	if (!ingame)
	{
		mouse_x = mouse_y = 0;
#ifdef VM_UI
		UI_MousePosition(mousecursor_x, mousecursor_y);
#endif
	}

	/*if the look-mouse is set to always strafe instead...*/
	if ( (in_strafe.state[pnum] & 1) || (lookstrafe.value && (in_mlook.state[pnum] & 1) ))
	{
		mousestrafe_x += mouse_x;
		mouse_x = 0;
	}
	if ( (in_strafe.state[pnum] & 1) || !(in_mlook.state[pnum] & 1))
	{
		mousestrafe_y += mouse_y;
		mouse_y = 0;
	}

	/*handle strafes*/
	if (movements)
	{
		float scale;

		scale = m_side.value * sensitivity.value;
		movements[1] += mousestrafe_x * scale;

		scale = m_forward.value * sensitivity.value;
		if ((in_strafe.state[pnum] & 1) && noclip_anglehack)
			movements[2] -= mousestrafe_y * scale;
		else
			movements[0] -= mousestrafe_y * scale;
	}

	if (in_mlook.state[pnum] & 1)
		V_StopPitchDrift (pnum);

	/*handle looks*/
	cl.viewanglechange[pnum][YAW] -= m_yaw.value * mouse_x * sensitivity.value;
	cl.viewanglechange[pnum][PITCH] += m_pitch.value * mouse_y * sensitivity.value;

	mouse_x = mouse_y = 0.0;
}



JNIEXPORT void JNICALL Java_com_fteqw_FTEDroidEngine_keypress(JNIEnv *env, jobject obj,
                 jint down, jint keycode, jint unicode)
{
	struct eventlist_s *ev = in_newevent();
	if (!ev)
		return;
	ev->type = down?IEV_KEYDOWN:IEV_KEYRELEASE;
	ev->devid = 0;
	ev->keyboard.scancode = keycode;
	ev->keyboard.unicode = unicode;
	in_finishevent();
}

JNIEXPORT void JNICALL Java_com_fteqw_FTEDroidEngine_motion(JNIEnv *env, jobject obj,
                 jint act, jint ptrid, jfloat x, jfloat y)
{
	struct eventlist_s *ev = in_newevent();
	if (!ev)
		return;
	ev->devid = ptrid;
	if (act)
	{
		ev->type = (act==1)?IEV_KEYDOWN:IEV_KEYRELEASE;
		ev->keyboard.scancode = K_MOUSE1;
		ev->keyboard.unicode = 0;
	}
	else
	{
		ev->type = IEV_MOUSEABS;
		ev->mouse.x = x;
		ev->mouse.y = y;
	}
	in_finishevent();
}
