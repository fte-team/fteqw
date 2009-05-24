/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// vid_x.c -- general x video driver

#define _BSD

#include "quakedef.h"
#include "d_local.h"

// these need to be fixed to assured n-bit types
typedef unsigned short PIXEL;
typedef unsigned int PIXEL32;

#include <ctype.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef __CYGWIN__
#include <cygwin/ipc.h>
#include <cygwin/shm.h>
#else
#include <sys/ipc.h>
#include <sys/shm.h>
#endif
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/extensions/XShm.h>

#ifdef __linux__
	#define WITH_VMODE	//undefine this if the following include fails.
#endif
#ifdef WITH_VMODE
#include <X11/extensions/xf86vmode.h>
#endif


#undef free

#ifdef __CYGWIN__
#define NOSHM
#endif

#define NOSHM

cvar_t		m_filter = {"m_filter","0", NULL, CVAR_ARCHIVE};
cvar_t  m_accel = {"m_accel", "0"};
#ifdef IN_XFLIP
cvar_t	in_xflip = {"in_xflip", "0"};
#endif
static qboolean old_windowed_mouse;

static qboolean        mouse_avail;
static qboolean	mouseactive;
static int             mouse_buttons=3;
static int             mouse_oldbuttonstate;
static int             mouse_buttonstate;
float   mouse_x, mouse_y;
static float   old_mouse_x, old_mouse_y;
static int p_mouse_x;
static int p_mouse_y;
static int ignorenext;

static int focusedapp;

typedef struct
{
	int input;
	int output;
} keymap_t;

viddef_t vid; // global video state
//unsigned short d_8to16table[256];

#ifdef RGLQUAKE
extern Display			*vid_dpy;
#else
static Display			*vid_dpy = NULL;
#endif
static Colormap			x_cmap;
static Window			x_win;
static GC				x_gc;
static Visual			*x_vis;
static XVisualInfo		*x_visinfo;
//static XImage			*x_image;

static qboolean			oktodraw = false;

#ifndef NOSHM
static int				x_shmeventtype;
static qboolean			doShm;
int XShmQueryExtension(Display *);
int XShmGetEventBase(Display *);
static XShmSegmentInfo	x_shminfo[2];
#endif

static int current_framebuffer;
static XImage			*x_framebuffer[2] = { 0, 0 };

qbyte vid_curpal[768];

static long X11_highhunkmark;
static long X11_buffersize;

static int vid_surfcachesize;
static void *vid_surfcache;

static PIXEL st2d_8to16table[256];
static PIXEL32 st3d_8to32table[256];
static int shiftmask_fl=0;
static long r_shift,g_shift,b_shift;
static unsigned long r_mask,g_mask,b_mask;

void shiftmask_init()
{
    unsigned int x;
    r_mask=x_vis->red_mask;
    g_mask=x_vis->green_mask;
    b_mask=x_vis->blue_mask;
    for(r_shift=-8,x=1;x<r_mask;x=x<<1)r_shift++;
    for(g_shift=-8,x=1;x<g_mask;x=x<<1)g_shift++;
    for(b_shift=-8,x=1;x<b_mask;x=x<<1)b_shift++;
    shiftmask_fl=1;

	redshift = 0;//r_shift;
	greenshift = 5;//g_shift;
	blueshift = 10;//b_shift;

	redbits = 5;
	greenbits = 5;
	bluebits = 5;
}

PIXEL xlib_rgb(int r,int g,int b)
{
    PIXEL p;
    if(shiftmask_fl==0) shiftmask_init();
    p=0;

    if(r_shift>0) {
        p=(r<<(r_shift))&r_mask;
    } else if(r_shift<0) {
        p=(r>>(-r_shift))&r_mask;
    } else p|=(r&r_mask);

    if(g_shift>0) {
        p|=(g<<(g_shift))&g_mask;
    } else if(g_shift<0) {
        p|=(g>>(-g_shift))&g_mask;
    } else p|=(g&g_mask);

    if(b_shift>0) {
        p|=(b<<(b_shift))&b_mask;
    } else if(b_shift<0) {
        p|=(b>>(-b_shift))&b_mask;
    } else p|=(b&b_mask);

    return p;
}
PIXEL32 xlib_rgb32(int r, int g, int b)
{
	PIXEL32 p=0;
	p = r<<16;
	p|= g<<8;
	p|= b;
	return p;
}

void st2_fixup( XImage *framebuf, int x, int y, int width, int height)
{
	int xi,yi;
	unsigned char *src;
	PIXEL *dest;

	if (r_pixbytes == 2)
		return;

	if( (x<0)||(y<0) )return;

	for (yi = y; yi < (y+height); yi++)
	{
		src = &framebuf->data [yi * framebuf->bytes_per_line];
		dest = (PIXEL*)src;
		for(xi = (x+width-1); xi >= x; xi--)
		{
			dest[xi] = st2d_8to16table[src[xi]];
		}
	}
}

void st3_fixup( XImage *framebuf, int x, int y, int width, int height)
{
	int xi, yi;
	unsigned char *src;
	PIXEL32 *dest;
	if (r_pixbytes == 4)
		return;
	for (yi = y; yi < (y+height); yi++)
	{
		src = &framebuf->data [yi * framebuf->bytes_per_line];
		dest = (PIXEL32*)src;
		for (xi = (x+width-1); xi >= x; xi--)
		{
			dest[xi] = st3d_8to32table[src[xi]];
		}
	}
}

// ========================================================================
// Tragic death handler
// ========================================================================

void TragicDeath(int signal_num)
{
	XAutoRepeatOn(vid_dpy);
	XCloseDisplay(vid_dpy);
	vid_dpy = NULL;
	Sys_Error("This death brought to you by the number %d\n", signal_num);
}

// ========================================================================
// makes a null cursor
// ========================================================================

static Cursor CreateNullCursor(Display *display, Window root)
{
    Pixmap cursormask; 
    XGCValues xgc;
    GC gc;
    XColor dummycolour;
    Cursor cursor;

    cursormask = XCreatePixmap(display, root, 1, 1, 1/*depth*/);
    xgc.function = GXclear;
    gc =  XCreateGC(display, cursormask, GCFunction, &xgc);
    XFillRectangle(display, cursormask, gc, 0, 0, 1, 1);
    dummycolour.pixel = 0;
    dummycolour.red = 0;
    dummycolour.flags = 04;
    cursor = XCreatePixmapCursor(display, cursormask, cursormask,
          &dummycolour,&dummycolour, 0,0);
    XFreePixmap(display,cursormask);
    XFreeGC(display,gc);
    return cursor;
}

void ResetFrameBuffer(void)
{
	int mem;
	int pwidth;

	if (x_framebuffer[0])
	{
		BZ_Free(x_framebuffer[0]->data);
		free(x_framebuffer[0]);
	}

	if (d_pzbuffer)
	{
		D_FlushCaches ();
		Hunk_FreeToHighMark (X11_highhunkmark);
		d_pzbuffer = NULL;
	}
	X11_highhunkmark = Hunk_HighMark ();

// alloc an extra line in case we want to wrap, and allocate the z-buffer
	X11_buffersize = vid.width * vid.height * sizeof (*d_pzbuffer);

	vid_surfcachesize = D_SurfaceCacheForRes (vid.width, vid.height, 0);

	X11_buffersize += vid_surfcachesize;

	d_pzbuffer = Hunk_HighAllocName (X11_buffersize, "video");
	if (d_pzbuffer == NULL)
		Sys_Error ("Not enough memory for video mode\n");

	vid_surfcache = (qbyte *) d_pzbuffer
		+ vid.width * vid.height * sizeof (*d_pzbuffer);

	D_InitCaches(vid_surfcache, vid_surfcachesize);

	pwidth = x_visinfo->depth / 8;
	if (pwidth == 3) pwidth = 4;
	mem = ((vid.width*pwidth+7)&~7) * vid.height * r_pixbytes;

	x_framebuffer[0] = XCreateImage(	vid_dpy,
		x_vis,
		x_visinfo->depth,
		ZPixmap,
		0,
		BZ_Malloc(mem),
		vid.width, vid.height,
		32,
		0);

	if (!x_framebuffer[0])
		Sys_Error("VID: XCreateImage failed\n");

	vid.buffer = (qbyte*) (x_framebuffer[0]->data);
	vid.conbuffer = vid.buffer;

}

#ifndef NOSHM
void ResetSharedFrameBuffers(void)
{

	int size;
	int key;
	int minsize = getpagesize();
	int frm;

	if (d_pzbuffer)
	{
		D_FlushCaches ();
		Hunk_FreeToHighMark (X11_highhunkmark);
		d_pzbuffer = NULL;
	}

	X11_highhunkmark = Hunk_HighMark ();

// alloc an extra line in case we want to wrap, and allocate the z-buffer
	X11_buffersize = vid.width * vid.height * sizeof (*d_pzbuffer);

	vid_surfcachesize = D_SurfaceCacheForRes (vid.width, vid.height, 0);

	X11_buffersize += vid_surfcachesize;

	d_pzbuffer = Hunk_HighAllocName (X11_buffersize, "video");
	if (d_pzbuffer == NULL)
		Sys_Error ("Not enough memory for video mode\n");

	vid_surfcache = (qbyte *) d_pzbuffer
		+ vid.width * vid.height * sizeof (*d_pzbuffer);

	D_InitCaches(vid_surfcache, vid_surfcachesize);

	for (frm=0 ; frm<2 ; frm++)
	{

	// free up old frame buffer memory

		if (x_framebuffer[frm])
		{
			XShmDetach(vid_dpy, &x_shminfo[frm]);
			free(x_framebuffer[frm]);
			shmdt(x_shminfo[frm].shmaddr);
		}

	// create the image

		x_framebuffer[frm] = XShmCreateImage(	vid_dpy,
						x_vis,
						x_visinfo->depth,
						ZPixmap,
						0,
						&x_shminfo[frm],
						vid.width,
						vid.height );

	// grab shared memory

		size = x_framebuffer[frm]->bytes_per_line*r_pixbytes
			* x_framebuffer[frm]->height;
		if (size < minsize)
			Sys_Error("VID: Window must use at least %d bytes\n", minsize);

		key = random();
		x_shminfo[frm].shmid = shmget((key_t)key, size, IPC_CREAT|0777);
		if (x_shminfo[frm].shmid==-1)
			Sys_Error("VID: Could not get any shared memory\n");

		// attach to the shared memory segment
		x_shminfo[frm].shmaddr =
			(void *) shmat(x_shminfo[frm].shmid, 0, 0);

		printf("VID: shared memory id=%d, addr=0x%lx\n", x_shminfo[frm].shmid,
			(long) x_shminfo[frm].shmaddr);

		x_framebuffer[frm]->data = x_shminfo[frm].shmaddr;

	// get the X server to attach to it

		if (!XShmAttach(vid_dpy, &x_shminfo[frm]))
			Sys_Error("VID: XShmAttach() failed\n");
		XSync(vid_dpy, 0);
		shmctl(x_shminfo[frm].shmid, IPC_RMID, 0);

	}

}
#endif

int X_TryChangeResolution(int trywidth, int tryheight, int onscreen)
{
#ifdef WITH_VMODE
	int MajorVersion = 0, MinorVersion = 0;
	int vidmode_ext;
	int vidmode_usemode;

	if (COM_CheckParm("-novmode") || !XF86VidModeQueryVersion(vid_dpy, &MajorVersion, &MinorVersion))
	{
		vidmode_ext = 0;
	}
	else
	{
		Con_Printf("Using XF86-VidModeExtension Ver. %d.%d\n", MajorVersion, MinorVersion);
		vidmode_ext = MajorVersion;
	}


	vidmode_usemode = -1;
	if (vidmode_ext)
	{
		int best_fit, best_dist, dist, x, y;

int i;
int num_vidmodes;
XF86VidModeModeInfo **vidmodes;

		XF86VidModeGetAllModeLines(vid_dpy, onscreen, &num_vidmodes, &vidmodes);
		// Are we going fullscreen?  If so, let's change video mode
		{
			best_dist = 9999999;
			best_fit = -1;

			for (i = 0; i < num_vidmodes; i++)
			{
				if (trywidth > vidmodes[i]->hdisplay ||
					tryheight > vidmodes[i]->vdisplay)
					continue;

				x = trywidth - vidmodes[i]->hdisplay;
				y = tryheight - vidmodes[i]->vdisplay;
				dist = (x * x) + (y * y);
				if (dist < best_dist)
				{
					best_dist = dist;
					best_fit = i;
				}
			}

			if (best_fit != -1)// && (!best_dist || COM_CheckParm("-fullscreen")))
			{
				vid.width = vidmodes[best_fit]->hdisplay;
				vid.height = vidmodes[best_fit]->vdisplay;

//				if (best_dist)
					Con_Printf("Matched res to %ix%i\n", vid.width, vid.height);

				// change to the mode
				XF86VidModeSwitchToMode(vid_dpy, onscreen, vidmodes[vidmode_usemode=best_fit]);
//				vidmode_active = true;
				// Move the viewport to top left
				XF86VidModeSetViewPort(vid_dpy, onscreen, 0, 0);

				return true;

			}
		}
	}
#endif
	return false;
}

void X_StoreIconData(unsigned long *data, unsigned int elems)
{
	Atom propname = XInternAtom(vid_dpy, "_NET_WM_ICON", false);
	Atom proptype = XInternAtom(vid_dpy, "CARDINAL", false);

	XChangeProperty(vid_dpy, x_win, propname, proptype, 32, PropModeReplace, (void*)data, elems);
}

void X_SetWMHints(void)
{	//this entire function is to appease wmaker
	char *fakeappname[] = {"false", NULL};
	XWMHints wmhints;
	XClassHint clshint;

	//the classname (the thing that never changes, as opposed to what the app says it is)
	memset(&clshint, 0, sizeof(clshint));
	clshint.res_name = "FTEQuakeWorld";
	clshint.res_class = "FTEQW";
	XSetClassHint(vid_dpy, x_win, &clshint);

	//the command to restart the app (no idea why wmaker wants this
	XSetCommand(vid_dpy, x_win, fakeappname, 1);

	memset(&wmhints, 0, sizeof(wmhints));
	wmhints.flags = InputHint|StateHint|WindowGroupHint;
	wmhints.input = true;	//give us input!!!
	wmhints.initial_state = NormalState;
	wmhints.window_group = x_win;	//this is the only bit that is really required
	XSetWMHints(vid_dpy, x_win, &wmhints);
}

#include "bymorphed.h"
void X_StoreIcon(void)
{
	int i;
	int elems;
	unsigned long data[64*64+2]; //xlib requires long, even on 64bit
	unsigned int *indata;
	indata = (unsigned int*)icon.pixel_data;

	X_SetWMHints();

	memset(data, 0, sizeof(data));

	data[0] = icon.width;
	data[1] = icon.height;
	elems = data[0]*data[1]+2;

	for (i = 0; i < data[0]*data[1]; i++)
	{
		data[i+2] = indata[i];
	}
	X_StoreIconData(data, elems);
}

// Called at startup to set up translation tables, takes 256 8 bit RGB values
// the palette data will go away after the call, so it must be copied off if
// the video driver will need it again

qboolean SWVID_Init (rendererstate_t *info, unsigned char *palette)
{

   int pnum, i;
   XVisualInfo template;
   int num_visuals;
   int template_mask;

	S_Startup();
   
   ignorenext=0;
   vid.width = info->width;
   vid.height = info->height;
   vid.maxwarpwidth = WARP_WIDTH;
   vid.maxwarpheight = WARP_HEIGHT;
   vid.numpages = 2;
   vid.colormap = host_colormap;
   //	vid.cbits = VID_CBITS;
   //	vid.grades = VID_GRADES;

	r_pixbytes = info->bpp/8;
   
	srandom(getpid());

// open the display
	if (!vid_dpy)
		vid_dpy = XOpenDisplay(0);
	if (!vid_dpy)
	{
		if (getenv("DISPLAY"))
			Con_Printf("VID: Could not open display [%s]\n",
				getenv("DISPLAY"));
		else
			Con_Printf("VID: Could not open local display\n");

		return false;
	}

// catch signals so i can turn on auto-repeat

	{
		struct sigaction sa;
		sigaction(SIGINT, 0, &sa);
		sa.sa_handler = TragicDeath;
		sigaction(SIGINT, &sa, 0);
		sigaction(SIGTERM, &sa, 0);
	}

//	XAutoRepeatOff(vid_dpy);

// for debugging only
	XSynchronize(vid_dpy, True);

	template_mask = 0;

// specify a visual id
	if ((pnum=COM_CheckParm("-visualid")))
	{
		if (pnum >= com_argc-1)
			Sys_Error("VID: -visualid <id#>\n");
		template.visualid = Q_atoi(com_argv[pnum+1]);
		template_mask = VisualIDMask;
	}

// If not specified, use default visual
	else
	{
		int screen;
		screen = XDefaultScreen(vid_dpy);
		template.visualid =
			XVisualIDFromVisual(XDefaultVisual(vid_dpy, screen));
		template_mask = VisualIDMask;
	}

// pick a visual- warn if more than one was available
	x_visinfo = XGetVisualInfo(vid_dpy, template_mask, &template, &num_visuals);
	if (num_visuals > 1)
	{
		Con_Printf("Found more than one visual id at depth %d:\n", template.depth);
		for (i=0 ; i<num_visuals ; i++)
			Con_Printf("	-visualid %d\n", (int)(x_visinfo[i].visualid));
	}
	else if (num_visuals == 0)
	{
		if (template_mask == VisualIDMask)
		{
			Con_Printf("VID: Bad visual id %d\n", template.visualid);
			return false;
		}
		else
		{
			Con_Printf("VID: No visuals at depth %d\n", template.depth);
			return false;
		}
	}

	Con_DPrintf("Using visualid %d:\n", (int)(x_visinfo->visualid));
	Con_DPrintf("	screen %d\n", x_visinfo->screen);
	Con_DPrintf("	red_mask 0x%x\n", (int)(x_visinfo->red_mask));
	Con_DPrintf("	green_mask 0x%x\n", (int)(x_visinfo->green_mask));
	Con_DPrintf("	blue_mask 0x%x\n", (int)(x_visinfo->blue_mask));
	Con_DPrintf("	colormap_size %d\n", x_visinfo->colormap_size);
	Con_DPrintf("	bits_per_rgb %d\n", x_visinfo->bits_per_rgb);

//our rendering works in 8, 16, or 32 bpp.
//only 8bpp is scaled to the x server's depth.
//the others break if they arn't rendered at the same depth as they are displayed.
//this is where we scale things down.
	if (info->bpp == 16 && x_visinfo->depth != 16 && x_visinfo->depth != 15)
	{
		Con_Printf("Forcing 8 bit rendering\n");
		r_pixbytes = 1;
	}
	else if (info->bpp == 32 && x_visinfo->depth < 24)
	{
		Con_Printf("Forcing 8 bit rendering\n");
		r_pixbytes = 1;
	}

	x_vis = x_visinfo->visual;

// setup attributes for main window
	{
	   int attribmask = CWEventMask  | CWColormap | CWBorderPixel;
	   XSetWindowAttributes attribs;
	   Colormap tmpcmap;
	   
	   tmpcmap = XCreateColormap(vid_dpy, XRootWindow(vid_dpy,
							 x_visinfo->screen), x_vis, AllocNone);
	   
           attribs.event_mask = StructureNotifyMask | KeyPressMask
	     | KeyReleaseMask | ExposureMask | PointerMotionMask |
	     ButtonPressMask | ButtonReleaseMask | FocusChangeMask;
	   attribs.border_pixel = 0;
	   attribs.colormap = tmpcmap;

// create the main window
		x_win = XCreateWindow(	vid_dpy,
			XRootWindow(vid_dpy, x_visinfo->screen),
			0, 0,	// x, y
			vid.width, vid.height,
			0, // borderwidth
			x_visinfo->depth,
			InputOutput,
			x_vis,
			attribmask,
			&attribs );
		XStoreName( vid_dpy,x_win,"FTE QuakeWorld");
		XSetIconName(vid_dpy, x_win, "FTEQW");


		if (x_visinfo->class != TrueColor)
			XFreeColormap(vid_dpy, tmpcmap);

	}

	if (info->fullscreen)
	{
		Atom val = XInternAtom(vid_dpy, "_NET_WM_STATE_FULLSCREEN", false);
		if (X_TryChangeResolution(info->width, info->height, x_visinfo->screen))
		{
			//kill off borders and everything else
			XChangeProperty(vid_dpy, x_win, XInternAtom(vid_dpy, "_NET_WM_STATE", false), XInternAtom(vid_dpy, "ATOM", false), 32, PropModeAppend, (void*)&val, 1);
			//make sure that we do take the full screen, even if the res chosen was a best-fit
			printf("%ix%i\n", vid.width, vid.height);
//			XMoveResizeWindow(vid_dpy, x_win, 0, 0, vid.width, vid.height);
		}
	}
	X_StoreIcon();

	if (x_visinfo->depth == 8)
	{

	// create and upload the palette
		if (x_visinfo->class == PseudoColor)
		{
			x_cmap = XCreateColormap(vid_dpy, x_win, x_vis, AllocAll);
			VID_SetPalette(palette);
			XSetWindowColormap(vid_dpy, x_win, x_cmap);
		}

	}

// inviso cursor
	XDefineCursor(vid_dpy, x_win, CreateNullCursor(vid_dpy, x_win));

// create the GC
	{
		XGCValues xgcvalues;
		int valuemask = GCGraphicsExposures;
		xgcvalues.graphics_exposures = False;
		x_gc = XCreateGC(vid_dpy, x_win, valuemask, &xgcvalues );
	}
	shiftmask_init();
	if (r_pixbytes == 2)	//16 bit needs a 16 bit colormap.
	{
		extern qbyte gammatable[256];
		int j, i;
		float f;
		int r, g, b;
		unsigned short *data;
		unsigned char *pal = host_basepal;
		
		shiftmask_init();
//		r_flushcache++;
		if (!vid.colormap16)
			vid.colormap16 = BZ_Malloc(sizeof(short)*256*VID_GRADES+sizeof(int));
		data = vid.colormap16;
		for (j = 0; j < VID_GRADES; j++)
		{
			f = (1 - ((float)j/VID_GRADES));
			f = (float)gammatable[(int)(f*255)]/255;
			f *= 2;
			for (i = 0; i < 256; i++)
			{
				r = pal[i*3+0]*f;
				g = pal[i*3+1]*f;
				b = pal[i*3+2]*f;
				if (r > 255)
					r = 255;
				if (g > 255)
					g = 255;
				if (b > 255)
					b = 255;
				r >>= 8 - redbits;
				g >>= 8 - greenbits;
				b >>= 8 - bluebits;
				data[i] = (r<<redshift) + (g<<greenshift) + (b<<blueshift);
			}
			data+=256;
		}
	}

// map the window
	XMapWindow(vid_dpy, x_win);

// wait for first exposure event
	{
		XEvent event;
		do
		{
			XNextEvent(vid_dpy, &event);
Con_Printf("Event %i\n", event.type);
			if (event.type == Expose && !event.xexpose.count)
				oktodraw = true;
		} while (!oktodraw);
	}
// now safe to draw

#ifndef NOSHM
// even if MITSHM is available, make sure it's a local connection
	if (XShmQueryExtension(vid_dpy))
	{
		char *displayname;
		doShm = true;
		displayname = (char *) getenv("DISPLAY");
		if (displayname)
		{
			char *d = displayname;
			while (*d && (*d != ':')) d++;
			if (*d) *d = 0;
			if (!(!strcasecmp(displayname, "unix") || !*displayname))
				doShm = false;
		}
	}

	if (doShm)
	{
		x_shmeventtype = XShmGetEventBase(vid_dpy) + ShmCompletion;
		ResetSharedFrameBuffers();
	}
	else
#endif
		ResetFrameBuffer();

	current_framebuffer = 0;
	vid.rowbytes = x_framebuffer[0]->bytes_per_line/r_pixbytes;
	vid.buffer = x_framebuffer[0]->data;
	vid.direct = 0;
	vid.conbuffer = x_framebuffer[0]->data;
	vid.conrowbytes = vid.rowbytes;
	vid.conwidth = vid.width;
	vid.conheight = vid.height;
	vid.aspect = ((float)vid.height / (float)vid.width) * (320.0 / 240.0);

//	XSynchronize(vid_dpy, False);

	return true;

}

void SWVID_ShiftPalette(unsigned char *p)
{
	SWVID_SetPalette(p);
}



void SWVID_SetPalette(unsigned char *palette)
{

	int i;
	XColor colors[256];

	for(i=0;i<256;i++)
	{
		st2d_8to16table[i]= xlib_rgb(palette[i*3],
			palette[i*3+1],palette[i*3+2]);
		st3d_8to32table[i]= xlib_rgb32(palette[i*3],
			palette[i*3+1],palette[i*3+2]);

		d_8to24bgrtable[i] = (palette[i*3+0]<<16) + (palette[i*3+1]<<8) + (palette[i*3+2]<<0);
	}

	if (x_visinfo->class == PseudoColor && x_visinfo->depth == 8)
	{
		for (i=0 ; i<256 ; i++)
		{
			colors[i].pixel = i;
			colors[i].flags = DoRed|DoGreen|DoBlue;
			colors[i].red = palette[i*3] * 257;
			colors[i].green = palette[i*3+1] * 257;
			colors[i].blue = palette[i*3+2] * 257;
		}
		XStoreColors(vid_dpy, x_cmap, colors, 256);
	}

	if (palette != vid_curpal)
		memcpy(vid_curpal, palette, 768);
}

// Called at shutdown

void	SWVID_Shutdown (void)
{
	Con_Printf("VID_Shutdown\n");
	if (vid_dpy)
	{
		XUngrabPointer(vid_dpy,CurrentTime);
		XAutoRepeatOn(vid_dpy);
		if (x_win)
			XDestroyWindow(vid_dpy, x_win);
	}
//	XCloseDisplay(vid_dpy);
//	vid_dpy = NULL;
}

int XLateKey(XKeyEvent *ev)
{

	int key;
	char buf[64];
	KeySym keysym;

	key = 0;

	XLookupString(ev, buf, sizeof buf, &keysym, 0);

	switch(keysym)
	{
		case XK_KP_Page_Up:
		case XK_Page_Up:	 key = K_PGUP; break;

		case XK_KP_Page_Down:
		case XK_Page_Down:	 key = K_PGDN; break;

		case XK_KP_Home:
		case XK_Home:	 key = K_HOME; break;

		case XK_KP_End:
		case XK_End:	 key = K_END; break;

		case XK_KP_Left:
		case XK_Left:	 key = K_LEFTARROW; break;

		case XK_KP_Right:
		case XK_Right:	key = K_RIGHTARROW;		break;

		case XK_KP_Down:
		case XK_Down:	 key = K_DOWNARROW; break;

		case XK_KP_Up:
		case XK_Up:		 key = K_UPARROW;	 break;

		case XK_Escape: key = K_ESCAPE;		break;

		case XK_KP_Enter:
		case XK_Return: key = K_ENTER;		 break;

		case XK_Tab:		key = K_TAB;			 break;

		case XK_F1:		 key = K_F1;				break;

		case XK_F2:		 key = K_F2;				break;

		case XK_F3:		 key = K_F3;				break;

		case XK_F4:		 key = K_F4;				break;

		case XK_F5:		 key = K_F5;				break;

		case XK_F6:		 key = K_F6;				break;

		case XK_F7:		 key = K_F7;				break;

		case XK_F8:		 key = K_F8;				break;

		case XK_F9:		 key = K_F9;				break;

		case XK_F10:		key = K_F10;			 break;

		case XK_F11:		key = K_F11;			 break;

		case XK_F12:		key = K_F12;			 break;

		case XK_BackSpace: key = K_BACKSPACE; break;

		case XK_KP_Delete:
		case XK_Delete: key = K_DEL; break;

		case XK_Pause:	key = K_PAUSE;		 break;

		case XK_Shift_L:
		case XK_Shift_R:	key = K_SHIFT;		break;

		case XK_Execute: 
		case XK_Control_L: 
		case XK_Control_R:	key = K_CTRL;		 break;

		case XK_Alt_L:	
		case XK_Meta_L: 
		case XK_Alt_R:	
		case XK_Meta_R: key = K_ALT;			break;

		case XK_KP_Begin: key = K_AUX30;	break;

		case XK_Insert:
		case XK_KP_Insert: key = K_INS; break;

		case XK_KP_Multiply: key = '*'; break;
		case XK_KP_Add: key = '+'; break;
		case XK_KP_Subtract: key = '-'; break;
		case XK_KP_Divide: key = '/'; break;

#if 0
		case 0x021: key = '1';break;/* [!] */
		case 0x040: key = '2';break;/* [@] */
		case 0x023: key = '3';break;/* [#] */
		case 0x024: key = '4';break;/* [$] */
		case 0x025: key = '5';break;/* [%] */
		case 0x05e: key = '6';break;/* [^] */
		case 0x026: key = '7';break;/* [&] */
		case 0x02a: key = '8';break;/* [*] */
		case 0x028: key = '9';;break;/* [(] */
		case 0x029: key = '0';break;/* [)] */
		case 0x05f: key = '-';break;/* [_] */
		case 0x02b: key = '=';break;/* [+] */
		case 0x07c: key = '\'';break;/* [|] */
		case 0x07d: key = '[';break;/* [}] */
		case 0x07b: key = ']';break;/* [{] */
		case 0x022: key = '\'';break;/* ["] */
		case 0x03a: key = ';';break;/* [:] */
		case 0x03f: key = '/';break;/* [?] */
		case 0x03e: key = '.';break;/* [>] */
		case 0x03c: key = ',';break;/* [<] */
#endif

		default:
			key = *(unsigned char*)buf;
			if (key >= 'A' && key <= 'Z')
				key = key - 'A' + 'a';
//			fprintf(stdout, "case 0x0%x: key = ___;break;/* [%c] */\n", keysym);
			break;
	} 

	return key;
}

struct
{
	int key;
	int down;
} keyq[64];
int keyq_head=0;
int keyq_tail=0;

int config_notify=0;
int config_notify_width;
int config_notify_height;
						      
void GetEvent(void)
{
	qboolean grabmouse;
	XEvent x_event;
	int b;
   
	XNextEvent(vid_dpy, &x_event);
	switch(x_event.type) {
	case KeyPress:
		keyq[keyq_head].key = XLateKey(&x_event.xkey);
		keyq[keyq_head].down = true;
		keyq_head = (keyq_head + 1) & 63;
		break;
	case KeyRelease:
		keyq[keyq_head].key = XLateKey(&x_event.xkey);
		keyq[keyq_head].down = false;
		keyq_head = (keyq_head + 1) & 63;
		break;

	case MotionNotify:
		if (old_windowed_mouse)
		{
			mouse_x = (float) ((int)x_event.xmotion.x - (int)(vid.width/2));
			mouse_y = (float) ((int)x_event.xmotion.y - (int)(vid.height/2));
//printf("m: x=%d,y=%d, mx=%3.2f,my=%3.2f\n", 
//	x_event.xmotion.x, x_event.xmotion.y, mouse_x, mouse_y);

			/* move the mouse to the window center again */
			XSelectInput(vid_dpy,x_win,StructureNotifyMask|KeyPressMask
				|KeyReleaseMask|ExposureMask
				|ButtonPressMask
				|ButtonReleaseMask);
			XWarpPointer(vid_dpy,None,x_win,0,0,0,0, 
				(vid.width/2),(vid.height/2));
			XSelectInput(vid_dpy,x_win,StructureNotifyMask|KeyPressMask
				|KeyReleaseMask|ExposureMask
				|PointerMotionMask|ButtonPressMask
				|ButtonReleaseMask);
		}
		else
		{
			mouse_x = (float) (x_event.xmotion.x-p_mouse_x);
			mouse_y = (float) (x_event.xmotion.y-p_mouse_y);
			p_mouse_x=x_event.xmotion.x;
			p_mouse_y=x_event.xmotion.y;
		}
		break;

	case ButtonPress:
		b=-1;
		if (x_event.xbutton.button == 1)
			b = 0;
		else if (x_event.xbutton.button == 2)
			b = 2;
		else if (x_event.xbutton.button == 3)
			b = 1;
		else if (x_event.xbutton.button == 4)
			b = 4;
		else if (x_event.xbutton.button == 5)
			b = 5;
		if (b>=0)
			mouse_buttonstate |= 1<<b;
		break;

	case ButtonRelease:
		b=-1;
		if (x_event.xbutton.button == 1)
			b = 0;
		else if (x_event.xbutton.button == 2)
			b = 2;
		else if (x_event.xbutton.button == 3)
			b = 1;
		else if (x_event.xbutton.button == 4)
			b = 4;
		else if (x_event.xbutton.button == 5)
			b = 5;
		if (b>=0)
			mouse_buttonstate &= ~(1<<b);
		break;
	
	case ConfigureNotify:
//printf("config notify\n");
		config_notify_width = x_event.xconfigure.width;
		config_notify_height = x_event.xconfigure.height;
		config_notify = 1;
		break;

	case FocusIn:
		for (b = 0; b < 256; b++)
			Key_Event(b, false);
		focusedapp = true;
		break;
	case FocusOut:
		focusedapp = false;
		break;

	default:
#ifndef NOSHM
		if (doShm && x_event.type == x_shmeventtype)
			oktodraw = true;
#endif
		break;
	}

	grabmouse = _windowed_mouse.value && focusedapp;
	if (key_dest == key_console)
		grabmouse = false;

	if (!mouseactive)
		grabmouse = false;	//nomouse is a misnomer
   
	if (old_windowed_mouse != grabmouse)
	{
		old_windowed_mouse = grabmouse;

		if (!grabmouse)
		{
			/* ungrab the pointer */
			XUngrabPointer(vid_dpy,CurrentTime);
		}
		else
		{
			/* grab the pointer */
			XGrabPointer(vid_dpy,x_win,True,0,GrabModeAsync,
				GrabModeAsync,x_win,None,CurrentTime);
			/* make the keyboard follow the mouse too (wmaker doesn't activate us) */
			XSetInputFocus(vid_dpy, x_win, RevertToPointerRoot, CurrentTime);
		}
	}
}

// flushes the given rectangles from the view buffer to the screen

void	SWVID_Update (vrect_t *rects)
{

// if the window changes dimension, skip this frame

	if (config_notify)
	{
		int ow, oh;
		ow = vid.width;
		oh = vid.height;
		config_notify = 0;
		vid.width = config_notify_width & ~7;
		vid.height = config_notify_height;
		if (ow != vid.width || oh != vid.height)	//did the size actually change?
		{	//yes
#ifndef NOSHM
			if (doShm)
				ResetSharedFrameBuffers();
			else
#endif
				ResetFrameBuffer();
			vid.rowbytes = x_framebuffer[0]->bytes_per_line/r_pixbytes;
			vid.buffer = x_framebuffer[current_framebuffer]->data;
			vid.conbuffer = vid.buffer;
			vid.conwidth = vid.width;
			vid.conheight = vid.height;
			vid.conrowbytes = vid.rowbytes;
			vid.recalc_refdef = 1;				// force a surface cache flush
			Con_CheckResize();
			Con_Clear_f();
			return;
		}
	}
#ifndef NOSHM
	if (doShm)
	{

		while (rects)
		{
			if (x_visinfo->depth == 16)
				st2_fixup( x_framebuffer[current_framebuffer], 
					rects->x, rects->y, rects->width,
					rects->height);
			if (x_visinfo->depth == 24)
				st3_fixup( x_framebuffer[current_framebuffer],
					rects->x, rects->y, rects->width,
					rects->height);
			if (!XShmPutImage(vid_dpy, x_win, x_gc,
				x_framebuffer[current_framebuffer], rects->x, rects->y,
				rects->x, rects->y, rects->width, rects->height, True))
					Sys_Error("VID_Update: XShmPutImage failed\n");
			oktodraw = false;
			while (!oktodraw) GetEvent();
			rects = rects->pnext;
		}
		current_framebuffer = !current_framebuffer;
		vid.buffer = x_framebuffer[current_framebuffer]->data;
		vid.conbuffer = vid.buffer;
		XSync(vid_dpy, False);
	}
	else
#endif
	{
		while (rects)
		{
			if (x_visinfo->depth == 16)
				st2_fixup( x_framebuffer[current_framebuffer], 
					rects->x, rects->y, rects->width,
					rects->height);
			else if (x_visinfo->depth == 24)
				st3_fixup( x_framebuffer[current_framebuffer],
					rects->x, rects->y, rects->width,
					rects->height);
			XPutImage(vid_dpy, x_win, x_gc, x_framebuffer[0], rects->x,
				rects->y, rects->x, rects->y, rects->width, rects->height);
			rects = rects->pnext;
		}
		XSync(vid_dpy, False);
	}

}

#ifdef RGLQUAKE
void GLSys_SendKeyEvents(void);	//merged.
#endif

void Sys_SendKeyEvents(void)
{
#ifdef RGLQUAKE
	if (qrenderer != QR_SOFTWARE)
	{
		GLSys_SendKeyEvents();
		return;
	}
#endif

// get events from x server
	if (vid_dpy)
	{
		while (XPending(vid_dpy)) GetEvent();
		while (keyq_head != keyq_tail)
		{
			Key_Event(keyq[keyq_tail].key, keyq[keyq_tail].down);
			keyq_tail = (keyq_tail + 1) & 63;
		}
	}
}

#if 0
char *Sys_ConsoleInput (void)
{

	static char	text[256];
	int		len;
	fd_set  readfds;
	int		ready;
	struct timeval timeout;

	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	FD_ZERO(&readfds);
	FD_SET(0, &readfds);
	ready = select(1, &readfds, 0, 0, &timeout);

	if (ready>0)
	{
		len = read (0, text, sizeof(text));
		if (len >= 1)
		{
			text[len-1] = 0;	// rip off the /n and terminate
			return text;
		}
	}

	return 0;
	
}
#endif

void SWD_BeginDirectRect (int x, int y, qbyte *pbitmap, int width, int height)
{
// direct drawing of the "accessing disk" icon isn't supported under Linux
}

void SWD_EndDirectRect (int x, int y, int width, int height)
{
// direct drawing of the "accessing disk" icon isn't supported under Linux
}

void IN_ReInit(void)
{
	if ( COM_CheckParm ("-nomouse") )
		return;
	mouse_x = mouse_y = 0.0;
	mouseactive = mouse_avail = 1;
}

void IN_Init (void)
{
	Cvar_Register (&_windowed_mouse, "Input Controls");
	Cvar_Register (&m_filter, "Input Controls");
	Cvar_Register (&m_accel, "Input Controls");
#ifdef IN_XFLIP
	Cvar_Register (&in_xflip, "Input Controls");
#endif

	IN_ReInit();
}

void IN_Shutdown (void)
{
	mouseactive = mouse_avail = 0;
	focusedapp = false;
}

void IN_Commands (void)
{
	int i;
   
	if (!mouse_avail) return;
   
	for (i=0 ; i<mouse_buttons ; i++)
	{
		if ( (mouse_buttonstate & (1<<i)) && !(mouse_oldbuttonstate & (1<<i)) )
			Key_Event (K_MOUSE1 + i, true);

		if ( !(mouse_buttonstate & (1<<i)) && (mouse_oldbuttonstate & (1<<i)) )
			Key_Event (K_MOUSE1 + i, false);
	}
	mouse_oldbuttonstate = mouse_buttonstate;
}

extern int mousecursor_x, mousecursor_y;
void IN_Move (float *movements, int pnum)
{
	float mx, my;
	float mouse_deltadist;

	if (!mouse_avail)
	{
		mousecursor_x = p_mouse_x;	//absolute offsets.
		mousecursor_y = p_mouse_y;
		return;
	}

	if (pnum != 0)
		return;	//we're lazy today.

	mousecursor_x += mouse_x;
	mousecursor_y += mouse_y;

	if (mousecursor_x < 0)
		mousecursor_x = 0;
	if (mousecursor_y < 0)
		mousecursor_y = 0;
	if (mousecursor_x > vid.width)
		mousecursor_x = vid.width;
	if (mousecursor_y > vid.height)
		mousecursor_y = vid.height;
   
	if (m_filter.value)
	{
		float fraction = bound(0, m_filter.value, 2) * 0.5;
		mouse_x = (mouse_x*(1-fraction) + old_mouse_x*fraction);
		mouse_y = (mouse_y*(1-fraction) + old_mouse_y*fraction);
	}

	old_mouse_x = mx;
	old_mouse_y = my;
   
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

#ifdef IN_XFLIP
	if(in_xflip.value) mouse_x *= -1;
#endif
	if (movements)
   	{
		if ( (in_strafe.state[pnum] & 1) || (lookstrafe.value && (in_mlook.state[pnum] & 1) ))
			movements[1] += m_side.value * mouse_x;
		else
			cl.viewangles[pnum][YAW] -= m_yaw.value * mouse_x;
		if (in_mlook.state[pnum] & 1)
			V_StopPitchDrift (pnum);
   
		if ( (in_mlook.state[pnum] & 1) && !(in_strafe.state[pnum] & 1))
		{
			cl.viewangles[pnum][PITCH] += m_pitch.value * mouse_y;
			CL_ClampPitch(pnum);
		}
		else
		{
			if ((in_strafe.state[pnum] & 1) && noclip_anglehack)
				movements[2] -= m_forward.value * mouse_y;
			else
				movements[0] -= m_forward.value * mouse_y;
		}
	}
	mouse_x = mouse_y = 0.0;
}

void SWVID_LockBuffer (void)
{
}

void SWVID_UnlockBuffer (void)
{
}



int SWVID_ForceUnlockedAndReturnState (void)
{
	return 0;
}

void SWVID_ForceLockState (int lk)
{
}

void SWVID_SetCaption(char *text)
{
	XStoreName(vid_dpy, x_win, text);
}

