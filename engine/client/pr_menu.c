#include "quakedef.h"

#include "pr_common.h"
#include "shader.h"

#ifdef GLQUAKE
#include "glquake.h"
#endif

#if defined(MENU_DAT) || defined(CSQC_DAT)
#include "cl_master.h"

extern unsigned int r2d_be_flags;
#define DRAWFLAG_NORMAL 0
#define DRAWFLAG_ADD 1
#define DRAWFLAG_MODULATE 2
#define DRAWFLAG_MODULATE2 3
static unsigned int PF_SelectDPDrawFlag(int flag)
{
	//flags:
	//0 = blend
	//1 = add
	//2 = modulate
	//3 = modulate*2
	flag &= 3;
	if (flag == DRAWFLAG_ADD)
		return BEF_FORCEADDITIVE;
	else
		return 0;
}

//float	drawfill(vector position, vector size, vector rgb, float alpha, float flag) = #457;
void QCBUILTIN PF_CL_drawfill (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	float *size = G_VECTOR(OFS_PARM1);
	float *rgb = G_VECTOR(OFS_PARM2);
	float alpha = G_FLOAT(OFS_PARM3);
	int flag = prinst->callargc >= 5?G_FLOAT(OFS_PARM4):0;

	r2d_be_flags = PF_SelectDPDrawFlag(flag);
	R2D_ImageColours(rgb[0], rgb[1], rgb[2], alpha);
	R2D_FillBlock(pos[0], pos[1], size[0], size[1]);
	r2d_be_flags = 0;

	G_FLOAT(OFS_RETURN) = 1;
}
//void	drawsetcliparea(float x, float y, float width, float height) = #458;
void QCBUILTIN PF_CL_drawsetcliparea (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	srect_t srect;
	srect.x = G_FLOAT(OFS_PARM0) / (float)vid.width;
	srect.y = (G_FLOAT(OFS_PARM1) / (float)vid.height);
	srect.width = G_FLOAT(OFS_PARM2) / (float)vid.width;
	srect.height = G_FLOAT(OFS_PARM3) / (float)vid.height;
	srect.dmin = -99999;
	srect.dmax = 99999;
	srect.y = (1-srect.y) - srect.height;
	BE_Scissor(&srect);

	G_FLOAT(OFS_RETURN) = 1;
}
//void	drawresetcliparea(void) = #459;
void QCBUILTIN PF_CL_drawresetcliparea (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	BE_Scissor(NULL);
	G_FLOAT(OFS_RETURN) = 1;
}

#define FONT_SLOTS 16
#define FONT_SIZES 4
struct {
	char slotname[16];
	char facename[64];
	int sizes;
	int size[4];
	struct font_s *font[4];
} fontslot[FONT_SLOTS];

static struct font_s *PR_CL_ChooseFont(world_t *world, float szx, float szy)
{
	int fontidx = 0;	//default by default...
	struct font_s *font = font_conchar;

	if (world->g.drawfont)
	{
		fontidx = *world->g.drawfont;
	}

	if (fontidx >= 0 && fontidx < FONT_SLOTS)
	{
		int i, j;
		int fontdiff = 10000;
		for (i = 0; i < fontslot[fontidx].sizes; i++)
		{
			j = abs(szy - fontslot[fontidx].size[i]);
			if (j < fontdiff && fontslot[fontidx].font[i])
			{
				fontdiff = j;
				font = fontslot[fontidx].font[i];
			}
		}
	}
	return font;
}
void PR_CL_BeginString(pubprogfuncs_t *prinst, float vx, float vy, float szx, float szy, float *px, float *py)
{
	world_t *world = prinst->parms->user;
	struct font_s *font = PR_CL_ChooseFont(world, szx, szy);
	if (world->g.drawfontscale && (world->g.drawfontscale[0] || world->g.drawfontscale[1]))
	{
		szx *= world->g.drawfontscale[0];
		szy *= world->g.drawfontscale[1];
	}
	Font_BeginScaledString(font, vx, vy, szx, szy, px, py);
}
int PR_findnamedfont(char *name, qboolean isslotname)
{
	int i;
	if (isslotname)
	{
		for (i = 0; i < FONT_SLOTS; i++)
		{
			if (!stricmp(fontslot[i].slotname, name))
				return i;
		}
	}
	else
	{
		for (i = 0; i < FONT_SLOTS; i++)
		{
			if (!stricmp(fontslot[i].facename, name))
				return i;
		}
	}
	return -1;
}
void PR_ResetFonts(qboolean purge)
{
	int i, j;
	for (i = 0; i < FONT_SLOTS; i++)
	{
		for (j = 0; j < fontslot[i].sizes; j++)
		{
			if (fontslot[i].font[j])
				Font_Free(fontslot[i].font[j]);
			fontslot[i].font[j] = NULL;
		}

		if (purge)
		{
			fontslot[i].sizes = 0;
			fontslot[i].slotname[0] = '\0';
			fontslot[i].facename[0] = '\0';
		}
		else
		{
			for (j = 0; j < fontslot[i].sizes; j++)
				fontslot[i].font[j] = Font_LoadFont(fontslot[i].size[j], fontslot[i].facename);
		}
	}
}
void QCBUILTIN PF_CL_findfont (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *slotname = PR_GetStringOfs(prinst, OFS_PARM0);
	G_FLOAT(OFS_RETURN) = PR_findnamedfont(slotname, true) + 1;	//return default on failure.
}
void QCBUILTIN PF_CL_loadfont (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *slotname = PR_GetStringOfs(prinst, OFS_PARM0);
	char *facename = PR_GetStringOfs(prinst, OFS_PARM1);
	char *sizestr = PR_GetStringOfs(prinst, OFS_PARM2);
	int slotnum = G_FLOAT(OFS_PARM3);
	//float fix_scale = G_FLOAT(OFS_PARM4);
	//float fix_voffset = G_FLOAT(OFS_PARM5);
	int i, sz;

	G_FLOAT(OFS_RETURN) = 0;	//return default on failure.

	if (slotnum < 0 && *slotname)
		slotnum = PR_findnamedfont(slotname, true);
	else if (slotnum < 0)
		slotnum = PR_findnamedfont(facename, false);
	if (slotnum < 0)
		slotnum = PR_findnamedfont("", true);
	if (slotnum < 0)
		return;	//eep.

	if ((unsigned)slotnum >= FONT_SLOTS)
		return;

	//if its changed, purge it.
	if (stricmp(fontslot[slotnum].slotname, slotname) || stricmp(fontslot[slotnum].facename, facename))
	{
		Q_strncpyz(fontslot[slotnum].slotname, slotname, sizeof(fontslot[slotnum].slotname));
		Q_strncpyz(fontslot[slotnum].facename, facename, sizeof(fontslot[slotnum].facename));
		for (i = 0; i < fontslot[slotnum].sizes; i++)
		{
			if (fontslot[slotnum].font[i])
				Font_Free(fontslot[slotnum].font[i]);
			fontslot[slotnum].font[i] = NULL;
		}
	}

	while(*sizestr)
	{
		sizestr = COM_Parse(sizestr);
		sz = atoi(com_token);
		for (i = 0; i < fontslot[slotnum].sizes; i++)
		{
			if (fontslot[slotnum].size[i] == sz)
				break;
		}
		if (i == fontslot[slotnum].sizes)
		{
			if (i >= FONT_SIZES)
				break;
			fontslot[slotnum].size[i] = sz;
			fontslot[slotnum].font[i] = Font_LoadFont(fontslot[slotnum].size[i], facename);
			fontslot[slotnum].sizes++;
		}
	}
	G_FLOAT(OFS_RETURN) = slotnum;
}

void QCBUILTIN PF_CL_DrawTextField (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	float *size = G_VECTOR(OFS_PARM1);
	unsigned int flags = G_FLOAT(OFS_PARM2);
	char *text = PR_GetStringOfs(prinst, OFS_PARM3);
	R_DrawTextField(pos[0], pos[1], size[0], size[1], text, CON_WHITEMASK, flags);
}

//float	drawstring(vector position, string text, vector scale, float alpha, float flag) = #455;
void QCBUILTIN PF_CL_drawcolouredstring (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	char *text = PR_GetStringOfs(prinst, OFS_PARM1);
	float *size = G_VECTOR(OFS_PARM2);
	float alpha = 0;
	float flag = 0;
	float r, g, b;
	float px, py, ipx;

	conchar_t buffer[2048], *str;

	if (prinst->callargc >= 6)
	{
		r = G_FLOAT(OFS_PARM3 + 0);
		g = G_FLOAT(OFS_PARM3 + 1);
		b = G_FLOAT(OFS_PARM3 + 2);
		alpha = G_FLOAT(OFS_PARM4);
		flag = G_FLOAT(OFS_PARM5);	//flag is mandatory to distinguish it.
	}
	else
	{
		r = 1;
		g = 1;
		b = 1;
		alpha = G_FLOAT(OFS_PARM3);
		flag = prinst->callargc >= 5?G_FLOAT(OFS_PARM4):0;
	}

	if (!text)
	{
		G_FLOAT(OFS_RETURN) = -1;	//was null..
		return;
	}

	COM_ParseFunString(CON_WHITEMASK, text, buffer, sizeof(buffer), false);
	str = buffer;

	r2d_be_flags = PF_SelectDPDrawFlag(flag);
	PR_CL_BeginString(prinst, pos[0], pos[1], size[0], size[1], &px, &py);
	ipx = px;
	Font_ForceColour(r, g, b, alpha);
	while(*str)
	{
		if ((*str & CON_CHARMASK) == '\n')
			py += Font_CharHeight();
		else if ((*str & CON_CHARMASK) == '\r')
			px = ipx;
		else
			px = Font_DrawScaleChar(px, py, *str);
		str++;
	}
	Font_InvalidateColour();
	Font_EndString(NULL);
	r2d_be_flags = 0;
}

void QCBUILTIN PF_CL_stringwidth(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	conchar_t buffer[2048], *end;
	float px, py;
	char *text = PR_GetStringOfs(prinst, OFS_PARM0);
	int usecolours = G_FLOAT(OFS_PARM1);
	float *size = (prinst->callargc > 2)?G_VECTOR(OFS_PARM2):NULL;

	end = COM_ParseFunString(CON_WHITEMASK, text, buffer, sizeof(buffer), !usecolours);

	PR_CL_BeginString(prinst, 0, 0, size?size[0]:8, size?size[1]:8, &px, &py);
	px = Font_LineScaleWidth(buffer, end);
	Font_EndString(NULL);

	G_FLOAT(OFS_RETURN) = (px * vid.width) / vid.rotpixelwidth;
}

//float	drawpic(vector position, string pic, vector size, vector rgb, float alpha, float flag) = #456;
void QCBUILTIN PF_CL_drawpic (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	char *picname = PR_GetStringOfs(prinst, OFS_PARM1);
	float *size = G_VECTOR(OFS_PARM2);
	float *rgb = G_VECTOR(OFS_PARM3);
	float alpha = G_FLOAT(OFS_PARM4);
	int flag = prinst->callargc >= 6?(int)G_FLOAT(OFS_PARM5):0;

	mpic_t *p;

	p = R2D_SafeCachePic(picname);
	if (!p)
		p = R2D_SafePicFromWad(picname);

	r2d_be_flags = PF_SelectDPDrawFlag(flag);
	R2D_ImageColours(rgb[0], rgb[1], rgb[2], alpha);
	R2D_Image(pos[0], pos[1], size[0], size[1], 0, 0, 1, 1, p);
	r2d_be_flags = 0;

	G_FLOAT(OFS_RETURN) = 1;
}

void QCBUILTIN PF_CL_drawsubpic (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	float *size = G_VECTOR(OFS_PARM1);
	char *picname = PR_GetStringOfs(prinst, OFS_PARM2);
	float *srcPos = G_VECTOR(OFS_PARM3);
	float *srcSize = G_VECTOR(OFS_PARM4);
	float *rgb = G_VECTOR(OFS_PARM5);
	float alpha = G_FLOAT(OFS_PARM6);
	int flag = prinst->callargc >= 8?(int) G_FLOAT(OFS_PARM7):0;

	mpic_t *p;

	p = R2D_SafeCachePic(picname);
	if (!p)
		p = R2D_SafePicFromWad(picname);

	r2d_be_flags = PF_SelectDPDrawFlag(flag);
	R2D_ImageColours(rgb[0], rgb[1], rgb[2], alpha);
	R2D_Image(	pos[0], pos[1],
				size[0], size[1],
				srcPos[0], srcPos[1],
				srcPos[0]+srcSize[0], srcPos[1]+srcSize[1],
				p);
	r2d_be_flags = 0;

	G_FLOAT(OFS_RETURN) = 1;
}




void QCBUILTIN PF_CL_is_cached_pic (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*str;
	str = PR_GetStringOfs(prinst, OFS_PARM0);
	G_FLOAT(OFS_RETURN) = !!R_RegisterCustom(str, SUF_2D, NULL, NULL);
}

void QCBUILTIN PF_CL_precache_pic (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*str;
	mpic_t	*pic;
	float fromwad;

	str = PR_GetStringOfs(prinst, OFS_PARM0);
	if (prinst->callargc > 1)
		fromwad = G_FLOAT(OFS_PARM1);
	else
		fromwad = false;

	if (fromwad)
		pic = R2D_SafePicFromWad(str);
	else
	{
		if (cls.state
#ifndef CLIENTONLY
			&& !sv.active
#endif
			)
			CL_CheckOrEnqueDownloadFile(str, str, 0);

		pic = R2D_SafeCachePic(str);
	}

	if (pic)
		G_INT(OFS_RETURN) = G_INT(OFS_PARM0);
	else
		G_INT(OFS_RETURN) = 0;
}

void QCBUILTIN PF_CL_free_pic (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	//we don't support this, as the shader could be used elsewhere also, and we have pointers to things.
/*
	char	*str;
	str = PR_GetStringOfs(prinst, OFS_PARM0);
	R_UnloadShader(R_RegisterCustom(str, NULL, NULL));
*/
}

//float	drawcharacter(vector position, float character, vector scale, vector rgb, float alpha, float flag) = #454;
void QCBUILTIN PF_CL_drawcharacter (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	int chara = G_FLOAT(OFS_PARM1);
	float *size = G_VECTOR(OFS_PARM2);
	float *rgb = G_VECTOR(OFS_PARM3);
	float alpha = G_FLOAT(OFS_PARM4);
	int flag = prinst->callargc >= 6?G_FLOAT(OFS_PARM5):0;

	float x, y;

	if (!chara)
	{
		G_FLOAT(OFS_RETURN) = -1;	//was null..
		return;
	}

	//no control chars. use quake ones if so
	if (!(flag & 4))
		if (chara < 32 && chara != '\t')
			chara |= 0xe000;

	r2d_be_flags = PF_SelectDPDrawFlag(flag);
	PR_CL_BeginString(prinst, pos[0], pos[1], size[0], size[1], &x, &y);
	Font_ForceColour(rgb[0], rgb[1], rgb[2], alpha);
	Font_DrawScaleChar(x, y, CON_WHITEMASK | chara);
	Font_InvalidateColour();
	Font_EndString(NULL);
	r2d_be_flags = 0;

	G_FLOAT(OFS_RETURN) = 1;
}

//float	drawrawstring(vector position, string text, vector scale, vector rgb, float alpha, float flag) = #455;
void QCBUILTIN PF_CL_drawrawstring (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{	
	float *pos = G_VECTOR(OFS_PARM0);
	char *text = PR_GetStringOfs(prinst, OFS_PARM1);
	float *size = G_VECTOR(OFS_PARM2);
	float *rgb = G_VECTOR(OFS_PARM3);
	float alpha = G_FLOAT(OFS_PARM4);
	int flag = prinst->callargc >= 6?G_FLOAT(OFS_PARM5):0;
	float x, y;
	unsigned int c;
	int error;

	if (!text)
	{
		G_FLOAT(OFS_RETURN) = -1;	//was null..
		return;
	}

	r2d_be_flags = PF_SelectDPDrawFlag(flag);
	PR_CL_BeginString(prinst, pos[0], pos[1], size[0], size[1], &x, &y);
	Font_ForceColour(rgb[0], rgb[1], rgb[2], alpha);

	while(*text)
	{
		if (1)//VMUTF8)
			c = unicode_decode(&error, text, &text);
		else
		{
			//FIXME: which charset is this meant to be using?
			//quakes? 8859-1? utf8? some weird hacky mixture?
			c = *text++&0xff;
			if ((c&0x7f) < 32)
				c |= 0xe000;	//if its a control char, just use the quake range instead.
			else if (c & 0x80)
				c |= 0xe000;	//if its a high char, just use the quake range instead. we could colour it, but why bother
		}
		x = Font_DrawScaleChar(x, y, CON_WHITEMASK|c);
	}
	Font_InvalidateColour();
	Font_EndString(NULL);
	r2d_be_flags = 0;
}

//void (float width, vector pos1, vector pos2, vector rgb, float alpha, optional float flags) drawline;
void QCBUILTIN PF_CL_drawline (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	//float width = G_FLOAT(OFS_PARM0);
	float *point1	= G_VECTOR(OFS_PARM1);
	float *point2	= G_VECTOR(OFS_PARM2);
	float *rgb	= G_VECTOR(OFS_PARM3);
	float alpha = G_FLOAT(OFS_PARM4);
	int flags	= prinst->callargc >= 6?G_FLOAT(OFS_PARM5):0;
	shader_t *shader_draw_line;

	mesh_t mesh;
	vecV_t vpos[2];
	vec2_t vst[2];
	vec4_t vcol[2];
	index_t idx[2];

	memset(&mesh, 0, sizeof(mesh));
	mesh.indexes = idx;
	mesh.xyz_array = vpos;
	mesh.st_array = vst;
	mesh.colors4f_array[0] = vcol;

	VectorCopy(point1, vpos[0]);
	Vector2Set(vst[0], 0, 0);
	Vector4Set(vcol[0], rgb[0], rgb[1], rgb[2], alpha);

	VectorCopy(point2, vpos[1]);
	Vector2Set(vst[1], 0, 0);
	Vector4Set(vcol[1], rgb[0], rgb[1], rgb[2], alpha);
	mesh.numvertexes = 2;

	mesh.indexes[0] = 0;
	mesh.indexes[1] = 1;
	mesh.numindexes = 2;

	//this shader lookup might get pricy.
	shader_draw_line = R_RegisterShader("shader_draw_line", SUF_NONE,
		"{\n"
			"program defaultfill\n"
			"{\n"
				"map $whiteimage\n"
				"rgbgen exactvertex\n"
				"alphagen vertex\n"
			"}\n"
		"}\n");

	BE_DrawMesh_Single(shader_draw_line, &mesh, NULL, &shader_draw_line->defaulttextures, flags|BEF_LINES);
}

//vector  drawgetimagesize(string pic) = #460;
void QCBUILTIN PF_CL_drawgetimagesize (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *picname = PR_GetStringOfs(prinst, OFS_PARM0);
	mpic_t *p = R2D_SafeCachePic(picname);

	float *ret = G_VECTOR(OFS_RETURN);

	if (p)
	{
		ret[0] = p->width;
		ret[1] = p->height;
		ret[2] = 0;
	}
	else
	{
		ret[0] = 0;
		ret[1] = 0;
		ret[2] = 0;
	}
}

//vector	getmousepos(void)  	= #66;
void QCBUILTIN PF_cl_getmousepos (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *ret = G_VECTOR(OFS_RETURN);
	world_t *world = prinst->parms->user;
	unsigned int target = world->keydestmask;

	if (key_dest_absolutemouse & target)
	{
		ret[0] = mousecursor_x;
		ret[1] = mousecursor_y;
	}
	else
	{
		ret[0] = mousemove_x;
		ret[1] = mousemove_y;
	}

	mousemove_x=0;
	mousemove_y=0;

//	extern int mousecursor_x, mousecursor_y;
//	ret[0] = mousecursor_x;
//	ret[1] = mousecursor_y;
	ret[2] = 0;
}


void QCBUILTIN PF_SubConGetSet (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *conname = PR_GetStringOfs(prinst, OFS_PARM0);
	char *field = PR_GetStringOfs(prinst, OFS_PARM1);
	char *value = (prinst->callargc>2)?PR_GetStringOfs(prinst, OFS_PARM2):NULL;
	console_t *con = Con_FindConsole(conname);
	G_INT(OFS_RETURN) = 0;
	if (!con)
	{
		//null if it doesn't exist
		return;
	}
	if (!strcmp(field, "title"))
	{
		RETURN_TSTRING(con->title);
		if (value)
			Q_strncpyz(con->title, value, sizeof(con->title));
	}
	else if (!strcmp(field, "name"))
	{
		RETURN_TSTRING(con->name);
		if (value && *value && *con->name)
			Q_strncpyz(con->name, value, sizeof(con->name));
	}
	else if (!strcmp(field, "next"))
	{
		con = con->next;
		if (con)
			RETURN_TSTRING(con->name);
	}
	else if (!strcmp(field, "unseen"))
	{
		RETURN_TSTRING(va("%i", con->unseentext));
		if (value)
			con->unseentext = atoi(value);
	}
	else if (!strcmp(field, "markup"))
	{
		int cur;
		if (con->parseflags & PFS_NOMARKUP)
			cur = 0;
		else if (con->parseflags & PFS_KEEPMARKUP)
			cur = 2;
		else
			cur = 1;
		RETURN_TSTRING(va("%i", cur));
		if (value)
		{
			cur = atoi(value);
			con->parseflags &= ~(PFS_NOMARKUP|PFS_KEEPMARKUP);
			if (cur == 0)
				con->parseflags |= PFS_NOMARKUP;
			else if (cur == 2)
				con->parseflags |= PFS_KEEPMARKUP;
		}
	}
	else if (!strcmp(field, "forceutf8"))
	{
		RETURN_TSTRING((con->parseflags&PFS_FORCEUTF8)?"1":"0");
		if (value)
		{
			con->parseflags &= ~PFS_FORCEUTF8;
			if (atoi(value))
				con->parseflags |= PFS_FORCEUTF8;
		}
	}
	else if (!strcmp(field, "hidden"))
	{
		RETURN_TSTRING((con->flags & CON_HIDDEN)?"1":"0");
		if (value)
			con->flags = (con->flags & ~CON_HIDDEN) | (atoi(value)?CON_HIDDEN:0);
	}
	else if (!strcmp(field, "linecount"))
	{
		RETURN_TSTRING(va("%i", con->linecount));
		if (value)
			con->unseentext = atoi(value);
	}
}
void QCBUILTIN PF_SubConPrintf (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char outbuf[4096];
	char *conname = PR_GetStringOfs(prinst, OFS_PARM0);
	char *fmt = PR_GetStringOfs(prinst, OFS_PARM1);
	console_t *con = Con_FindConsole(conname);
	if (!con)
		return;
	PF_sprintf_internal(prinst, pr_globals, fmt, 2, outbuf, sizeof(outbuf));
	Con_PrintCon(con, outbuf);
}
void QCBUILTIN PF_SubConDraw (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *conname = PR_GetStringOfs(prinst, OFS_PARM0);
	float *pos = G_VECTOR(OFS_PARM1);
	float *size = G_VECTOR(OFS_PARM2);
	float fontsize = G_FLOAT(OFS_PARM3);
	console_t *con = Con_FindConsole(conname);
	world_t *world = prinst->parms->user;
	if (!con)
		return;

	if (world->g.drawfontscale)
	{
//		szx *= world->g.drawfontscale[0];
		fontsize *= world->g.drawfontscale[1];
	}

	Con_DrawOneConsole(con, PR_CL_ChooseFont(world, fontsize, fontsize), pos[0], pos[1], size[0], size[1]);
}
qboolean Key_Console (console_t *con, unsigned int unicode, int key);
void Key_ConsoleRelease (console_t *con, unsigned int unicode, int key);
void QCBUILTIN PF_SubConInput (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *conname = PR_GetStringOfs(prinst, OFS_PARM0);
	int ie = G_FLOAT(OFS_PARM1);
	float pa = G_FLOAT(OFS_PARM2);
	float pb = G_FLOAT(OFS_PARM3);
	float pc = G_FLOAT(OFS_PARM4);
	console_t *con = Con_FindConsole(conname);
	G_FLOAT(OFS_RETURN) = 0;
	if (!con)
		return;
	switch(ie)
	{
	case CSIE_KEYDOWN:
		//scan, char
		if ((pa && qcinput_scan != pa) || (pb && pb != qcinput_unicode))
			G_FLOAT(OFS_RETURN) = 0;
		else
			G_FLOAT(OFS_RETURN) = Key_Console(con, pb, MP_TranslateQCtoFTECodes(pa));
		break;
	case CSIE_KEYUP:
		//scan, char
		Key_ConsoleRelease(con, MP_TranslateQCtoFTECodes(pa), pb);
		G_FLOAT(OFS_RETURN) = 0;	//does not inhibit
		break;
	case CSIE_MOUSEABS:
		//x, y
		if (con == con_current && (key_dest_mask & kdm_console))
			break;	//no interfering with the main console!
		con->mousecursor[0] = pa;
		con->mousecursor[1] = pb;
		G_FLOAT(OFS_RETURN) = true;
		break;
	case CSIE_FOCUS:
		//mouse, key
		if (pb >= 0)
		{
			con->flags = (con->flags & ~CONF_KEYFOCUSED) | (pb?CONF_KEYFOCUSED:0);
			G_FLOAT(OFS_RETURN) = true;
		}
		break;
	}
}
#endif







#ifdef MENU_DAT

typedef struct menuedict_s
{
	qboolean	isfree;
	float		freetime; // sv.time when the object was freed
	int			entnum;
	qboolean	readonly;	//world
	void		*fields;
} menuedict_t;



evalc_t menuc_eval_chain;

int menuentsize;

// cvars
#define MENUPROGSGROUP "Menu progs control"
cvar_t forceqmenu = SCVAR("forceqmenu", "0");
cvar_t pr_menuqc_coreonerror = SCVAR("pr_menuqc_coreonerror", "1");


//new generic functions.

void QCBUILTIN PF_mod (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int a = G_FLOAT(OFS_PARM0);
	int b = G_FLOAT(OFS_PARM1);
	if (b == 0)
	{
		Con_Printf("mod by zero\n");
		prinst->pr_trace = 1;
		G_FLOAT(OFS_RETURN) = 0;
	}
	else
		G_FLOAT(OFS_RETURN) = a % b;
}

char *RemapCvarNameFromDPToFTE(char *name)
{
	if (!stricmp(name, "vid_bitsperpixel"))
		return "vid_bpp";
	if (!stricmp(name, "_cl_playermodel"))
		return "model";
	if (!stricmp(name, "_cl_playerskin"))
		return "skin";
	if (!stricmp(name, "_cl_color"))
		return "topcolor";
	if (!stricmp(name, "_cl_name"))
		return "name";

	if (!stricmp(name, "v_contrast"))
		return "v_contrast";
	if (!stricmp(name, "v_hwgamma"))
		return "vid_hardwaregamma";
	if (!stricmp(name, "showfps"))
		return "show_fps";
	if (!stricmp(name, "sv_progs"))
		return "progs";

	return name;
}

static void QCBUILTIN PF_menu_cvar (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	cvar_t	*var;
	char	*str;

	str = PR_GetStringOfs(prinst, OFS_PARM0);

	if (!strcmp(str, "vid_conwidth"))
		G_FLOAT(OFS_RETURN) = vid.width;
	else if (!strcmp(str, "vid_conheight"))
		G_FLOAT(OFS_RETURN) = vid.height;
	else if (!strcmp(str, "vid_pixwidth"))
		G_FLOAT(OFS_RETURN) = vid.pixelwidth;
	else if (!strcmp(str, "vid_pixheight"))
		G_FLOAT(OFS_RETURN) = vid.pixelheight;
	else
	{
		str = RemapCvarNameFromDPToFTE(str);
		var = Cvar_Get(str, "", 0, "menu cvars");
		if (var)
		{
			if (var->latched_string)
				G_FLOAT(OFS_RETURN) = atof(var->latched_string);
			else
				G_FLOAT(OFS_RETURN) = var->value;
		}
		else
			G_FLOAT(OFS_RETURN) = 0;
	}
}
static void QCBUILTIN PF_menu_cvar_set (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*var_name, *val;
	cvar_t *var;

	var_name = PR_GetStringOfs(prinst, OFS_PARM0);
	var_name = RemapCvarNameFromDPToFTE(var_name);
	val = PR_GetStringOfs(prinst, OFS_PARM1);

	var = Cvar_Get(var_name, val, 0, "QC variables");
	Cvar_Set (var, val);
}
static void QCBUILTIN PF_menu_cvar_string (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*str = PR_GetStringOfs(prinst, OFS_PARM0);
	cvar_t *cv = Cvar_Get(RemapCvarNameFromDPToFTE(str), "", 0, "QC variables");
	G_INT( OFS_RETURN ) = (int)PR_SetString( prinst, cv->string );
}

qboolean M_Vid_GetMode(int num, int *w, int *h);
//a bit pointless really
void QCBUILTIN PF_cl_getresolution (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
//	extern cvar_t vid_fullscreen;
	float mode = G_FLOAT(OFS_PARM0);
//	qboolean forfullscreen = (prinst->callargc >= 2)?G_FLOAT(OFS_PARM1):vid_fullscreen.ival;
	float *ret = G_VECTOR(OFS_RETURN);
	int w, h;

	w=h=0;
	M_Vid_GetMode(mode, &w, &h);

	ret[0] = w;
	ret[1] = h;
	ret[2] = 0;
}




void QCBUILTIN PF_nonfatalobjerror (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*s;
	struct edict_s	*ed;
	eval_t *selfp;

	s = PF_VarString(prinst, 0, pr_globals);

	PR_StackTrace(prinst);

	selfp = PR_FindGlobal(prinst, "self", PR_CURRENT, NULL);
	if (selfp && selfp->_int)
	{
		ed = PROG_TO_EDICT(prinst, selfp->_int);

		PR_PrintEdict(prinst, ed);


		if (developer.value)
			prinst->pr_trace = 2;
		else
		{
			ED_Free (prinst, ed);
		}
	}

	Con_Printf ("======OBJECT ERROR======\n%s\n", s);
}






//float	isserver(void)  = #60;
void QCBUILTIN PF_isserver (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
#ifdef CLIENTONLY
	G_FLOAT(OFS_RETURN) = false;
#else
	G_FLOAT(OFS_RETURN) = sv.state != ss_dead;
#endif
}
void QCBUILTIN PF_isdemo (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = !!cls.demoplayback;
}

//float	clientstate(void)  = #62;
void QCBUILTIN PF_clientstate (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = cls.state >= ca_connected ? 2 : 1;	//fit in with netquake	 (we never run a menu.dat dedicated)
}

//too specific to the prinst's builtins.
static void QCBUILTIN PF_Fixme (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	Con_Printf("\n");

	prinst->RunError(prinst, "\nBuiltin %i not implemented.\nMenu is not compatible.", prinst->lastcalledbuiltinnumber);
	PR_BIError (prinst, "bulitin not implemented");
}



void QCBUILTIN PF_CL_precache_sound (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*str;

	str = PR_GetStringOfs(prinst, OFS_PARM0);

	if (S_PrecacheSound(str))
		G_INT(OFS_RETURN) = G_INT(OFS_PARM0);
	else
		G_INT(OFS_RETURN) = 0;
}

//void	setkeydest(float dest) 	= #601;
void QCBUILTIN PF_cl_setkeydest (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	switch((int)G_FLOAT(OFS_PARM0))
	{
	case 0:
		// key_game
		if (!(cls.state == ca_active))
			Key_Dest_Add(kdm_console);
		Key_Dest_Remove(kdm_menu);
		Key_Dest_Remove(kdm_message);
		break;
	case 2:
		// key_menu
		m_state = m_menu_dat;
		Key_Dest_Remove(kdm_message);
		if (!Key_Dest_Has(kdm_menu))
			Key_Dest_Remove(kdm_console);
		Key_Dest_Add(kdm_menu);
		break;
	case 1:
		// key_message
		//Key_Dest_Remove(kdm_menu);
		//Key_Dest_Add(kdm_message);
		// break;
	default:
		PR_BIError (prinst, "PF_setkeydest: wrong destination %i !\n",(int)G_FLOAT(OFS_PARM0));
	}
}
//float	getkeydest(void)	= #602;
void QCBUILTIN PF_cl_getkeydest (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (Key_Dest_Has(kdm_menu))
	{
		if (m_state == m_menu_dat)
			G_FLOAT(OFS_RETURN) = 2;
		else
			G_FLOAT(OFS_RETURN) = 3;
	}
//	else if (Key_Dest_Has(kdm_message))
//		G_FLOAT(OFS_RETURN) = 1;
	else
		G_FLOAT(OFS_RETURN) = 0;
}

//void	setmousetarget(float trg) = #603;
void QCBUILTIN PF_cl_setmousetarget (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	world_t *world = prinst->parms->user;
	unsigned int target = world->keydestmask;
	switch ((int)G_FLOAT(OFS_PARM0))
	{
	case 1:	//1 is delta-based (mt_menu).
		key_dest_absolutemouse &= ~target;
		break;
	case 2:	//2 is absolute (mt_client).
		key_dest_absolutemouse |= target;
		break;
	default:
		PR_BIError(prinst, "PF_setmousetarget: not a valid destination\n");
	}
}

//float	getmousetarget(void)	  = #604;
void QCBUILTIN PF_cl_getmousetarget (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	world_t *world = prinst->parms->user;
	unsigned int target = world->keydestmask;
	G_FLOAT(OFS_RETURN) = (key_dest_absolutemouse&target)?2:1;
}

static void QCBUILTIN PF_Remove_ (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	menuedict_t *ed;

	ed = (void*)G_EDICT(prinst, OFS_PARM0);

	if (ed->isfree)
	{
		Con_DPrintf("Tried removing free entity\n");
		PR_StackTrace(prinst);
		return;
	}

	ED_Free (prinst, (void*)ed);
}

static void QCBUILTIN PF_CopyEntity (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	menuedict_t *in, *out;

	in = (menuedict_t*)G_EDICT(prinst, OFS_PARM0);
	out = (menuedict_t*)G_EDICT(prinst, OFS_PARM1);

	memcpy(out->fields, in->fields, menuentsize);
}

void QCBUILTIN PF_localsound (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *soundname = PR_GetStringOfs(prinst, OFS_PARM0);
	S_LocalSound (soundname);
}

void QCBUILTIN PF_menu_checkextension (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *extname = PR_GetStringOfs(prinst, OFS_PARM0);
	int i;
	G_FLOAT(OFS_RETURN) = 0;

	for (i = 0; i < QSG_Extensions_count; i++)
	{
		if (!QSG_Extensions[i].name)
			continue;
		if (!stricmp(extname, QSG_Extensions[i].name))
		{
			G_FLOAT(OFS_RETURN) = 1;
			break;
		}
	}
}

void QCBUILTIN PF_gettime (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = *prinst->parms->gametime;
}

void QCBUILTIN PF_CL_precache_file (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_INT(OFS_RETURN) = G_INT(OFS_PARM0);
}

//entity	findchainstring(.string _field, string match) = #26;
void QCBUILTIN PF_menu_findchain (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int i, f;
	char *s;
	string_t t;
	menuedict_t *ent, *chain;	//note, all edicts share the common header, but don't use it's fields!
	eval_t *val;

	chain = (menuedict_t *) *prinst->parms->sv_edicts;

	f = G_INT(OFS_PARM0)+prinst->fieldadjust;
	s = PR_GetStringOfs(prinst, OFS_PARM1);

	for (i = 1; i < *prinst->parms->sv_num_edicts; i++)
	{
		ent = (menuedict_t *)EDICT_NUM(prinst, i);
		if (ent->isfree)
			continue;
		t = *(string_t *)&((float*)ent->fields)[f];
		if (!t)
			continue;
		if (strcmp(PR_GetString(prinst, t), s))
			continue;

		val = prinst->GetEdictFieldValue(prinst, (void*)ent, "chain", &menuc_eval_chain);
		if (val)
			val->edict = EDICT_TO_PROG(prinst, (void*)chain);
		chain = ent;
	}

	RETURN_EDICT(prinst, (void*)chain);
}
//entity	findchainfloat(.float _field, float match) = #27;
void QCBUILTIN PF_menu_findchainfloat (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int i, f;
	float s;
	menuedict_t	*ent, *chain;	//note, all edicts share the common header, but don't use it's fields!
	eval_t *val;

	chain = (menuedict_t *) *prinst->parms->sv_edicts;

	f = G_INT(OFS_PARM0)+prinst->fieldadjust;
	s = G_FLOAT(OFS_PARM1);

	for (i = 1; i < *prinst->parms->sv_num_edicts; i++)
	{
		ent = (menuedict_t*)EDICT_NUM(prinst, i);
		if (ent->isfree)
			continue;
		if (((float *)ent->fields)[f] != s)
			continue;

		val = prinst->GetEdictFieldValue(prinst, (void*)ent, "chain", NULL);
		if (val)
			val->edict = EDICT_TO_PROG(prinst, (void*)chain);
		chain = ent;
	}

	RETURN_EDICT(prinst, (void*)chain);
}
//entity	findchainflags(.float _field, float match);
void QCBUILTIN PF_menu_findchainflags (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int i, f;
	int s;
	menuedict_t	*ent, *chain;	//note, all edicts share the common header, but don't use it's fields!
	eval_t *val;

	chain = (menuedict_t *) *prinst->parms->sv_edicts;

	f = G_INT(OFS_PARM0)+prinst->fieldadjust;
	s = G_FLOAT(OFS_PARM1);

	for (i = 1; i < *prinst->parms->sv_num_edicts; i++)
	{
		ent = (menuedict_t*)EDICT_NUM(prinst, i);
		if (ent->isfree)
			continue;
		if ((int)((float *)ent->fields)[f] & s)
			continue;

		val = prinst->GetEdictFieldValue(prinst, (void*)ent, "chain", NULL);
		if (val)
			val->edict = EDICT_TO_PROG(prinst, (void*)chain);
		chain = ent;
	}

	RETURN_EDICT(prinst, (void*)chain);
}

void QCBUILTIN PF_etof(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = G_EDICTNUM(prinst, OFS_PARM0);
}
void QCBUILTIN PF_ftoe(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int entnum = G_FLOAT(OFS_PARM0);

	RETURN_EDICT(prinst, EDICT_NUM(prinst, entnum));
}

void QCBUILTIN PF_IsNotNull(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int str = G_INT(OFS_PARM0);
	G_FLOAT(OFS_RETURN) = !!str;
}

//float 	altstr_count(string str) = #82;
//returns number of single quoted strings in the string.
void QCBUILTIN PF_altstr_count(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *s;
	int count = 0;
	s = PR_GetStringOfs(prinst, OFS_PARM0);
	for (;*s;s++)
	{
		if (*s == '\\')
		{
			if (!*++s)
				break;
		}
		else if (*s == '\'')
			count++;
	}
	G_FLOAT(OFS_RETURN) = count/2;
}
//string  altstr_prepare(string str) = #83;
void QCBUILTIN PF_altstr_prepare(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char outstr[8192], *out;
	char *instr, *in;
	int size;

//	VM_SAFEPARMCOUNT( 1, VM_altstr_prepare );

	instr = PR_GetStringOfs(prinst, OFS_PARM0 );
	//VM_CheckEmptyString( instr );

	for( out = outstr, in = instr, size = sizeof(outstr) - 1 ; size && *in ; size--, in++, out++ )
	{
		if( *in == '\'' )
		{
			*out++ = '\\';
			*out = '\'';
			size--;
		}
		else
			*out = *in;
	}
	*out = 0;

	G_INT( OFS_RETURN ) = (int)PR_TempString( prinst, outstr );
}
//string  altstr_get(string str, float num) = #84;
void QCBUILTIN PF_altstr_get(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *altstr, *pos, outstr[8192], *out;
	int count, size;

//	VM_SAFEPARMCOUNT( 2, VM_altstr_get );

	altstr = PR_GetStringOfs(prinst, OFS_PARM0 );
	//VM_CheckEmptyString( altstr );

	count = G_FLOAT( OFS_PARM1 );
	count = count * 2 + 1;

	for( pos = altstr ; *pos && count ; pos++ )
	{
		if( *pos == '\\' && !*++pos )
			break;
		else if( *pos == '\'' )
			count--;
	}

	if( !*pos )
	{
		G_INT( OFS_RETURN ) = (int)PR_SetString( prinst, "" );
		return;
	}

	for( out = outstr, size = sizeof(outstr) - 1 ; size && *pos ; size--, pos++, out++ )
	{
		if( *pos == '\\' )
		{
			if( !*++pos )
				break;
			*out = *pos;
			size--;
		}
		else if( *pos == '\'' )
			break;
		else
			*out = *pos;
	}

	*out = 0;
	G_INT( OFS_RETURN ) = (int)PR_TempString( prinst, outstr );
}
//string  altstr_set(string str, float num, string set) = #85
void QCBUILTIN PF_altstr_set(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int num;
	char *altstr, *str;
	char *in;
	char outstr[8192], *out;

//	VM_SAFEPARMCOUNT( 3, VM_altstr_set );

	altstr = PR_GetStringOfs(prinst, OFS_PARM0 );
	//VM_CheckEmptyString( altstr );

	num = G_FLOAT( OFS_PARM1 );

	str = PR_GetStringOfs(prinst, OFS_PARM2 );
	//VM_CheckEmptyString( str );

	out = outstr;
	for( num = num * 2 + 1, in = altstr; *in && num; *out++ = *in++ )
	{
		if( *in == '\\' && !*++in )
			break;
		else if( *in == '\'' )
			num--;
	}

	if( !in )
	{
		G_INT( OFS_RETURN ) = (int)PR_SetString( prinst, "" );
		return;
	}
	// copy set in
	for( ; *str; *out++ = *str++ )
		;
	// now jump over the old contents
	for( ; *in ; in++ )
	{
		if( *in == '\'' || (*in == '\\' && !*++in) )
			break;
	}

	if( !in ) {
		G_INT( OFS_RETURN ) = (int)PR_SetString( prinst, "" );
		return;
	}

	strcpy( out, in );
	G_INT( OFS_RETURN ) = (int)PR_TempString( prinst, outstr );

}

//string(string serveraddress) crypto_getkeyfp
void QCBUILTIN PF_crypto_getkeyfp(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	//not supported.
	G_INT(OFS_RETURN) = 0;
}
//string(string serveraddress) crypto_getidfp
void QCBUILTIN PF_crypto_getidfp(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	//not supported.
	G_INT(OFS_RETURN) = 0;
}
//string(string serveraddress) crypto_getencryptlevel
void QCBUILTIN PF_crypto_getencryptlevel(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	//not supported.
	G_INT(OFS_RETURN) = 0;
}
//string(float i) crypto_getmykeyfp
void QCBUILTIN PF_crypto_getmykeyfp(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	//not supported.
	G_INT(OFS_RETURN) = 0;
}
//string(float i) crypto_getmyidfp
void QCBUILTIN PF_crypto_getmyidfp(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	//not supported.
	G_INT(OFS_RETURN) = 0;
}

static struct {
	char *name;
	builtin_t bifunc;
	int ebfsnum;
}  BuiltinList[] = {
	{"checkextension",			PF_menu_checkextension,		1},
	{"error",					PF_error,					2},
	{"objerror",				PF_nonfatalobjerror,		3},
	{"print",					PF_print,					4},
	{"bprint",					PF_Fixme,					5},
	{"msprint",					PF_Fixme,					6},
	{"cprint",					PF_Fixme,					7},
	{"normalize",				PF_normalize,				8},
	{"vlen",					PF_vlen,					9},
	{"vectoyaw",				PF_vectoyaw,				10},
	{"vectoangles",				PF_vectoangles,				11},
	{"random",					PF_random,					12},
	{"localcmd",				PF_localcmd,				13},
	{"cvar",					PF_menu_cvar,				14},
	{"cvar_set",				PF_menu_cvar_set,			15},
	{"dprint",					PF_dprint,					16},
	{"ftos",					PF_ftos,					17},
	{"fabs",					PF_fabs,					18},
	{"vtos",					PF_vtos,					19},
	{"etos",					PF_etos,					20},
	{"stof",					PF_stof,					21},
	{"spawn",					PF_Spawn,					22},
	{"remove",					PF_Remove_,					23},
	{"find",					PF_FindString,				24},
	{"findfloat",				PF_FindFloat,				25},
	{"findchain",				PF_menu_findchain,			26},
	{"findchainfloat",			PF_menu_findchainfloat,		27},
	{"precache_file",			PF_CL_precache_file,		28},
	{"precache_sound",			PF_CL_precache_sound,		29},
	{"coredump",				PF_coredump,				30},
	{"traceon",					PF_traceon,					31},
	{"traceoff",				PF_traceoff,				32},
	{"eprint",					PF_eprint,					33},
	{"rint",					PF_rint,					34},
	{"floor",					PF_floor,					35},
	{"ceil",					PF_ceil,					36},
	{"nextent",					PF_nextent,					37},
	{"sin",						PF_Sin,						38},
	{"cos",						PF_Cos,						39},
	{"sqrt",					PF_Sqrt,					40},
	{"randomvector",			PF_randomvector,			41},
	{"registercvar",			PF_registercvar,			42},
	{"min",						PF_min,						43},
	{"max",						PF_max,						44},
	{"bound",					PF_bound,					45},
	{"pow",						PF_pow,						46},
	{"copyentity",				PF_CopyEntity,				47},
	{"fopen",					PF_fopen,					48},
	{"fclose",					PF_fclose,					49},
	{"fgets",					PF_fgets,					50},
	{"fputs",					PF_fputs,					51},
	{"strlen",					PF_strlen,					52},
	{"strcat",					PF_strcat,					53},
	{"substring",				PF_substring,				54},
	{"stov",					PF_stov,					55},
	{"strzone",					PF_dupstring,				56},
	{"strunzone",				PF_forgetstring,			57},
	{"tokenize",				PF_Tokenize,				58},
	{"argv",					PF_ArgV,					59},
	{"isserver",				PF_isserver,				60},
	{"clientcount",				PF_Fixme,					61},						//float	clientcount(void)  = #61;
	{"clientstate",				PF_clientstate,				62},
	{"clientcommand",			PF_Fixme,					63},						//void	clientcommand(float client, string s)  = #63;
	{"changelevel",				PF_Fixme,					64},						//void	changelevel(string map)  = #64;
	{"localsound",				PF_localsound,				65},
	{"getmousepos",				PF_cl_getmousepos,			66},
	{"gettime",					PF_gettime,					67},
	{"loadfromdata",			PF_loadfromdata,			68},
	{"loadfromfile",			PF_loadfromfile,			69},
	{"mod",						PF_mod,						70},
	{"cvar_string",				PF_menu_cvar_string,		71},
	{"crash",					PF_Fixme,					72},				//void	crash(void)	= #72;
	{"stackdump",				PF_Fixme,					73},			//void	stackdump(void) = #73;
	{"search_begin",			PF_search_begin,			74},
	{"search_end",				PF_search_end,				75},
	{"search_getsize",			PF_search_getsize,			76},
	{"search_getfilename",		PF_search_getfilename,		77},
	{"chr2str",					PF_chr2str,					78},
	{"etof",					PF_etof,					79},
	{"ftoe",					PF_ftoe,					80},
	{"validstring",				PF_IsNotNull,				81},
	{"altstr_count",			PF_altstr_count, 			82},
	{"altstr_prepare",			PF_altstr_prepare, 			83},
	{"altstr_get",				PF_altstr_get,				84},
	{"altstr_set",				PF_altstr_set, 				85},
	{"altstr_ins",				PF_Fixme,					86},
	{"findflags",				PF_FindFlags,				87},
	{"findchainflags",			PF_menu_findchainflags,		88},
	{"mcvar_defstring",			PF_cvar_defstring,			89},
															//gap
	{"abort",					PF_Abort,					211},
															//gap
	{"strstrofs",				PF_strstrofs,				221},
	{"str2chr",					PF_str2chr,					222},
	{"chr2str",					PF_chr2str,					223},
	{"strconv",					PF_strconv,					224},
	{"strpad",					PF_strpad,					225},
	{"infoadd",					PF_infoadd,					226},
	{"infoget",					PF_infoget,					227},
	{"strcmp",					PF_strncmp,					228},
	{"strncmp",					PF_strncmp,					228},
	{"strcasecmp",				PF_strncasecmp,				229},
	{"strncasecmp",				PF_strncasecmp,				230},
															//gap
	{"shaderforname",			PF_shaderforname,			238},
															//gap
	{"hash_createtab",			PF_hash_createtab,			287},
	{"hash_destroytab",			PF_hash_destroytab,			288},
	{"hash_add",				PF_hash_add,				289},
	{"hash_get",				PF_hash_get,				290},
	{"hash_delete",				PF_hash_delete,				291},
	{"hash_getkey",				PF_hash_getkey,				292},
	{"hash_getcb",				PF_hash_getcb,				293},
	{"checkcommand",			PF_checkcommand,			294},
															//gap
	{"print",					PF_print,					339},
	{"keynumtostring_csqc",		PF_cl_keynumtostring,		340},
	{"stringtokeynum",			PF_cl_stringtokeynum,		341},
	{"getkeybind",				PF_cl_getkeybind,			342},
															//gap
	{"isdemo",					PF_isdemo,					349},
															//gap
	{"findfont",				PF_CL_findfont,				356},
	{"loadfont",				PF_CL_loadfont,				357},
															//gap
	{"memalloc",				PF_memalloc,				384},
	{"memfree",					PF_memfree,					385},
	{"memcpy",					PF_memcpy,					386},
	{"memfill8",				PF_memfill8,				387},
	{"memgetval",				PF_memgetval,				388},
	{"memsetval",				PF_memsetval,				389},
	{"memptradd",				PF_memptradd,				390},
															//gap
	{"buf_create",				PF_buf_create,				440},
	{"buf_del",					PF_buf_del,					441},
	{"buf_getsize",				PF_buf_getsize,				442},
	{"buf_copy",				PF_buf_copy,				443},
	{"buf_sort",				PF_buf_sort,				444},
	{"buf_implode",				PF_buf_implode,				445},
	{"bufstr_get",				PF_bufstr_get,				446},
	{"bufstr_set",				PF_bufstr_set,				447},
	{"bufstr_add",				PF_bufstr_add,				448},
	{"bufstr_free",				PF_bufstr_free,				449},
															//gap
	{"iscachedpic",				PF_CL_is_cached_pic,		451},
	{"precache_pic",			PF_CL_precache_pic,			452},
	{"free_pic",				PF_CL_free_pic,				453},
	{"drawcharacter",			PF_CL_drawcharacter,		454},
	{"drawrawstring",			PF_CL_drawrawstring,		455},
	{"drawpic",					PF_CL_drawpic,				456},
	{"drawfill",				PF_CL_drawfill,				457},
	{"drawsetcliparea",			PF_CL_drawsetcliparea,		458},
	{"drawresetcliparea",		PF_CL_drawresetcliparea,	459},
	{"drawgetimagesize",		PF_CL_drawgetimagesize,		460},
	{"cin_open",				PF_cin_open,				461},
	{"cin_close",				PF_cin_close,				462},
	{"cin_setstate",			PF_cin_setstate,			463},
	{"cin_getstate",			PF_cin_getstate,			464},
	{"cin_restart",				PF_cin_restart, 			465},
	{"drawline",				PF_drawline,				466},
	{"drawstring",				PF_CL_drawcolouredstring,	467},
	{"stringwidth",				PF_CL_stringwidth,			468},
	{"drawsubpic",				PF_CL_drawsubpic,			469},
															//gap
//MERGES WITH CLIENT+SERVER BUILTIN MAPPINGS BELOW
	{"asin",					PF_asin,					471},
	{"acos",					PF_acos,					472},
	{"atan",					PF_atan,					473},
	{"atan2",					PF_atan2,					474},
	{"tan",						PF_tan,						475},
	{"strlennocol",				PF_strlennocol,				476},
	{"strdecolorize",			PF_strdecolorize,			477},
	{"strftime",				PF_strftime,				478},
	{"tokenizebyseparator",		PF_tokenizebyseparator,		479},
	{"strtolower",				PF_strtolower,				480},
	{"strtoupper",				PF_strtoupper,				481},
	{"cvar_defstring",			PF_cvar_defstring,			482},
															//gap
	{"strreplace",				PF_strreplace,				484},
	{"strireplace",				PF_strireplace,				485},
															//gap
	{"gecko_create",			PF_cs_gecko_create,			487},
	{"gecko_destroy",			PF_cs_gecko_destroy,		488},
	{"gecko_navigate",			PF_cs_gecko_navigate,		489},
	{"gecko_keyevent",			PF_cs_gecko_keyevent,		490},
	{"gecko_mousemove",			PF_cs_gecko_mousemove,		491},
	{"gecko_resize",			PF_cs_gecko_resize,			492},
	{"gecko_get_texture_extent",PF_cs_gecko_get_texture_extent,493},
	{"crc16",					PF_crc16,					494},
	{"cvar_type",				PF_cvar_type,				495},
	{"numentityfields",			PF_numentityfields,			496},
	{"entityfieldname",			PF_entityfieldname,			497},
	{"entityfieldtype",			PF_entityfieldtype,			498},
	{"getentityfieldstring",	PF_getentityfieldstring,	499},
	{"putentityfieldstring",	PF_putentityfieldstring,	500},
	{"whichpack",				PF_whichpack,				503},
															//gap
	{"uri_escape",				PF_uri_escape,				510},
	{"uri_unescape",			PF_uri_unescape,			511},
	{"num_for_edict",			PF_etof,					512},
	{"uri_get",					PF_uri_get,					513},
	{"tokenize_console",		PF_tokenize_console,		514},
	{"argv_start_index",		PF_argv_start_index,		515},
	{"argv_end_index",			PF_argv_end_index,			516},
	{"buf_cvarlist",			PF_buf_cvarlist,			517},
	{"cvar_description",		PF_cvar_description,		518},
															//gap
	{"soundlength",				PF_soundlength,				534},
	{"buf_loadfile",			PF_buf_loadfile,			535},
	{"buf_writefile",			PF_buf_writefile,			536},
															//gap
	{"setkeydest",				PF_cl_setkeydest,			601},
	{"getkeydest",				PF_cl_getkeydest,			602},
	{"setmousetarget",			PF_cl_setmousetarget,		603},
	{"getmousetarget",			PF_cl_getmousetarget,		604},
	{"callfunction",			PF_callfunction,			605},
	{"writetofile",				PF_writetofile,				606},
	{"isfunction",				PF_isfunction,				607},
	{"getresolution",			PF_cl_getresolution,		608},
	{"keynumtostring",			PF_cl_keynumtostring,		609},
	{"findkeysforcommand",		PF_cl_findkeysforcommand,	610},
	{"gethostcachevalue",		PF_cl_gethostcachevalue,	611},
	{"gethostcachestring",		PF_cl_gethostcachestring,	612},
	{"parseentitydata",			PF_parseentitydata,			613},

	{"stringtokeynum",			PF_cl_stringtokeynum,		614},

	{"resethostcachemasks",		PF_cl_resethostcachemasks,	615},
	{"sethostcachemaskstring",	PF_cl_sethostcachemaskstring,616},
	{"sethostcachemasknumber",	PF_cl_sethostcachemasknumber,617},
	{"resorthostcache",			PF_cl_resorthostcache,		618},
	{"sethostcachesort",		PF_cl_sethostcachesort,		619},
	{"refreshhostcache",		PF_cl_refreshhostcache,		620},
	{"gethostcachenumber",		PF_cl_gethostcachenumber,	621},
	{"gethostcacheindexforkey",	PF_cl_gethostcacheindexforkey,622},
	{"addwantedhostcachekey",	PF_cl_addwantedhostcachekey,623},
#ifdef CL_MASTER
	{"getextresponse",			PF_cl_getextresponse,		624},
#endif
	{"netaddress_resolve",		PF_netaddress_resolve,		625},
															//gap
	{"sprintf",					PF_sprintf,					627},
															//gap
	{"setkeybind",				PF_Fixme,					630},
	{"getbindmaps",				PF_cl_GetBindMap,			631},
	{"setbindmaps",				PF_cl_SetBindMap,			632},
	{"crypto_getkeyfp",			PF_crypto_getkeyfp,			633},
	{"crypto_getidfp",			PF_crypto_getidfp,			634},
	{"crypto_getencryptlevel",	PF_crypto_getencryptlevel,	635},
	{"crypto_getmykeyfp",		PF_crypto_getmykeyfp,		636},
	{"crypto_getmyidfp",		PF_crypto_getmyidfp,		637},
	{"digest_hex",				PF_digest_hex,				639},
	{"crypto_getmyidstatus",	PF_crypto_getmyidfp,		641},
	{NULL}
};
builtin_t menu_builtins[1024];


int MP_BuiltinValid(char *name, int num)
{
	int i;
	for (i = 0; BuiltinList[i].name; i++)
	{
		if (BuiltinList[i].ebfsnum == num)
		{
			if (!strcmp(BuiltinList[i].name, name))
			{
				if (/*BuiltinList[i].bifunc == PF_NoMenu ||*/ BuiltinList[i].bifunc == PF_Fixme)
					return false;
				else
					return true;
			}
		}
	}
	return false;
}

static void MP_SetupBuiltins(void)
{
	int i;
	for (i = 0; i < sizeof(menu_builtins)/sizeof(menu_builtins[0]); i++)
		menu_builtins[i] = PF_Fixme;
	for (i = 0; BuiltinList[i].bifunc; i++)
	{
		if (BuiltinList[i].ebfsnum)
			menu_builtins[BuiltinList[i].ebfsnum] = BuiltinList[i].bifunc;
	}
}

void M_Init_Internal (void);
void M_DeInit_Internal (void);

int inmenuprogs;
progparms_t menuprogparms;
menuedict_t *menu_edicts;
int num_menu_edicts;
world_t menu_world;

func_t mp_init_function;
func_t mp_shutdown_function;
func_t mp_draw_function;
func_t mp_keydown_function;
func_t mp_keyup_function;
func_t mp_toggle_function;

float *mp_time;

jmp_buf mp_abort;


void MP_Shutdown (void)
{
	func_t temp;
	if (!menu_world.progs)
		return;
/*
	{
		char *buffer;
		int size = 1024*1024*8;
		buffer = Z_Malloc(size);
		menuprogs->save_ents(menuprogs, buffer, &size, 1);
		COM_WriteFile("menucore.txt", buffer, size);
		Z_Free(buffer);
	}
*/
	temp = mp_shutdown_function;
	mp_shutdown_function = 0;
	if (temp && !inmenuprogs)
		PR_ExecuteProgram(menu_world.progs, temp);

	PR_Common_Shutdown(menu_world.progs, false);
	menu_world.progs->CloseProgs(menu_world.progs);
	memset(&menu_world, 0, sizeof(menu_world));
	PR_ResetFonts(true);

#ifdef CL_MASTER
	Master_ClearMasks();
#endif

	Key_Dest_Remove(kdm_menu);
	m_state = 0;

	key_dest_absolutemouse &= ~kdm_menu;
}

void *VARGS PR_CB_Malloc(int size);	//these functions should be tracked by the library reliably, so there should be no need to track them ourselves.
void VARGS PR_CB_Free(void *mem);

//Any menu builtin error or anything like that will come here.
void VARGS Menu_Abort (char *format, ...)
{
	va_list		argptr;
	char		string[1024];

	va_start (argptr, format);
	vsnprintf (string,sizeof(string)-1, format,argptr);
	va_end (argptr);

	Con_Printf("Menu_Abort: %s\nShutting down menu.dat\n", string);

	if (pr_menuqc_coreonerror.value)
	{
		char *buffer;
		int size = 1024*1024*8;
		buffer = Z_Malloc(size);
		menu_world.progs->save_ents(menu_world.progs, buffer, &size, size, 3);
		COM_WriteFile("menucore.txt", buffer, size);
		Z_Free(buffer);
	}

	MP_Shutdown();
	M_Init_Internal();

	if (inmenuprogs)	//something in the menu caused the problem, so...
	{
		inmenuprogs = 0;
		longjmp(mp_abort, 1);
	}
}

void MP_CvarChanged(cvar_t *var)
{
	if (menu_world.progs)
	{
		PR_AutoCvar(menu_world.progs, var);
	}
}

double  menutime;
qboolean MP_Init (void)
{
	if (qrenderer == QR_NONE)
	{
		return false;
	}

	if (forceqmenu.value)
		return false;

	M_DeInit_Internal();

	MP_SetupBuiltins();

	memset(&menuc_eval_chain, 0, sizeof(menuc_eval_chain));


	menuprogparms.progsversion = PROGSTRUCT_VERSION;
	menuprogparms.ReadFile = COM_LoadStackFile;//char *(*ReadFile) (char *fname, void *buffer, int *len);
	menuprogparms.FileSize = COM_FileSize;//int (*FileSize) (char *fname);	//-1 if file does not exist
	menuprogparms.WriteFile = QC_WriteFile;//bool (*WriteFile) (char *name, void *data, int len);
	menuprogparms.Printf = (void *)Con_Printf;//Con_Printf;//void (*printf) (char *, ...);
	menuprogparms.Sys_Error = Sys_Error;
	menuprogparms.Abort = Menu_Abort;
	menuprogparms.edictsize = sizeof(menuedict_t);

	menuprogparms.entspawn = NULL;//void (*entspawn) (struct edict_s *ent);	//ent has been spawned, but may not have all the extra variables (that may need to be set) set
	menuprogparms.entcanfree = NULL;//bool (*entcanfree) (struct edict_s *ent);	//return true to stop ent from being freed
	menuprogparms.stateop = NULL;//StateOp;//void (*stateop) (float var, func_t func);
	menuprogparms.cstateop = NULL;//CStateOp;
	menuprogparms.cwstateop = NULL;//CWStateOp;
	menuprogparms.thinktimeop = NULL;//ThinkTimeOp;

	//used when loading a game
	menuprogparms.builtinsfor = NULL;//builtin_t *(*builtinsfor) (int num);	//must return a pointer to the builtins that were used before the state was saved.
	menuprogparms.loadcompleate = NULL;//void (*loadcompleate) (int edictsize);	//notification to reset any pointers.

	menuprogparms.memalloc = PR_CB_Malloc;//void *(*memalloc) (int size);	//small string allocation	malloced and freed randomly
	menuprogparms.memfree = PR_CB_Free;//void (*memfree) (void * mem);


	menuprogparms.globalbuiltins = menu_builtins;//builtin_t *globalbuiltins;	//these are available to all progs
	menuprogparms.numglobalbuiltins = sizeof(menu_builtins) / sizeof(menu_builtins[0]);

	menuprogparms.autocompile = PR_COMPILEIGNORE;//PR_COMPILEEXISTANDCHANGED;//enum {PR_NOCOMPILE, PR_COMPILENEXIST, PR_COMPILECHANGED, PR_COMPILEALWAYS} autocompile;

	menuprogparms.gametime = &menutime;

	menuprogparms.sv_edicts = (struct edict_s **)&menu_edicts;
	menuprogparms.sv_num_edicts = &num_menu_edicts;

	menuprogparms.useeditor = NULL;//sorry... QCEditor;//void (*useeditor) (char *filename, int line, int nump, char **parms);
	menuprogparms.useeditor = QCEditor;//void (*useeditor) (char *filename, int line, int nump, char **parms);
	menuprogparms.user = &menu_world;
	menu_world.keydestmask = kdm_menu;

	menutime = Sys_DoubleTime();
	if (!menu_world.progs)
	{
		int mprogs;
		Con_DPrintf("Initializing menu.dat\n");
		menu_world.progs = InitProgs(&menuprogparms);
		PR_Configure(menu_world.progs, 64*1024*1024, 1);
		mprogs = PR_LoadProgs(menu_world.progs, "menu.dat", 10020, NULL, 0);
		if (mprogs < 0) //no per-progs builtins.
		{
			//failed to load or something
//			CloseProgs(menu_world.progs);
//			menuprogs = NULL;
			return false;
		}
		if (setjmp(mp_abort))
		{
			Con_DPrintf("Failed to initialize menu.dat\n");
			inmenuprogs = false;
			return false;
		}
		inmenuprogs++;

		PF_InitTempStrings(menu_world.progs);

		mp_time = (float*)PR_FindGlobal(menu_world.progs, "time", 0, NULL);
		if (mp_time)
			*mp_time = Sys_DoubleTime();

		menu_world.g.drawfont = (float*)PR_FindGlobal(menu_world.progs, "drawfont", 0, NULL);
		menu_world.g.drawfontscale = (float*)PR_FindGlobal(menu_world.progs, "drawfontscale", 0, NULL);

		PR_ProgsAdded(menu_world.progs, mprogs, "menu.dat");

		menuentsize = PR_InitEnts(menu_world.progs, 8192);


		//'world' edict
//		EDICT_NUM(menu_world.progs, 0)->readonly = true;
		EDICT_NUM(menu_world.progs, 0)->isfree = false;


		mp_init_function = PR_FindFunction(menu_world.progs, "m_init", PR_ANY);
		mp_shutdown_function = PR_FindFunction(menu_world.progs, "m_shutdown", PR_ANY);
		mp_draw_function = PR_FindFunction(menu_world.progs, "m_draw", PR_ANY);
		mp_keydown_function = PR_FindFunction(menu_world.progs, "m_keydown", PR_ANY);
		mp_keyup_function = PR_FindFunction(menu_world.progs, "m_keyup", PR_ANY);
		mp_toggle_function = PR_FindFunction(menu_world.progs, "m_toggle", PR_ANY);
		if (mp_init_function)
			PR_ExecuteProgram(menu_world.progs, mp_init_function);
		inmenuprogs--;

		Con_DPrintf("Initialized menu.dat\n");
		return true;
	}
	return false;
}

static void MP_GameCommand_f(void)
{
	void *pr_globals;
	func_t gamecommand;
	if (!menu_world.progs)
		return;
	gamecommand = PR_FindFunction(menu_world.progs, "GameCommand", PR_ANY);
	if (!gamecommand)
		return;

	pr_globals = PR_globals(menu_world.progs, PR_CURRENT);
	(((string_t *)pr_globals)[OFS_PARM0] = PR_TempString(menu_world.progs, Cmd_Args()));
	PR_ExecuteProgram (menu_world.progs, gamecommand);
}

void MP_CoreDump_f(void)
{
	if (!menu_world.progs)
	{
		Con_Printf("Can't core dump, you need to be running the CSQC progs first.");
		return;
	}

	{
		int size = 1024*1024*8;
		char *buffer = BZ_Malloc(size);
		menu_world.progs->save_ents(menu_world.progs, buffer, &size, size, 3);
		COM_WriteFile("menucore.txt", buffer, size);
		BZ_Free(buffer);
	}
}

void MP_Reload_f(void)
{
	M_Shutdown();
	M_Reinit();
}

void MP_Breakpoint_f(void)
{
	int wasset;
	int isset;
	char *filename = Cmd_Argv(1);
	int line = atoi(Cmd_Argv(2));

	if (!menu_world.progs)
	{
		Con_Printf("Menu not running\n");
		return;
	}
	wasset = menu_world.progs->ToggleBreak(menu_world.progs, filename, line, 3);
	isset = menu_world.progs->ToggleBreak(menu_world.progs, filename, line, 2);

	if (wasset == isset)
		Con_Printf("Breakpoint was not valid\n");
	else if (isset)
		Con_Printf("Breakpoint has been set\n");
	else
		Con_Printf("Breakpoint has been cleared\n");

	Cvar_Set(Cvar_FindVar("debugger"), "1");
}

void MP_RegisterCvarsAndCmds(void)
{
	Cmd_AddCommand("coredump_menuqc", MP_CoreDump_f);
	Cmd_AddCommand("menu_restart", MP_Reload_f);
	Cmd_AddCommand("menu_cmd", MP_GameCommand_f);
	Cmd_AddCommand("breakpoint_menu", MP_Breakpoint_f);

	Cvar_Register(&forceqmenu, MENUPROGSGROUP);
	Cvar_Register(&pr_menuqc_coreonerror, MENUPROGSGROUP);

	if (COM_CheckParm("-qmenu"))
		Cvar_Set(&forceqmenu, "1");
}

void MP_Draw(void)
{
	if (!menu_world.progs)
		return;
	if (setjmp(mp_abort))
		return;

	menutime = Sys_DoubleTime();
	if (mp_time)
		*mp_time = menutime;

	inmenuprogs++;
	if (mp_draw_function)
		PR_ExecuteProgram(menu_world.progs, mp_draw_function);
	inmenuprogs--;
}

void MP_Keydown(int key, int unicode)
{
	extern qboolean	keydown[K_MAX];

#ifdef TEXTEDITOR
	if (editormodal)
		return;
#endif

	if (setjmp(mp_abort))
		return;

	if (key == 'c')
	{
		if (keydown[K_LCTRL] || keydown[K_RCTRL])
		{
			MP_Shutdown();
			M_Init_Internal();
			return;
		}
	}
	if (key == K_ESCAPE)
	{
		if (keydown[K_LSHIFT] || keydown[K_RSHIFT])
		{
			Con_ToggleConsole_Force();
			return;
		}
	}

	menutime = Sys_DoubleTime();
	if (mp_time)
		*mp_time = menutime;

	inmenuprogs++;
	if (mp_keydown_function)
	{
		void *pr_globals = PR_globals(menu_world.progs, PR_CURRENT);
		G_FLOAT(OFS_PARM0) = MP_TranslateFTEtoQCCodes(key);
		G_FLOAT(OFS_PARM1) = unicode;
		PR_ExecuteProgram(menu_world.progs, mp_keydown_function);
	}
	inmenuprogs--;
}

void MP_Keyup(int key, int unicode)
{
#ifdef TEXTEDITOR
	if (editormodal)
		return;
#endif

	if (setjmp(mp_abort))
		return;

	menutime = Sys_DoubleTime();
	if (mp_time)
		*mp_time = menutime;

	inmenuprogs++;
	if (mp_keyup_function)
	{
		void *pr_globals = PR_globals(menu_world.progs, PR_CURRENT);
		G_FLOAT(OFS_PARM0) = MP_TranslateFTEtoQCCodes(key);
		G_FLOAT(OFS_PARM1) = unicode;
		PR_ExecuteProgram(menu_world.progs, mp_keyup_function);
	}
	inmenuprogs--;
}

qboolean MP_Toggle(void)
{
	if (!menu_world.progs)
		return false;
#ifdef TEXTEDITOR
	if (editormodal)
		return false;
#endif

	if (setjmp(mp_abort))
		return false;

	menutime = Sys_DoubleTime();
	if (mp_time)
		*mp_time = menutime;

	inmenuprogs++;
	if (mp_toggle_function)
	{
		void *pr_globals = PR_globals(menu_world.progs, PR_CURRENT);
		G_FLOAT(OFS_PARM0) = 1;
		PR_ExecuteProgram(menu_world.progs, mp_toggle_function);
	}
	inmenuprogs--;

	return true;
}
#endif
