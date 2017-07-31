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
	int savedsamplerquant[RQUANT_MAX];	//so we don't count the r_speeds debug spam in draw counts.
	char *RSpNames[RSPEED_MAX];
	char *RQntNames[RQUANT_MAX];
	char *s;
	static int framecount;
	int frameinterval = 100;

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
	RSpNames[RSPEED_SETUP] = "Setup/Acquire";

	RSpNames[RSPEED_SUBMIT] = "submit/finish";
	RSpNames[RSPEED_PRESENT] = "present";
	RSpNames[RSPEED_ACQUIRE] = "acquire";

	memset(RQntNames, 0, sizeof(RQntNames));
	RQntNames[RQUANT_MSECS] = "Microseconds";
	RQntNames[RQUANT_PRIMITIVEINDICIES] = "Draw Indicies";
	RQntNames[RQUANT_DRAWS] = "Draw Calls";
	RQntNames[RQUANT_2DBATCHES] = "2d Batches";
	RQntNames[RQUANT_WORLDBATCHES] = "World Batches";
	RQntNames[RQUANT_ENTBATCHES] = "Ent Batches";
	RQntNames[RQUANT_SHADOWINDICIES] = "Shadow Indicies";
	RQntNames[RQUANT_SHADOWEDGES] = "Shadow Edges";
	RQntNames[RQUANT_SHADOWSIDES] = "Shadowmap Sides";
	RQntNames[RQUANT_LITFACES] = "Lit faces";

	RQntNames[RQUANT_RTLIGHT_DRAWN] = "Lights Drawn";
	RQntNames[RQUANT_RTLIGHT_CULL_FRUSTUM] = "Lights offscreen";
	RQntNames[RQUANT_RTLIGHT_CULL_PVS] = "Lights PVS Culled";
	RQntNames[RQUANT_RTLIGHT_CULL_SCISSOR] = "Lights Scissored";

	memcpy(savedsamplerquant, rquant, sizeof(savedsamplerquant));
	if (r_speeds.ival > 1)
	{
		for (i = 0; i < RSPEED_MAX; i++)
		{
			s = va("%g %-20s", samplerspeeds[i]/(float)frameinterval, RSpNames[i]);
			Draw_FunString(vid.width-strlen(s)*8, i*8, s);
		}
	}
	for (i = 0; i < RQUANT_MAX; i++)
	{
		s = va("%u.%.3u %-20s", samplerquant[i]/frameinterval, (samplerquant[i]%100), RQntNames[i]);
		Draw_FunString(vid.width-strlen(s)*8, (i+RSPEED_MAX)*8, s);
	}
	if (r_speeds.ival > 1)
	{
		s = va("%f %-20s", (frameinterval*1000*1000.0f)/(samplerspeeds[RSPEED_TOTALREFRESH]+samplerspeeds[RSPEED_PRESENT]), "Framerate (refresh only)");
		Draw_FunString(vid.width-strlen(s)*8, (i+RSPEED_MAX)*8, s);
	}
	memcpy(rquant, savedsamplerquant, sizeof(rquant));

	if (++framecount>=frameinterval)
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


float mousecursor_x, mousecursor_y;
float mousemove_x, mousemove_y;

float multicursor_x[8], multicursor_y[8];
qboolean multicursor_active[8];

float           scr_con_current;
float           scr_conlines;           // lines of console to display

qboolean		scr_con_forcedraw;

extern cvar_t			scr_viewsize;
extern cvar_t			scr_fov;
extern cvar_t			scr_conspeed;
extern cvar_t			scr_centertime;
extern cvar_t			scr_logcenterprint;
extern cvar_t			scr_showturtle;
extern cvar_t			scr_turtlefps;
extern cvar_t			scr_showpause;
extern cvar_t			scr_printspeed;
extern cvar_t			scr_allowsnap;
extern cvar_t			scr_sshot_type;
extern cvar_t			scr_sshot_prefix;
extern cvar_t			scr_sshot_compression;
extern cvar_t			crosshair;
extern cvar_t			scr_consize;
cvar_t			scr_neticontimeout = CVAR("scr_neticontimeout", "0.3");

qboolean        scr_initialized;                // ready to draw

mpic_t          *scr_net;
mpic_t          *scr_turtle;

int                     clearconsole;
int                     clearnotify;

viddef_t        vid;                            // global video state

vrect_t         scr_vrect;

qboolean        scr_disabled_for_loading;
qboolean        scr_drawloading;
float           scr_disabled_time;

float oldsbar = 0;

cvar_t	con_stayhidden = CVARFD("con_stayhidden", "0", CVAR_NOTFROMSERVER, "0: allow console to pounce on the user\n1: console stays hidden unless explicitly invoked\n2:toggleconsole command no longer works\n3: shift+escape key no longer works");
cvar_t	show_fps	= CVARF("show_fps", "0", CVAR_ARCHIVE);
cvar_t	show_fps_x	= CVAR("show_fps_x", "-1");
cvar_t	show_fps_y	= CVAR("show_fps_y", "-1");
cvar_t	show_clock	= CVAR("cl_clock", "0");
cvar_t	show_clock_x	= CVAR("cl_clock_x", "0");
cvar_t	show_clock_y	= CVAR("cl_clock_y", "-1");
cvar_t	show_gameclock	= CVAR("cl_gameclock", "0");
cvar_t	show_gameclock_x	= CVAR("cl_gameclock_x", "0");
cvar_t	show_gameclock_y	= CVAR("cl_gameclock_y", "-1");
cvar_t	show_speed	= CVAR("show_speed", "0");
cvar_t	show_speed_x	= CVAR("show_speed_x", "-1");
cvar_t	show_speed_y	= CVAR("show_speed_y", "-9");
cvar_t	scr_loadingrefresh = CVAR("scr_loadingrefresh", "0");
cvar_t	scr_showloading	= CVAR("scr_showloading", "1");
cvar_t	scr_showobituaries = CVAR("scr_showobituaries", "0");

void *scr_curcursor;


static void SCR_CPrint_f(void)
{
	if (Cmd_Argc() == 2)
		SCR_CenterPrint(0, Cmd_Argv(1), true);
	else
		SCR_CenterPrint(0, Cmd_Args(), true);
}

extern char cl_screengroup[];
void CLSCR_Init(void)
{
	int i;
	Cmd_AddCommand("cprint", SCR_CPrint_f);

	Cvar_Register(&con_stayhidden, cl_screengroup);
	Cvar_Register(&scr_loadingrefresh, cl_screengroup);
	Cvar_Register(&scr_showloading, cl_screengroup);
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
	Cvar_Register(&scr_neticontimeout, cl_screengroup);
	Cvar_Register(&scr_showobituaries, cl_screengroup);


	memset(&key_customcursor, 0, sizeof(key_customcursor));
	for (i = 0; i < kc_max; i++)
		key_customcursor[i].dirty = true;
	scr_curcursor = NULL;
	if (rf && rf->VID_SetCursor)
		rf->VID_SetCursor(scr_curcursor);
}

/*
===============================================================================

CENTER PRINTING

===============================================================================
*/

typedef struct {
	unsigned int	flags;

	conchar_t		*string;
	conchar_t		*cursorchar;	//pointer into string
	size_t			stringbytes;
	size_t			charcount;
	char			titleimage[MAX_QPATH];
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

	//hex values
	if (!strncmp(rgbstring, "0x", 2))
	{
		char *end;
		unsigned int val = strtoul(rgbstring+2, &end, 16);
		if (end == rgbstring + 5)
		{
			rgb[0] = ((val&0xf00)>>8)/15.0;
			rgb[1] = ((val&0x0f0)>>4)/15.0;
			rgb[2] = ((val&0x00f)>>0)/15.0;
		}
		else if (end == rgbstring + 8)
		{
			rgb[0] = ((val&0xff0000)>>12)/255.0;
			rgb[1] = ((val&0x00ff00)>> 8)/255.0;
			rgb[2] = ((val&0x0000ff)>> 0)/255.0;
		}
		else
			rgb[0] = rgb[1] = rgb[2] = 1;

		return ;
	}

	t = strstr(rgbstring, " ");

	if (!t) // palette index
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
	else // use RGB coloring (input is scaled from 0-rgbinputscale)
	{
		t++;
		rgb[0] = atof(rgbstring);
		rgb[1] = atof(t);
		t = strstr(t, " "); // find last value
		if (t)
			rgb[2] = atof(t+1);
		else
			rgb[2] = 0.0;

		rgbinputscale = 1/rgbinputscale;
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
	size_t i;
	cprint_t *p;
	if (!str)
	{
		if (cl.intermissionmode == IM_NONE)
		{
			p = &scr_centerprint[pnum];
			p->flags = 0;
			p->time_off = 0;
		}
		return;
	}
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
	p->titleimage[0] = 0;
	p->cursorchar = NULL;

	if (*str != '/')
	{
		if (cl.intermissionmode != IM_NONE)
		{
			p->flags |= CPRINT_TYPEWRITER | CPRINT_PERSIST | CPRINT_TALIGN;
			if (cl.intermissionmode != IM_NQCUTSCENE)
				Q_strncpyz(p->titleimage, "gfx/finale.lmp", sizeof(p->titleimage));
		}
	}

	while (*str == '/')
	{
		if (str[1] == '.')
		{
/* /. means text actually starts after, no more flags */
			str+=2;
			break;
		}
		else if (str[1] == 'P')
		{
			p->flags |= CPRINT_PERSIST | CPRINT_BACKGROUND;
			p->flags &= ~CPRINT_TALIGN;
		}
		else if (str[1] == 'C')
			p->flags |= CPRINT_CURSOR;	//this can be a little jarring if there's no links, so this forces consistent behaviour.
		else if (str[1] == 'W')	//wait between each char
			p->flags ^= CPRINT_TYPEWRITER;
		else if (str[1] == 'S')	//Stay
			p->flags ^= CPRINT_PERSIST;
		else if (str[1] == 'M')	//'Mask' the background so that its readable.
			p->flags ^= CPRINT_BACKGROUND;
		else if (str[1] == 'O')	//Obituaries are shown at the bottom, ish.
			p->flags ^= CPRINT_OBITUARTY;
		else if (str[1] == 'B')
			p->flags ^= CPRINT_BALIGN;	//Note: you probably want to add some blank lines...
		else if (str[1] == 'T')
			p->flags ^= CPRINT_TALIGN;
		else if (str[1] == 'L')
			p->flags ^= CPRINT_LALIGN;
		else if (str[1] == 'R')
			p->flags ^= CPRINT_RALIGN;
		else if (str[1] == 'F')
		{	//'F' is reserved for special handling via svc_finale
			if (cl.intermissionmode == IM_NONE)
				cl.completed_time = cl.time;
			str+=2;
			switch(*str++)
			{
			case 'R':	//remove intermission
				cl.intermissionmode = IM_NONE;
				break;
			case 'I':	//standard intermission
			case 'S':	//score board / map stats
				cl.intermissionmode = IM_NQSCORES;
				break;
			case 'F':	//finale
				cl.intermissionmode = IM_NQFINALE;
				break;
			case 'f':	//finale, but with the view offset properly
				cl.intermissionmode = IM_H2FINALE;
				break;
			case 0:
				str--;
				break;
			default:
				break;	//no idea. suck it up for compat.
			}
			continue;
		}
		else if (str[1] == 'I')
		{
			char *e;
			int l;
			str+=2;
			e = strchr(str, ':');
			if (!e)
				e = strchr(str, ' ');	//probably an error
			if (!e)
				e = str + strlen(str)-1;	//error
			l = e - str;
			if (l >= sizeof(p->titleimage))
				l = sizeof(p->titleimage)-1;
			strncpy(p->titleimage, str, l);
			p->titleimage[l] = 0;
			str = e+1;
			continue;
		}
		else
			break;
		str += 2;
	}

	if (((scr_logcenterprint.ival && !cl.deathmatch) || scr_logcenterprint.ival == 2) && !(p->flags & CPRINT_PERSIST))
	{
		//don't spam too much.
		if (*str && strncmp(cl.lastcenterprint, str, sizeof(cl.lastcenterprint)-1))
		{
			Q_strncpyz(cl.lastcenterprint, str, sizeof(cl.lastcenterprint));
			Con_CenterPrint(str);
		}
	}

	for (;;)
	{
		p->charcount = COM_ParseFunString(CON_WHITEMASK, str, p->string, p->stringbytes, false) - p->string;

		if ((p->charcount+1)*sizeof(*p->string) < p->stringbytes)
			break;
		else
		{
			p->stringbytes=p->stringbytes*2+sizeof(*p->string);
			Z_Free(p->string);
			p->string = Z_Malloc(p->stringbytes);
		}
	}

	if (!(p->flags & CPRINT_CURSOR))
	{	//autodetect links
		for (i = 0; i < p->charcount; i++)
		{
			if (p->string[i] == CON_LINKSTART)
				p->flags |= CPRINT_CURSOR;
		}
	}

	p->time_off = scr_centertime.value;
	p->time_start = cl.time;
}

void VARGS Stats_Message(char *msg, ...)
{
	va_list		argptr;
	char str[2048];
	cprint_t *p = &scr_centerprint[0];
	if (!scr_showobituaries.ival)
		return;
	if (p->time_off >= 0)
		return;

	va_start (argptr, msg);
	vsnprintf (str,sizeof(str)-1, msg, argptr);
	va_end (argptr);

	p->flags = CPRINT_OBITUARTY;
	p->titleimage[0] = 0;

	for (;;)
	{
		p->charcount = COM_ParseFunString(CON_WHITEMASK, str, p->string, p->stringbytes, false) - p->string;

		if ((p->charcount+1)*sizeof(*p->string) < p->stringbytes)
			break;
		else
		{
			p->stringbytes=p->stringbytes*2+sizeof(*p->string);
			Z_Free(p->string);
			p->string = Z_Malloc(p->stringbytes);
		}
	}

	p->time_off = scr_centertime.value;
	p->time_start = cl.time;
}

static char *SCR_CopyCenterPrint(cprint_t *p)	//reads the link under the mouse cursor. non-links are ignored.
{
	size_t maxlen, outlen;
	char *result;

	conchar_t *start = p->cursorchar, *end;

	if (!start)	//cursor isn't over anything.
		return NULL;

	//scan backwards to find any link enclosure
	for(end = start-1; end >= p->string; end--)
	{
		if (*end == CON_LINKSTART)
		{
			//found one
			start = end;
			break;
		}
		if (*end == CON_LINKEND)
		{
			//some other link ended here. don't use its start.
			break;
		}
	}
	//scan forwards to find the end of the selected link
	if (start < p->string+p->charcount && *start == CON_LINKSTART)
	{
		for(end = start; end < p->string + p->charcount; end++)
		{
			if (*end == CON_LINKEND)
			{
				end++;
				break;
			}
		}
	}
	else// if (onlyiflink)
		return NULL;

	maxlen = 1024*1024;
	result = Z_Malloc(maxlen+1);

	outlen = COM_DeFunString(start, end, result, maxlen, false, false) - result;

	result[outlen++] = 0;
	return result;
}

#define MAX_CPRINT_LINES 512
void SCR_DrawCenterString (vrect_t *rect, cprint_t *p, struct font_s *font)
{
	int				l;
	int				y, x;
	int				left;
	int				right;
	int				top;
	int				bottom;
	int				remaining;
	shader_t		*pic;
	int				ch;

	conchar_t *line_start[MAX_CPRINT_LINES];
	conchar_t *line_end[MAX_CPRINT_LINES];
	int linecount;

// the finale prints the characters one at a time
	if (p->flags & CPRINT_TYPEWRITER)
		remaining = scr_printspeed.value * (cl.time - p->time_start);
	else
		remaining = 9999;

	p->erase_center = 0;

	if (*p->titleimage)
		pic = R2D_SafeCachePic (p->titleimage);
	else
		pic = NULL;

	if (p->flags & CPRINT_BACKGROUND)
	{	//hexen2 style plaque.
		int w = 320, h=200;
		if (pic)
			R_GetShaderSizes(pic, &w, &h, false);
		if (rect->width > w)
		{
			rect->x = (rect->x + rect->width/2) - (w / 2);
			rect->width = w;
		}

		if (rect->width < 32)
			return;
		rect->x += 16;
		rect->width -= 32;

		/*keep the text inside the image too*/
		if (pic)
		{
			if (rect->height > h)
			{
				rect->y = (rect->y + rect->height/2) - (h/2);
				rect->height = h;
			}
			rect->y += 16;
			rect->height -= 32;
		}
	}

	y = rect->y;

	if (pic)
	{
		//the pic is just a header
		if (!(p->flags & CPRINT_BACKGROUND))
		{
			int w, h;
			R_GetShaderSizes(pic, &w, &h, false);
			w *= 24.0/h;
			h = 24;
			y+= 16;
			R2D_ScalePic ( (vid.width-w)/2, 16, w, h, pic);
			y+= h;
			y+= 8;
		}
	}

	Font_BeginString(font, rect->x, y, &left, &top);
	Font_BeginString(font, rect->x+rect->width, rect->y+rect->height, &right, &bottom);
	linecount = Font_LineBreaks(p->string, p->string + p->charcount, right - left, MAX_CPRINT_LINES, line_start, line_end);

	ch = Font_CharHeight();

	if (p->flags & CPRINT_TALIGN)
		y = top;
	else if (p->flags & CPRINT_BALIGN)
		y = bottom - ch*linecount;
	else if (p->flags & CPRINT_OBITUARTY)
		//'obituary' messages appear at the bottom of the screen
		y = (bottom-top - ch*linecount) * 0.65 + top;
	else
	{
		if (linecount <= 5)
		{
			//small messages appear above and away from the crosshair
			y = (bottom-top - ch*linecount) * 0.35 + top;
		}
		else
		{
			//longer messages are fully centered
			y = (bottom-top - ch*linecount) * 0.5 + top;
		}
	}

	if (p->flags & CPRINT_BACKGROUND)
	{	//hexen2 style plaque.
		int px, py, pw;
		px = rect->x;
		py = (     y * vid.height) / (float)vid.pixelheight;
		pw = rect->width+8;
		Font_EndString(font);

		if (*p->titleimage)
			R2D_ScalePic (rect->x + ((int)rect->width - pic->width)/2, rect->y + ((int)rect->height - pic->height)/2, pic->width, pic->height, pic);
		else
			Draw_TextBox(px-16, py-8-8, pw/8, linecount+2);

		Font_BeginString(font, rect->x, y, &left, &top);
	}

	for (l = 0; l < linecount; l++, y += ch)
	{
		if (y >= bottom)
			break;
		if (p->flags & CPRINT_RALIGN)
			x = right - Font_LineWidth(line_start[l], line_end[l]);
		else if (p->flags & CPRINT_LALIGN)
			x = left;
		else
			x = left + (right - left - Font_LineWidth(line_start[l], line_end[l]))/2;

		if (mousecursor_y >= y && mousecursor_y < y+ch)
		{
			p->cursorchar = Font_CharAt(mousecursor_x - x, line_start[l], line_end[l]);
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

	Font_EndString(font);
}

qboolean Key_Centerprint(int key, int unicode, unsigned int devid)
{
	int pnum;
	cprint_t *p;
	char *link;

	if (key == K_MOUSE1)
	{
		//figure out which player has the cursor
		for (pnum = 0; pnum < cl.splitclients; pnum++)
		{
			p = &scr_centerprint[pnum];
			if (cl.playerview[pnum].gamerectknown == cls.framecount)
			{
				link = SCR_CopyCenterPrint(p);
				if (link)
				{

					if (link[0] == '^' && link[1] == '[')
					{
						//looks like it might be a link!
						char *end = NULL;
						char *info;
						for (info = link + 2; *info; )
						{
							if (info[0] == '^' && info[1] == ']')
								break; //end of tag, with no actual info, apparently
							if (*info == '\\')
								break;
							else if (info[0] == '^' && info[1] == '^')
								info+=2;
							else
								info++;
						}
						for(end = info; *end; )
						{
							if (end[0] == '^' && end[1] == ']')
							{
								//okay, its a valid link that they clicked
								*end = 0;

#ifdef PLUGINS
								if (!Plug_ConsoleLink(link+2, info, ""))
#endif
#ifdef CSQC_DAT
								if (!CSQC_ConsoleLink(link+2, info))
#endif
									Key_DefaultLinkClicked(NULL, link+2, info);

								break;
							}
							if (end[0] == '^' && end[1] == '^')
								end+=2;
							else
								end++;
						}
					}
				}
			}
		}
		return true;	//handled
	}
	else if (key == K_MOUSE2)
	{
		for (pnum = 0; pnum < cl.splitclients; pnum++)
		{
			p = &scr_centerprint[pnum];
			p->flags &= ~CPRINT_CURSOR;
		}
		return true;
	}
	return false;
}

void SCR_CheckDrawCenterString (void)
{
	int pnum;
	cprint_t *p;

	Key_Dest_Remove(kdm_centerprint);

	for (pnum = 0; pnum < cl.splitclients; pnum++)
	{
		p = &scr_centerprint[pnum];
		p->cursorchar = NULL;

		if (p->time_off <= 0 && !(p->flags & CPRINT_PERSIST))
			continue;	//'/P' prefix doesn't time out

		p->time_off -= host_frametime;

		if (Key_Dest_Has(~(kdm_game|kdm_centerprint)))	//don't let progs guis/centerprints interfere with the game menu
			continue;					//should probably allow the console with a scissor region or something.

#ifdef QUAKEHUD
		if (cl.playerview[pnum].sb_showscores)	//this was annoying
			continue;
#endif

		if (cl.playerview[pnum].gamerectknown == cls.framecount)
		{
			SCR_DrawCenterString(&cl.playerview[pnum].gamerect, p, font_default);
			if (p->flags & CPRINT_CURSOR)
				Key_Dest_Add(kdm_centerprint);
		}
	}
}

void R_DrawTextField(int x, int y, int w, int h, const char *text, unsigned int defaultmask, unsigned int fieldflags, struct font_s *font, vec2_t fontscale)
{
	cprint_t p;
	vrect_t r;
	conchar_t buffer[16384];	//FIXME: make dynamic.

	p.string = buffer;
	p.stringbytes = sizeof(buffer);
	r.x = x;
	r.y = y;
	r.width = w;
	r.height = h;

	p.flags = fieldflags;
	p.charcount = COM_ParseFunString(defaultmask, text, p.string, p.stringbytes, false) - p.string;
	p.time_off = scr_centertime.value;
	p.time_start = cl.time;
	*p.titleimage = 0;

	SCR_DrawCenterString(&r, &p, font);
}

qboolean SCR_HardwareCursorIsActive(void)
{
	if (Key_MouseShouldBeFree())
		return !!scr_curcursor;
	return false;
}
void SCR_DrawCursor(void)
{
	extern cvar_t cl_cursor, cl_cursorbiasx, cl_cursorbiasy, cl_cursorscale, cl_prydoncursor;
	mpic_t *p;
	char *newc;
	int prydoncursornum = 0;
	extern qboolean cursor_active;
	int cmod = kc_console;
	void *oldcurs = NULL;

	if (cursor_active && cl_prydoncursor.ival > 0)
		prydoncursornum = cl_prydoncursor.ival;
	else if (!Key_MouseShouldBeFree())
		return;

	//choose the cursor based upon the module that has primary focus
	if (key_dest_mask & key_dest_absolutemouse & (kdm_console|kdm_cwindows|kdm_editor))
		cmod = kc_console;
	else if ((key_dest_mask & key_dest_absolutemouse & kdm_emenu))
		cmod = kc_console;
	else if ((key_dest_mask & key_dest_absolutemouse & kdm_gmenu))
		cmod = kc_menu;
	else// if (key_dest_mask & key_dest_absolutemouse)
		cmod = prydoncursornum?kc_console:kc_game;

	if (cmod == kc_console)
	{
		if (!*cl_cursor.string || prydoncursornum>1)
			newc = va("gfx/prydoncursor%03i.lmp", prydoncursornum);
		else
			newc = cl_cursor.string;
		if (strcmp(key_customcursor[kc_console].name, newc) || key_customcursor[kc_console].hotspot[0] != cl_cursorbiasx.value || key_customcursor[kc_console].hotspot[1] != cl_cursorbiasy.value || key_customcursor[kc_console].scale != cl_cursorscale.value)
		{
			key_customcursor[kc_console].dirty = true;
			Q_strncpyz(key_customcursor[cmod].name, newc, sizeof(key_customcursor[cmod].name));
			key_customcursor[kc_console].hotspot[0] = cl_cursorbiasx.value;
			key_customcursor[kc_console].hotspot[1] = cl_cursorbiasy.value;
			key_customcursor[kc_console].scale = cl_cursorscale.value;
		}
	}

	if (key_customcursor[cmod].dirty)
	{
		if (key_customcursor[cmod].scale <= 0 || !*key_customcursor[cmod].name)
		{
			key_customcursor[cmod].hotspot[0] = cl_cursorbiasx.value;
			key_customcursor[cmod].hotspot[1] = cl_cursorbiasy.value;
			key_customcursor[cmod].scale = cl_cursorscale.value;
		}

		key_customcursor[cmod].dirty = false;
		oldcurs = key_customcursor[cmod].handle;
		if (rf->VID_CreateCursor && strcmp(key_customcursor[cmod].name, "none"))
		{
			key_customcursor[cmod].handle = NULL;
			if (!key_customcursor[cmod].handle && *key_customcursor[cmod].name)
				key_customcursor[cmod].handle = rf->VID_CreateCursor(key_customcursor[cmod].name, key_customcursor[cmod].hotspot[0], key_customcursor[cmod].hotspot[1], key_customcursor[cmod].scale);
			if (!key_customcursor[cmod].handle)
				key_customcursor[cmod].handle = rf->VID_CreateCursor("gfx/cursor.tga", key_customcursor[cmod].hotspot[0], key_customcursor[cmod].hotspot[1], key_customcursor[cmod].scale);	//try the fallback
			if (!key_customcursor[cmod].handle)
				key_customcursor[cmod].handle = rf->VID_CreateCursor("gfx/cursor.png", key_customcursor[cmod].hotspot[0], key_customcursor[cmod].hotspot[1], key_customcursor[cmod].scale);	//try the fallback
			if (!key_customcursor[cmod].handle)
				key_customcursor[cmod].handle = rf->VID_CreateCursor("gfx/cursor.lmp", key_customcursor[cmod].hotspot[0], key_customcursor[cmod].hotspot[1], key_customcursor[cmod].scale);	//try the fallback
		}
		else
			key_customcursor[cmod].handle = NULL;
	}

	if (scr_curcursor != key_customcursor[cmod].handle)
	{
		scr_curcursor = key_customcursor[cmod].handle;
		rf->VID_SetCursor(scr_curcursor);
	}
	if (oldcurs)
		rf->VID_DestroyCursor(oldcurs);

	if (scr_curcursor)
		return;
	//system doesn't support a hardware cursor, so try to draw a software one.

	if (!strcmp(key_customcursor[cmod].name, "none"))
		return;

	p = R2D_SafeCachePic(key_customcursor[cmod].name);
	if (!p || !R_GetShaderSizes(p, NULL, NULL, false))
		p = R2D_SafeCachePic("gfx/cursor.lmp");
	if (p && R_GetShaderSizes(p, NULL, NULL, false))
	{
		R2D_ImageColours(1, 1, 1, 1);
		R2D_Image(mousecursor_x-key_customcursor[cmod].hotspot[0], mousecursor_y-key_customcursor[cmod].hotspot[1], p->width*cl_cursorscale.value, p->height*cl_cursorscale.value, 0, 0, 1, 1, p);
	}
	else
	{
		float x, y;
		Font_BeginScaledString(font_default, mousecursor_x, mousecursor_y, 8, 8, &x, &y);
		x -= Font_CharScaleWidth(CON_WHITEMASK, '+' | 0xe000)/2;
		y -= Font_CharHeight()/2;
		Font_DrawScaleChar(x, y, CON_WHITEMASK, '+' | 0xe000);
		Font_EndString(font_default);
	}
}
static void SCR_DrawSimMTouchCursor(void)
{
	int i;
	float x, y;
	for (i = 0; i < 8; i++)
	{
		if (multicursor_active[i])
		{
			Font_BeginScaledString(font_default, multicursor_x[i], multicursor_y[i], 8, 8, &x, &y);
			x -= Font_CharScaleWidth(CON_WHITEMASK, '+' | 0xe000)/2;
			y -= Font_CharHeight()/2;
			Font_DrawScaleChar(x, y, CON_WHITEMASK, '+' | 0xe000);
			Font_EndString(font_default);
		}
	}
}

////////////////////////////////////////////////////////////////
//TEI_SHOWLMP2 (not 3)
//
typedef struct showpic_s {
	struct showpic_s *next;
	qbyte zone;
	qboolean persist;
	short x, y, w, h;
	char *name;
	char *picname;
	char *tcommand;
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

	switch (origin)
	{
		//tei's original encoding
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

		//spike's attempt to provide sane origins that are a little more predictable.
		case SL_ORG_TL:
			break;
		case SL_ORG_TR:
			x += vid.width;
			break;
		case SL_ORG_BL:
			y += vid.height;
			break;
		case SL_ORG_BR:
			x += vid.width;
			y += vid.height;
			break;
		case SL_ORG_MM:
			x += midx;
			y += midy;
			break;
		case SL_ORG_TM:
			x += midx;
			break;
		case SL_ORG_BM:
			x += midx;
			y += vid.height;
			break;
		case SL_ORG_ML:
			y += midy;
			break;
		case SL_ORG_MR:
			x += vid.height;
			y += midy;
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
		R2D_ScalePic(x, y, sp->w?sp->w:p->width, sp->h?sp->h:p->height, p);
	}
}
char *SCR_ShowPics_ClickCommand(int cx, int cy)
{
	downloadlist_t *failed;
	float x, y, w, h;
	showpic_t *sp;
	mpic_t *p;
	for (sp = showpics; sp; sp = sp->next)
	{
		if (!sp->tcommand || !*sp->tcommand)
			continue;

		x = sp->x;
		y = sp->y;
		w = sp->w;
		h = sp->h;
		SP_RecalcXY(&x, &y, sp->zone);

		if (!w || !h)
		{
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
			w = w?w:sp->w;
			h = h?h:sp->h;
		}
		if (cx >= x && cx < x+w)
			if (cy >= y && cy < y+h)
				return sp->tcommand;	//if they overlap, that's your own damn fault.
	}
	return NULL;
}

//all=false clears only server pics, not ones from configs.
void SCR_ShowPic_ClearAll(qboolean persistflag)
{
	showpic_t **link, *sp;
	int pnum;

	for (pnum = 0; pnum < MAX_SPLITS; pnum++)
	{
		scr_centerprint[pnum].flags = 0;
		scr_centerprint[pnum].charcount = 0;
	}

	for (link = &showpics; (sp=*link); )
	{
		if (sp->persist != persistflag)
		{
			link = &sp->next;
			continue;
		}
		
		*link = sp->next;

		Z_Free(sp->name);
		Z_Free(sp->picname);
		Z_Free(sp->tcommand);
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

static int SCR_ShowPic_MapZone(char *zone)
{
	//sane coding scheme
	if (!Q_strcasecmp(zone, "tr"))
		return SL_ORG_TR;
	if (!Q_strcasecmp(zone, "tl"))
		return SL_ORG_TL;
	if (!Q_strcasecmp(zone, "br"))
		return SL_ORG_BR;
	if (!Q_strcasecmp(zone, "bl"))
		return SL_ORG_BL;
	if (!Q_strcasecmp(zone, "mm"))
		return SL_ORG_MM;
	if (!Q_strcasecmp(zone, "tm"))
		return SL_ORG_TM;
	if (!Q_strcasecmp(zone, "bm"))
		return SL_ORG_BM;
	if (!Q_strcasecmp(zone, "mr"))
		return SL_ORG_MR;
	if (!Q_strcasecmp(zone, "ml"))
		return SL_ORG_MR;

	//compasy directions (but uses tei's coding scheme...)
	if (!Q_strcasecmp(zone, "nw"))
		return SL_ORG_NW;
	if (!Q_strcasecmp(zone, "ne"))
		return SL_ORG_NE;
	if (!Q_strcasecmp(zone, "sw"))
		return SL_ORG_SW;
	if (!Q_strcasecmp(zone, "sw"))
		return SL_ORG_SE;
	if (!Q_strcasecmp(zone, "cc"))
		return SL_ORG_CC;
	if (!Q_strcasecmp(zone, "cn"))
		return SL_ORG_CN;
	if (!Q_strcasecmp(zone, "cs"))
		return SL_ORG_CS;
	if (!Q_strcasecmp(zone, "cw"))
		return SL_ORG_CW;
	if (!Q_strcasecmp(zone, "ce"))
		return SL_ORG_CE;

	return atoi(zone);
}
void SCR_ShowPic_Script_f(void)
{
	char *imgname;
	char *name;
	char *tcommand;
	int x, y, w, h;
	int zone;
	showpic_t *sp;

	imgname = Cmd_Argv(1);
	name = Cmd_Argv(2);
	x = atoi(Cmd_Argv(3));
	y = atoi(Cmd_Argv(4));
	zone = SCR_ShowPic_MapZone(Cmd_Argv(5));

	w = atoi(Cmd_Argv(6));
	h = atoi(Cmd_Argv(7));
	tcommand = Cmd_Argv(8);


	sp = SCR_ShowPic_Find(name);

	Z_Free(sp->picname);
	Z_Free(sp->tcommand);
	sp->picname = Z_StrDup(imgname);
	sp->tcommand = Z_StrDup(tcommand);

	sp->zone = zone;
	sp->x = x;
	sp->y = y;
	sp->w = w;
	sp->h = h;

	if (!sp->persist)
		sp->persist = !Cmd_FromGamecode();

}

void SCR_ShowPic_Remove_f(void)
{
	SCR_ShowPic_ClearAll(!Cmd_FromGamecode());
}

//=============================================================================

void QDECL SCR_Fov_Callback (struct cvar_s *var, char *oldvalue)
{
	if (var->value < 10)
		Cvar_ForceSet(var, "10");
	//highs are capped elsewhere. this allows fisheye to use this cvar automatically.
}

void QDECL SCR_Viewsize_Callback (struct cvar_s *var, char *oldvalue)
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
}

void QDECL CL_Sbar_Callback(struct cvar_s *var, char *oldvalue)
{
}

void SCR_CrosshairPosition(playerview_t *pview, float *x, float *y)
{
	extern cvar_t cl_crossx, cl_crossy, crosshaircorrect, v_viewheight;

	vrect_t rect;
	rect = r_refdef.vrect;

	if (cl.worldmodel && crosshaircorrect.ival)
	{
		float adj;
		trace_t tr;
		vec3_t end;
		vec3_t start;
		vec3_t right, up, fwds;

		AngleVectors(pview->simangles, fwds, right, up);

		VectorCopy(pview->simorg, start);
		start[2]+=16;
		VectorMA(start, 100000, fwds, end);

		memset(&tr, 0, sizeof(tr));
		tr.fraction = 1;
		cl.worldmodel->funcs.NativeTrace(cl.worldmodel, 0, NULLFRAMESTATE, NULL, start, end, vec3_origin, vec3_origin, false, MASK_WORLDSOLID, &tr);
		start[2]-=16;
		if (tr.fraction != 1)
		{
			adj=pview->viewheight;
			if (v_viewheight.value < -7)
				adj+=-7;
			else if (v_viewheight.value > 4)
				adj+=4;
			else
				adj+=v_viewheight.value;

			start[2]+=adj;
			Matrix4x4_CM_Project(tr.endpos, end, pview->simangles, start, r_refdef.fov_x, r_refdef.fov_y);
			*x = rect.x+rect.width*end[0] + cl_crossx.value;
			*y = rect.y+rect.height*(1-end[1]) + cl_crossy.value;
			return;
		}
	}

	*x = rect.x + rect.width/2 + cl_crossx.value;
	*y = rect.y + rect.height/2 + cl_crossy.value;
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

void SCR_DrawDisk (void)
{
//	if (!draw_disc)
		return;

//	if (!COM_HasWork())
//		return;

//	R2D_ScalePic (scr_vrect.x + vid.width-24, scr_vrect.y, 24, 24, draw_disc);
}

/*
==============
SCR_DrawNet
==============
*/
void SCR_DrawNet (void)
{
	if (realtime - cls.netchan.last_received < scr_neticontimeout.value)
		return;
	if (cls.demoplayback || !scr_net)
		return;

	R2D_ScalePic (scr_vrect.x+64, scr_vrect.y, 64, 64, scr_net);
}

void SCR_StringXY(char *str, float x, float y)
{
	char *s2;
	int px, py;
	unsigned int codepoint;
	int error;

	Font_BeginString(font_default, ((x<0)?vid.width:x), ((y<0)?vid.height - sb_lines:y), &px, &py);

	if (x < 0)
	{
		for (s2 = str; *s2; )
		{
			codepoint = unicode_decode(&error, s2, &s2, true);
			px -= Font_CharWidth(CON_WHITEMASK, codepoint);
		}
	}

	if (y < 0)
		py += y*Font_CharHeight();

	while (*str)
	{
		codepoint = unicode_decode(&error, str, &str, true);
		px = Font_DrawChar(px, py, CON_WHITEMASK, codepoint);
	}
	Font_EndString(font_default);
}

void SCR_DrawFPS (void)
{
	extern cvar_t show_fps;
	static double lastupdatetime;
	static double lastsystemtime;
	double t;
	extern int fps_count;
	static float lastfps;
	static double deviationtimes[64];
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
	case 5:
		R_FrameTimeGraph((int)(1000.0*2*frametime));
		break;
	case 7:
		R_FrameTimeGraph((int)(1000.0*1*frametime));
		break;
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

void SCR_DrawClock(void)
{
	struct tm *newtime;
	time_t long_time;
	char str[16];

	if (!show_clock.ival)
		return;

	time( &long_time );
	newtime = localtime( &long_time );
	if (show_clock.ival == 2)
		strftime( str, sizeof(str)-1, "%I:%M %p   ", newtime);
	else
		strftime( str, sizeof(str)-1, "%H:%M    ", newtime);

	SCR_StringXY(str, show_clock_x.value, show_clock_y.value);
}

void SCR_DrawGameClock(void)
{
#ifdef QUAKESTATS
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

	if (cl.matchstate == MATCH_STANDBY)
		showtime = cl.servertime;
	else if (cl.playerview[0].statsf[STAT_MATCHSTARTTIME])
		showtime = timelimit - (cl.servertime - cl.playerview[0].statsf[STAT_MATCHSTARTTIME]/1000);
	else 
		showtime = timelimit - (cl.servertime - cl.matchgametimestart);

	if (showtime < 0)
		showtime *= -1;

	minutes = showtime/60;
	seconds = (int)showtime - (minutes*60);
	sprintf(str, "%02i:%02i", minutes, seconds);

	SCR_StringXY(str, show_gameclock_x.value, show_gameclock_y.value);
#endif
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

#ifndef CLIENTONLY
	if (sv.active && sv.paused == PAUSE_AUTO)
		return;
#endif

	if (Key_Dest_Has(kdm_emenu) || Key_Dest_Has(kdm_gmenu))
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
char		*loadingfile;
int CL_DownloadRate(void);

int SCR_GetLoadingStage(void)
{
	return loading_stage;
}
void SCR_SetLoadingStage(int stage)
{
	switch(stage)
	{
	case LS_NONE:
		if (loadingfile)
			Z_Free(loadingfile);
		loadingfile = NULL;
		scr_disabled_for_loading = scr_drawloading = false;
		break;
	case LS_CONNECTION:
		SCR_SetLoadingFile("waiting for connection...");
		break;
	case LS_SERVER:
		if (scr_con_current > vid.height*scr_consize.value)
			scr_con_current = vid.height*scr_consize.value;
		SCR_SetLoadingFile("starting server...");
		break;
	case LS_CLIENT:
		SCR_SetLoadingFile("receiving map info");
		break;
	}
	loading_stage = stage;
}
void SCR_SetLoadingFile(char *str)
{
	if (loadingfile && !strcmp(loadingfile, str))
		return;

	if (loadingfile)
		Z_Free(loadingfile);
	loadingfile = Z_Malloc(strlen(str)+1);
	strcpy(loadingfile, str);

	if (scr_loadingrefresh.ival)
	{
		SCR_UpdateScreen();
	}
}

void SCR_DrawLoading (qboolean opaque)
{
	int sizex, x, y, w, h;
	mpic_t  *pic;
	char *s;
	int qdepth;
	int h2depth;

	if (CSQC_UseGamecodeLoadingScreen())
		return;	//will be drawn as part of the regular screen updates
#ifdef MENU_DAT
	if (MP_UsingGamecodeLoadingScreen())
	{
		if (opaque)
			MP_Draw();
		return;	//menuqc should have just drawn whatever overlays it wanted.
	}
#endif

	//int mtype = M_GameType(); //unused variable
	y = vid.height/2;

	if (*levelshotname)
	{
		pic = R2D_SafeCachePic (levelshotname);
		R_GetShaderSizes(pic, NULL, NULL, true);
		R2D_ImageColours(1, 1, 1, 1);
		R2D_ScalePic (0, 0, vid.width, vid.height, pic);
	}
	else if (opaque)
		R2D_ConsoleBackground (0, vid.height, true);

	if (scr_showloading.ival)
	{
		qdepth = COM_FDepthFile("gfx/loading.lmp", true);
		h2depth = COM_FDepthFile("gfx/menu/loading.lmp", true);

		if (qdepth < h2depth || h2depth > 0xffffff)
		{	//quake files

			pic = R2D_SafeCachePic ("gfx/loading.lmp");
			if (R_GetShaderSizes(pic, &w, &h, true))
			{
				x = (vid.width - w)/2;
				y = (vid.height - 48 - h)/2;
				R2D_ScalePic (x, y, w, h, pic);
				x = (vid.width/2) - 96;
				y += h + 8;
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

				R2D_ImageColours(1, 1, 1, 1);
				Draw_FunString(x+8, y+4, va("Loading %s... %i%%",
					(loading_stage == LS_SERVER) ? "server" : "client",
					current_loading_size * 100 / total_loading_size));
			}
			y += 16;

			if (loadingfile)
			{
				Draw_FunString(x+8, y+4, loadingfile);
				y+=8;
			}
		}
		else
		{	//hexen2 files
			pic = R2D_SafeCachePic ("gfx/menu/loading.lmp");
			if (R_GetShaderSizes(pic, &w, &h, true))
			{
				int		size, count, offset;

				if (!scr_drawloading && loading_stage == 0)
					return;

				offset = (vid.width - w)/2;
				R2D_ScalePic (offset, 0, w, h, pic);

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
	}
	R2D_ImageColours(1, 1, 1, 1);

	if (cl.downloadlist || cls.download)
	{
		unsigned int fcount;
		qofs_t tsize;
		qboolean sizeextra;

		x = vid.width/2 - 160;

		CL_GetDownloadSizes(&fcount, &tsize, &sizeextra);
		//downloading files?
		if (cls.download)
			Draw_FunString(x+8, y+4, va("Downloading %s... %i%%",
				cls.download->localname,
				(int)cls.download->percent));

		if (tsize > 1024*1024*16)
		{
			Draw_FunString(x+8, y+8+4, va("%5ukbps %8umb%s remaining (%i files)",
				(unsigned int)(CL_DownloadRate()/1000.0f),
				(unsigned int)(tsize/(1024*1024)),
				sizeextra?"+":"",
				fcount));
		}
		else
		{
			Draw_FunString(x+8, y+8+4, va("%5ukbps %8ukb%s remaining (%i files)",
				(unsigned int)(CL_DownloadRate()/1000.0f),
				(unsigned int)(tsize/1024),
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
	if (!scr_disabled_for_loading)
	{
		Sbar_Changed ();
		scr_drawloading = true;
		SCR_UpdateScreen ();
		scr_drawloading = false;
		scr_disabled_for_loading = true;
	}

	scr_disabled_time = Sys_DoubleTime();	//realtime tends to change... Hmmm....
}

void SCR_EndLoadingPlaque (void)
{
//	if (!scr_initialized)
//		return;

	scr_disabled_for_loading = false;
	*levelshotname = '\0';
	SCR_SetLoadingStage(0);
	scr_drawloading = false;
}

void SCR_ImageName (const char *mapname)
{
	strcpy(levelshotname, "levelshots/");
	COM_FileBase(mapname, levelshotname + strlen(levelshotname), sizeof(levelshotname)-strlen(levelshotname));

	if (qrenderer)
	{
		R_LoadHiResTexture(levelshotname, NULL, IF_NOWORKER|IF_UIPIC|IF_NOPICMIP|IF_NOMIPMAP|IF_CLAMP);

		if (!R_GetShaderSizes(R2D_SafeCachePic (levelshotname), NULL, NULL, true))
		{
			*levelshotname = '\0';
			if (scr_disabled_for_loading)
				return;
		}
	}
	else
	{
		*levelshotname = '\0';
		if (scr_disabled_for_loading)
			return;
	}

	if (!scr_disabled_for_loading)
	{
		scr_drawloading = true;
		if (qrenderer != QR_NONE)
		{
			Sbar_Changed ();
			SCR_UpdateScreen ();
		}
		scr_drawloading = false;
		scr_disabled_for_loading = true;
	}
	else
	{
		scr_disabled_for_loading = false;
		scr_drawloading = true;
#ifdef GLQUAKE
		if (qrenderer == QR_OPENGL)
		{
			SCR_DrawLoading(false);
			SCR_SetUpToDrawConsole();
			if (Key_Dest_Has(kdm_console) || !*levelshotname)
				SCR_DrawConsole(!!*levelshotname);
		}
#endif
	}
	scr_drawloading = false;

	scr_disabled_time = Sys_DoubleTime();	//realtime tends to change... Hmmm....
	scr_disabled_for_loading = true;
}


//=============================================================================


/*
==================
SCR_SetUpToDrawConsole
==================
*/
void SCR_SetUpToDrawConsole (void)
{
	extern int startuppending;	//true if we're downloading media or something and have not yet triggered the startup action (read: main menu or cinematic)
	if (scr_drawloading)
		return;         // never a console with loading plaque

// decide on the height of the console
	if (!scr_disabled_for_loading)
	{
		float fullscreenpercent = 1;
#ifdef ANDROID
		//android has an onscreen imm that we don't want to obscure
		fullscreenpercent = scr_consize.value;
#endif
		if (!con_stayhidden.ival && (!Key_Dest_Has(~(kdm_console|kdm_game))) && (!cl.sendprespawn && cl.worldmodel && cl.worldmodel->loadstate != MLS_LOADED))
		{
			//force console to fullscreen if we're loading stuff
//			Key_Dest_Add(kdm_console);
			scr_conlines = scr_con_current = vid.height * fullscreenpercent;
		}
		else if (!startuppending && !Key_Dest_Has(kdm_emenu|kdm_gmenu) && (!Key_Dest_Has(~((!con_stayhidden.ival?kdm_console:0)|kdm_game))) && SCR_GetLoadingStage() == LS_NONE && cls.state < ca_active && !Media_PlayingFullScreen() && !CSQC_UnconnectedOkay(false))
		{
			//go fullscreen if we're not doing anything
			if (con_curwindow && !cls.state)
			{
				Key_Dest_Add(kdm_cwindows);
				scr_conlines = 0;
			}
#ifdef VM_UI
			else if (UI_MenuState() || UI_OpenMenu())
				scr_con_current = scr_conlines = 0;
#endif
			else
			{
				if (cls.state < ca_demostart)
				{
					if (con_stayhidden.ival)
					{
						extern int startuppending;
						scr_conlines = 0;
						if (SCR_GetLoadingStage() == LS_NONE)
						{
							if (CL_TryingToConnect())	//if we're trying to connect, make sure there's a loading/connecting screen showing instead of forcing the menu visible
								SCR_SetLoadingStage(LS_CONNECTION);
							else if (!Key_Dest_Has(kdm_emenu) && !startuppending)	//don't force anything until the startup stuff has been done
								M_ToggleMenu_f();
						}
					}
					else
						Key_Dest_Add(kdm_console);
				}
			}
			if (!con_stayhidden.ival && !startuppending && Key_Dest_Has(kdm_console))
				scr_con_current = scr_conlines = vid.height * fullscreenpercent;
			else
				scr_conlines = 0;
		}
		else if ((Key_Dest_Has(kdm_console) || scr_chatmode))
		{
			//go half-screen if we're meant to have the console visible
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
	if (!scr_con_current)
	{
		if (!Key_Dest_Has(kdm_console|kdm_gmenu|kdm_emenu))
			Con_DrawNotify ();      // only draw notify in game
	}
	Con_DrawConsole (scr_con_current, noback);
	if (scr_con_current || Key_Dest_Has(kdm_cwindows))
		clearconsole = 0;
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
qboolean screenshotJPEG(char *filename, enum fs_relative fsroot, int compression, qbyte *screendata, int bytestride, int screenwidth, int screenheight, enum uploadfmt fmt);
#endif
#ifdef AVAIL_PNGLIB
int Image_WritePNG (char *filename, enum fs_relative fsroot, int compression, void **buffers, int numbuffers, int bytestride, int width, int height, enum uploadfmt fmt);
#endif
qboolean WriteBMPFile(char *filename, enum fs_relative fsroot, qbyte *in, int instride, int width, int height, uploadfmt_t fmt);

qboolean WriteTGA(char *filename, enum fs_relative fsroot, qbyte *fte_restrict rgb_buffer, int bytestride, int width, int height, enum uploadfmt fmt)
{
	size_t c, i;
	vfsfile_t *vfs;
	if (fmt != TF_BGRA32 && fmt != TF_RGB24 && fmt != TF_RGBA32 && fmt != TF_BGR24 && fmt != TF_RGBX32 && fmt != TF_BGRX32)
		return false;
	FS_CreatePath(filename, fsroot);
	vfs = FS_OpenVFS(filename, "wb", fsroot);
	if (vfs)
	{
		int ipx,opx;
		qboolean rgb;
		unsigned char header[18];
		memset (header, 0, 18);

		if (fmt == TF_BGRA32 || fmt == TF_RGBA32)
		{
			rgb = fmt==TF_RGBA32;
			ipx = 4;
			opx = 4;
		}
		else if (fmt == TF_RGBX32 || fmt == TF_BGRX32)
		{
			rgb = fmt==TF_RGBX32;
			ipx = 4;
			opx = 3;
		}
		else
		{
			rgb = fmt==TF_RGB24;
			ipx = 3;
			opx = 3;
		}

		header[2] = 2;			// uncompressed type
		header[12] = width&255;
		header[13] = width>>8;
		header[14] = height&255;
		header[15] = height>>8;
		header[16] = opx*8;		// pixel size
		header[17] = 0x00;		// flags

		if (bytestride < 0)
		{	//if we're upside down, lets just use an upside down tga.
			rgb_buffer += bytestride*(height-1);
			bytestride = -bytestride;
			//now we can just do everything else in-place
		}
		else	//our data is top-down, set up the header to also be top-down.
			header[17] = 0x20;

		if (ipx == opx && !rgb)
		{	//can just directly write it
			//bgr24, bgra24
			c = width*height*opx;
		}
		else
		{
			//no need to swap alpha, and if we're just swapping alpha will be fine in-place.
			if (rgb)
			{	//rgb24, rgbx32, rgba32
				qbyte tmp[3];
				// compact in place, and swap
				c = width*height;
				for (i=0 ; i<c ; i++)
				{
					tmp[2] = rgb_buffer[i*ipx+0];
					tmp[1] = rgb_buffer[i*ipx+1];
					tmp[0] = rgb_buffer[i*ipx+2];
					rgb_buffer[i*opx+0] = tmp[0];
					rgb_buffer[i*opx+1] = tmp[1];
					rgb_buffer[i*opx+2] = tmp[2];
				}
			}
			else
			{	//(bgr24), bgrx32, (bgra32)
				qbyte tmp[3];
				// compact in place
				c = width*height;
				for (i=0 ; i<c ; i++)
				{
					tmp[0] = rgb_buffer[i*ipx+0];
					tmp[1] = rgb_buffer[i*ipx+1];
					tmp[2] = rgb_buffer[i*ipx+2];
					rgb_buffer[i*opx+0] = tmp[0];
					rgb_buffer[i*opx+1] = tmp[1];
					rgb_buffer[i*opx+2] = tmp[2];
				}
			}
			c *= opx;
		}

		VFS_WRITE(vfs, header, sizeof(header));
		VFS_WRITE(vfs, rgb_buffer, c);
		VFS_CLOSE(vfs);
	}
	return true;
}

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

qboolean SCR_ScreenShot (char *filename, enum fs_relative fsroot, void **buffer, int numbuffers, int bytestride, int width, int height, enum uploadfmt fmt)
{
#if defined(AVAIL_PNGLIB) || defined(AVAIL_JPEGLIB)
	extern cvar_t scr_sshot_compression;
#endif

	char ext[8];
	void *nbuffers[2];

	switch(fmt)
	{	//nuke any alpha channel...
	case TF_RGBA32: fmt = TF_RGBX32; break;
	case TF_BGRA32: fmt = TF_BGRX32; break;
	default: break;
	}

	if (!bytestride)
		bytestride = width*4;
	if (bytestride < 0)
	{	//fix up the buffers so callers don't have to.
		int nb = numbuffers;
		for (numbuffers = 0; numbuffers < nb && numbuffers < countof(nbuffers); numbuffers++)
			nbuffers[numbuffers] = (char*)buffer[numbuffers] - bytestride*(height-1);
		buffer = nbuffers;
	}

	COM_FileExtension(filename, ext, sizeof(ext));

#ifdef AVAIL_PNGLIB
	if (!Q_strcasecmp(ext, "png") || !Q_strcasecmp(ext, "pns"))
	{
		//png can do bgr+rgb
		//rgba bgra will result in an extra alpha chan
		//actual stereo is also supported. huzzah.
		return Image_WritePNG(filename, fsroot, scr_sshot_compression.value, buffer, numbuffers, bytestride, width, height, fmt);
	}
	else
#endif
#ifdef AVAIL_JPEGLIB
		if (!Q_strcasecmp(ext, "jpeg") || !Q_strcasecmp(ext, "jpg"))
	{
		return screenshotJPEG(filename, fsroot, scr_sshot_compression.value, buffer[0], bytestride, width, height, fmt);
	}
	else
#endif
		if (!Q_strcasecmp(ext, "bmp"))
	{
		return WriteBMPFile(filename, fsroot, buffer[0], bytestride, width, height, fmt);
	}
	else
		if (!Q_strcasecmp(ext, "pcx"))
	{
		int y, x, s;
		qbyte *src, *dest;
		qbyte *srcbuf = buffer[0], *dstbuf;
		if (fmt == TF_RGB24 || fmt == TF_RGBA32 || fmt == TF_RGBX32)
		{
			dstbuf = malloc(width*height);
			s = (fmt == TF_RGB24)?3:4;
			// convert in-place to eight bit
			for (y = 0; y < height; y++)
			{
				src = srcbuf + (bytestride * y);
				dest = dstbuf + (width * y);

				for (x = 0; x < width; x++) {
					*dest++ = MipColor(src[0], src[1], src[2]);
					src += s;
				}
			}
		}
		else if (fmt == TF_BGR24 || fmt == TF_BGRA32 || fmt == TF_BGRX32)
		{
			dstbuf = malloc(width*height);
			s = (fmt == TF_BGR24)?3:4;
			// convert in-place to eight bit
			for (y = 0; y < height; y++)
			{
				src = srcbuf + (bytestride * y);
				dest = dstbuf + (width * y);

				for (x = 0; x < width; x++) {
					*dest++ = MipColor(src[2], src[1], src[0]);
					src += s;
				}
			}
		}
		else
			return false;

		WritePCXfile (filename, fsroot, dstbuf, width, height, width, host_basepal, false);
		free(dstbuf);
	}
	else if (!Q_strcasecmp(ext, "tga"))	//tga
		return WriteTGA(filename, fsroot, buffer[0], bytestride, width, height, fmt);
	else	//extension / type not recognised.
		return false;
	return true;
}

/*
==================
SCR_ScreenShot_f
==================
*/
static void SCR_ScreenShot_f (void)
{
	char			sysname[1024];
	char            pcxname[MAX_QPATH];
	int                     i;
	vfsfile_t *vfs;
	void *rgbbuffer;
	int stride, width, height;
	enum uploadfmt fmt;

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
		int stop = 1000;
		char date[MAX_QPATH];
		time_t tm = time(NULL);
		strftime(date, sizeof(date), "%Y%m%d%H%M%S", localtime(&tm));
	//
	// find a file name to save it to
	//
		for (i=0 ; i<stop ; i++)
		{
			Q_snprintfz(pcxname, sizeof(pcxname), "%s%s-%i.%s", scr_sshot_prefix.string, date, i, scr_sshot_type.string);

			if (!(vfs = FS_OpenVFS(pcxname, "rb", FS_GAMEONLY)))
				break;  // file doesn't exist
			VFS_CLOSE(vfs);
		}
		if (i==stop)
		{
			Con_Printf ("SCR_ScreenShot_f: Couldn't create sequentially named file\n");
			return;
		}
	}

	FS_NativePath(pcxname, FS_GAMEONLY, sysname, sizeof(sysname));

	rgbbuffer = VID_GetRGBInfo(&stride, &width, &height, &fmt);
	if (rgbbuffer)
	{
		if (SCR_ScreenShot(pcxname, FS_GAMEONLY, &rgbbuffer, 1, stride, width, height, fmt))
		{
			Con_Printf ("Wrote %s\n", sysname);
			BZ_Free(rgbbuffer);
			return;
		}
		BZ_Free(rgbbuffer);
	}
	Con_Printf (CON_ERROR "Couldn't write %s\n", sysname);
}

static void *SCR_ScreenShot_Capture(int fbwidth, int fbheight, int *stride, enum uploadfmt *fmt)
{
	int width, height;
	void *buf;
	qboolean okay = false;

	Q_strncpyz(r_refdef.rt_destcolour[0].texname, "megascreeny", sizeof(r_refdef.rt_destcolour[0].texname));
	R2D_RT_Configure(r_refdef.rt_destcolour[0].texname, fbwidth, fbheight, 1, RT_IMAGEFLAGS);
	BE_RenderToTextureUpdate2d(true);

	R2D_FillBlock(0, 0, vid.fbvwidth, vid.fbvheight);

#ifdef VM_CG
	if (!okay && CG_Refresh())
		okay = true;
#endif
#ifdef CSQC_DAT
	if (!okay && CSQC_DrawView())
		okay = true;
//	if (!*r_refdef.rt_destcolour[0].texname)
	{	//csqc protects its own. lazily.
		Q_strncpyz(r_refdef.rt_destcolour[0].texname, "megascreeny", sizeof(r_refdef.rt_destcolour[0].texname));
		BE_RenderToTextureUpdate2d(true);
	}
#endif
	if (!okay && r_worldentity.model)
	{
		V_RenderView ();
		okay = true;
	}
	//fixme: add a way to get+save the depth values too
	if (!okay)
	{
		buf = NULL;
		*stride = width = height = 0;
		*fmt = TF_INVALID;
	}
	else
		buf = VID_GetRGBInfo(stride, &width, &height, fmt);

	R2D_RT_Configure(r_refdef.rt_destcolour[0].texname, 0, 0, 0, RT_IMAGEFLAGS);
	Q_strncpyz(r_refdef.rt_destcolour[0].texname, "", sizeof(r_refdef.rt_destcolour[0].texname));
	BE_RenderToTextureUpdate2d(true);

	if (!buf || width != fbwidth || height != fbheight)
	{
		*fmt = TF_INVALID;
		BZ_Free(buf);
		return NULL;
	}
	return buf;
}

static void SCR_ScreenShot_Mega_f(void)
{
	int stride[2];
	int width[2];
	int height[2];
	void *buffers[2];
	enum uploadfmt fmt[2];
	int numbuffers = 0;
	int buf;
	char filename[MAX_QPATH];
	stereomethod_t osm = r_refdef.stereomethod;

	//massage the rendering code to redraw the screen with an fbo forced.
	//this allows us to generate screenshots which are not otherwise possible to actually draw.
	//this includes larger resolutions (although the lack of stiching leaves us subject to gpu limits of about 16k)

	char *screenyname = Cmd_Argv(1);
	unsigned int fbwidth = strtoul(Cmd_Argv(2), NULL, 0);
	unsigned int fbheight = strtoul(Cmd_Argv(3), NULL, 0);
	char ext[8];

	if (Cmd_IsInsecure())
		return;

	if (qrenderer <= QR_HEADLESS)
	{
		Con_Printf("No renderer active\n");
		return;
	}

	if (!fbwidth)
		fbwidth = sh_config.texture_maxsize;
	fbwidth = bound(1, fbwidth, sh_config.texture_maxsize);
	if (!fbheight)
		fbheight = (fbwidth * 3)/4;
	fbheight = bound(1, fbheight, sh_config.texture_maxsize);

	if (strstr (screenyname, "..") || strchr(screenyname, ':') || *screenyname == '.' || *screenyname == '/')
		screenyname = "";
	if (!*screenyname)
	{
		screenyname = "megascreeny";
		Q_snprintfz(filename, sizeof(filename), "%s%s", scr_sshot_prefix.string, screenyname);
	}
	else
	{
		char *mangle;
		Q_snprintfz(filename, sizeof(filename), "%s", scr_sshot_prefix.string);
		mangle = COM_SkipPath(filename);
		Q_snprintfz(mangle, sizeof(filename) - (mangle-filename), "%s", screenyname);
	}
	if (!strcmp(Cmd_Argv(0), "screenshot_stereo"))
		COM_DefaultExtension (filename, "png", sizeof(filename));
	else
		COM_DefaultExtension (filename, scr_sshot_type.string, sizeof(filename));

	COM_FileExtension(filename, ext, sizeof(ext));
	if (!strcmp(Cmd_Argv(0), "screenshot_stereo") || !strcmp(ext, "pns") || osm == STEREO_QUAD)
		numbuffers = 2;	//stereo png
	else
		numbuffers = 1;

	if (numbuffers == 2 && Q_strcasecmp(ext, "png") && Q_strcasecmp(ext, "pns"))
	{
		Con_Printf("Only png format is supported for stereo screenshots\n");
		return;
	}

	for(buf = 0; buf < numbuffers; buf++)
	{
		if (numbuffers == 2)
		{
			if (buf)
				r_refdef.stereomethod = STEREO_RIGHTONLY;
			else
				r_refdef.stereomethod = STEREO_LEFTONLY;
		}

		buffers[buf] = SCR_ScreenShot_Capture(fbwidth, fbheight, &stride[buf], &fmt[buf]);
		width[buf] = fbwidth;
		height[buf] = fbheight;

		if (width[buf] != width[0] || height[buf] != height[0] || fmt[buf] != fmt[0])
		{	//invalid is better than unmatched.
			BZ_Free(buffers[buf]);
			buffers[buf] = NULL;
		}
	}

	if (numbuffers == 2 && !buffers[1])
		numbuffers = 1;

	//okay, we drew something, we're good to save a screeny.
	if (buffers[0])
	{
		if (SCR_ScreenShot(filename, FS_GAMEONLY, buffers, numbuffers, stride[0], width[0], height[0], fmt[0]))
		{
			char			sysname[1024];
			FS_NativePath(filename, FS_GAMEONLY, sysname, sizeof(sysname));
			Con_Printf ("Wrote %s\n", sysname);
		}
	}
	else
		Con_Printf("Unable to capture screenshot\n");

	for(buf = 0; buf < numbuffers; buf++)
		BZ_Free(buffers[buf]);

	r_refdef.stereomethod = osm;
}

static void SCR_ScreenShot_VR_f(void)
{
	char *screenyname = Cmd_Argv(1);
	int width = atoi(Cmd_Argv(2));
	int stride=0;
	//we spin the camera around, taking slices from equirectangular screenshots
	char filename[MAX_QPATH];
	int height;	//equirectangular 360 * 180 gives a nice clean ratio
	int px = 4;
	int step = atof(Cmd_Argv(3));
	void *left_buffer, *right_buffer, *buf;
	enum uploadfmt fmt;
	int lx, rx, x, y;
	vec3_t baseang;
	float ang;
	qboolean fail = false;
	extern cvar_t r_projection, r_stereo_separation, r_stereo_convergence;;
	VectorCopy(r_refdef.viewangles, baseang);

	if (width <= 2)
		width = 2048;
	if (width > sh_config.texture_maxsize)
		width = sh_config.texture_maxsize;
	height = width/2;
	if (step <= 0)
		step = 5;

	left_buffer = BZF_Malloc (width*height*2*px);
	right_buffer = (qbyte*)left_buffer + width*height*px;

	if (strstr (screenyname, "..") || strchr(screenyname, ':') || *screenyname == '.' || *screenyname == '/')
		screenyname = "";
	if (!*screenyname)
	{
		screenyname = "vr";
		Q_snprintfz(filename, sizeof(filename), "%s%s", scr_sshot_prefix.string, screenyname);
	}
	else
	{
		char *mangle;
		Q_snprintfz(filename, sizeof(filename), "%s", scr_sshot_prefix.string);
		mangle = COM_SkipPath(filename);
		Q_snprintfz(mangle, sizeof(filename) - (mangle-filename), "%s", screenyname);
	}
	COM_DefaultExtension (filename, scr_sshot_type.string, sizeof(filename));



	if (!left_buffer)
	{
		Con_Printf("out of memory\n");
		return;
	}

	r_projection.ival = PROJ_EQUIRECTANGULAR;
	for (lx = 0; lx < width; lx = rx)
	{
		rx = lx + step;
		if (rx > width)
			rx = width;

		r_refdef.stereomethod = STEREO_OFF;

		cl.playerview->simangles[0] = 0;	//pitch is BAD
		cl.playerview->simangles[1] = baseang[1] + r_stereo_convergence.value*0.5;
		cl.playerview->simangles[2] = 0; //roll is BAD
		VectorCopy(cl.playerview->simangles, cl.playerview->viewangles);

		//FIXME: it should be possible to do this more inteligently, and get both strips with a single render.

		ang = M_PI*2*(baseang[1]/360.0 + (lx+0.5*(rx-lx))/width);
		r_refdef.eyeoffset[0] = sin(ang) * r_stereo_separation.value * 0.5;
		r_refdef.eyeoffset[1] = cos(ang) * r_stereo_separation.value * 0.5;
		r_refdef.eyeoffset[2] = 0;
		buf = SCR_ScreenShot_Capture(width, height, &stride, &fmt);
		switch(fmt)
		{
		case TF_BGRA32:
			for (y = 0; y < height; y++)
			for (x = lx; x < rx; x++)
				((unsigned int*)left_buffer)[y*width + x] = ((unsigned int*)buf)[y*width + x];
			break;
		case TF_RGB24:
			for (y = 0; y < height; y++)
			for (x = lx; x < rx; x++)
			{
				((qbyte*)left_buffer)[(y*width + x)*4+0] = ((qbyte*)buf)[(y*width + x)*3+2];
				((qbyte*)left_buffer)[(y*width + x)*4+1] = ((qbyte*)buf)[(y*width + x)*3+1];
				((qbyte*)left_buffer)[(y*width + x)*4+2] = ((qbyte*)buf)[(y*width + x)*3+0];
				((qbyte*)left_buffer)[(y*width + x)*4+3] = 255;
			}
			break;
		default:
			fail = true;
			break;
		}
		BZ_Free(buf);


		cl.playerview->simangles[0] = 0;	//pitch is BAD
		cl.playerview->simangles[1] = baseang[1] - r_stereo_convergence.value*0.5;
		cl.playerview->simangles[2] = 0; //roll is BAD
		VectorCopy(cl.playerview->simangles, cl.playerview->viewangles);


		r_refdef.eyeoffset[0] *= -1;
		r_refdef.eyeoffset[1] *= -1;
		r_refdef.eyeoffset[2] = 0;
		buf = SCR_ScreenShot_Capture(width, height, &stride, &fmt);
		switch(fmt)
		{
		case TF_BGRA32:
			for (y = 0; y < height; y++)
			for (x = lx; x < rx; x++)
				((unsigned int*)right_buffer)[y*width + x] = ((unsigned int*)buf)[y*width + x];
			break;
		case TF_RGB24:
			for (y = 0; y < height; y++)
			for (x = lx; x < rx; x++)
			{
				((qbyte*)right_buffer)[(y*width + x)*4+0] = ((qbyte*)buf)[(y*width + x)*3+2];
				((qbyte*)right_buffer)[(y*width + x)*4+1] = ((qbyte*)buf)[(y*width + x)*3+1];
				((qbyte*)right_buffer)[(y*width + x)*4+2] = ((qbyte*)buf)[(y*width + x)*3+0];
				((qbyte*)right_buffer)[(y*width + x)*4+3] = 255;
			}
			break;
		default:
			fail = true;
			break;
		}
		BZ_Free(buf);
	}

	if (fail)
		Con_Printf ("Unable to capture suitable screen image\n");
	else if (SCR_ScreenShot(filename, FS_GAMEONLY, &left_buffer, 1, stride, width, height*2, TF_BGRA32))
	{
		char			sysname[1024];
		FS_NativePath(filename, FS_GAMEONLY, sysname, sizeof(sysname));
		Con_Printf ("Wrote %s\n", sysname);
	}

	BZ_Free(left_buffer);

	r_projection.ival = r_projection.value;
	VectorCopy(baseang, r_refdef.viewangles);
	VectorClear(r_refdef.eyeoffset);
}

void SCR_ScreenShot_Cubemap_f(void)
{
	void *buffer;
	int stride, fbwidth, fbheight;
	uploadfmt_t fmt;
	char filename[MAX_QPATH];
	char *fname = Cmd_Argv(1);
	int i;
	const struct
	{
		vec3_t angle;
		const char *postfix;
	} sides[] =
	{
		{{0, 0, 0}, "_px"},
		{{0, 180, 0}, "_nx"},
		{{0, 90, 0}, "_py"},
		{{0, 270, 0}, "_ny"},
		{{90, 0, 0}, "_pz"},
		{{-90, 0, 0}, "_nz"}
	};

	r_refdef.stereomethod = STEREO_OFF;

	fbheight = atoi(Cmd_Argv(2));
	if (fbheight < 1)
		fbheight = 512;
	fbwidth = fbheight;

	for (i = 0; i < countof(sides); i++)
	{
		Q_snprintfz(filename, sizeof(filename), "cubemaps/%s%s", fname, sides[i].postfix);
		COM_DefaultExtension (filename, scr_sshot_type.string, sizeof(filename));

		buffer = SCR_ScreenShot_Capture(fbwidth, fbheight, &stride, &fmt);
		if (buffer)
		{
			char			sysname[1024];
			if (SCR_ScreenShot(filename, FS_GAMEONLY, &buffer, 1, stride, fbwidth, fbheight, fmt))
			{
				FS_NativePath(filename, FS_GAMEONLY, sysname, sizeof(sysname));
				Con_Printf ("Wrote %s\n", sysname);
			}
			else
			{
				FS_NativePath(filename, FS_GAMEONLY, sysname, sizeof(sysname));
				Con_Printf ("Failed to write %s\n", sysname);
			}
			BZ_Free(buffer);
		}
	}
}


// from gl_draw.c
qbyte		*draw_chars;				// 8*8 graphic characters

static void SCR_DrawCharToSnap (int num, qbyte *dest, int width)
{
	int		row, col;
	qbyte	*source;
	int		drawline;
	int		x;

	if (!draw_chars)
	{
		size_t lumpsize;
		draw_chars = W_SafeGetLumpName("conchars", &lumpsize);
		if (!draw_chars || lumpsize != 128*128)
			return;
	}

	row = num>>4;
	col = num&15;
	source = draw_chars + (row<<10) + (col<<3);

	drawline = 8;

	while (drawline--)
	{
		for (x=0 ; x<8 ; x++)
			if (source[x] && source[x]!=255)
				dest[x] = source[x];
		source += 128;
		dest -= width;
	}

}

static void SCR_DrawStringToSnap (const char *s, qbyte *buf, int x, int y, int width)
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
	int stride;
	int truewidth;
	int trueheight;

	int     x, y;
	unsigned char		*src, *dest;
	unsigned char		*newbuf;
	int w, h;
	int dx, dy, dex, dey, nx;
	int r, b, g;
	int count;
	float fracw, frach;
	char st[80];
	time_t now;
	enum uploadfmt fmt;
	int src_size;
	int src_red;
	int src_green;
	int src_blue;

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
	newbuf = VID_GetRGBInfo(&truewidth, &trueheight, &stride, &fmt);

	if (fmt == TF_INVALID)
		return false;
	switch(fmt)
	{
	case TF_RGB24:
	case TF_RGBA32:
		src_size = (fmt==TF_RGB24)?3:4;
		src_red = 0;
		src_green = 1;
		src_blue = 2;
		break;
	case TF_BGR24:
	case TF_BGRA32:
		src_size =  (fmt==TF_BGR24)?3:4;
		src_red = 2;
		src_green = 1;
		src_blue = 0;
		break;
	default:
		BZ_Free(newbuf);
		return false;
	}
	//FIXME: bgra

	w = RSSHOT_WIDTH;
	h = RSSHOT_HEIGHT;

	fracw = (float)truewidth / (float)w;
	frach = (float)trueheight / (float)h;

	//scale down first.
	for (y = 0; y < h; y++) {
		dest = newbuf + (w*src_size * y);

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
				src = newbuf + (truewidth * src_size * dy) + dx * src_size;
				for (nx = dx; nx < dex; nx++) {
					r += src[src_red];
					g += src[src_green];
					b += src[src_blue];
					src += src_size;
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
		src = newbuf + (w * src_size * y);
		dest = newbuf + (w * y);

		for (x = 0; x < w; x++) {
			*dest++ = MipColor(src[0], src[1], src[2]);
			src += src_size;
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

	WritePCXfile ("snap.pcx", FS_GAMEONLY, newbuf, w, h, w, host_basepal, true);

	BZ_Free(newbuf);

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
	int pnum;

	for (pnum = 0; pnum < cl.splitclients; pnum++)
	{
		scr_centerprint[pnum].charcount = 0;
		cl.playerview[pnum].cshifts[CSHIFT_CONTENTS].percent = 0;              // no area contents palette on next frame
	}
}

void SCR_TileClear (int skipbottom)
{
	if (r_refdef.vrect.width < r_refdef.grect.width)
	{
		float w;
		// left
		R2D_TileClear (r_refdef.grect.x, r_refdef.grect.y, r_refdef.vrect.x-r_refdef.grect.x, r_refdef.grect.height - skipbottom);
		// right
		w = (r_refdef.grect.x+r_refdef.grect.width) - (r_refdef.vrect.x+r_refdef.vrect.width);
		R2D_TileClear ((r_refdef.grect.x+r_refdef.grect.width) - (w), r_refdef.grect.y, w, r_refdef.grect.height - skipbottom);
	}
	if (r_refdef.vrect.height < r_refdef.grect.height)
	{
		// top
		R2D_TileClear (r_refdef.vrect.x, r_refdef.grect.y,
			r_refdef.vrect.width,
			r_refdef.vrect.y - r_refdef.grect.y);
		// bottom
		R2D_TileClear (r_refdef.vrect.x,
			r_refdef.vrect.y + r_refdef.vrect.height,
			r_refdef.vrect.width,
			(r_refdef.grect.y+r_refdef.grect.height) - skipbottom - (r_refdef.vrect.y + r_refdef.vrect.height));
	}
}



// The 2d refresh stuff.
void SCR_DrawTwoDimensional(int uimenu, qboolean nohud)
{
	qboolean consolefocused = !!Key_Dest_Has(kdm_console|kdm_cwindows);
	RSpeedMark();

	r_refdef.playerview = &cl.playerview[0];

	R2D_ImageColours(1, 1, 1, 1);

	//
	// draw any areas not covered by the refresh
	//
	if (r_netgraph.value)
		R_NetGraph ();

	if (scr_drawloading || loading_stage)
	{
		SCR_DrawLoading(false);

		SCR_ShowPics_Draw();

		if (!scr_disabled_for_loading)
			consolefocused = false;
	}
	else if (nohud)
	{
		SCR_DrawFPS ();
		SCR_CheckDrawCenterString ();
	}
	else if (cl.intermissionmode == IM_NQFINALE || cl.intermissionmode == IM_NQCUTSCENE || cl.intermissionmode == IM_H2FINALE)
	{
		SCR_CheckDrawCenterString ();
	}
	else if (cl.intermissionmode != IM_NONE)
	{
		Sbar_IntermissionOverlay (r_refdef.playerview);
	}
	else
	{
		SCR_DrawNet ();
		SCR_DrawDisk();
		SCR_DrawFPS ();
		SCR_DrawClock();
		SCR_DrawGameClock();
		SCR_DrawTurtle ();
		SCR_DrawPause ();
		SCR_ShowPics_Draw();
		SCR_CheckDrawCenterString ();
	}

//#ifdef TEXTEDITOR
//	if (editoractive)
//		Editor_Draw();
//#endif

	//if the console is not focused, show it scrolling back up behind the menu
	if (!consolefocused)
		SCR_DrawConsole (false);

#ifdef MENU_DAT
	MP_Draw();
#endif
	M_Draw (uimenu);

	//but if the console IS focused, then always show it infront.
	if (consolefocused)
		SCR_DrawConsole (false);

	SCR_DrawCursor();
	SCR_DrawSimMTouchCursor();

	if (R2D_Flush)
		R2D_Flush();

	RSpeedEnd(RSPEED_2D);
}






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
	Cmd_AddCommandD ("screenshot_mega",SCR_ScreenShot_Mega_f, "screenshot_mega <name> [width] [height]\nTakes a screenshot with explicit sizes that are not tied to the size of your monitor, allowing for true monstrosities.");
	Cmd_AddCommandD ("screenshot_stereo",SCR_ScreenShot_Mega_f, "screenshot_stereo <name> [width] [height]\nTakes a simple stereo screenshot.");
	Cmd_AddCommandD ("screenshot_vr",SCR_ScreenShot_VR_f, "screenshot_vr <name> [width]\nTakes a spherical stereoscopic panorama image, for viewing with VR displays.");
	Cmd_AddCommandD ("screenshot_cubemap",SCR_ScreenShot_Cubemap_f, "screenshot_cubemap <name> [size]\nTakes 6 screenshots forming a single cubemap.");
	Cmd_AddCommandD ("envmap",SCR_ScreenShot_Cubemap_f, "Legacy name for the screenshot_cubemap command.");	//legacy 
	Cmd_AddCommand ("screenshot",SCR_ScreenShot_f);
	Cmd_AddCommand ("sizeup",SCR_SizeUp_f);
	Cmd_AddCommand ("sizedown",SCR_SizeDown_f);

	scr_net = R2D_SafePicFromWad ("net");
	scr_turtle = R2D_SafePicFromWad ("turtle");

	scr_initialized = true;
}

void SCR_DeInit (void)
{
	int i;
	if (scr_curcursor)
	{
		rf->VID_SetCursor(scr_curcursor);
		scr_curcursor = NULL;
	}
	for (i = 0; i < countof(key_customcursor); i++)
	{
		if (key_customcursor[i].handle)
		{
			rf->VID_DestroyCursor(key_customcursor[i].handle);
			key_customcursor[i].handle = NULL;
		}
		key_customcursor[i].dirty = true;
	}
	for (i = 0; i < countof(scr_centerprint); i++)
	{
		Z_Free(scr_centerprint[i].string);
		memset(&scr_centerprint[i], 0, sizeof(scr_centerprint[i]));
	}
	if (scr_initialized)
	{
		scr_initialized = false;

		Cmd_RemoveCommand ("screenshot");
		Cmd_RemoveCommand ("screenshot_mega");
		Cmd_RemoveCommand ("screenshot_stereo");
		Cmd_RemoveCommand ("screenshot_vr");
		Cmd_RemoveCommand ("screenshot_cubemap");
		Cmd_RemoveCommand ("envmap");
		Cmd_RemoveCommand ("sizeup");
		Cmd_RemoveCommand ("sizedown");
	}
}