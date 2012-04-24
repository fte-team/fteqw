#include "quakedef.h"
extern qboolean mouse_active;

cvar_t m_simtouch = CVARF("m_simtouch", "0", CVAR_ARCHIVE);
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
		IEV_MOUSEABS,
		IEV_MOUSEDELTA
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
static struct eventlist_s *in_lastevent(void)
{
	if (events_avail == events_used)
		return NULL;
	return &eventlist[(events_avail-1) & (EVENTQUEUELENGTH-1)];
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
	enum
	{
		MT_UNPRESSED,
		MT_PRESSED,
		MT_DELTA
	} mtype;
} ptr[MAXPOINTERS];



void IN_Shutdown(void)
{
}

void IN_ReInit()
{
}

void IN_Init(void)
{
	Cvar_Register (&m_simtouch, "input controls");
	Cvar_Register (&m_filter, "input controls");
	Cvar_Register (&m_strafeonright, "input controls");

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
			if (ev->keyboard.scancode == K_MOUSE1 && ev->devid < MAXPOINTERS && ptr[ev->devid].mtype != MT_DELTA)
			{
				if (Key_MouseShouldBeFree())
					ptr[ev->devid].mtype = MT_UNPRESSED;
				else
				{
					if (ev->type == IEV_KEYDOWN)
					{
						ptr[ev->devid].mtype = MT_PRESSED;
						ptr[ev->devid].movedist = 0;
						ptr[ev->devid].downpos[0] = ptr[ev->devid].oldpos[0];
						ptr[ev->devid].downpos[1] = ptr[ev->devid].oldpos[1];
						ptr[ev->devid].move[0] = 0;
						ptr[ev->devid].move[1] = 0;
					}
					else
					{
						if (ptr[ev->devid].mtype == MT_PRESSED)
						{
							if (ptr[ev->devid].movedist < 5)
							{
								/*if its on the right, make it a mouse2*/
								int key = (m_strafeonright.ival && ptr[ev->devid].downpos[0] > vid.pixelwidth/2)?K_MOUSE3:K_MOUSE1;
								Key_Event(ev->devid, key, 0, true);
								Key_Event(ev->devid, key, 0, false);
							}
						}
						ptr[ev->devid].mtype = MT_UNPRESSED;
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
				if (ptr[ev->devid%MAXPOINTERS].mtype == MT_DELTA)
					ptr[ev->devid%MAXPOINTERS].mtype = MT_UNPRESSED;
				ptr[ev->devid].move[0] += ev->mouse.x - ptr[ev->devid].oldpos[0];
				ptr[ev->devid].move[1] += ev->mouse.y - ptr[ev->devid].oldpos[1];
	
				ptr[ev->devid].movedist += fabs(ev->mouse.x - ptr[ev->devid].oldpos[0]) + fabs(ev->mouse.y - ptr[ev->devid].oldpos[1]);

				ptr[ev->devid].oldpos[0] = ev->mouse.x;
				ptr[ev->devid].oldpos[1] = ev->mouse.y;
			}
			break;
		case IEV_MOUSEDELTA:
			/*unlike abs, we can combine the mice properly*/
			mousecursor_x += ev->mouse.x;
			mousecursor_y += ev->mouse.y;
			mousecursor_x = bound(0, mousecursor_x, vid.width - 1);
			mousecursor_y = bound(0, mousecursor_y, vid.height - 1);

			ptr[ev->devid%MAXPOINTERS].move[0] += ev->mouse.x;
			ptr[ev->devid%MAXPOINTERS].move[1] += ev->mouse.y;

			ptr[ev->devid%MAXPOINTERS].movedist += fabs(ev->mouse.x) + fabs(ev->mouse.y);

			if (m_simtouch.ival)
			{
				if (ptr[ev->devid%MAXPOINTERS].mtype == MT_DELTA)
					ptr[ev->devid%MAXPOINTERS].mtype = MT_UNPRESSED;
				ptr[ev->devid].oldpos[0] = mousecursor_x;
				ptr[ev->devid].oldpos[1] = mousecursor_y;
			}
			else
				ptr[ev->devid%MAXPOINTERS].mtype = MT_DELTA;
			
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
		if (ptr[i].mtype == MT_UNPRESSED && !ptr[i].move[0] && !ptr[i].move[1])
			continue;

		if (ptr[i].mtype == MT_DELTA || !CSQC_MousePosition(ptr[i].oldpos[0], ptr[i].oldpos[1], i))
		{
			if (!CSQC_MouseMove(ptr[i].move[0], ptr[i].move[1], i))
			{
				switch(ptr[i].mtype)
				{
				case MT_UNPRESSED:
					break;
				case MT_PRESSED:
					if (m_strafeonright.ival && ptr[i].downpos[0] > vid.pixelwidth/2 && ingame)
					{
						mousestrafe_x += ptr[i].oldpos[0] - ptr[i].downpos[0];
						mousestrafe_y += ptr[i].oldpos[1] - ptr[i].downpos[1];
					}
					else
					{
						mouse_x += ptr[i].move[0];
						mouse_y += ptr[i].move[1];
					}
					break;
				case MT_DELTA:
					mouse_x += ptr[i].move[0];
					mouse_y += ptr[i].move[1];
					break;
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


/*regular key event*/
void IN_QueueKey(int down, int keycode, int unicode)
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
/*
in ppapi, we have 'keycode' and 'char' events completely separately
this doesn't match the rest of the system very well
so we update the previous key event instead, where possible (IME can still trigger multiple chars at a time)

this is risky and may drop the key in rare situations
*/
void IN_AmmendUnicode(int unicode)
{
	struct eventlist_s *ev = in_lastevent();
	if (ev && ev->type == IEV_KEYDOWN)
	{
		if (!ev->keyboard.unicode)
		{
			ev->keyboard.unicode = unicode;
			return;
		}
	}
	/*last command was already used? that makes things painful. maybe noone will notice*/
	IN_QueueKey(true, 0, unicode);
	IN_QueueKey(false, 0, unicode);
}

void IN_QueueMouse(int act, int ptrid, float x, float y, int button)
{
	struct eventlist_s *ev = in_newevent();
	if (!ev)
		return;
	ev->devid = ptrid;
	switch(act)
	{
	case 0:
	case 3:
		ev->type = (act==0)?IEV_MOUSEABS:IEV_MOUSEDELTA;
		ev->mouse.x = x;
		ev->mouse.y = y;
		break;
	case 1:
	case 2:
		ev->type = (act==1)?IEV_KEYDOWN:IEV_KEYRELEASE;
		ev->keyboard.scancode = K_MOUSE1+button;
		ev->keyboard.unicode = 0;
		break;
	}
	in_finishevent();
}
