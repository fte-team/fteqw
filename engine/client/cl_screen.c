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

// cl_screen.c -- master for refresh, status bar, console, chat, notify, etc

#include "quakedef.h"
#ifdef GLQUAKE
#include "glquake.h"//would prefer not to have this
#endif
#include "shader.h"

//name of the current backdrop for the loading screen
char levelshotname[MAX_QPATH];


void RSpeedShow(void)
{
	int i;
	static int samplerspeeds[RSPEED_MAX];
	static int samplerquant[RQUANT_MAX];
	char *RSpNames[RSPEED_MAX];
	char *RQntNames[RQUANT_MAX];
	char *s;
	static int framecount;

	if (!r_speeds.ival)
		return;

	memset(RSpNames, 0, sizeof(RSpNames));
	RSpNames[RSPEED_TOTALREFRESH] = "Total refresh";
	RSpNames[RSPEED_PROTOCOL] = "Protocol";
	RSpNames[RSPEED_LINKENTITIES] = "Entity setup";
	RSpNames[RSPEED_WORLDNODE] = "World walking";
	RSpNames[RSPEED_WORLD] = "World rendering";
	RSpNames[RSPEED_DYNAMIC] = "Lightmap updates";
	RSpNames[RSPEED_PARTICLES] = "Particle phys/sort";
	RSpNames[RSPEED_PARTICLESDRAW] = "Particle drawing";
	RSpNames[RSPEED_2D] = "2d elements";
	RSpNames[RSPEED_SERVER] = "Server";

	RSpNames[RSPEED_DRAWENTITIES] = "Entity rendering";

	RSpNames[RSPEED_PALETTEFLASHES] = "Palette flashes";
	RSpNames[RSPEED_STENCILSHADOWS] = "Stencil Shadows";

	RSpNames[RSPEED_FULLBRIGHTS] = "World fullbrights";

	RSpNames[RSPEED_FINISH] = "glFinish";

	RQntNames[RQUANT_MSECS] = "Microseconds";
	RQntNames[RQUANT_EPOLYS] = "Entity Polys";
	RQntNames[RQUANT_WPOLYS] = "World Polys";
	RQntNames[RQUANT_DRAWS] = "Draw Calls";
	RQntNames[RQUANT_2DBATCHES] = "2d Batches";
	RQntNames[RQUANT_WORLDBATCHES] = "World Batches";
	RQntNames[RQUANT_ENTBATCHES] = "Ent Batches";
	RQntNames[RQUANT_SHADOWFACES] = "Shadow Faces";
	RQntNames[RQUANT_SHADOWEDGES] = "Shadow edges";
	RQntNames[RQUANT_LITFACES] = "Lit faces";

	if (r_speeds.ival > 1)
	{
		for (i = 0; i < RSPEED_MAX; i++)
		{
			s = va("%i %-20s", samplerspeeds[i], RSpNames[i]);
			Draw_FunString(vid.width-strlen(s)*8, i*8, s);
		}
	}
	for (i = 0; i < RQUANT_MAX; i++)
	{
		s = va("%i %-20s", samplerquant[i], RQntNames[i]);
		Draw_FunString(vid.width-strlen(s)*8, (i+RSPEED_MAX)*8, s);
	}
	s = va("%f %-20s", 100000000.0f/samplerspeeds[RSPEED_TOTALREFRESH], "Framerate");
	Draw_FunString(vid.width-strlen(s)*8, (i+RSPEED_MAX)*8, s);

	if (++framecount>=100)
	{
		for (i = 0; i < RSPEED_MAX; i++)
		{
			samplerspeeds[i] = rspeeds[i];
			rspeeds[i] = 0;
		}
		for (i = 0; i < RQUANT_MAX; i++)
		{
			samplerquant[i] = rquant[i];
			rquant[i] = 0;
		}
		framecount=0;
	}
}


/*

background clear
rendering
turtle/net/ram icons
sbar
centerprint / slow centerprint
notify lines
intermission / finale overlay
loading plaque
console
menu

required background clears
required update regions


syncronous draw mode or async
One off screen buffer, with updates either copied or xblited
Need to double buffer?


async draw will require the refresh area to be cleared, because it will be
xblited, but sync draw can just ignore it.

sync
draw

CenterPrint ()
SlowPrint ()
Screen_Update ();
Con_Printf ();

net
turn off messages option

the refresh is always rendered, unless the console is full screen


console is:
	notify lines
	half
	full


*/



int scr_chatmode;
extern cvar_t scr_chatmodecvar;


int mouseusedforgui;
int mousecursor_x, mousecursor_y;
int mousemove_x, mousemove_y;

float           scr_con_current;
float           scr_conlines;           // lines of console to display

qboolean		scr_con_forcedraw;

extern cvar_t          scr_viewsize;
extern cvar_t          scr_fov;
extern cvar_t          scr_conspeed;
extern cvar_t          scr_centertime;
extern cvar_t          scr_showturtle;
extern cvar_t			scr_turtlefps;
extern cvar_t          scr_showpause;
extern cvar_t          scr_printspeed;
extern cvar_t			scr_allowsnap;
extern cvar_t			scr_sshot_type;
extern cvar_t			scr_sshot_compression;
extern  		cvar_t  crosshair;
extern cvar_t			scr_consize;

qboolean        scr_initialized;                // ready to draw

mpic_t          *scr_net;
mpic_t          *scr_turtle;

int                     clearconsole;
int                     clearnotify;

int                     sb_lines;

viddef_t        vid;                            // global video state

vrect_t         scr_vrect;

qboolean        scr_disabled_for_loading;
qboolean        scr_drawloading;
float           scr_disabled_time;

float oldsbar = 0;

void SCR_ScreenShot_f (void);
void SCR_RSShot_f (void);
void SCR_CPrint_f(void);

cvar_t	show_fps	= SCVARF("show_fps", "0", CVAR_ARCHIVE);
cvar_t	show_fps_x	= SCVAR("show_fps_x", "-1");
cvar_t	show_fps_y	= SCVAR("show_fps_y", "-1");
cvar_t	show_clock	= SCVAR("cl_clock", "0");
cvar_t	show_clock_x	= SCVAR("cl_clock_x", "0");
cvar_t	show_clock_y	= SCVAR("cl_clock_y", "-1");
cvar_t	show_gameclock	= SCVAR("cl_gameclock", "0");
cvar_t	show_gameclock_x	= SCVAR("cl_gameclock_x", "0");
cvar_t	show_gameclock_y	= SCVAR("cl_gameclock_y", "-1");
cvar_t	show_speed	= SCVAR("show_speed", "0");
cvar_t	show_speed_x	= SCVAR("show_speed_x", "-1");
cvar_t	show_speed_y	= SCVAR("show_speed_y", "-9");

extern char cl_screengroup[];
void CLSCR_Init(void)
{
	Cmd_AddCommand("cprint", SCR_CPrint_f);

	Cvar_Register(&show_fps, cl_screengroup);
	Cvar_Register(&show_fps_x, cl_screengroup);
	Cvar_Register(&show_fps_y, cl_screengroup);
	Cvar_Register(&show_clock, cl_screengroup);
	Cvar_Register(&show_clock_x, cl_screengroup);
	Cvar_Register(&show_clock_y, cl_screengroup);
	Cvar_Register(&show_gameclock, cl_screengroup);
	Cvar_Register(&show_gameclock_x, cl_screengroup);
	Cvar_Register(&show_gameclock_y, cl_screengroup);
	Cvar_Register(&show_speed, cl_screengroup);
	Cvar_Register(&show_speed_x, cl_screengroup);
	Cvar_Register(&show_speed_y, cl_screengroup);
}

/*
===============================================================================

CENTER PRINTING

===============================================================================
*/

typedef struct {
	unsigned int	flags;

	conchar_t		string[1024];
	unsigned int charcount;
	float			time_start;   // for slow victory printing
	float			time_off;
	int				erase_lines;
	int				erase_center;
} cprint_t;

cprint_t scr_centerprint[MAX_SPLITS];

// SCR_StringToRGB: takes in "<index>" or "<r> <g> <b>" and converts to an RGB vector
void SCR_StringToRGB (char *rgbstring, float *rgb, float rgbinputscale)
{
	char *t;

	rgbinputscale = 1/rgbinputscale;
	t = strstr(rgbstring, " ");

	if (!t) // use standard coloring
	{
		qbyte *pal;
		int i = atoi(rgbstring);
		i = bound(0, i, 255);

		pal = host_basepal;

		pal += (i * 3);
		// convert r8g8b8 to rgb floats
		rgb[0] = (float)(pal[0]);
		rgb[1] = (float)(pal[1]);
		rgb[2] = (float)(pal[2]);

		VectorScale(rgb, 1/255.0, rgb);
	}
	else // use RGB coloring
	{
		t++;
		rgb[0] = atof(rgbstring);
		rgb[1] = atof(t);
		t = strstr(t, " "); // find last value
		if (t)
			rgb[2] = atof(t+1);
		else
			rgb[2] = 0.0;
		VectorScale(rgb, rgbinputscale, rgb);
	} // i contains the crosshair color
}

/*
==============
SCR_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void SCR_CenterPrint (int pnum, char *str, qboolean skipgamecode)
{
	cprint_t *p;
	if (!skipgamecode)
	{
#ifdef CSQC_DAT
		if (CSQC_CenterPrint(pnum, str))	//csqc nabbed it.
			return;
#endif
	}

	if (Cmd_AliasExist("f_centerprint", RESTRICT_LOCAL))
	{
		cvar_t *var;
		var = Cvar_FindVar ("scr_centerprinttext");
		if (!var)
			Cvar_Get("scr_centerprinttext", "", 0, "Script Notifications");
		Cvar_Set(var, str);
		Cbuf_AddText("f_centerprint\n", RESTRICT_LOCAL);
	}

	p = &scr_centerprint[pnum];
	p->flags = 0;
	if (cl.intermission)
		p->flags |= CPRINT_TYPEWRITER | CPRINT_PERSIST;

	while (*str == '/')
	{
		if (str[1] == '.')
		{
/* /. means text actually starts after, no more flags */
			str+=2;
			break;
		}
		else if (str[1] == 'P')
			p->flags |= CPRINT_PERSIST | CPRINT_BACKGROUND;
		else if (str[1] == 'O')
			p->flags = CPRINT_OBITUARTY;
		else if (str[1] == 'B')
			p->flags |= CPRINT_BALIGN;	//Note: you probably want to add some blank lines...
		else if (str[1] == 'T')
			p->flags |= CPRINT_TALIGN;
		else if (str[1] == 'L')
			p->flags |= CPRINT_LALIGN;
		else if (str[1] == 'R')
			p->flags |= CPRINT_RALIGN;
		else
			break;
		str += 2;
	}
	p->charcount = COM_ParseFunString(CON_WHITEMASK, str, p->string, sizeof(p->string), false) - p->string;
	p->time_off = scr_centertime.value;
	p->time_start = cl.time;
}

void SCR_CPrint_f(void)
{
	SCR_CenterPrint(0, Cmd_Args(), true);
}

void SCR_EraseCenterString (void)
{
	cprint_t *p;
	int pnum;
	int		y;

	if (cl.splitclients>1)
		return;	//no viewsize with split

	for (pnum = 0; pnum < cl.splitclients; pnum++)
	{
		p = &scr_centerprint[pnum];

		if (p->erase_center++ > vid.numpages)
		{
			p->erase_lines = 0;
			continue;
		}

		y = vid.height>>1;
		R2D_TileClear (0, y, vid.width, min(8*p->erase_lines, vid.height - y - 1));
	}
}

#define MAX_CPRINT_LINES 128
void SCR_DrawCenterString (vrect_t *rect, cprint_t *p)
{
	int             l;
	int             y, x;
	int				left;
	int				right;
	int				top;
	int				bottom;
	int             remaining;

	conchar_t *line_start[MAX_CPRINT_LINES];
	conchar_t *line_end[MAX_CPRINT_LINES];
	int linecount;


// the finale prints the characters one at a time
	if (p->flags & CPRINT_TYPEWRITER)
		remaining = scr_printspeed.value * (cl.time - p->time_start);
	else
		remaining = 9999;

	p->erase_center = 0;

	if (p->flags & CPRINT_BACKGROUND)
	{	//hexen2 style plaque.
		if (rect->width > 320)
		{
			rect->x = (rect->x + rect->width/2) - (160);
			rect->width = 320;
		}
		if (rect->width < 32)
			return;
		rect->x += 16;
		rect->width -= 32;
	}

	Font_BeginString(font_conchar, rect->x, rect->y, &left, &top);
	Font_BeginString(font_conchar, rect->x+rect->width, rect->y+rect->height, &right, &bottom);
	linecount = Font_LineBreaks(p->string, p->string + p->charcount, right - left, MAX_CPRINT_LINES, line_start, line_end);

	if (p->flags & CPRINT_TALIGN)
		y = top;
	else if (p->flags & CPRINT_BALIGN)
		y = bottom - Font_CharHeight()*linecount;
	else if (p->flags & CPRINT_OBITUARTY)
		//'obituary' messages appear at the bottom of the screen
		y = (bottom-top - Font_CharHeight()*linecount) * 0.65 + top;
	else if (p->flags & CPRINT_TYPEWRITER)
		Font_BeginString(font_conchar, 48, rect->y, &y, &top);
	else
	{
		if (linecount <= 4)
		{
			//small messages appear above and away from the crosshair
			y = (bottom-top - Font_CharHeight()*linecount) * 0.35 + top;
		}
		else
		{
			//longer messages are fully centered
			y = (bottom-top - Font_CharHeight()*linecount) * 0.5 + top;
		}
	}

	if (p->flags & CPRINT_BACKGROUND)
	{	//hexen2 style plaque.
		int px, py, pw;
		px = rect->x;
		py = (     y * vid.height) / (float)vid.pixelheight;
		pw = rect->width+8;
		Draw_TextBox(px-16, py-8-8, pw/8, linecount+2);
	}

	for (l = 0; l < linecount; l++, y += Font_CharHeight())
	{
		if (p->flags & CPRINT_RALIGN)
		{
			x = right - Font_LineWidth(line_start[l], line_end[l]);
		}
		else if (p->flags & CPRINT_LALIGN)
			x = left;
		else
		{
			x = (right + left - Font_LineWidth(line_start[l], line_end[l]))/2;
		}

		remaining -= line_end[l]-line_start[l];
		if (remaining <= 0)
		{
			line_end[l] += remaining;
			if (line_end[l] <= line_start[l])
				break;
		}
		Font_LineDraw(x, y, line_start[l], line_end[l]);
	}
	Font_EndString(font_conchar);
}

void SCR_CheckDrawCenterString (void)
{
extern qboolean sb_showscores;
	int pnum;
	cprint_t *p;
	vrect_t rect;

	for (pnum = 0; pnum < cl.splitclients; pnum++)
	{
		p = &scr_centerprint[pnum];

		if (p->time_off <= 0 && !cl.intermission && !(p->flags & CPRINT_PERSIST))
			continue;	//'/P' prefix doesn't time out

		p->time_off -= host_frametime;

		if (key_dest != key_game)	//don't let progs guis/centerprints interfere with the game menu
			continue;

		if (sb_showscores)	//this was annoying
			continue;

		SCR_VRectForPlayer(&rect, pnum);
		SCR_DrawCenterString(&rect, p);
	}
}

void R_DrawTextField(int x, int y, int w, int h, char *text, unsigned int defaultmask, unsigned int fieldflags)
{
	cprint_t p;
	vrect_t r;

	r.x = x;
	r.y = y;
	r.width = w;
	r.height = h;

	p.flags = fieldflags;
	p.charcount = COM_ParseFunString(defaultmask, text, p.string, sizeof(p.string), false) - p.string;
	p.time_off = scr_centertime.value;
	p.time_start = cl.time;

	SCR_DrawCenterString(&r, &p);
}

void SCR_DrawCursor(int prydoncursornum)
{
	extern cvar_t cl_cursor, cl_cursorbias, cl_cursorsize;
	extern int mousecursor_x, mousecursor_y;
	mpic_t *p;
	if (!*cl_cursor.string || prydoncursornum>1)
		p = R2D_SafeCachePic(va("gfx/prydoncursor%03i.lmp", prydoncursornum));
	else
		p = R2D_SafeCachePic(cl_cursor.string);
	if (p)
	{
		R2D_ImageColours(1, 1, 1, 1);
		R2D_Image(mousecursor_x-cl_cursorbias.value, mousecursor_y-cl_cursorbias.value, cl_cursorsize.value, cl_cursorsize.value, 0, 0, 1, 1, p);
	}
	else
	{
		int x, y;
		Font_BeginString(font_conchar, mousecursor_x, mousecursor_y, &x, &y);
		x -= Font_CharWidth('+' | 0xe000 | CON_WHITEMASK)/2;
		y -= Font_CharHeight()/2;
		Font_DrawChar(x, y, '+' | 0xe000 | CON_WHITEMASK);
		Font_EndString(font_conchar);
	}

}

////////////////////////////////////////////////////////////////
//TEI_SHOWLMP2 (not 3)
//
typedef struct showpic_s {
	struct showpic_s *next;
	qbyte zone;
	short x, y;
	char *name;
	char *picname;
} showpic_t;
showpic_t *showpics;

static void SP_RecalcXY ( float *xx, float *yy, int origin )
{
	int midx, midy;
	float x,y;

	x = xx[0];
	y = yy[0];

	midy = vid.height * 0.5;// >>1
	midx = vid.width * 0.5;// >>1

	// Tei - new showlmp
	switch ( origin )
	{
		case SL_ORG_NW:
			break;
		case SL_ORG_NE:
			x = vid.width - x;//Inv
			break;
		case SL_ORG_SW:
			y = vid.height - y;//Inv
			break;
		case SL_ORG_SE:
			y = vid.height - y;//inv
			x = vid.width - x;//Inv
			break;
		case SL_ORG_CC:
			y = midy + (y - 8000);//NegCoded
			x = midx + (x - 8000);//NegCoded
			break;
		case SL_ORG_CN:
			x = midx + (x - 8000);//NegCoded
			break;
		case SL_ORG_CS:
			x = midx + (x - 8000);//NegCoded
			y = vid.height - y;//Inverse
			break;
		case SL_ORG_CW:
			y = midy + (y - 8000);//NegCoded
			break;
		case SL_ORG_CE:
			y = midy + (y - 8000);//NegCoded
			x = vid.height - x; //Inverse
			break;
		default:
			break;
	}

	xx[0] = x;
	yy[0] = y;
}
void SCR_ShowPics_Draw(void)
{
	downloadlist_t *failed;
	float x, y;
	showpic_t *sp;
	mpic_t *p;
	for (sp = showpics; sp; sp = sp->next)
	{
		x = sp->x;
		y = sp->y;
		SP_RecalcXY(&x, &y, sp->zone);
		if (!*sp->picname)
			continue;

		for (failed = cl.faileddownloads; failed; failed = failed->next)
		{	//don't try displaying ones that we know to have failed.
			if (!strcmp(failed->rname, sp->picname))
				break;
		}
		if (failed)
			continue;

		p = R2D_SafeCachePic(sp->picname);
		if (!p)
			continue;
		R2D_ScalePic(x, y, p->width, p->height, p);
	}
}

void SCR_ShowPic_Clear(void)
{
	showpic_t *sp;
	int pnum;

	for (pnum = 0; pnum < MAX_SPLITS; pnum++)
	{
		scr_centerprint[pnum].flags = 0;
		scr_centerprint[pnum].charcount = 0;
	}

	while((sp = showpics))
	{
		showpics = sp->next;

		Z_Free(sp->name);
		Z_Free(sp->picname);
		Z_Free(sp);
	}
}

showpic_t *SCR_ShowPic_Find(char *name)
{
	showpic_t *sp, *last;
	for (sp = showpics; sp; sp = sp->next)
	{
		if (!strcmp(sp->name, name))
			return sp;
	}

	if (showpics)
	{
		for (last = showpics; last->next; last = last->next)
			;
	}
	else
		last = NULL;
	sp = Z_Malloc(sizeof(showpic_t));
	if (last)
	{
		last->next = sp;
		sp->next = NULL;
	}
	else
	{
		sp->next = showpics;
		showpics = sp;
	}
	sp->name = Z_Malloc(strlen(name)+1);
	strcpy(sp->name, name);
	sp->picname = Z_Malloc(1);
	sp->x = 0;
	sp->y = 0;
	sp->zone = 0;

	return sp;
}

void SCR_ShowPic_Create(void)
{
	int zone = MSG_ReadByte();
	showpic_t *sp;
	char *s;

	sp = SCR_ShowPic_Find(MSG_ReadString());

	s = MSG_ReadString();

	Z_Free(sp->picname);
	sp->picname = Z_Malloc(strlen(s)+1);
	strcpy(sp->picname, s);
	sp->zone = zone;
	sp->x = MSG_ReadShort();
	sp->y = MSG_ReadShort();

	CL_CheckOrEnqueDownloadFile(sp->picname, sp->picname, 0);
}

void SCR_ShowPic_Hide(void)
{
	showpic_t *sp, *prev;

	sp = SCR_ShowPic_Find(MSG_ReadString());

	if (sp == showpics)
		showpics = sp->next;
	else
	{
		for (prev = showpics; prev->next != sp; prev = prev->next)
			;
		prev->next = sp->next;
	}

	Z_Free(sp->name);
	Z_Free(sp->picname);
	Z_Free(sp);
}

void SCR_ShowPic_Move(void)
{
	int zone = MSG_ReadByte();
	showpic_t *sp;

	sp = SCR_ShowPic_Find(MSG_ReadString());

	sp->zone = zone;
	sp->x = MSG_ReadShort();
	sp->y = MSG_ReadShort();
}

void SCR_ShowPic_Update(void)
{
	showpic_t *sp;
	char *s;

	sp = SCR_ShowPic_Find(MSG_ReadString());

	s = MSG_ReadString();

	Z_Free(sp->picname);
	sp->picname = Z_Malloc(strlen(s)+1);
	strcpy(sp->picname, s);

	CL_CheckOrEnqueDownloadFile(sp->picname, sp->picname, 0);
}

void SCR_ShowPic_Script_f(void)
{
	char *imgname;
	char *name;
	int x, y;
	int zone;
	showpic_t *sp;

	imgname = Cmd_Argv(1);
	name = Cmd_Argv(2);
	x = atoi(Cmd_Argv(3));
	y = atoi(Cmd_Argv(4));
	zone = atoi(Cmd_Argv(5));



	sp = SCR_ShowPic_Find(name);

	Z_Free(sp->picname);
	sp->picname = Z_Malloc(strlen(imgname)+1);
	strcpy(sp->picname, imgname);

	sp->zone = zone;
	sp->x = x;
	sp->y = y;
}

//=============================================================================

/*
====================
CalcFov
====================
*/
float CalcFov (float fov_x, float width, float height)
{
    float   a;
    float   x;

    if (fov_x < 1 || fov_x > 179)
            Sys_Error ("Bad fov: %f", fov_x);

	x = fov_x/360*M_PI;
	x = tan(x);
    x = width/x;

    a = atan (height/x);

    a = a*360/M_PI;

    return a;
}

void SCR_Fov_Callback (struct cvar_s *var, char *oldvalue)
{
	if (var->value < 10)
	{
		Cvar_ForceSet (var, "10");
		return;
	}
	if (var->value > 170)
	{
		Cvar_ForceSet (var, "170");
		return;
	}

	vid.recalc_refdef = true;
}

void SCR_Viewsize_Callback (struct cvar_s *var, char *oldvalue)
{
	if (var->value < 30)
	{
		Cvar_ForceSet (var, "30");
		return;
	}
	if (var->value > 120)
	{
		Cvar_ForceSet (var, "120");
		return;
	}

	vid.recalc_refdef = true;
}

void CL_Sbar_Callback(struct cvar_s *var, char *oldvalue)
{
	vid.recalc_refdef = true;
}

/*
=================
SCR_CalcRefdef

Must be called whenever vid changes
Internal use only
=================
*/
void SCR_CalcRefdef (void)
{
	float           size;
	int             h;
	qboolean		full = false;

	vid.recalc_refdef = 0;

// force the status bar to redraw
	Sbar_Changed ();

//========================================

	r_refdef.flags = 0;

// intermission is always full screen
	if (cl.intermission)
		size = 120;
	else
		size = scr_viewsize.value;

#ifdef Q2CLIENT
	if (cls.protocol == CP_QUAKE2)	//q2 never has a hud.
		sb_lines = 0;
	else
#endif

	if (size >= 120)
		sb_lines = 0;           // no status bar at all
	else if (size >= 110)
		sb_lines = 24;          // no inventory
	else
		sb_lines = 24+16+8;

	if (scr_viewsize.value >= 100.0 || scr_chatmode)
	{
		full = true;
		size = 100.0;
	}
	else
		size = scr_viewsize.value;

	if (cl.intermission)
	{
		full = true;
		size = 100.0;
		sb_lines = 0;
	}
	size /= 100.0;

	if (cl_sbar.value!=1 && full)
		h = vid.height;
	else
		h = vid.height - sb_lines;

	r_refdef.vrect.width = vid.width * size;
	if (r_refdef.vrect.width < 96)
	{
		size = 96.0 / r_refdef.vrect.width;
		r_refdef.vrect.width = 96;      // min for icons
	}

	r_refdef.vrect.height = vid.height * size;
	if (cl_sbar.value==1 || !full)
	{
  		if (r_refdef.vrect.height > vid.height - sb_lines)
  			r_refdef.vrect.height = vid.height - sb_lines;
	}
	else if (r_refdef.vrect.height > vid.height)
			r_refdef.vrect.height = vid.height;

	r_refdef.vrect.x = (vid.width - r_refdef.vrect.width)/2;
	if (full)
		r_refdef.vrect.y = 0;
	else
		r_refdef.vrect.y = (h - r_refdef.vrect.height)/2;

	if (scr_chatmode)
	{
		if (scr_chatmode != 2)
			r_refdef.vrect.height= r_refdef.vrect.y=vid.height/2;
		r_refdef.vrect.width = r_refdef.vrect.x=vid.width/2;
		if (r_refdef.vrect.width<320 || r_refdef.vrect.height<200)	//disable hud if too small
			sb_lines=0;
	}

	r_refdef.fov_x = scr_fov.value;
	if (cl.stats[0][STAT_VIEWZOOM])
		r_refdef.fov_x *= cl.stats[0][STAT_VIEWZOOM]/255.0f;

	if (r_refdef.fov_x < 1)
		r_refdef.fov_x = 1;
	else if (r_refdef.fov_x > 170)
		r_refdef.fov_x = 170;


	r_refdef.fov_y = CalcFov (r_refdef.fov_x, r_refdef.vrect.width, r_refdef.vrect.height);




//	r_refdef.vrect.height/=2;

	scr_vrect = r_refdef.vrect;
}

void SCR_CrosshairPosition(int pnum, int *x, int *y)
{
	extern cvar_t cl_crossx, cl_crossy, crosshaircorrect, v_viewheight;

	vrect_t rect;
	SCR_VRectForPlayer(&rect, pnum);

	if (cl.worldmodel && crosshaircorrect.ival)
	{
		float adj;
		trace_t tr;
		vec3_t end;
		vec3_t start;
		vec3_t right, up, fwds;

		AngleVectors(cl.simangles[pnum], fwds, right, up);

		VectorCopy(cl.simorg[pnum], start);
		start[2]+=16;
		VectorMA(start, 100000, fwds, end);

		memset(&tr, 0, sizeof(tr));
		tr.fraction = 1;
		cl.worldmodel->funcs.Trace(cl.worldmodel, 0, 0, NULL, start, end, vec3_origin, vec3_origin, &tr);
		start[2]-=16;
		if (tr.fraction == 1)
		{
			*x = rect.x + rect.width/2 + cl_crossx.value;
			*y = rect.y + rect.height/2 + cl_crossy.value;
			return;
		}
		else
		{
			adj=cl.viewheight[pnum];
			if (v_viewheight.value < -7)
				adj+=-7;
			else if (v_viewheight.value > 4)
				adj+=4;
			else
				adj+=v_viewheight.value;

			start[2]+=adj;
			Matrix4_Project(tr.endpos, end, cl.simangles[pnum], start, r_refdef.fov_x, r_refdef.fov_y);
			*x = rect.x+rect.width*end[0];
			*y = rect.y+rect.height*(1-end[1]);
			return;
		}
	}
	else
	{
		*x = rect.x + rect.width/2 + cl_crossx.value;
		*y = rect.y + rect.height/2 + cl_crossy.value;
		return;
	}
}

/*
=================
SCR_SizeUp_f

Keybinding command
=================
*/
void SCR_SizeUp_f (void)
{
	if (Cmd_FromGamecode())
		Cvar_ForceSet(&scr_viewsize,va("%i", scr_viewsize.ival+10));
	else
		Cvar_SetValue (&scr_viewsize,scr_viewsize.value+10);
}


/*
=================
SCR_SizeDown_f

Keybinding command
=================
*/
void SCR_SizeDown_f (void)
{
	if (Cmd_FromGamecode())
		Cvar_ForceSet(&scr_viewsize,va("%i", scr_viewsize.ival-10));
	else
		Cvar_SetValue (&scr_viewsize,scr_viewsize.value-10);
}

//============================================================================

/*
==================
SCR_Init
==================
*/
void SCR_Init (void)
{
//
// register our commands
//
	Cmd_AddRemCommand ("screenshot",SCR_ScreenShot_f);
	Cmd_AddRemCommand ("sizeup",SCR_SizeUp_f);
	Cmd_AddRemCommand ("sizedown",SCR_SizeDown_f);

	scr_net = R2D_SafePicFromWad ("net");
	scr_turtle = R2D_SafePicFromWad ("turtle");

	scr_initialized = true;
}

void SCR_DeInit (void)
{
	if (scr_initialized)
	{
		scr_initialized = false;

		Cmd_RemoveCommand ("screenshot");
		Cmd_RemoveCommand ("sizeup");
		Cmd_RemoveCommand ("sizedown");
	}
}


/*
==============
SCR_DrawTurtle
==============
*/
void SCR_DrawTurtle (void)
{
	static int      count;

	if (!scr_showturtle.ival || !scr_turtle)
		return;

	if (host_frametime <= 1.0/scr_turtlefps.value)
	{
		count = 0;
		return;
	}

	count++;
	if (count < 3)
		return;

	R2D_ScalePic (scr_vrect.x, scr_vrect.y, 64, 64, scr_turtle);
}

/*
==============
SCR_DrawNet
==============
*/
void SCR_DrawNet (void)
{
	if (cls.netchan.outgoing_sequence - cls.netchan.incoming_acknowledged < UPDATE_BACKUP-1)
		return;
	if (cls.demoplayback || !scr_net)
		return;

	R2D_ScalePic (scr_vrect.x+64, scr_vrect.y, 64, 64, scr_net);
}

void SCR_StringXY(char *str, float x, float y)
{
	char *s2;
	int px, py;

	Font_BeginString(font_conchar, ((x<0)?vid.width:x), ((y<0)?vid.height - sb_lines:y), &px, &py);

	if (x < 0)
	{
		for (s2 = str; *s2; s2++)
			px -= Font_CharWidth(*s2);
	}

	if (y < 0)
		py += y*Font_CharHeight();

	while (*str)
		px = Font_DrawChar(px, py, CON_WHITEMASK|*str++);
	Font_EndString(font_conchar);
}

void SCR_DrawFPS (void)
{
	extern cvar_t show_fps;
	static double lastupdatetime;
	static double lastsystemtime;
	double t;
	extern int fps_count;
	static float lastfps;
	static float deviationtimes[64];
	static int deviationframe;
	char str[80];
	int sfps, frame;
	qboolean usemsecs = false;

	float frametime;

	if (!show_fps.ival)
		return;

	t = Sys_DoubleTime();
	if ((t - lastupdatetime) >= 1.0)
	{
		lastfps = fps_count/(t - lastupdatetime);
		fps_count = 0;
		lastupdatetime = t;
	}
	frametime = t - lastsystemtime;
	lastsystemtime = t;

	sfps = show_fps.ival;
	if (sfps < 0)
	{
		sfps = -sfps;
		usemsecs = true;
	}

	switch (sfps)
	{
	case 2: // lowest FPS, highest MS encountered
		if (lastfps > 1/frametime)
		{
			lastfps = 1/frametime;
			fps_count = 0;
			lastupdatetime = t;
		}
		break;
	case 3: // highest FPS, lowest MS encountered
		if (lastfps < 1/frametime)
		{
			lastfps = 1/frametime;
			fps_count = 0;
			lastupdatetime = t;
		}
		break;
	case 4: // immediate FPS/MS
		lastfps = 1/frametime;
		lastupdatetime = t;
		break;
#ifdef GLQUAKE
	case 5:
		if (qrenderer == QR_OPENGL)
			GLR_FrameTimeGraph((int)(1000.0*2*frametime));
		break;
	case 7:
		if (qrenderer == QR_OPENGL)
			GLR_FrameTimeGraph((int)(1000.0*1*frametime));
		break;
#endif
	case 6:
		{
			float mean, deviation;
			deviationtimes[deviationframe++&63] = frametime*1000;
			mean = 0;
			for (frame = 0; frame < 64; frame++)
			{
				mean += deviationtimes[frame];
			}
			mean /= 64;
			deviation = 0;
			for (frame = 0; frame < 64; frame++)
			{
				deviation += (deviationtimes[frame] - mean)*(deviationtimes[frame] - mean);
			}
			deviation /= 64;
			deviation = sqrt(deviation);


			SCR_StringXY(va("%f deviation", deviation), show_fps_x.value, show_fps_y.value-8);
		}
		break;
	case 8:
		if (cls.timedemo)
			Con_Printf("%f\n", frametime);
		break;
	}

	if (usemsecs)
		sprintf(str, "%4.1f MS", 1000.0/lastfps);
	else
		sprintf(str, "%3.1f FPS", lastfps);
	SCR_StringXY(str, show_fps_x.value, show_fps_y.value);
}

void SCR_DrawUPS (void)
{
	extern cvar_t show_speed;
	static double lastupstime;
	double t;
	static float lastups;
	char str[80];
	float *vel;
	int track;

	if (!show_speed.ival)
		return;

	t = Sys_DoubleTime();
	if ((t - lastupstime) >= 1.0/20)
	{
		if (cl.spectator)
			track = Cam_TrackNum(0);
		else
			track = -1;
		if (track != -1)
			vel = cl.frames[cl.validsequence&UPDATE_MASK].playerstate[track].velocity;
		else
			vel = cl.simvel[0];
		lastups = sqrt((vel[0]*vel[0]) + (vel[1]*vel[1]));
		lastupstime = t;
	}

	sprintf(str, "%3.1f UPS", lastups);
	SCR_StringXY(str, show_speed_x.value, show_speed_y.value);
}

void SCR_DrawClock(void)
{
	struct tm *newtime;
	time_t long_time;
	char str[16];

	if (!show_clock.ival)
		return;

	time( &long_time );
	newtime = localtime( &long_time );
	strftime( str, sizeof(str)-1, "%H:%M    ", newtime);

	SCR_StringXY(str, show_clock_x.value, show_clock_y.value);
}

void SCR_DrawGameClock(void)
{
	float showtime;
	int minutes;
	int seconds;
	char str[16];
	int flags;
	float timelimit;

	if (!show_gameclock.ival)
		return;

	flags = (show_gameclock.value-1);
	if (flags & 1)
		timelimit = 60 * atof(Info_ValueForKey(cl.serverinfo, "timelimit"));
	else
		timelimit = 0;

	showtime = timelimit - cl.matchgametime;

	if (showtime < 0)
	{
		showtime *= -1;
		minutes = showtime/60;
		seconds = (int)showtime - (minutes*60);
		minutes *= -1;
	}
	else
	{
		minutes = showtime/60;
		seconds = (int)showtime - (minutes*60);
	}

	sprintf(str, " %02i:%02i", minutes, seconds);

	SCR_StringXY(str, show_gameclock_x.value, show_gameclock_y.value);
}

/*
==============
DrawPause
==============
*/
void SCR_DrawPause (void)
{
	mpic_t  *pic;

	if (!scr_showpause.ival)               // turn off for screenshots
		return;

	if (!cl.paused)
		return;

	if (key_dest == key_menu)
		return;

	pic = R2D_SafeCachePic ("gfx/pause.lmp");
	if (pic)
	{
		R2D_ScalePic ( (vid.width - pic->width)/2,
			(vid.height - 48 - pic->height)/2, pic->width, pic->height, pic);
	}
	else
		Draw_FunString((vid.width-strlen("Paused")*8)/2, (vid.height-8)/2, "Paused");
}



/*
==============
SCR_DrawLoading
==============
*/

int			total_loading_size, current_loading_size, loading_stage;
int CL_DownloadRate(void);

int SCR_GetLoadingStage(void)
{
	return loading_stage;
}
void SCR_SetLoadingStage(int stage)
{
	loading_stage = stage;
}

void SCR_DrawLoading (void)
{
	int sizex, x, y;
	mpic_t  *pic;
	char *s;
	int qdepth;
	int h2depth;
	//int mtype = M_GameType(); //unused variable
	y = vid.height/2;

	qdepth = COM_FDepthFile("gfx/loading.lmp", true);
	h2depth = COM_FDepthFile("gfx/menu/loading.lmp", true);

	if (qdepth < h2depth || h2depth > 0xffffff)
	{	//quake files

		pic = R2D_SafeCachePic ("gfx/loading.lmp");
		if (pic)
		{
			x = (vid.width - pic->width)/2;
			y = (vid.height - 48 - pic->height)/2;
			R2D_ScalePic (x, y, pic->width, pic->height, pic);
			x = (vid.width/2) - 96;
			y += pic->height + 8;
		}
		else
		{
			x = (vid.width/2) - 96;
			y = (vid.height/2) - 8;

			Draw_FunString((vid.width-7*8)/2, y-16, "Loading");
		}

		if (!total_loading_size)
			total_loading_size = 1;
		if (loading_stage > LS_CONNECTION)
		{
			sizex = current_loading_size * 192 / total_loading_size;
			if (loading_stage == LS_SERVER)
			{
				R2D_ImageColours(1.0, 0.0, 0.0, 1.0);
				R2D_FillBlock(x, y, sizex, 16);
				R2D_ImageColours(0.0, 0.0, 0.0, 1.0);
				R2D_FillBlock(x+sizex, y, 192-sizex, 16);
			}
			else
			{
				R2D_ImageColours(1.0, 1.0, 0.0, 1.0);
				R2D_FillBlock(x, y, sizex, 16);
				R2D_ImageColours(1.0, 0.0, 0.0, 1.0);
				R2D_FillBlock(x+sizex, y, 192-sizex, 16);
			}

			Draw_FunString(x+8, y+4, va("Loading %s... %i%%",
				(loading_stage == LS_SERVER) ? "server" : "client",
				current_loading_size * 100 / total_loading_size));

			y += 16;
		}
	}
	else
	{	//hexen2 files
		pic = R2D_SafeCachePic ("gfx/menu/loading.lmp");
		if (pic)
		{
			int		size, count, offset;

			if (!scr_drawloading && loading_stage == 0)
				return;

			offset = (vid.width - pic->width)/2;
			R2D_ScalePic (offset, 0, pic->width, pic->height, pic);

			if (loading_stage == LS_NONE)
				return;

			if (total_loading_size)
				size = current_loading_size * 106 / total_loading_size;
			else
				size = 0;

			if (loading_stage == LS_CLIENT)
				count = size;
			else
				count = 106;

			R2D_ImagePaletteColour (136, 1.0);
			R2D_FillBlock (offset+42, 87, count, 1);
			R2D_FillBlock (offset+42, 87+5, count, 1);
			R2D_ImagePaletteColour (138, 1.0);
			R2D_FillBlock (offset+42, 87+1, count, 4);

			if (loading_stage == LS_SERVER)
				count = size;
			else
				count = 0;

			R2D_ImagePaletteColour(168, 1.0);
			R2D_FillBlock (offset+42, 97, count, 1);
			R2D_FillBlock (offset+42, 97+5, count, 1);
			R2D_ImagePaletteColour(170, 1.0);
			R2D_FillBlock (offset+42, 97+1, count, 4);

			y = 104;
		}
	}

	if (cl.downloadlist || cls.downloadmethod)
	{
		unsigned int fcount;
		unsigned int tsize;
		qboolean sizeextra;

		x = vid.width/2 - 160;

		CL_GetDownloadSizes(&fcount, &tsize, &sizeextra);
		//downloading files?
		if (cls.downloadmethod)
			Draw_FunString(x+8, y+4, va("Downloading %s... %i%%",
				cls.downloadlocalname,
				cls.downloadpercent));

		if (tsize > 1024*1024*16)
		{
			Draw_FunString(x+8, y+8+4, va("%5ukbps %8umb%s remaining (%i files)",
				(unsigned int)(CL_DownloadRate()/1000.0f),
				tsize/(1024*1024),
				sizeextra?"+":"",
				fcount));
		}
		else
		{
			Draw_FunString(x+8, y+8+4, va("%5ukbps %8ukb%s remaining (%i files)",
				(unsigned int)(CL_DownloadRate()/1000.0f),
				tsize/1024,
				sizeextra?"+":"",
				fcount));
		}

		y+= 16+8;
	}
	else if (CL_TryingToConnect())
	{
		char dots[4];

		s = CL_TryingToConnect();
		x = (vid.width - (strlen(s)+15)*8) / 2;
		dots[0] = '.';
		dots[1] = '.';
		dots[2] = '.';
		dots[(int)realtime & 3] = 0;
		Draw_FunString(x, y+4, va("Connecting to: %s%s", s, dots));
	}
}

void SCR_BeginLoadingPlaque (void)
{
	if (cls.state != ca_active && cls.protocol != CP_QUAKE3)
		return;

	if (!scr_initialized)
		return;

//	if (key_dest == key_console) //not really appropriate if client is to show it on a remote server.
//		return;

// redraw with no console and the loading plaque
	Sbar_Changed ();
	scr_drawloading = true;
	SCR_UpdateScreen ();
	scr_drawloading = false;

	scr_disabled_for_loading = true;
	scr_disabled_time = Sys_DoubleTime();	//realtime tends to change... Hmmm....
}

void SCR_EndLoadingPlaque (void)
{
//	if (!scr_initialized)
//		return;

	scr_disabled_for_loading = false;
	*levelshotname = '\0';
	scr_drawloading = false;
	loading_stage = 0;
}

void SCR_ImageName (char *mapname)
{
	strcpy(levelshotname, "levelshots/");
	COM_FileBase(mapname, levelshotname + strlen(levelshotname), sizeof(levelshotname)-strlen(levelshotname));

#ifdef GLQUAKE
	if (qrenderer == QR_OPENGL)
	{
		if (!R2D_SafeCachePic (levelshotname))
		{
			*levelshotname = '\0';
			return;
		}
	}
	else
	{
		*levelshotname = '\0';
		return;
	}

	scr_disabled_for_loading = false;
	scr_drawloading = true;
	GL_BeginRendering ();
	SCR_DrawLoading();
	SCR_SetUpToDrawConsole();
	SCR_DrawConsole(!!*levelshotname);
	GL_EndRendering();
	scr_drawloading = false;

	scr_disabled_time = Sys_DoubleTime();	//realtime tends to change... Hmmm....
	scr_disabled_for_loading = true;

#endif
}


//=============================================================================


/*
==================
SCR_SetUpToDrawConsole
==================
*/
void SCR_SetUpToDrawConsole (void)
{
#ifdef TEXTEDITOR
	//extern qboolean editoractive; //unused variable
#endif
	if (scr_drawloading)
		return;         // never a console with loading plaque

// decide on the height of the console
	if (!scr_disabled_for_loading)
	{
		if ((key_dest == key_console || key_dest == key_game) && !cl.sendprespawn && cl.worldmodel && cl.worldmodel->needload)
			Con_ForceActiveNow();
		else if (key_dest == key_console || scr_chatmode)
		{
			scr_conlines = vid.height*scr_consize.value;    // half screen
			if (scr_conlines < 32)
				scr_conlines = 32;	//prevent total loss of console.
			else if (scr_conlines>vid.height)
				scr_conlines = vid.height;
		}
		else
			scr_conlines = 0;                               // none visible

		if (scr_conlines < scr_con_current)
		{
			scr_con_current -= scr_conspeed.value*host_frametime * (vid.height/320.0f);
			if (scr_conlines > scr_con_current)
				scr_con_current = scr_conlines;

		}
		else if (scr_conlines > scr_con_current)
		{
			scr_con_current += scr_conspeed.value*host_frametime * (vid.height/320.0f);
			if (scr_conlines < scr_con_current)
				scr_con_current = scr_conlines;
		}
	}

	if (scr_con_current>vid.height)
		scr_con_current = vid.height;

	if (clearconsole++ < vid.numpages)
	{
		Sbar_Changed ();
	}
	else if (clearnotify++ < vid.numpages)
	{
	}
}

/*
==================
SCR_DrawConsole
==================
*/
void SCR_DrawConsole (qboolean noback)
{
	if (scr_con_current)
	{
		Con_DrawConsole (scr_con_current, noback);
		clearconsole = 0;
	}
	else
	{
		if (key_dest == key_game || key_dest == key_message)
			Con_DrawNotify ();      // only draw notify in game
	}
}


/*
==============================================================================

						SCREEN SHOTS

==============================================================================
*/

typedef struct _TargaHeader {
	unsigned char   id_length, colormap_type, image_type;
	unsigned short  colormap_index, colormap_length;
	unsigned char   colormap_size;
	unsigned short  x_origin, y_origin, width, height;
	unsigned char   pixel_size, attributes;
} TargaHeader;


#if defined(AVAIL_JPEGLIB) && !defined(NO_JPEG)
	void screenshotJPEG(char *filename, int compression, qbyte *screendata, int screenwidth, int screenheight);
#endif
#ifdef AVAIL_PNGLIB
int Image_WritePNG (char *filename, int compression, qbyte *pixels, int width, int height);
#endif
void WriteBMPFile(char *filename, qbyte *in, int width, int height);

/*
Find closest color in the palette for named color
*/
int MipColor(int r, int g, int b)
{
	int i;
	float dist;
	int best=15;
	float bestdist;
	int r1, g1, b1;
	static int lr = -1, lg = -1, lb = -1;
	static int lastbest;

	if (r == lr && g == lg && b == lb)
		return lastbest;

	bestdist = 256*256*3;

	for (i = 0; i < 256; i++) {
		r1 = host_basepal[i*3] - r;
		g1 = host_basepal[i*3+1] - g;
		b1 = host_basepal[i*3+2] - b;
		dist = r1*r1 + g1*g1 + b1*b1;
		if (dist < bestdist) {
			bestdist = dist;
			best = i;
		}
	}
	lr = r; lg = g; lb = b;
	lastbest = best;
	return best;
}

qboolean SCR_ScreenShot (char *filename)
{
	int truewidth, trueheight;
	qbyte            *buffer;
	int                     i, c, temp;
#if defined(AVAIL_PNGLIB) || defined(AVAIL_JPEGLIB)
	extern cvar_t scr_sshot_compression;
#endif

#define MAX_PREPAD	128
	char *ext;

	ext = COM_FileExtension(filename);

	buffer = VID_GetRGBInfo(MAX_PREPAD, &truewidth, &trueheight);
#ifdef _MSC_VER
#pragma message("Need to ensure that the various image writing routines can cope with ((width|height)&3")
#endif

	if (!buffer)
		return false;

#ifdef AVAIL_PNGLIB
	if (!strcmp(ext, "png"))
	{
		Image_WritePNG(filename, scr_sshot_compression.value, buffer+MAX_PREPAD, truewidth, trueheight);
	}
	else
#endif
#ifdef AVAIL_JPEGLIB
		if (!strcmp(ext, "jpeg") || !strcmp(ext, "jpg"))
	{
		screenshotJPEG(filename, scr_sshot_compression.value, buffer+MAX_PREPAD, truewidth, trueheight);
	}
	else
#endif
	/*	if (!strcmp(ext, "bmp"))
	{
		WriteBMPFile(pcxname, buffer+MAX_PREPAD, truewidth, trueheight);
	}
	else*/
		if (!strcmp(ext, "pcx"))
	{
		int y, x;
		qbyte *src, *dest;
		qbyte *newbuf = buffer + MAX_PREPAD;
		// convert to eight bit
		for (y = 0; y < trueheight; y++) {
			src = newbuf + (truewidth * 3 * y);
			dest = newbuf + (truewidth * y);

			for (x = 0; x < truewidth; x++) {
				*dest++ = MipColor(src[0], src[1], src[2]);
				src += 3;
			}
		}

		WritePCXfile (filename, newbuf, truewidth, trueheight, truewidth, host_basepal, false);
	}
	else	//tga
	{
		buffer+=MAX_PREPAD-18;
		memset (buffer, 0, 18);
		buffer[2] = 2;          // uncompressed type
		buffer[12] = truewidth&255;
		buffer[13] = truewidth>>8;
		buffer[14] = trueheight&255;
		buffer[15] = trueheight>>8;
		buffer[16] = 24;        // pixel size

		// swap rgb to bgr
		c = 18+truewidth*trueheight*3;
		for (i=18 ; i<c ; i+=3)
		{
			temp = buffer[i];
			buffer[i] = buffer[i+2];
			buffer[i+2] = temp;
		}
		COM_WriteFile (filename, buffer, truewidth*trueheight*3 + 18 );
		buffer-=MAX_PREPAD-18;
	}


	BZ_Free (buffer);

	return true;
}

/*
==================
SCR_ScreenShot_f
==================
*/
void SCR_ScreenShot_f (void)
{
	char			sysname[1024];
	char            pcxname[80];
	int                     i;
	vfsfile_t *vfs;

	if (!VID_GetRGBInfo)
	{
		Con_Printf("Screenshots are not supported with the current renderer\n");
		return;
	}

	if (Cmd_Argc() == 2)
	{
		Q_strncpyz(pcxname, Cmd_Argv(1), sizeof(pcxname));
		if (strstr (pcxname, "..") || strchr(pcxname, ':') || *pcxname == '.' || *pcxname == '/')
		{
			Con_Printf("Screenshot name refused\n");
			return;
		}
		COM_DefaultExtension (pcxname, scr_sshot_type.string, sizeof(pcxname));
	}
	else
	{
	//
	// find a file name to save it to
	//
		sprintf(pcxname,"screenshots/fte00000.%s", scr_sshot_type.string);

		for (i=0 ; i<=100000 ; i++)
		{
			pcxname[16] = (i%10000)/1000 + '0';
			pcxname[17] = (i%1000)/100 + '0';
			pcxname[18] = (i%100)/10 + '0';
			pcxname[19] = (i%10) + '0';

			if (!(vfs = FS_OpenVFS(pcxname, "rb", FS_GAMEONLY)))
				break;  // file doesn't exist
			VFS_CLOSE(vfs);
		}
		if (i==100000)
		{
			Con_Printf ("SCR_ScreenShot_f: Couldn't create sequentially named file\n");
			return;
		}
	}

	FS_NativePath(pcxname, FS_GAMEONLY, sysname, sizeof(sysname));

	if (SCR_ScreenShot(pcxname))
		Con_Printf ("Wrote %s\n", sysname);
	else
		Con_Printf ("Screenshot failed\n");
}


// from gl_draw.c
qbyte		*draw_chars;				// 8*8 graphic characters

void SCR_DrawCharToSnap (int num, qbyte *dest, int width)
{
	int		row, col;
	qbyte	*source;
	int		drawline;
	int		x;

	row = num>>4;
	col = num&15;
	source = draw_chars + (row<<10) + (col<<3);

	drawline = 8;

	while (drawline--)
	{
		for (x=0 ; x<8 ; x++)
			if (source[x]!=255)
				dest[x] = source[x];
		source += 128;
		dest -= width;
	}

}

void SCR_DrawStringToSnap (const char *s, qbyte *buf, int x, int y, int width)
{
	qbyte *dest;
	const unsigned char *p;

	dest = buf + ((y * width) + x);

	p = (const unsigned char *)s;
	while (*p) {
		SCR_DrawCharToSnap(*p++, dest, width);
		dest += 8;
	}
}


/*
==================
SCR_RSShot
==================
*/
qboolean SCR_RSShot (void)
{
	int truewidth;
	int trueheight;

	int     x, y;
	unsigned char		*src, *dest;
	char		pcxname[80];
	unsigned char		*newbuf;
	int w, h;
	int dx, dy, dex, dey, nx;
	int r, b, g;
	int count;
	float fracw, frach;
	char st[80];
	time_t now;

	if (!scr_allowsnap.ival)
		return false;

	if (CL_IsUploading())
		return false; // already one pending

	if (cls.state < ca_onserver)
		return false; // gotta be connected

	if (!VID_GetRGBInfo || !scr_initialized)
	{
		return false;
	}

	Con_Printf("Remote screen shot requested.\n");


//
// save the pcx file
//
	newbuf = VID_GetRGBInfo(0, &truewidth, &trueheight);

	w = RSSHOT_WIDTH;
	h = RSSHOT_HEIGHT;

	fracw = (float)truewidth / (float)w;
	frach = (float)trueheight / (float)h;

	//scale down first.
	for (y = 0; y < h; y++) {
		dest = newbuf + (w*3 * y);

		for (x = 0; x < w; x++) {
			r = g = b = 0;

			dx = x * fracw;
			dex = (x + 1) * fracw;
			if (dex == dx) dex++; // at least one
			dy = y * frach;
			dey = (y + 1) * frach;
			if (dey == dy) dey++; // at least one

			count = 0;
			for (/* */; dy < dey; dy++) {
				src = newbuf + (truewidth * 3 * dy) + dx * 3;
				for (nx = dx; nx < dex; nx++) {
					r += *src++;
					g += *src++;
					b += *src++;
					count++;
				}
			}
			r /= count;
			g /= count;
			b /= count;
			*dest++ = r;
			*dest++ = g;
			*dest++ = b;
		}
	}

	// convert to eight bit
	for (y = 0; y < h; y++) {
		src = newbuf + (w * 3 * y);
		dest = newbuf + (w * y);

		for (x = 0; x < w; x++) {
			*dest++ = MipColor(src[0], src[1], src[2]);
			src += 3;
		}
	}

	time(&now);
	strcpy(st, ctime(&now));
	st[strlen(st) - 1] = 0;
	SCR_DrawStringToSnap (st, newbuf, w - strlen(st)*8, h - 1, w);

	Q_strncpyz(st, cls.servername, sizeof(st));
	SCR_DrawStringToSnap (st, newbuf, w - strlen(st)*8, h - 11, w);

	Q_strncpyz(st, name.string, sizeof(st));
	SCR_DrawStringToSnap (st, newbuf, w - strlen(st)*8, h - 21, w);

	WritePCXfile (pcxname, newbuf, w, h, w, host_basepal, true);

	BZ_Free(newbuf);

	Con_Printf ("Wrote %s\n", pcxname);

	return true;
}

//=============================================================================


//=============================================================================

/*
===============
SCR_BringDownConsole

Brings the console down and fades the palettes back to normal
================
*/
void SCR_BringDownConsole (void)
{
	int             i;
	int pnum;

	for (pnum = 0; pnum < cl.splitclients; pnum++)
		scr_centerprint[pnum].charcount = 0;

	for (i=0 ; i<20 && scr_conlines != scr_con_current ; i++)
		SCR_UpdateScreen ();

	cl.cshifts[CSHIFT_CONTENTS].percent = 0;              // no area contents palette on next frame
	VID_SetPalette (host_basepal);
}

void SCR_TileClear (void)
{
#ifdef PLUGINS
	extern cvar_t plug_sbar;
#endif

	if (cl.splitclients>1)
		return;	//splitclients always takes the entire screen.

#ifdef PLUGINS
	if (plug_sbar.ival)
	{
		if (scr_vrect.x > 0)
		{
			// left
			R2D_TileClear (0, 0, scr_vrect.x, vid.height);
			// right
			R2D_TileClear (scr_vrect.x + scr_vrect.width, 0,
				vid.width - scr_vrect.x + scr_vrect.width,
				vid.height);
		}
		if (scr_vrect.y > 0 || scr_vrect.height != vid.height)
		{
			// top
			R2D_TileClear (scr_vrect.x, 0,
				scr_vrect.width,
				scr_vrect.y);
			// bottom
			R2D_TileClear (scr_vrect.x,
				scr_vrect.y + scr_vrect.height,
				scr_vrect.width,
				vid.height);
		}
	}
	else
#endif
	{
		if (scr_vrect.x > 0)
		{
			// left
			R2D_TileClear (0, 0, scr_vrect.x, vid.height - sb_lines);
			// right
			R2D_TileClear (scr_vrect.x + scr_vrect.width, 0,
				vid.width - scr_vrect.x + scr_vrect.width,
				vid.height - sb_lines);
		}
		if (scr_vrect.y > 0)
		{
			// top
			R2D_TileClear (scr_vrect.x, 0,
				scr_vrect.width,
				scr_vrect.y);
			// bottom
			R2D_TileClear (scr_vrect.x,
				scr_vrect.y + scr_vrect.height,
				scr_vrect.width,
				vid.height - cl_sbar.value?sb_lines:0 -
				(scr_vrect.height + scr_vrect.y));
		}
	}
}



// The 2d refresh stuff.
void SCR_DrawTwoDimensional(int uimenu, qboolean nohud)
{
	RSpeedMark();

	//
	// draw any areas not covered by the refresh
	//
#ifdef GLQUAKE
	if (r_netgraph.value && qrenderer == QR_OPENGL)
		GLR_NetGraph ();
#endif

	if (scr_drawloading || loading_stage)
	{
		SCR_DrawLoading();

		if (!nohud)
		{
#ifdef PLUGINS
			Plug_SBar ();
#else
			if (Sbar_ShouldDraw())
			{
				Sbar_Draw ();
				Sbar_DrawScoreboard ();
			}
#endif
		}
		SCR_ShowPics_Draw();
	}
	else if (cl.intermission == 1)
	{
		Sbar_IntermissionOverlay ();
	}
	else if (cl.intermission == 2)
	{
		Sbar_FinaleOverlay ();
		SCR_CheckDrawCenterString ();
	}
	else if (cl.intermission == 3)
	{
	}
	else
	{
		if (!nohud)
		{
			R2D_DrawCrosshair();

			SCR_DrawNet ();
			SCR_DrawFPS ();
			SCR_DrawUPS ();
			SCR_DrawClock();
			SCR_DrawGameClock();
			SCR_DrawTurtle ();
			SCR_DrawPause ();
#ifdef PLUGINS
			Plug_SBar ();
#else
			if (Sbar_ShouldDraw())
			{
				Sbar_Draw ();
				Sbar_DrawScoreboard ();
			}
#endif
			SCR_ShowPics_Draw();

			CL_DrawPrydonCursor();
		}
		else
		{
			SCR_DrawFPS ();
			SCR_DrawUPS ();
		}
		SCR_CheckDrawCenterString ();
	}

#ifdef TEXTEDITOR
	if (editoractive)
		Editor_Draw();
#endif

	if (key_dest != key_console)
		SCR_DrawConsole (false);

	M_Draw (uimenu);
#ifdef MENU_DAT
	MP_Draw();
#endif

	if (key_dest == key_console)
		SCR_DrawConsole (false);

	RSpeedEnd(RSPEED_2D);
}
