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
#ifdef RGLQUAKE
#include "glquake.h"//would prefer not to have this
#endif

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

	if (!r_speeds.value)
		return;

	memset(RSpNames, 0, sizeof(RSpNames));
	RSpNames[RSPEED_TOTALREFRESH] = "Total refresh";
	RSpNames[RSPEED_PROTOCOL] = "Protocol";
	RSpNames[RSPEED_LINKENTITIES] = "Entity setup";
	RSpNames[RSPEED_WORLDNODE] = "World walking";
	RSpNames[RSPEED_WORLD] = "World rendering";
	RSpNames[RSPEED_DYNAMIC] = "Lightmap updates";
	RSpNames[RSPEED_PARTICLES] = "Particle physics and sorting";
	RSpNames[RSPEED_PARTICLESDRAW] = "Particle drawing";
	RSpNames[RSPEED_2D] = "2d elements";
	RSpNames[RSPEED_SERVER] = "Server";

	RSpNames[RSPEED_DRAWENTITIES] = "Entity rendering";

	RSpNames[RSPEED_PALETTEFLASHES] = "Palette flashes";
	RSpNames[RSPEED_STENCILSHADOWS] = "Stencil Shadows";

	RSpNames[RSPEED_FULLBRIGHTS] = "World fullbrights";

	RSpNames[RSPEED_FINISH] = "Waiting for card to catch up";

	RQntNames[RQUANT_MSECS] = "Microseconds";
	RQntNames[RQUANT_EPOLYS] = "Entity Polys";
	RQntNames[RQUANT_WPOLYS] = "World Polys";
	RQntNames[RQUANT_SHADOWFACES] = "Shadow Faces";
	RQntNames[RQUANT_SHADOWEDGES] = "Shadow edges";
	RQntNames[RQUANT_LITFACES] = "Lit faces";

	for (i = 0; i < RSPEED_MAX; i++)
	{
		s = va("%i %-30s", samplerspeeds[i], RSpNames[i]);
		Draw_String(vid.width-strlen(s)*8, i*8, s);
	}
	for (i = 0; i < RQUANT_MAX; i++)
	{
		s = va("%i %-30s", samplerquant[i], RQntNames[i]);
		Draw_String(vid.width-strlen(s)*8, (i+RSPEED_MAX)*8, s);
	}
	s = va("%f %-30s", 100000000.0f/samplerspeeds[RSPEED_TOTALREFRESH], "Framerate");
	Draw_String(vid.width-strlen(s)*8, (i+RSPEED_MAX)*8, s);

	if (framecount++>=100)
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

// only the refresh window will be updated unless these variables are flagged
int                     scr_copytop;
int                     scr_copyeverything;

float           scr_con_current;
float           scr_conlines;           // lines of console to display

qboolean		scr_con_forcedraw;

extern cvar_t          scr_viewsize;
extern cvar_t          scr_fov;
extern cvar_t          scr_conspeed;
extern cvar_t          scr_centertime;
extern cvar_t          scr_showram;
extern cvar_t          scr_showturtle;
extern cvar_t          scr_showpause;
extern cvar_t          scr_printspeed;
extern cvar_t			scr_allowsnap;
extern cvar_t			scr_sshot_type;
extern cvar_t			scr_sshot_compression;
extern  		cvar_t  crosshair;
extern cvar_t			scr_consize;

qboolean        scr_initialized;                // ready to draw

mpic_t          *scr_ram;
mpic_t          *scr_net;
mpic_t          *scr_turtle;

int                     scr_fullupdate;

int                     clearconsole;
int                     clearnotify;

int                     sb_lines;

viddef_t        vid;                            // global video state

vrect_t         scr_vrect;

qboolean        scr_disabled_for_loading;
qboolean        scr_drawloading;
float           scr_disabled_time;

qboolean        block_drawing;
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

conchar_t       scr_centerstring[MAX_SPLITS][1024];
float           scr_centertime_start[MAX_SPLITS];   // for slow victory printing
float           scr_centertime_off[MAX_SPLITS];
int                     scr_center_lines[MAX_SPLITS];
int                     scr_erase_lines[MAX_SPLITS];
int                     scr_erase_center[MAX_SPLITS];

void CopyAndMarkup(conchar_t *dest, qbyte *src, int maxlength)
{
	conchar_t ext = CON_WHITEMASK;
	conchar_t extstack[20];
	int extstackdepth = 0;

	if (maxlength < 0)
		return;	// ...

	while(*src && maxlength>0)
	{
		if (*src == '^')
		{
			src++;
			if (*src >= '0' && *src <= '9')
			{
				ext = q3codemasks[*src - '0'] | (ext&~CON_Q3MASK);
				src++;
				continue;
			}
			else if (*src == '&') // extended code
			{
				if (isextendedcode(src[1]) && isextendedcode(src[2]))
				{
					src++; // foreground char
					if (*src == '-') // default for FG
						ext = (COLOR_WHITE << CON_FGSHIFT) | (ext&~CON_FGMASK);
					else if (*src >= 'A')
						ext = ((*src - ('A' - 10)) << CON_FGSHIFT) | (ext&~CON_FGMASK);
					else
						ext = ((*src - '0') << CON_FGSHIFT) | (ext&~CON_FGMASK);
					src++; // background char
					if (*src == '-') // default (clear) for BG
						ext &= ~CON_BGMASK & ~CON_NONCLEARBG;
					else if (*src >= 'A')
						ext = ((*src - ('A' - 10)) << CON_BGSHIFT) | (ext&~CON_BGMASK) | CON_NONCLEARBG;
					else
						ext = ((*src - '0') << CON_BGSHIFT) | (ext&~CON_BGMASK) | CON_NONCLEARBG;
					src++;
					continue;
				}
				// else invalid code
				*dest++ = '^' | ext;
				maxlength--;
				if (maxlength <= 0)
					break; // need an extra check for writing length
			}
			else if (*src == 'b') // toggle blink bit
			{
				src++;
				ext ^= CON_BLINKTEXT;
				continue;
			}
			else if (*src == 'a') // toggle alternate charset
			{
				src++;
				ext ^= CON_2NDCHARSETTEXT;
				continue;
			}
			else if (*src == 'h')
			{
				src++;
				ext ^= CON_HALFALPHA;
				continue;
			}
			else if (*src == 's')
			{
				src++;
				if (extstackdepth < sizeof(extstack)/sizeof(extstack[0]))
				{
					extstack[extstackdepth] = ext;
					extstackdepth++;
				}
				continue;
			}
			else if (*src == 'r')
			{
				src++;
				if (extstackdepth)
				{
					extstackdepth--;
					ext = extstack[extstackdepth];
				}
				continue;
			}
			else if (*src != '^')
				src--;
		}
		if (*src == '\n')
			*dest++ = *src++;
		else
			*dest++ = *src++ | ext;
		maxlength--;
	}
	*dest = 0;
}

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

// SCR_StringToPalIndex: takes in "<index>" or "<r> <g> <b>" and converts to a 
// Quake palette index
int SCR_StringToPalIndex (char *rgbstring, float rgbinputscale)
{
	int i;
	char *t;

	rgbinputscale = 255/rgbinputscale;
	t = strstr(rgbstring, " ");

	if (t)
	{
		int r, g, b;

		t++;
		r = atof(rgbstring) * rgbinputscale;
		g = atof(t) * rgbinputscale;
		t = strstr(t, " ");
		if (t)
			b = atof(t) * rgbinputscale;
		else
			b = 0;

		r = bound(0, r, 255);
		g = bound(0, g, 255);
		b = bound(0, b, 255);
		i = GetPaletteIndex(r, g, b);
	}
	else
	{
		i = atoi(rgbstring);
		i = bound(0, i, 255);
	}

	return i;
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

	CopyAndMarkup (scr_centerstring[pnum], str, sizeof(scr_centerstring[pnum]));
	scr_centertime_off[pnum] = scr_centertime.value;
	scr_centertime_start[pnum] = cl.time;

// count the number of lines for centering
	scr_center_lines[pnum] = 1;
	while (*str)
	{
		if (*str == '\n')
			scr_center_lines[pnum]++;
		str++;
	}
}

void SCR_CPrint_f(void)
{
	SCR_CenterPrint(0, Cmd_Args(), true);
}

void SCR_EraseCenterString (void)
{
	int pnum;
	int		y;

	if (cl.splitclients>1)
		return;	//no viewsize with split

	for (pnum = 0; pnum < cl.splitclients; pnum++)
	{
		if (scr_erase_center[pnum]++ > vid.numpages)
		{
			scr_erase_lines[pnum] = 0;
			continue;
		}

		if (scr_center_lines[pnum] <= 4)
			y = vid.height*0.35;
		else
			y = 48;

		scr_copytop = 1;
		Draw_TileClear (0, y, vid.width, min(8*scr_erase_lines[pnum], vid.height - y - 1));
	}
}

void SCR_CenterPrintBreaks(conchar_t *start, int *lines, int *maxlength)
{
	int l;
	*lines = 0;
	*maxlength = 0;
	do
	{
	// scan the width of the line
		for (l=0 ; l<40 ; l++)
			if ((start[l]&255) == '\n' || !(start[l]&255))
				break;
		if (l == 40)
		{
			while(l > 0 && (start[l-1]&255)>' ')
			{
				l--;
			}
		}

		(*lines)++;
		if (*maxlength < l)
			*maxlength = l;

		start+=l;
//		for (l=0 ; l<40 && *start && *start != '\n'; l++)
 //			start++;

		if (!(*start&255))
			break;
		else if ((*start&255) == '\n'||!l)
			start++;                // skip the \n
	} while (1);
}

void SCR_DrawCenterString (int pnum)
{
	conchar_t    *start;
	int             l;
	int             j;
	int             x, y;
	int             remaining;
	int hd = 1;
	int screenwidth;

	vrect_t rect;

	int telejanostyle = 0;

	if (cl.splitclients)
		hd = cl.splitclients;

// the finale prints the characters one at a time
	if (cl.intermission)
		remaining = scr_printspeed.value * (cl.time - scr_centertime_start[pnum]);
	else
		remaining = 9999;

	scr_erase_center[pnum] = 0;
	start = scr_centerstring[pnum];

	if (scr_center_lines[pnum] <= 4)
		y = vid.height/hd*0.35;
	else
		y = 48;

	SCR_VRectForPlayer(&rect, pnum);

	y += rect.y;

	screenwidth = 40;//vid.width/8;

	if ((start[0]&255) == '/')
	{
		if ((start[1]&255) == 'O')
		{
			telejanostyle = (start[1]&255);
			start+=2;
		}
		else if ((start[1]&255) == 'P')
		{	//hexen2 style plaque.
			int lines, len;
			start+=2;
			SCR_CenterPrintBreaks(start, &lines, &len);
			x = rect.x+(rect.width-len*8)/2;
			Draw_TextBox(x-6, y-8, len-1, lines);
		}
	}

	do
	{
	// scan the width of the line
		for (l=0 ; l<=screenwidth ; l++)
			if ((start[l]&255) == '\n' || !(start[l]&255))
				break;
		if (l == screenwidth+1)
		{
			while(l > 0 && (start[l-1]&255)>' ' && (start[l-1]&255) != ' '+128)
			{
				l--;
			}
		}
		x = rect.x + (rect.width - l*8)/2+4;
		for (j=0 ; j<l ; j++, x+=8)
		{
			switch(telejanostyle)
			{
			case 'O':
				Draw_ColouredCharacter (x, y+vid.height/2, start[j]);
				break;
			default:
				Draw_ColouredCharacter (x, y, start[j]);
			}
			if (!remaining--)
				return;
		}

		y += 8;

		start+=l;
//		for (l=0 ; l<40 && *start && *start != '\n'; l++)
 //			start++;

		if (!(*start&255))
			break;
		else if ((*start&255) == '\n'||!l)
			start++;                // skip the \n
	} while (1);
}

void SCR_CheckDrawCenterString (void)
{
extern qboolean sb_showscores;
	int pnum;

	for (pnum = 0; pnum < cl.splitclients; pnum++)
	{
		scr_copytop = 1;
		if (scr_center_lines[pnum] > scr_erase_lines[pnum])
			scr_erase_lines[pnum] = scr_center_lines[pnum];

		scr_centertime_off[pnum] -= host_frametime;

		if (key_dest != key_game)	//don't let progs guis/centerprints interfere with the game menu
			continue;

		if (sb_showscores)	//this was annoying
			continue;

		if (scr_centertime_off[pnum] <= 0 && !cl.intermission && (scr_centerstring[pnum][0]&255) != '/' && (scr_centerstring[pnum][1]&255) != 'P')
			continue;	//'/P' prefix doesn't time out

		SCR_DrawCenterString (pnum);
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
			if (!strcmp(failed->name, sp->picname))
				break;
		}
		if (failed)
			continue;

		p = Draw_SafeCachePic(sp->picname);
		if (!p)
			continue;
		Draw_Pic(x, y, p);
	}
}

void SCR_ShowPic_Clear(void)
{
	showpic_t *sp;
	int pnum;

	for (pnum = 0; pnum < MAX_SPLITS; pnum++)
		*scr_centerstring[pnum] = '\0';

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

	scr_fullupdate = 0;             // force a background redraw
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

	if (r_refdef.fov_x < 10)
		r_refdef.fov_x = 10;
	else if (r_refdef.fov_x > 170)
		r_refdef.fov_x = 170;

	
	r_refdef.fov_y = CalcFov (r_refdef.fov_x, r_refdef.vrect.width, r_refdef.vrect.height);




//	r_refdef.vrect.height/=2;

#ifdef SWQUAKE
	if (qrenderer == QR_SOFTWARE)
	{

		R_SetVrect (&r_refdef.vrect, &scr_vrect, sb_lines);

	// guard against going from one mode to another that's less than half the
	// vertical resolution
		if (scr_con_current > vid.height)
			scr_con_current = vid.height;

	// notify the refresh of the change
		SWR_ViewChanged (&r_refdef.vrect, sb_lines, vid.aspect);
	}
#endif


	scr_vrect = r_refdef.vrect;
}

void SCR_CrosshairPosition(int pnum, int *x, int *y)
{
	extern cvar_t cl_crossx, cl_crossy, crosshaircorrect, v_viewheight;

	vrect_t rect;
	SCR_VRectForPlayer(&rect, pnum);

	if (cl.worldmodel && crosshaircorrect.value)
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
		cl.worldmodel->funcs.Trace(cl.worldmodel, 0, 0, start, end, vec3_origin, vec3_origin, &tr);
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
			Matrix4_Project(tr.endpos, end, cl.simangles[pnum], start, (float)rect.width/rect.height, r_refdef.fov_y);
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

	scr_ram = Draw_SafePicFromWad ("ram");
	scr_net = Draw_SafePicFromWad ("net");
	scr_turtle = Draw_SafePicFromWad ("turtle");

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
SCR_DrawRam
==============
*/
void SCR_DrawRam (void)
{
	if (!scr_showram.value || !scr_ram)
		return;

	if (!r_cache_thrash)
		return;

	Draw_Pic (scr_vrect.x+32, scr_vrect.y, scr_ram);
}

/*
==============
SCR_DrawTurtle
==============
*/
void SCR_DrawTurtle (void)
{
	static int      count;

	if (!scr_showturtle.value || !scr_turtle)
		return;

	if (host_frametime < 0.1)
	{
		count = 0;
		return;
	}

	count++;
	if (count < 3)
		return;

	Draw_Pic (scr_vrect.x, scr_vrect.y, scr_turtle);
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

	Draw_Pic (scr_vrect.x+64, scr_vrect.y, scr_net);
}

void SCR_StringXY(char *str, float x, float y)
{
	if (x < 0)
		x = vid.width + x*strlen(str)*8;
	if (y < 0)
		y = vid.height - sb_lines + y*8;

	Draw_String(x, y, str);
}

void SCR_DrawFPS (void)
{
	extern cvar_t show_fps;
	static double lastframetime;
	double t;
	extern int fps_count;
	static float lastfps;
	char str[80];
	int sfps;
	qboolean usemsecs = false;

	if (!show_fps.value)
		return;

	t = Sys_DoubleTime();
	if ((t - lastframetime) >= 1.0)
	{
		lastfps = fps_count/(t - lastframetime);
		fps_count = 0;
		lastframetime = t;
	}

	sfps = show_fps.value;
	if (sfps < 0)
	{
		sfps = -sfps;
		usemsecs = true;
	}

	switch (sfps)
	{
	case 2: // lowest FPS, highest MS encountered
		if (lastfps > 1/host_frametime)
		{
			lastfps = 1/host_frametime;
			fps_count = 0;
			lastframetime = t;
		}
		break;
	case 3: // highest FPS, lowest MS encountered
		if (lastfps < 1/host_frametime)
		{
			lastfps = 1/host_frametime;
			fps_count = 0;
			lastframetime = t;
		}
		break;
	case 4: // immediate FPS/MS
		lastfps = 1/host_frametime;
		lastframetime = t;
		break;
#ifdef RGLQUAKE
	case 5:
		if (qrenderer == QR_OPENGL)
			GLR_FrameTimeGraph((int)(1000.0*host_frametime));
		break;
#endif
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

	if (!show_speed.value)
		return;

	t = Sys_DoubleTime();
	if ((t - lastupstime) >= 1.0/20)
	{
		lastups = sqrt((cl.simvel[0][0]*cl.simvel[0][0]) + (cl.simvel[0][1]*cl.simvel[0][1]));
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

	if (!show_clock.value)
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

	if (!show_gameclock.value)
		return;

	flags = (show_gameclock.value-1);
	if (flags & 1) 
		timelimit = 60 * atof(Info_ValueForKey(cl.serverinfo, "timelimit"));
	else
		timelimit = 0;

	showtime = timelimit - cl.ktprogametime;

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

	if (!scr_showpause.value)               // turn off for screenshots
		return;

	if (!cl.paused)
		return;

	pic = Draw_SafeCachePic ("gfx/pause.lmp");
	if (pic)
	{
		Draw_Pic ( (vid.width - pic->width)/2,
			(vid.height - 48 - pic->height)/2, pic);
	}
	else
		Draw_String((vid.width-strlen("Paused")*8)/2, (vid.height-8)/2, "Paused");
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
	int mtype = M_GameType();
	y = vid.height/2;

	qdepth = COM_FDepthFile("gfx/loading.lmp", true);
	h2depth = COM_FDepthFile("gfx/menu/loading.lmp", true);

	if (qdepth < h2depth || h2depth > 0xffffff)
	{	//quake files

		pic = Draw_SafeCachePic ("gfx/loading.lmp");
		if (pic)
		{
			x = (vid.width - pic->width)/2;
			y = (vid.height - 48 - pic->height)/2;
			Draw_Pic (x, y, pic);
			x = (vid.width/2) - 96;
			y += pic->height + 8;
		}
		else
		{
			x = (vid.width/2) - 96;
			y = (vid.height/2) - 8;

			Draw_String((vid.width-7*8)/2, y-16, "Loading");
		}

		if (!total_loading_size)
			total_loading_size = 1;
		if (loading_stage > LS_CONNECTION)
		{
			sizex = current_loading_size * 192 / total_loading_size;
			if (loading_stage == LS_SERVER)
			{
				Draw_FillRGB(x, y, sizex, 16, 1.0, 0.0, 0.0);
				Draw_FillRGB(x+sizex, y, 192-sizex, 16, 0.0, 0.0, 0.0);
			}
			else
			{
				Draw_FillRGB(x, y, sizex, 16, 1.0, 1.0, 0.0);
				Draw_FillRGB(x+sizex, y, 192-sizex, 16, 1.0, 0.0, 0.0);
			}

			Draw_String(x+8, y+4, va("Loading %s... %i%%", 
				(loading_stage == LS_SERVER) ? "server" : "client",
				current_loading_size * 100 / total_loading_size));

			y += 16;
		}
	}
	else
	{	//hexen2 files
		pic = Draw_SafeCachePic ("gfx/menu/loading.lmp");
		if (pic)
		{
			int		size, count, offset;

			if (!scr_drawloading && loading_stage == 0)
				return;

			offset = (vid.width - pic->width)/2;
			Draw_TransPic (offset , 0, pic);

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

			Draw_Fill (offset+42, 87, count, 1, 136);
			Draw_Fill (offset+42, 87+1, count, 4, 138);
			Draw_Fill (offset+42, 87+5, count, 1, 136);

			if (loading_stage == LS_SERVER)
				count = size;
			else
				count = 0;

			Draw_Fill (offset+42, 97, count, 1, 168);
			Draw_Fill (offset+42, 97+1, count, 4, 170);
			Draw_Fill (offset+42, 97+5, count, 1, 168);

			y = 104;
		}
	}

	if (cl.downloadlist || cls.downloadmethod)
	{
		unsigned int fcount;
		unsigned int tsize;
		qboolean sizeextra;
		
		x = vid.conwidth/2 - 160;

		CL_GetDownloadSizes(&fcount, &tsize, &sizeextra);
		//downloading files?
		if (cls.downloadmethod)
			Draw_String(x+8, y+4, va("Downloading %s... %i%%", 
				cls.downloadname,
				cls.downloadpercent));

		if (tsize > 1024*1024*16)
		{
			Draw_String(x+8, y+8+4, va("%5ukbps %8umb%s remaining (%i files)", 
				(unsigned int)(CL_DownloadRate()/1000.0f),
				tsize/(1024*1024),
				sizeextra?"+":"",
				fcount));
		}
		else
		{
			Draw_String(x+8, y+8+4, va("%5ukbps %8ukb%s remaining (%i files)", 
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
		Draw_String(x, y+4, va("Connecting to: %s%s", s, dots)); 
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
	scr_fullupdate = 0;
	Sbar_Changed ();
	scr_drawloading = true;
	SCR_UpdateScreen ();
	scr_drawloading = false;

	scr_disabled_for_loading = true;
	scr_disabled_time = Sys_DoubleTime();	//realtime tends to change... Hmmm....
	scr_fullupdate = 0;
}

void SCR_EndLoadingPlaque (void)
{
//	if (!scr_initialized)
//		return;

	scr_disabled_for_loading = false;
	scr_fullupdate = 0;
	*levelshotname = '\0';
}

void SCR_ImageName (char *mapname)
{
	strcpy(levelshotname, "levelshots/");
	COM_FileBase(mapname, levelshotname + strlen(levelshotname), sizeof(levelshotname)-strlen(levelshotname));

#ifdef RGLQUAKE
	if (qrenderer == QR_OPENGL)
	{
		if (!Draw_SafeCachePic (levelshotname))
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
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
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
	extern qboolean editoractive;
#endif
	Con_CheckResize ();

	scr_con_forcedraw = false;

	if (scr_drawloading)
		return;         // never a console with loading plaque

// decide on the height of the console
	if (!scr_disabled_for_loading)
	{
		/*if (cls.state != ca_active && !Media_PlayingFullScreen()
	#ifdef TEXTEDITOR
			&& !editoractive
	#endif
	#ifdef VM_UI
			&& !UI_MenuState()
	#endif
			)
		{
			scr_conlines = vid.height;              // full screen
			scr_con_current = scr_conlines;
			scr_con_forcedraw = true;
		}
		else */
		scr_con_forcedraw = false;
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
		if (qrenderer == QR_SOFTWARE &&	!Media_PlayingFullScreen())
		{
			scr_copytop = 1;
			Draw_TileClear (0, (int) scr_con_current, vid.width, vid.height - (int) scr_con_current);
		}

		Sbar_Changed ();
	}
	else if (clearnotify++ < vid.numpages)
	{
		if (qrenderer == QR_SOFTWARE &&	!Media_PlayingFullScreen())
		{
			scr_copytop = 1;
			Draw_TileClear (0, 0, vid.width, con_notifylines);
		}
	}
	else
		con_notifylines = 0;
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
		scr_copyeverything = 1;
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


#ifdef AVAIL_JPEGLIB
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

	if (SCR_ScreenShot(pcxname))
		Con_Printf ("Wrote %s\n", pcxname);
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

	if (!scr_allowsnap.value)
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

char    *scr_notifystring;
qboolean        scr_drawdialog;

void SCR_DrawNotifyString (void)
{
	char    *start;
	int             l;
	int             j;
	int             x, y;

	start = scr_notifystring;

	y = vid.height*0.35;

	do
	{
	// scan the width of the line
		for (l=0 ; l<40 ; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (vid.width - l*8)/2;
		for (j=0 ; j<l ; j++, x+=8)
			Draw_Character (x, y, start[j]);

		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++;                // skip the \n
	} while (1);
}

/*
==================
SCR_ModalMessage

Displays a text string in the center of the screen and waits for a Y or N
keypress.
==================
*/
int SCR_ModalMessage (char *text)
{
	scr_notifystring = text;

// draw a fresh screen
	scr_fullupdate = 0;
	scr_drawdialog = true;
	SCR_UpdateScreen ();
	scr_drawdialog = false;

	S_StopAllSounds (true);               // so dma doesn't loop current sound

	do
	{
		key_count = -1;         // wait for a key down and up
		Sys_SendKeyEvents ();
	} while (key_lastpress != 'y' && key_lastpress != 'n' && key_lastpress != K_ESCAPE);

	scr_fullupdate = 0;
	SCR_UpdateScreen ();

	return key_lastpress == 'y';
}


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
		scr_centertime_off[pnum] = 0;

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

	if (qrenderer == QR_SOFTWARE)
	{
		if (scr_fullupdate++ < vid.numpages)
		{	// clear the entire screen
			scr_copyeverything = 1;
			Draw_TileClear (0, 0, vid.width, vid.height);
			Sbar_Changed ();
		}
		else
		{
			if (scr_viewsize.value < 100)
			{
				int x, y;
				x = vid.width - 10 * 8 - 8;
				y = vid.height - sb_lines - 8;
				// clear background for counters
				if (show_fps.value)
					Draw_TileClear(x, y, 10 * 8, 8);
			}
		}
	}
#ifdef PLUGINS
	else if (plug_sbar.value)
	{
		if (scr_vrect.x > 0)
		{
			// left
			Draw_TileClear (0, 0, scr_vrect.x, vid.height);
			// right
			Draw_TileClear (scr_vrect.x + scr_vrect.width, 0,
				vid.width - scr_vrect.x + scr_vrect.width,
				vid.height);
		}
		if (scr_vrect.y > 0 || scr_vrect.height != vid.height)
		{
			// top
			Draw_TileClear (scr_vrect.x, 0,
				scr_vrect.x + scr_vrect.width,
				scr_vrect.y);
			// bottom
			Draw_TileClear (scr_vrect.x,
				scr_vrect.y + scr_vrect.height,
				scr_vrect.width,
				vid.height);
		}
	}
#endif
	else
	{
		if (scr_vrect.x > 0)
		{
			// left
			Draw_TileClear (0, 0, scr_vrect.x, vid.height - sb_lines);
			// right
			Draw_TileClear (scr_vrect.x + scr_vrect.width, 0,
				vid.width - scr_vrect.x + scr_vrect.width,
				vid.height - sb_lines);
		}
		if (scr_vrect.y > 0)
		{
			// top
			Draw_TileClear (scr_vrect.x, 0,
				scr_vrect.x + scr_vrect.width,
				scr_vrect.y);
			// bottom
			Draw_TileClear (scr_vrect.x,
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
#ifdef RGLQUAKE
	if (r_netgraph.value && qrenderer == QR_OPENGL)
		GLR_NetGraph ();
#endif

	if (scr_drawdialog)
	{
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
		Draw_FadeScreen ();
		SCR_DrawNotifyString ();
		scr_copyeverything = true;
	}
	else if (scr_drawloading || loading_stage)
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
			Draw_Crosshair();

			SCR_DrawRam ();
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
			SCR_DrawFPS ();
		SCR_CheckDrawCenterString ();
#ifdef RGLQUAKE
		if (qrenderer == QR_OPENGL)
			qglTexEnvi ( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
#endif
	}

#ifdef TEXTEDITOR
	if (editoractive)
		Editor_Draw();
#endif

	SCR_DrawConsole (false);

	M_Draw (uimenu);
#ifdef MENU_DAT
	MP_Draw();
#endif

	RSpeedEnd(RSPEED_2D);
}
