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
// gl_ngraph.c

#include "quakedef.h"
#include "shader.h"

void Draw_ExpandedString(struct font_s *font, float x, float y, conchar_t *str);
static float timehistory[NET_TIMINGS];
static int findex;

#define NET_GRAPHHEIGHT 32

//#define GRAPHTEX
#ifdef GRAPHTEX
static texid_t	netgraphtexture;	// netgraph texture
static shader_t *netgraphshader;
static	unsigned int ngraph_texels[NET_GRAPHHEIGHT][NET_TIMINGS];
#else
static	struct
{
	unsigned int col;
	float height;
} ngraph[NET_TIMINGS];
#endif

static void R_LineGraph (int x, float h)
{
	int		s;
	unsigned		color;

	s = NET_GRAPHHEIGHT;

	if (h == 10000 || h<0)
	{
		color = 0xff00ffff;	// yellow
		h=fabs(h);
	}
	else if (h == 9999)
		color = 0xff0000ff;	// red
	else if (h == 9998)
		color = 0xffff0000;	// blue
	else
		color = 0xffffffff;	// white

#ifdef GRAPHTEX
	if (h>s)
		h = s;
	
	for (i=0 ; i<h ; i++)
		if (i & 1)
			ngraph_texels[NET_GRAPHHEIGHT - i - 1][x] = color&0xffefefef;
		else
			ngraph_texels[NET_GRAPHHEIGHT - i - 1][x] = color;

	for ( ; i<s ; i++)
		ngraph_texels[NET_GRAPHHEIGHT - i - 1][x] = 0x00000000;
#else
	ngraph[x].col = color;
	if (h > s)
		ngraph[x].height = 1;
	else
		ngraph[x].height = h/(float)s;
#endif
}

/*
==============
R_NetGraph
==============
*/
void R_NetGraph (void)
{
	int		a, x, i;
	float y;
	float pi, po, bi, bo;

	vec2_t p[4];
	vec2_t tc[4];
	vec4_t rgba[4];
	extern shader_t *shader_draw_fill;
	conchar_t line[2048];
	float textheight, graphtop;

	float pings, pings_min, pings_max, pingms_stddev, pingfr, dropped, choked, invalid;
	int pingfr_min, pingfr_max;

	x = 0;
	if (r_netgraph.value < 0)
	{
		if (!cl.paused)
			timehistory[++findex&NET_TIMINGSMASK] = (cl.currentpackentities?(cl.currentpackentities->servertime - cl.servertime)*NET_GRAPHHEIGHT*5:0);
		for (a=0 ; a<NET_TIMINGS ; a++)
		{
			i = (findex-a) & NET_TIMINGSMASK;
			R_LineGraph (NET_TIMINGS-1-a, timehistory[i]<0?10000:timehistory[i]);
		}
	}
	else
	{
		float last = 10000;
		CL_CalcNet(r_netgraph.value);
		for (a=0 ; a<NET_TIMINGS ; a++)
		{
			i = (cl.movesequence-a) & NET_TIMINGSMASK;
			if (packet_latency[i] != 10000)
				last = packet_latency[i];
			else if (last >= 0)
				last = -last;
			R_LineGraph (NET_TIMINGS-1-a, last);
		}
	}

	textheight = 4;
#ifdef HAVE_SERVER
	if (sv.state && sv.allocated_client_slots != 1)
		textheight+=2;
#endif
	textheight = ceil(textheight*Font_CharVHeight(font_console)/8)*8;	//might have a small gap underneath

	x =	((vid.width - 320)>>1);	//eww
	x=-x;
	y = vid.height - sb_lines - textheight - NET_GRAPHHEIGHT - 2*8/*box borders*/;

	M_DrawTextBox (x, y, NET_TIMINGS/8, (NET_GRAPHHEIGHT + textheight)/8);
	x = 8;
	if (r_netgraph.ival > 1)
		CL_ShowTrafficUsage(x + NET_TIMINGS + 8, y);
	y += 8; //top border
	graphtop = y+textheight;

	CL_CalcNet2(&pings, &pings_min, &pings_max, &pingms_stddev, &pingfr, &pingfr_min, &pingfr_max, &dropped, &choked, &invalid);
	{
		COM_ParseFunString(CON_WHITEMASK, va("%3.0f%% lost, %3.0f%% choked, %3.0f%% bad", dropped*100, choked*100, invalid*100), line, sizeof(line), false);
		Draw_ExpandedString(font_console, x, y, line);
		y += Font_CharVHeight(font_console);

		COM_ParseFunString(CON_WHITEMASK, va(" ping: %4.1fms %6.2f (%.1f-%.1f)\n", pings*1000, pingms_stddev, pings_min*1000, pings_max*1000), line, sizeof(line), false);
		Draw_ExpandedString(font_console, x, y, line);
		y += Font_CharVHeight(font_console);
	}

	if (NET_GetRates(cls.sockets, &pi, &po, &bi, &bo))
	{
		COM_ParseFunString(CON_WHITEMASK, va("   in: %.1f %.0fb\n", pi, bi), line, sizeof(line), false);
		Draw_ExpandedString(font_console, x, y, line);
		y += Font_CharVHeight(font_console);
		COM_ParseFunString(CON_WHITEMASK, va("  out: %.1f %.0fb\n", po, bo), line, sizeof(line), false);
		Draw_ExpandedString(font_console, x, y, line);
		y += Font_CharVHeight(font_console);
	}
#ifdef HAVE_SERVER
	if (sv.state && sv.allocated_client_slots != 1 && NET_GetRates(svs.sockets, &pi, &po, &bi, &bo))
	{
		COM_ParseFunString(CON_WHITEMASK, va("sv in: %.1f %.0fb\n", pi, bi), line, sizeof(line), false);
		Draw_ExpandedString(font_console, x, y, line);
		y += Font_CharVHeight(font_console);
		COM_ParseFunString(CON_WHITEMASK, va("svout: %.1f %.0fb\n", po, bo), line, sizeof(line), false);
		Draw_ExpandedString(font_console, x, y, line);
		y += Font_CharVHeight(font_console);
	}
#endif

	y = graphtop;	//rounding makes it ugly.

#ifdef GRAPHTEX
	Image_Upload(netgraphtexture, TF_RGBA32, ngraph_texels, NULL, NET_TIMINGS, NET_GRAPHHEIGHT, IF_UIPIC|IF_NOMIPMAP|IF_NOPICMIP);
	R2D_Image(x, y, NET_TIMINGS, NET_GRAPHHEIGHT, 0, 0, 1, 1, netgraphshader);
#else
	for (a=0 ; a<NET_TIMINGS ; a++)
	{
		Vector2Copy(p[3], p[0]);	Vector4Copy(rgba[3], rgba[0]);
		Vector2Copy(p[2], p[1]);	Vector4Copy(rgba[2], rgba[1]);

		Vector2Set(p[2+0], x+a,		y+(1-ngraph[a].height)*NET_GRAPHHEIGHT);
		Vector2Set(p[2+1], x+a,		y+NET_GRAPHHEIGHT);

		Vector2Set(tc[2+0], x/(float)NET_TIMINGS,		(1-ngraph[a].height));
		Vector2Set(tc[2+1], x/(float)NET_TIMINGS,		1);
		Vector4Set(rgba[2+0], ((ngraph[a].col>>0)&0xff)/255.0, ((ngraph[a].col>>8)&0xff)/255.0, ((ngraph[a].col>>16)&0xff)/255.0, ((ngraph[a].col>>24)&0xff)/255.0);
		Vector4Copy(rgba[2+0], rgba[2+1]);

		if (a)
			R2D_Image2dQuad((const vec2_t*)p, (const vec2_t*)tc, (const vec4_t*)rgba, shader_draw_fill);
	}
#endif
}

void R_FrameTimeGraph (float frametime)
{
	int		a, x, i, y;

	vec2_t p[4];
	vec2_t tc[4];
	vec4_t rgba[4];
	extern shader_t *shader_draw_fill;

	timehistory[findex++&NET_TIMINGSMASK] = frametime;

	x = 0;
	for (a=0 ; a<NET_TIMINGS ; a++)
	{
		i = (findex-a) & NET_TIMINGSMASK;
		R_LineGraph (NET_TIMINGS-1-a, timehistory[i]);
	}

	x =	((vid.width - 320)>>1);
	x=-x;
	y = vid.height - sb_lines - 16 - NET_GRAPHHEIGHT;

	M_DrawTextBox (x, y, NET_TIMINGS/8, NET_GRAPHHEIGHT/8);
	x=8;
	y += 8;

#ifdef GRAPHTEX
	Image_Upload(netgraphtexture, TF_RGBA32, ngraph_texels, NULL, NET_TIMINGS, NET_GRAPHHEIGHT, IF_UIPIC|IF_NOMIPMAP|IF_NOPICMIP);
	x=8;
	R2D_Image(x, y, NET_TIMINGS, NET_GRAPHHEIGHT, 0, 0, 1, 1, netgraphshader);
#else
	for (a=0 ; a<NET_TIMINGS ; a++)
	{
		Vector2Copy(p[3], p[0]);	Vector4Copy(rgba[3], rgba[0]);
		Vector2Copy(p[2], p[1]);	Vector4Copy(rgba[2], rgba[1]);

		Vector2Set(p[2+0], x+a,		y+(1-ngraph[a].height)*NET_GRAPHHEIGHT);
		Vector2Set(p[2+1], x+a,		y+NET_GRAPHHEIGHT);

		Vector2Set(tc[2+0], x/(float)NET_TIMINGS,		(1-ngraph[a].height));
		Vector2Set(tc[2+1], x/(float)NET_TIMINGS,		1);
		Vector4Set(rgba[2+0], ((ngraph[a].col>>0)&0xff)/255.0, ((ngraph[a].col>>8)&0xff)/255.0, ((ngraph[a].col>>16)&0xff)/255.0, ((ngraph[a].col>>24)&0xff)/255.0);
		Vector4Copy(rgba[2+0], rgba[2+1]);

		if (a)
			R2D_Image2dQuad((const vec2_t*)p, (const vec2_t*)tc, (const vec4_t*)rgba, shader_draw_fill);
	}
#endif
}

void R_NetgraphInit(void)
{
#ifdef GRAPHTEX
	TEXASSIGN(netgraphtexture, Image_CreateTexture("***netgraph***", NULL, IF_UIPIC|IF_NOMIPMAP|IF_CLAMP));
	netgraphshader = R_RegisterShader("netgraph", SUF_NONE,
		"{\n"
			"program default2d\n"
			"{\n"
				"map $diffuse\n"
				"blendfunc blend\n"
			"}\n"
		"}\n"
		);
	netgraphshader->defaulttextures->base = netgraphtexture;
#endif
}
