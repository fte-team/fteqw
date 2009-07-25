#include "quakedef.h"

#ifdef MENU_DAT

#ifdef RGLQUAKE
#include "glquake.h"
#ifdef Q3SHADERS
#include "shader.h"
#endif
#endif

#include "pr_common.h"


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

//float	isfunction(string function_name) = #607;
void PF_isfunction (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*name = PR_GetStringOfs(prinst, OFS_PARM0);
	G_FLOAT(OFS_RETURN) = !!PR_FindFunction(prinst, name, PR_CURRENT);
}

//void	callfunction(...) = #605;
void PF_callfunction (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*name;
	func_t f;
	if (*prinst->callargc < 1)
		PR_BIError(prinst, "callfunction needs at least one argument\n");
	name = PR_GetStringOfs(prinst, OFS_PARM0+(*prinst->callargc-1)*3);
	f = PR_FindFunction(prinst, name, PR_CURRENT);
	if (f)
		PR_ExecuteProgram(prinst, f);
}

//void	loadfromfile(string file) = #69;
void PF_loadfromfile (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*filename = PR_GetStringOfs(prinst, OFS_PARM0);
	char *file = COM_LoadTempFile(filename);

	int size;

	if (!file)
	{
		G_FLOAT(OFS_RETURN) = -1;
		return;
	}

	while(prinst->restoreent(prinst, file, &size, NULL))
	{
		file += size;
	}

	G_FLOAT(OFS_RETURN) = 0;
}

void PF_loadfromdata (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*file = PR_GetStringOfs(prinst, OFS_PARM0);

	int size;

	if (!*file)
	{
		G_FLOAT(OFS_RETURN) = -1;
		return;
	}

	while(prinst->restoreent(prinst, file, &size, NULL))
	{
		file += size;
	}

	G_FLOAT(OFS_RETURN) = 0;
}

void PF_parseentitydata(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	void	*ed = G_EDICT(prinst, OFS_PARM0);
	char	*file = PR_GetStringOfs(prinst, OFS_PARM1);

	int size;

	if (!*file)
	{
		G_FLOAT(OFS_RETURN) = -1;
		return;
	}

	if (!prinst->restoreent(prinst, file, &size, ed))
		Con_Printf("parseentitydata: missing opening data\n");
	else
	{
		file += size;
		while(*file < ' ' && *file)
			file++;
		if (*file)
			Con_Printf("parseentitydata: too much data\n");
	}
	
	G_FLOAT(OFS_RETURN) = 0;
}

void PF_mod (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = (float)(((int)G_FLOAT(OFS_PARM0))%((int)G_FLOAT(OFS_PARM1)));
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

static void PF_menu_cvar (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	cvar_t	*var;
	char	*str;
	
	str = PR_GetStringOfs(prinst, OFS_PARM0);
	str = RemapCvarNameFromDPToFTE(str);
	{
		var = Cvar_Get(str, "", 0, "menu cvars");
		if (var)
		{
			if (var->latched_string)
				G_FLOAT(OFS_RETURN) = atof(var->latched_string);			else
				G_FLOAT(OFS_RETURN) = var->value;
		}
		else
			G_FLOAT(OFS_RETURN) = 0;
	}
}
static void PF_menu_cvar_set (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*var_name, *val;
	cvar_t *var;

	var_name = PR_GetStringOfs(prinst, OFS_PARM0);
	var_name = RemapCvarNameFromDPToFTE(var_name);
	val = PR_GetStringOfs(prinst, OFS_PARM1);

	var = Cvar_Get(var_name, val, 0, "QC variables");
	Cvar_Set (var, val);
}
static void PF_menu_cvar_string (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*str = PR_GetStringOfs(prinst, OFS_PARM0);
	cvar_t *cv = Cvar_Get(RemapCvarNameFromDPToFTE(str), "", 0, "QC variables");
	G_INT( OFS_RETURN ) = (int)PR_SetString( prinst, cv->string );
}

qboolean M_Vid_GetMove(int num, int *w, int *h);
//a bit pointless really
void PF_cl_getresolution (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float mode = G_FLOAT(OFS_PARM0);
	float *ret = G_VECTOR(OFS_RETURN);
	int w, h;

	w=h=0;
	M_Vid_GetMove(mode, &w, &h);

	ret[0] = w;
	ret[1] = h;
	ret[2] = 0;
}




void PF_nonfatalobjerror (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*s;
	struct edict_s	*ed;
	eval_t *selfp;
	
	s = PF_VarString(prinst, 0, pr_globals);

	PR_StackTrace(prinst);

	selfp = PR_FindGlobal(prinst, "self", PR_CURRENT);
	if (selfp && selfp->_int)
	{
		ed = PROG_TO_EDICT(prinst, selfp->_int);

		PR_PrintEdict(prinst, ed);


		if (developer.value)
			*prinst->pr_trace = 2;
		else
		{
			ED_Free (prinst, ed);
		}
	}

	Con_Printf ("======OBJECT ERROR======\n%s\n", s);
}






//float	isserver(void)  = #60;
void PF_isserver (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
#ifdef CLIENTONLY
	G_FLOAT(OFS_RETURN) = false;
#else
	G_FLOAT(OFS_RETURN) = sv.state != ss_dead;
#endif
}

//float	clientstate(void)  = #62;
void PF_clientstate (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = cls.state >= ca_connected ? 2 : 1;	//fit in with netquake	 (we never run a menu.dat dedicated)
}

//too specific to the prinst's builtins.
static void PF_Fixme (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	Con_Printf("\n");

	prinst->RunError(prinst, "\nBuiltin %i not implemented.\nMenu is not compatible.", prinst->lastcalledbuiltinnumber);
	PR_BIError (prinst, "bulitin not implemented");
}



void PF_CL_precache_sound (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*str;
	
	str = PR_GetStringOfs(prinst, OFS_PARM0);

	if (S_PrecacheSound(str))
		G_INT(OFS_RETURN) = G_INT(OFS_PARM0);
	else
		G_INT(OFS_RETURN) = 0;
}


void PF_CL_is_cached_pic (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*str;
	
	str = PR_GetStringOfs(prinst, OFS_PARM0);

//	if (Draw_IsCached)
//		G_FLOAT(OFS_RETURN) = !!Draw_IsCached(str);
//	else
		G_FLOAT(OFS_RETURN) = 1;
}

void PF_CL_precache_pic (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*str;
	mpic_t	*pic;
	float fromwad;

	str = PR_GetStringOfs(prinst, OFS_PARM0);
	if (*prinst->callargc > 1)
		fromwad = G_FLOAT(OFS_PARM1);
	else
		fromwad = false;

	if (fromwad)
		pic = Draw_SafePicFromWad(str);
	else
	{
		if (cls.state
#ifndef CLIENTONLY
			&& !sv.active
#endif
			)
			CL_CheckOrEnqueDownloadFile(str, str, 0);

		pic = Draw_SafeCachePic(str);
	}

	if (pic)
		G_INT(OFS_RETURN) = G_INT(OFS_PARM0);
	else
		G_INT(OFS_RETURN) = 0;
}

void PF_CL_free_pic (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*str;
	
	str = PR_GetStringOfs(prinst, OFS_PARM0);

	//we don't support this.
}


//float	drawcharacter(vector position, float character, vector scale, vector rgb, float alpha, float flag) = #454;
void PF_CL_drawcharacter (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	int chara = G_FLOAT(OFS_PARM1);
	float *size = G_VECTOR(OFS_PARM2);
	float *rgb = G_VECTOR(OFS_PARM3);
	float alpha = G_FLOAT(OFS_PARM4);
//	float flag = G_FLOAT(OFS_PARM5);

	const float fsize = 0.0625;
	float frow, fcol;

	if (!chara)
	{
		G_FLOAT(OFS_RETURN) = -1;	//was null..
		return;
	}

	chara &= 255;
	frow = (chara>>4)*fsize;
	fcol = (chara&15)*fsize;

	if (Draw_ImageColours)
		Draw_ImageColours(rgb[0], rgb[1], rgb[2], alpha);
	if (Draw_Image)
		Draw_Image(pos[0], pos[1], size[0], size[1], fcol, frow, fcol+fsize, frow+fsize, Draw_CachePic("conchars"));

	G_FLOAT(OFS_RETURN) = 1;
}
//float	drawrawstring(vector position, string text, vector scale, vector rgb, float alpha, float flag) = #455;
void PF_CL_drawrawstring (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	char *text = PR_GetStringOfs(prinst, OFS_PARM1);
	float *size = G_VECTOR(OFS_PARM2);
//	float *rgb = G_VECTOR(OFS_PARM3);
//	float alpha = G_FLOAT(OFS_PARM4);
//	float flag = G_FLOAT(OFS_PARM5);

	if (!text)
	{
		G_FLOAT(OFS_RETURN) = -1;	//was null..
		return;
	}

	while(*text)
	{
		G_FLOAT(OFS_PARM1) = *text++;
		PF_CL_drawcharacter(prinst, pr_globals);
		pos[0] += size[0];
	}
}

//float	drawstring(vector position, string text, vector scale, float alpha, float flag) = #455;
void PF_CL_drawcolouredstring (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	char *text = PR_GetStringOfs(prinst, OFS_PARM1);
	float *size = G_VECTOR(OFS_PARM2);
//	float *alpha = G_FLOAT(OFS_PARM3);
//	float flag = G_FLOAT(OFS_PARM4);

	if (!text)
	{
		G_FLOAT(OFS_RETURN) = -1;	//was null..
		return;
	}

	Draw_FunString(pos[0], pos[1], text);
}

void PF_CL_stringwidth(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *text = PR_GetStringOfs(prinst, OFS_PARM0);
	qboolean usecolours = G_FLOAT(OFS_PARM1);
	float fontsize;
	if (*prinst->callargc > 2)
		fontsize = G_FLOAT(OFS_PARM2);
	else
		fontsize = 1;
	if (usecolours)
	{
		G_FLOAT(OFS_RETURN) = COM_FunStringLength(text)*fontsize;
	}
	else
	{
		G_FLOAT(OFS_RETURN) = strlen(text)*fontsize;
	}
}

#define DRAWFLAG_NORMAL 0
#define DRAWFLAG_ADD 1
#define DRAWFLAG_MODULATE 2
#define DRAWFLAG_MODULATE2 3

#ifdef Q3SHADERS
void GLDraw_ShaderPic (int x, int y, int width, int height, shader_t *pic, float r, float g, float b, float a);
#endif
//float	drawpic(vector position, string pic, vector size, vector rgb, float alpha, float flag) = #456;
void PF_CL_drawpic (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	char *picname = PR_GetStringOfs(prinst, OFS_PARM1);
	float *size = G_VECTOR(OFS_PARM2);
	float *rgb = G_VECTOR(OFS_PARM3);
	float alpha = G_FLOAT(OFS_PARM4);
	float flag = G_FLOAT(OFS_PARM5);

	mpic_t *p;

#ifdef RGLQUAKE
	if (qrenderer == QR_OPENGL)
	{
#ifdef Q3SHADERS
		shader_t *s;

		s = R_RegisterCustom(picname, NULL, NULL);
		if (s)
		{
			GLDraw_ShaderPic(pos[0], pos[1], size[0], size[1], s, rgb[0], rgb[1], rgb[2], alpha);
			return;
		}
#endif

		if (flag == 1)	//add
			qglBlendFunc(GL_SRC_ALPHA, GL_ONE);
		else if(flag == 2)	//modulate
			qglBlendFunc(GL_DST_COLOR, GL_ZERO);
		else if(flag == 3)	//modulate*2
			qglBlendFunc(GL_DST_COLOR,GL_SRC_COLOR);
		else	//blend
			qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
#endif

	p = Draw_SafeCachePic(picname);

	if (Draw_ImageColours)
		Draw_ImageColours(rgb[0], rgb[1], rgb[2], alpha);
	if (Draw_Image)
		Draw_Image(pos[0], pos[1], size[0], size[1], 0, 0, 1, 1, p);

#ifdef RGLQUAKE
	if (qrenderer == QR_OPENGL)
		qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#endif

	G_FLOAT(OFS_RETURN) = 1;
}

void PF_CL_drawsubpic (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	float *size = G_VECTOR(OFS_PARM1);
	char *picname = PR_GetStringOfs(prinst, OFS_PARM2);
	float *srcPos = G_VECTOR(OFS_PARM3);
	float *srcSize = G_VECTOR(OFS_PARM4);
	float *rgb = G_VECTOR(OFS_PARM5);
	float alpha = G_FLOAT(OFS_PARM6);
	int flag = (int) G_FLOAT(OFS_PARM7);

	mpic_t *p;

	if(pos[2] || size[2])
		Con_Printf("VM_drawsubpic: z value%s from %s discarded\n",(pos[2] && size[2]) ? "s" : " ",((pos[2] && size[2]) ? "pos and size" : (pos[2] ? "pos" : "size")));

#ifdef RGLQUAKE
	if (qrenderer == QR_OPENGL)
	{
#ifdef Q3SHADERS
		shader_t *s;

		s = R_RegisterCustom(picname, NULL, NULL);
		if (s)
		{
			GLDraw_ShaderPic(pos[0], pos[1], size[0], size[1], s, rgb[0], rgb[1], rgb[2], alpha);
			return;
		}
#endif

		if (flag == 1)	//add
			qglBlendFunc(GL_SRC_ALPHA, GL_ONE);
		else if(flag == 2)	//modulate
			qglBlendFunc(GL_DST_COLOR, GL_ZERO);
		else if(flag == 3)	//modulate*2
			qglBlendFunc(GL_DST_COLOR,GL_SRC_COLOR);
		else	//blend
			qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
#endif

	p = Draw_SafeCachePic(picname);

	if (Draw_ImageColours)
		Draw_ImageColours(rgb[0], rgb[1], rgb[2], alpha);
	if (Draw_Image)
		Draw_Image(	pos[0], pos[1],
					size[0], size[1],
					srcPos[0], srcPos[1],
					srcPos[0]+srcSize[0], srcPos[1]+srcSize[1],
					p);

#ifdef RGLQUAKE
	if (qrenderer == QR_OPENGL)
		qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#endif

	G_FLOAT(OFS_RETURN) = 1;
}

//float	drawfill(vector position, vector size, vector rgb, float alpha, float flag) = #457;
void PF_CL_drawfill (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	float *size = G_VECTOR(OFS_PARM1);
	float *rgb = G_VECTOR(OFS_PARM2);
	float alpha = G_FLOAT(OFS_PARM3);
#ifdef RGLQUAKE
	if (qrenderer == QR_OPENGL)
	{
		qglColor4f(rgb[0], rgb[1], rgb[2], alpha);

		qglDisable(GL_TEXTURE_2D);

		qglBegin(GL_QUADS);
		qglVertex2f(pos[0],			pos[1]);
		qglVertex2f(pos[0]+size[0],	pos[1]);
		qglVertex2f(pos[0]+size[0],	pos[1]+size[1]);
		qglVertex2f(pos[0],			pos[1]+size[1]);
		qglEnd();

		qglEnable(GL_TEXTURE_2D);
	}
#endif
	G_FLOAT(OFS_RETURN) = 1;
}
//void	drawsetcliparea(float x, float y, float width, float height) = #458;
void PF_CL_drawsetcliparea (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float x = G_FLOAT(OFS_PARM0), y = G_FLOAT(OFS_PARM1), w = G_FLOAT(OFS_PARM2), h = G_FLOAT(OFS_PARM3);

#ifdef RGLQUAKE
	if (qrenderer == QR_OPENGL && qglScissor)
	{

		x *= (float)glwidth/vid.width;
		y *= (float)glheight/vid.height;

		w *= (float)glwidth/vid.width;
		h *= (float)glheight/vid.height;

		//add a pixel because this makes DP's menus come out right.
		x-=1;
		y-=1;
		w+=2;
		h+=2;


		qglScissor (x, glheight-(y+h), w, h);
		qglEnable(GL_SCISSOR_TEST);
		G_FLOAT(OFS_RETURN) = 1;
		return;
	}
#endif
	G_FLOAT(OFS_RETURN) = 0;
}
//void	drawresetcliparea(void) = #459;
void PF_CL_drawresetcliparea (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
#ifdef RGLQUAKE
	if (qrenderer == QR_OPENGL)
	{
		qglDisable(GL_SCISSOR_TEST);
		G_FLOAT(OFS_RETURN) = 1;
		return;
	}
#endif
	G_FLOAT(OFS_RETURN) = 0;
}

//void (float width, vector rgb, float alpha, float flags, vector pos1, ...) drawline;
void PF_CL_drawline (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *rgb = G_VECTOR(OFS_PARM1);
	float alpha = G_FLOAT(OFS_PARM2);
	float *pos = G_VECTOR(OFS_PARM4);
	int numpoints = *prinst->callargc-4;

#ifdef RGLQUAKE	// :(

	if (qrenderer == QR_OPENGL)
	{
		qglColor4f(rgb[0], rgb[1], rgb[2], alpha);
		qglBegin(GL_LINES);
		while (numpoints-->0)
		{
			qglVertex3fv(pos);
			pos += 3;
		}

		qglEnd();
	}
#endif
}

//vector  drawgetimagesize(string pic) = #460;
void PF_CL_drawgetimagesize (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *picname = PR_GetStringOfs(prinst, OFS_PARM0);
	mpic_t *p = Draw_SafeCachePic(picname);

	float *ret = G_VECTOR(OFS_RETURN);

	if (!p)
		p = Draw_SafeCachePic(va("%s.tga", picname));

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

//void	setkeydest(float dest) 	= #601;
void PF_cl_setkeydest (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	switch((int)G_FLOAT(OFS_PARM0))
	{
	case 0:
		// key_game
		key_dest = key_game;
		break;
	case 2:
		// key_menu
		if (key_dest != key_console)
			key_dest = key_menu;
		m_state = m_menu_dat;
		break;
	case 1:
		// key_message
		// key_dest = key_message
		// break;
	default:
		PR_BIError (prinst, "PF_setkeydest: wrong destination %i !\n",(int)G_FLOAT(OFS_PARM0));
	}
}
//float	getkeydest(void)	= #602;
void PF_cl_getkeydest (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	switch(key_dest)
	{
	case key_game:
		G_FLOAT(OFS_RETURN) = 0;
		break;
	case key_menu:
		if (m_state == m_menu_dat)
			G_FLOAT(OFS_RETURN) = 2;
		else
			G_FLOAT(OFS_RETURN) = 3;
		break;
	case 1:
		// key_message
		// key_dest = key_message
		// break;
	default:
		G_FLOAT(OFS_RETURN) = 3;
	}
}

//void	setmousetarget(float trg) = #603;
void PF_cl_setmousetarget (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	extern int mouseusedforgui;
	switch ((int)G_FLOAT(OFS_PARM0))
	{
	case 1:
		mouseusedforgui = true;
		break;
	case 2:
		mouseusedforgui = false;
		break;
	default:
		PR_BIError(prinst, "PF_setmousetarget: not a valid destination\n");
	}
}

//float	getmousetarget(void)	  = #604;
void PF_cl_getmousetarget (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	extern int mouseusedforgui;
	G_FLOAT(OFS_RETURN) = mouseusedforgui?1:2;
}

int MP_TranslateDPtoFTECodes(int code);
//string	keynumtostring(float keynum) = #609;
void PF_cl_keynumtostring (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int code = G_FLOAT(OFS_PARM0);

	code = MP_TranslateDPtoFTECodes (code);

	RETURN_TSTRING(Key_KeynumToString(code));
}

void PF_cl_getkeybind (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *binding = Key_GetBinding(G_FLOAT(OFS_PARM0));
	RETURN_TSTRING(binding);
}

int MP_TranslateDPtoFTECodes(int code);
//string	findkeysforcommand(string command) = #610;
void PF_cl_findkeysforcommand (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *cmdname = PR_GetStringOfs(prinst, OFS_PARM0);
	int keynums[2];
	char keyname[512];

	M_FindKeysForCommand(cmdname, keynums);

	keyname[0] = '\0';

	Q_strncatz (keyname, va(" \'%i\'", MP_TranslateFTEtoDPCodes(keynums[0])), sizeof(keyname));
	Q_strncatz (keyname, va(" \'%i\'", MP_TranslateFTEtoDPCodes(keynums[1])), sizeof(keyname));

	RETURN_TSTRING(keyname);
}

//vector	getmousepos(void)  	= #66;
void PF_cl_getmousepos (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *ret = G_VECTOR(OFS_RETURN);
	extern int mousemove_x, mousemove_y;
	extern int mousecursor_x, mousecursor_y;

	ret[0] = mousemove_x;
	ret[1] = mousemove_y;

	mousemove_x=0;
	mousemove_y=0;

//	ret[0] = mousecursor_x;
//	ret[1] = mousecursor_y;
	ret[2] = 0;
}


static void PF_Remove_ (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	menuedict_t *ed;
	
	ed = (void*)G_EDICT(prinst, OFS_PARM0);

	if (ed->isfree)
	{
		Con_DPrintf("Tried removing free entity\n");
		return;
	}

	ED_Free (prinst, (void*)ed);
}

static void PF_CopyEntity (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	menuedict_t *in, *out;

	in = (menuedict_t*)G_EDICT(prinst, OFS_PARM0);
	out = (menuedict_t*)G_EDICT(prinst, OFS_PARM1);

	memcpy(out->fields, in->fields, menuentsize);
}

#ifdef CL_MASTER
#include "cl_master.h"

typedef enum{
	SLIST_HOSTCACHEVIEWCOUNT,
	SLIST_HOSTCACHETOTALCOUNT,
	SLIST_MASTERQUERYCOUNT,
	SLIST_MASTERREPLYCOUNT,
	SLIST_SERVERQUERYCOUNT,
	SLIST_SERVERREPLYCOUNT,
	SLIST_SORTFIELD,
	SLIST_SORTDESCENDING
} hostcacheglobal_t;

void PF_M_gethostcachevalue (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	hostcacheglobal_t hcg = G_FLOAT(OFS_PARM0);
	G_FLOAT(OFS_RETURN) = 0;
	switch(hcg)
	{
	case SLIST_HOSTCACHEVIEWCOUNT:
		CL_QueryServers();
		NET_CheckPollSockets();
		G_FLOAT(OFS_RETURN) = Master_NumSorted();
		return;
	case SLIST_HOSTCACHETOTALCOUNT:
		CL_QueryServers();
		NET_CheckPollSockets();
		G_FLOAT(OFS_RETURN) = Master_TotalCount();
		return;

	case SLIST_MASTERQUERYCOUNT:
	case SLIST_MASTERREPLYCOUNT:
	case SLIST_SERVERQUERYCOUNT:
	case SLIST_SERVERREPLYCOUNT:
		G_FLOAT(OFS_RETURN) = 0;
		return;

	case SLIST_SORTFIELD:
		G_FLOAT(OFS_RETURN) = Master_GetSortField();
		return;
	case SLIST_SORTDESCENDING:
		G_FLOAT(OFS_RETURN) = Master_GetSortDescending();
		return;
	default:
		return;
	}
}

//void 	resethostcachemasks(void) = #615;
void PF_M_resethostcachemasks(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	Master_ClearMasks();
}
//void 	sethostcachemaskstring(float mask, float fld, string str, float op) = #616;
void PF_M_sethostcachemaskstring(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int mask = G_FLOAT(OFS_PARM0);
	int field = G_FLOAT(OFS_PARM1);
	char *str = PR_GetStringOfs(prinst, OFS_PARM2);
	int op = G_FLOAT(OFS_PARM3);

	Master_SetMaskString(mask, field, str, op);
}
//void	sethostcachemasknumber(float mask, float fld, float num, float op) = #617;
void PF_M_sethostcachemasknumber(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int mask = G_FLOAT(OFS_PARM0);
	int field = G_FLOAT(OFS_PARM1);
	int str = G_FLOAT(OFS_PARM2);
	int op = G_FLOAT(OFS_PARM3);

	Master_SetMaskInteger(mask, field, str, op);
}
//void 	resorthostcache(void) = #618;
void PF_M_resorthostcache(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	Master_SortServers();
}
//void	sethostcachesort(float fld, float descending) = #619;
void PF_M_sethostcachesort(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	Master_SetSortField(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1));
}
//void	refreshhostcache(void) = #620;
void PF_M_refreshhostcache(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	MasterInfo_Begin();
}
//float	gethostcachenumber(float fld, float hostnr) = #621;
void PF_M_gethostcachenumber(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float ret = 0;
	int keynum = G_FLOAT(OFS_PARM0);
	int svnum = G_FLOAT(OFS_PARM1);
	serverinfo_t *sv;
	sv = Master_SortedServer(svnum);

	ret = Master_ReadKeyFloat(sv, keynum);

	G_FLOAT(OFS_RETURN) = ret;
}
void PF_M_gethostcachestring (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *ret;
	int keynum = G_FLOAT(OFS_PARM0);
	int svnum = G_FLOAT(OFS_PARM1);
	serverinfo_t *sv;

	sv = Master_SortedServer(svnum);
	ret = Master_ReadKeyString(sv, keynum);

	RETURN_TSTRING(ret);
}

//float	gethostcacheindexforkey(string key) = #622;
void PF_M_gethostcacheindexforkey(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *keyname = PR_GetStringOfs(prinst, OFS_PARM0);

	G_FLOAT(OFS_RETURN) = Master_KeyForName(keyname);
}
//void	addwantedhostcachekey(string key) = #623;
void PF_M_addwantedhostcachekey(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	PF_M_gethostcacheindexforkey(prinst, pr_globals);
}

void PF_M_getextresponse(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	//this does something weird
	G_INT(OFS_RETURN) = 0;
}
#else

void PF_gethostcachevalue (progfuncs_t *prinst, struct globalvars_s *pr_globals){G_FLOAT(OFS_RETURN) = 0;}
void PF_gethostcachestring (progfuncs_t *prinst, struct globalvars_s *pr_globals) {G_INT(OFS_RETURN) = 0;}
//void 	resethostcachemasks(void) = #615;
void PF_M_resethostcachemasks(progfuncs_t *prinst, struct globalvars_s *pr_globals){}
//void 	sethostcachemaskstring(float mask, float fld, string str, float op) = #616;
void PF_M_sethostcachemaskstring(progfuncs_t *prinst, struct globalvars_s *pr_globals){}
//void	sethostcachemasknumber(float mask, float fld, float num, float op) = #617;
void PF_M_sethostcachemasknumber(progfuncs_t *prinst, struct globalvars_s *pr_globals){}
//void 	resorthostcache(void) = #618;
void PF_M_resorthostcache(progfuncs_t *prinst, struct globalvars_s *pr_globals){}
//void	sethostcachesort(float fld, float descending) = #619;
void PF_M_sethostcachesort(progfuncs_t *prinst, struct globalvars_s *pr_globals){}
//void	refreshhostcache(void) = #620;
void PF_M_refreshhostcache(progfuncs_t *prinst, struct globalvars_s *pr_globals) {}
//float	gethostcachenumber(float fld, float hostnr) = #621;
void PF_M_gethostcachenumber(progfuncs_t *prinst, struct globalvars_s *pr_globals){G_FLOAT(OFS_RETURN) = 0;}
//float	gethostcacheindexforkey(string key) = #622;
void PF_M_gethostcacheindexforkey(progfuncs_t *prinst, struct globalvars_s *pr_globals){G_FLOAT(OFS_RETURN) = 0;}
//void	addwantedhostcachekey(string key) = #623;
void PF_M_addwantedhostcachekey(progfuncs_t *prinst, struct globalvars_s *pr_globals){}
#endif


void PF_localsound (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *soundname = PR_GetStringOfs(prinst, OFS_PARM0);
	S_LocalSound (soundname);
}

#define skip1 PF_Fixme,
#define skip5 skip1 skip1 skip1 skip1 skip1
#define skip10 skip5 skip5
#define skip50 skip10 skip10 skip10 skip10 skip10
#define skip100 skip50 skip50

void PF_menu_checkextension (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	//yeah, this is a stub... not sure what form extex
	G_FLOAT(OFS_RETURN) = 0;
}

void PF_gettime (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = *prinst->parms->gametime;
}

void PF_CL_precache_file (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_INT(OFS_RETURN) = G_INT(OFS_PARM0);
}

//entity	findchainstring(.string _field, string match) = #26;
void PF_menu_findchain (progfuncs_t *prinst, struct globalvars_s *pr_globals)
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
void PF_menu_findchainfloat (progfuncs_t *prinst, struct globalvars_s *pr_globals)
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

void PF_etof(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = G_EDICTNUM(prinst, OFS_PARM0);
}
void PF_ftoe(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int entnum = G_FLOAT(OFS_PARM0);

	RETURN_EDICT(prinst, EDICT_NUM(prinst, entnum));
}

void PF_IsNotNull(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int str = G_INT(OFS_PARM0);
	G_FLOAT(OFS_RETURN) = !!str;
}

void PF_cl_stringtokeynum(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int i;
	int modifier;
	char *s;

	s = PR_GetStringOfs(prinst, OFS_PARM0);
	i = Key_StringToKeynum(s, &modifier);
	if (i < 0 || modifier != ~0)
	{
		G_FLOAT(OFS_RETURN) = -1;
		return;
	}
	i = MP_TranslateFTEtoDPCodes(i);
	G_FLOAT(OFS_RETURN) = i;
}


//float 	altstr_count(string str) = #82;
//returns number of single quoted strings in the string.
void PF_altstr_count(progfuncs_t *prinst, struct globalvars_s *pr_globals)
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
void PF_altstr_prepare(progfuncs_t *prinst, struct globalvars_s *pr_globals)
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
void PF_altstr_get(progfuncs_t *prinst, struct globalvars_s *pr_globals)
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
	G_INT( OFS_RETURN ) = (int)PR_SetString( prinst, outstr );
}
//string  altstr_set(string str, float num, string set) = #85
void PF_altstr_set(progfuncs_t *prinst, struct globalvars_s *pr_globals)
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

builtin_t menu_builtins[] = {
//0
	PF_Fixme,
	PF_menu_checkextension,				//void 	checkextension(string ext) = #1;
	PF_error,
	PF_nonfatalobjerror,
	PF_print,
	skip1				//void 	bprint(string text,...)	= #5;
	skip1				//void	sprint(float clientnum, string text,...) = #6;
	skip1				//void 	centerprint(string text,...) = #7;
	PF_normalize,		//vector	normalize(vector v) 	= #8;
	PF_vlen,			//float 	vlen(vector v)			= #9;
//10
	PF_vectoyaw,//10		//float  	vectoyaw(vector v)		= #10;
	PF_vectoangles,//11		//vector 	vectoangles(vector v)	= #11;
	PF_random,//12
	PF_localcmd,//13
	PF_menu_cvar,//14
	PF_menu_cvar_set,//15
	PF_dprint,//16
	PF_ftos,//17
	PF_fabs,//18
	PF_vtos,//19

	PF_etos,//20
	PF_stof,//21
	PF_Spawn,//22
	PF_Remove_,//23
	PF_FindString,//24
	PF_FindFloat,//25
	PF_menu_findchain,//26			//entity	findchainstring(.string _field, string match) = #26;
	PF_menu_findchainfloat,//27		//entity	findchainfloat(.float _field, float match) = #27;
	PF_CL_precache_file,//28
	PF_CL_precache_sound,//29

//30
	PF_coredump,				//void	coredump(void) = #30;
	PF_traceon,				//void	traceon(void) = #31;
	PF_traceoff,				//void	traceoff(void) = #32;
	PF_eprint,				//void	eprint(entity e)  = #33;
	PF_rint,
	PF_floor,
	PF_ceil,
	PF_nextent,
	PF_Sin,
	PF_Cos,
//40
	PF_Sqrt,
	PF_randomvector,
	PF_registercvar,
	PF_min,
	PF_max,
	PF_bound,
	PF_pow,
	PF_CopyEntity,
	PF_fopen,
	PF_fclose,
//50
	PF_fgets,
	PF_fputs,
	PF_strlen,
	PF_strcat,
	PF_substring,
	PF_stov,
	PF_dupstring,
	PF_forgetstring,
	PF_Tokenize,
	PF_ArgV,
//60
	PF_isserver,
	skip1						//float	clientcount(void)  = #61;
	PF_clientstate,
	skip1						//void	clientcommand(float client, string s)  = #63;
	skip1						//void	changelevel(string map)  = #64;
	PF_localsound,
	PF_cl_getmousepos,
	PF_gettime,
	PF_loadfromdata,
	PF_loadfromfile,
//70
	PF_mod,//0
	PF_menu_cvar_string,//1
	PF_Fixme,//2				//void	crash(void)	= #72;
	PF_Fixme,//3				//void	stackdump(void) = #73;
	PF_search_begin,//4
	PF_search_end,//5
	PF_search_getsize ,//6
	PF_search_getfilename,//7
	PF_chr2str,//8
	PF_etof,//9				//float 	etof(entity ent) = #79;
//80
	PF_ftoe,//10
	PF_IsNotNull,


	PF_altstr_count, //float 	altstr_count(string str) = #82;
	PF_altstr_prepare, //string  altstr_prepare(string str) = #83;
	PF_altstr_get, //string  altstr_get(string str, float num) = #84;
	PF_altstr_set, //string  altstr_set(string str, float num, string set) = #85

	skip1	//altstr_ins
	skip1	//findflags
	skip1	//findchainflags
	PF_cvar_defstring,
//90
	skip10
//100
	skip100
//200
	skip10
	skip10
//220
	skip1
	PF_strstrofs,						// #221 float(string str, string sub[, float startpos]) strstrofs (FTE_STRINGS)
	PF_str2chr,						// #222 float(string str, float ofs) str2chr (FTE_STRINGS)
	PF_chr2str,						// #223 string(float c, ...) chr2str (FTE_STRINGS)
	PF_strconv,						// #224 string(float ccase, float calpha, float cnum, string s, ...) strconv (FTE_STRINGS)
	PF_strpad,						// #225 string(float chars, string s, ...) strpad (FTE_STRINGS)
	PF_infoadd,						// #226 string(string info, string key, string value, ...) infoadd (FTE_STRINGS)
	PF_infoget,						// #227 string(string info, string key) infoget (FTE_STRINGS)
	PF_strncmp,							// #228 float(string s1, string s2, float len) strncmp (FTE_STRINGS)
	PF_strncasecmp,					// #229 float(string s1, string s2) strcasecmp (FTE_STRINGS)
//230
	PF_strncasecmp,					// #230 float(string s1, string s2, float len) strncasecmp (FTE_STRINGS)
	skip1
	skip1
	skip1
	skip1
	skip1
	skip1
	skip1
	skip1
	skip1
//240
	skip10
	skip50
//300
	skip100
//400
	skip10
	skip10
	skip10
	skip10
//440
	PF_buf_create,					// #440 float() buf_create (DP_QC_STRINGBUFFERS)
	PF_buf_del,						// #441 void(float bufhandle) buf_del (DP_QC_STRINGBUFFERS)
	PF_buf_getsize,					// #442 float(float bufhandle) buf_getsize (DP_QC_STRINGBUFFERS)
	PF_buf_copy,					// #443 void(float bufhandle_from, float bufhandle_to) buf_copy (DP_QC_STRINGBUFFERS)
	PF_buf_sort,					// #444 void(float bufhandle, float sortpower, float backward) buf_sort (DP_QC_STRINGBUFFERS)
	PF_buf_implode,					// #445 string(float bufhandle, string glue) buf_implode (DP_QC_STRINGBUFFERS)
	PF_bufstr_get,					// #446 string(float bufhandle, float string_index) bufstr_get (DP_QC_STRINGBUFFERS)
	PF_bufstr_set,					// #447 void(float bufhandle, float string_index, string str) bufstr_set (DP_QC_STRINGBUFFERS)
	PF_bufstr_add,					// #448 float(float bufhandle, string str, float order) bufstr_add (DP_QC_STRINGBUFFERS)
	PF_bufstr_free,					// #449 void(float bufhandle, float string_index) bufstr_free (DP_QC_STRINGBUFFERS)
//450
	PF_Fixme,//0
	PF_CL_is_cached_pic,//1
	PF_CL_precache_pic,//2
	PF_CL_free_pic,//3
	PF_CL_drawcharacter,//4
	PF_CL_drawrawstring,//5
	PF_CL_drawpic,//6
	PF_CL_drawfill,//7
	PF_CL_drawsetcliparea,//8
	PF_CL_drawresetcliparea,//9

//460
	PF_CL_drawgetimagesize,//460
	PF_cin_open,						// #461
	PF_cin_close,						// #462
	PF_cin_setstate,					// #463
	PF_cin_getstate,					// #464
	PF_cin_restart, 					// #465
	PF_drawline,						// #466
	PF_drawcolorcodedstring,		// #467
	PF_CL_stringwidth,					// #468
	PF_CL_drawsubpic,						// #469
	
//470
	skip1					// #470
	PF_asin,				// #471
	PF_acos,					// #472
	PF_atan,						// #473
	PF_atan2,									// #474
	PF_tan,									// #475
	PF_strlennocol,									// #476
	PF_strdecolorize,									// #477
	PF_strftime,									// #478
	PF_tokenizebyseparator,									// #479

//480
	PF_strtolower,						// #480 string(string s) VM_strtolower : DRESK - Return string as lowercase
	PF_strtoupper,						// #481 string(string s) VM_strtoupper : DRESK - Return string as uppercase
	skip1									// #482
	skip1									// #483
	PF_strreplace,						// #484 string(string search, string replace, string subject) strreplace (DP_QC_STRREPLACE)
	PF_strireplace,					// #485 string(string search, string replace, string subject) strireplace (DP_QC_STRREPLACE)
	skip1									// #486
	PF_gecko_create,					// #487 float gecko_create( string name )
	PF_gecko_destroy,					// #488 void gecko_destroy( string name )
	PF_gecko_navigate,				// #489 void gecko_navigate( string name, string URI )

//490
	PF_gecko_keyevent,				// #490 float gecko_keyevent( string name, float key, float eventtype )
	PF_gecko_movemouse,				// #491 void gecko_mousemove( string name, float x, float y )
	PF_gecko_resize,					// #492 void gecko_resize( string name, float w, float h )
	PF_gecko_get_texture_extent,	// #493 vector gecko_get_texture_extent( string name )
	PF_crc16,						// #494 float(float caseinsensitive, string s, ...) crc16 = #494 (DP_QC_CRC16)
	PF_cvar_type,					// #495 float(string name) cvar_type = #495; (DP_QC_CVAR_TYPE)
	skip1									// #496
	skip1									// #497
	skip1									// #498
	skip1									// #499

//500
	skip1									// #500
	skip1									// #501
	skip1									// #502
	PF_whichpack,					// #503 string(string) whichpack = #503;
	skip1									// #504
	skip1									// #505
	skip1									// #506
	skip1									// #507
	skip1									// #508
	skip1									// #509

//510
	PF_uri_escape,					// #510 string(string in) uri_escape = #510;
	PF_uri_unescape,				// #511 string(string in) uri_unescape = #511;
	PF_etof,					// #512 float(entity ent) num_for_edict = #512 (DP_QC_NUM_FOR_EDICT)
	PF_uri_get,						// #513 float(string uril, float id) uri_get = #513; (DP_QC_URI_GET)
	skip1									// #514
	skip1									// #515
	skip1									// #516
	skip1									// #517
	skip1									// #518
	skip1									// #519

//520
	skip10
	skip10
	skip10
	skip50
//600
	skip1
	PF_cl_setkeydest,
	PF_cl_getkeydest,
	PF_cl_setmousetarget,
	PF_cl_getmousetarget,
	PF_callfunction,
	skip1				//void	writetofile(float fhandle, entity ent) = #606;
	PF_isfunction,
	PF_cl_getresolution,
	PF_cl_keynumtostring,
	PF_cl_findkeysforcommand,
	PF_M_gethostcachevalue,
	PF_M_gethostcachestring,
	PF_parseentitydata,			//void 	parseentitydata(entity ent, string data) = #613;

	PF_cl_stringtokeynum,

	PF_M_resethostcachemasks,
	PF_M_sethostcachemaskstring,
	PF_M_sethostcachemasknumber,
	PF_M_resorthostcache,
	PF_M_sethostcachesort,
	PF_M_refreshhostcache,
	PF_M_gethostcachenumber,
	PF_M_gethostcacheindexforkey,
	PF_M_addwantedhostcachekey,
	PF_M_getextresponse			// #624
};
int menu_numbuiltins = sizeof(menu_builtins)/sizeof(menu_builtins[0]);




void M_Init_Internal (void);
void M_DeInit_Internal (void);

int inmenuprogs;
progfuncs_t *menuprogs;
progparms_t menuprogparms;
menuedict_t *menu_edicts;
int num_menu_edicts;

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
	extern int mouseusedforgui;
	func_t temp;
	if (!menuprogs)
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
		PR_ExecuteProgram(menuprogs, temp);

	PR_fclose_progs(menuprogs);
	search_close_progs(menuprogs, true);

	CloseProgs(menuprogs);
	menuprogs = NULL;

	key_dest = key_game;
	m_state = 0;

	mouseusedforgui = false;

	M_Init_Internal();

	if (inmenuprogs)	//something in the menu caused the problem, so...
	{
		inmenuprogs = 0;
		longjmp(mp_abort, 1);
	}
}

pbool QC_WriteFile(char *name, void *data, int len);
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
		menuprogs->save_ents(menuprogs, buffer, &size, 3);
		COM_WriteFile("menucore.txt", buffer, size);
		Z_Free(buffer);
	}

	MP_Shutdown();
}

double  menutime;
void MP_Init (void)
{
	if (!qrenderer)
	{
		return;
	}

	if (forceqmenu.value)
		return;

	M_DeInit_Internal();

	memset(&menuc_eval_chain, 0, sizeof(menuc_eval_chain));


	menuprogparms.progsversion = PROGSTRUCT_VERSION;
	menuprogparms.ReadFile = COM_LoadStackFile;//char *(*ReadFile) (char *fname, void *buffer, int *len);
	menuprogparms.FileSize = COM_FileSize;//int (*FileSize) (char *fname);	//-1 if file does not exist
	menuprogparms.WriteFile = QC_WriteFile;//bool (*WriteFile) (char *name, void *data, int len);
	menuprogparms.printf = (void *)Con_Printf;//Con_Printf;//void (*printf) (char *, ...);
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
	menuprogparms.numglobalbuiltins = menu_numbuiltins;

	menuprogparms.autocompile = PR_COMPILEIGNORE;//PR_COMPILEEXISTANDCHANGED;//enum {PR_NOCOMPILE, PR_COMPILENEXIST, PR_COMPILECHANGED, PR_COMPILEALWAYS} autocompile;

	menuprogparms.gametime = &menutime;

	menuprogparms.sv_edicts = (struct edict_s **)&menu_edicts;
	menuprogparms.sv_num_edicts = &num_menu_edicts;

	menuprogparms.useeditor = NULL;//sorry... QCEditor;//void (*useeditor) (char *filename, int line, int nump, char **parms);

	menutime = Sys_DoubleTime();
	if (!menuprogs)
	{
		Con_DPrintf("Initializing menu.dat\n");
		menuprogs = InitProgs(&menuprogparms);
		PR_Configure(menuprogs, -1, 1);
		if (PR_LoadProgs(menuprogs, "menu.dat", 10020, NULL, 0) < 0) //no per-progs builtins.
		{
			//failed to load or something
//			CloseProgs(menuprogs);
//			menuprogs = NULL;
			M_Init_Internal();
			return;
		}
		if (setjmp(mp_abort))
		{
			M_Init_Internal();
			Con_DPrintf("Failed to initialize menu.dat\n");
			inmenuprogs = false;
			return;
		}
		inmenuprogs++;

		PF_InitTempStrings(menuprogs);

		mp_time = (float*)PR_FindGlobal(menuprogs, "time", 0);
		if (mp_time)
			*mp_time = Sys_DoubleTime();

		menuentsize = PR_InitEnts(menuprogs, 8192);


		//'world' edict
//		EDICT_NUM(menuprogs, 0)->readonly = true;
		EDICT_NUM(menuprogs, 0)->isfree = false;


		mp_init_function = PR_FindFunction(menuprogs, "m_init", PR_ANY);
		mp_shutdown_function = PR_FindFunction(menuprogs, "m_shutdown", PR_ANY);
		mp_draw_function = PR_FindFunction(menuprogs, "m_draw", PR_ANY);
		mp_keydown_function = PR_FindFunction(menuprogs, "m_keydown", PR_ANY);
		mp_keyup_function = PR_FindFunction(menuprogs, "m_keyup", PR_ANY);
		mp_toggle_function = PR_FindFunction(menuprogs, "m_toggle", PR_ANY);
		if (mp_init_function)
			PR_ExecuteProgram(menuprogs, mp_init_function);
		inmenuprogs--;

		Con_DPrintf("Initialized menu.dat\n");
	}
}

void MP_CoreDump_f(void)
{
	if (!menuprogs)
	{
		Con_Printf("Can't core dump, you need to be running the CSQC progs first.");
		return;
	}

	{
		int size = 1024*1024*8;
		char *buffer = BZ_Malloc(size);
		menuprogs->save_ents(menuprogs, buffer, &size, 3);
		COM_WriteFile("menucore.txt", buffer, size);
		BZ_Free(buffer);
	}
}

void MP_Reload_f(void)
{
	MP_Shutdown();
	MP_Init();
}

void MP_RegisterCvarsAndCmds(void)
{
	PF_Common_RegisterCvars();

	Cmd_AddCommand("coredump_menuqc", MP_CoreDump_f);
	Cmd_AddCommand("menu_restart", MP_Reload_f);

	Cvar_Register(&forceqmenu, MENUPROGSGROUP);
	Cvar_Register(&pr_menuqc_coreonerror, MENUPROGSGROUP);

	if (COM_CheckParm("-qmenu"))
		Cvar_Set(&forceqmenu, "1");
}

void MP_Draw(void)
{
	if (!menuprogs)
		return;
	if (setjmp(mp_abort))
		return;

#ifdef RGLQUAKE
	if (qrenderer == QR_OPENGL)
	{
		GL_TexEnv(GL_MODULATE);
		qglDisable(GL_ALPHA_TEST);
		qglEnable(GL_BLEND);
	}
#endif

	menutime = Sys_DoubleTime();
	if (mp_time)
		*mp_time = menutime;

	inmenuprogs++;
	if (mp_draw_function)
		PR_ExecuteProgram(menuprogs, mp_draw_function);
	inmenuprogs--;
}

int MP_TranslateFTEtoDPCodes(int code)
{
	switch(code)
	{
	case K_TAB:				return 9;
	case K_ENTER:			return 13;
	case K_ESCAPE:			return 27;
	case K_SPACE:			return 32;
	case K_BACKSPACE:		return 127;
	case K_UPARROW:			return 128;
	case K_DOWNARROW:		return 129;
	case K_LEFTARROW:		return 130;
	case K_RIGHTARROW:		return 131;
	case K_ALT:				return 132;
	case K_CTRL:			return 133;
	case K_SHIFT:			return 134;
	case K_F1:				return 135;
	case K_F2:				return 136;
	case K_F3:				return 137;
	case K_F4:				return 138;
	case K_F5:				return 139;
	case K_F6:				return 140;
	case K_F7:				return 141;
	case K_F8:				return 142;
	case K_F9:				return 143;
	case K_F10:				return 144;
	case K_F11:				return 145;
	case K_F12:				return 146;
	case K_INS:				return 147;
	case K_DEL:				return 148;
	case K_PGDN:			return 149;
	case K_PGUP:			return 150;
	case K_HOME:			return 151;
	case K_END:				return 152;
	case K_KP_HOME:			return 160;
	case K_KP_UPARROW:		return 161;
	case K_KP_PGUP:			return 162;
	case K_KP_LEFTARROW:	return 163;
	case K_KP_5:			return 164;
	case K_KP_RIGHTARROW:	return 165;
	case K_KP_END:			return 166;
	case K_KP_DOWNARROW:	return 167;
	case K_KP_PGDN:			return 168;
	case K_KP_ENTER:		return 169;
	case K_KP_INS:			return 170;
	case K_KP_DEL:			return 171;
	case K_KP_SLASH:		return 172;
	case K_KP_MINUS:		return 173;
	case K_KP_PLUS:			return 174;
	case K_PAUSE:			return 255;
	case K_JOY1:			return 768;
	case K_JOY2:			return 769;
	case K_JOY3:			return 770;
	case K_JOY4:			return 771;
	case K_AUX1:			return 772;
	case K_AUX2:			return 773;
	case K_AUX3:			return 774;
	case K_AUX4:			return 775;
	case K_AUX5:			return 776;
	case K_AUX6:			return 777;
	case K_AUX7:			return 778;
	case K_AUX8:			return 779;
	case K_AUX9:			return 780;
	case K_AUX10:			return 781;
	case K_AUX11:			return 782;
	case K_AUX12:			return 783;
	case K_AUX13:			return 784;
	case K_AUX14:			return 785;
	case K_AUX15:			return 786;
	case K_AUX16:			return 787;
	case K_AUX17:			return 788;
	case K_AUX18:			return 789;
	case K_AUX19:			return 790;
	case K_AUX20:			return 791;
	case K_AUX21:			return 792;
	case K_AUX22:			return 793;
	case K_AUX23:			return 794;
	case K_AUX24:			return 795;
	case K_AUX25:			return 796;
	case K_AUX26:			return 797;
	case K_AUX27:			return 798;
	case K_AUX28:			return 799;
	case K_AUX29:			return 800;
	case K_AUX30:			return 801;
	case K_AUX31:			return 802;
	case K_AUX32:			return 803;
	case K_MOUSE1:			return 512;
	case K_MOUSE2:			return 513;
	case K_MOUSE3:			return 514;
	case K_MOUSE4:			return 515;
	case K_MOUSE5:			return 516;
//	case K_MOUSE6:			return 517;
//	case K_MOUSE7:			return 518;
//	case K_MOUSE8:			return 519;
//	case K_MOUSE9:			return 520;
//	case K_MOUSE10:			return 521;
	case K_MWHEELDOWN:		return 515;//K_MOUSE4;
	case K_MWHEELUP:		return 516;//K_MOUSE5;
	default:				return code;
	}
}

int MP_TranslateDPtoFTECodes(int code)
{
	switch(code)
	{
	case 9:			return K_TAB;
	case 13:		return K_ENTER;
	case 27:		return K_ESCAPE;
	case 32:		return K_SPACE;
	case 127:		return K_BACKSPACE;
	case 128:		return K_UPARROW;
	case 129:		return K_DOWNARROW;
	case 130:		return K_LEFTARROW;
	case 131:		return K_RIGHTARROW;
	case 132:		return K_ALT;
	case 133:		return K_CTRL;
	case 134:		return K_SHIFT;
	case 135:		return K_F1;
	case 136:		return K_F2;
	case 137:		return K_F3;
	case 138:		return K_F4;
	case 139:		return K_F5;
	case 140:		return K_F6;
	case 141:		return K_F7;
	case 142:		return K_F8;
	case 143:		return K_F9;
	case 144:		return K_F10;
	case 145:		return K_F11;
	case 146:		return K_F12;
	case 147:		return K_INS;
	case 148:		return K_DEL;
	case 149:		return K_PGDN;
	case 150:		return K_PGUP;
	case 151:		return K_HOME;
	case 152:		return K_END;
	case 160:		return K_KP_HOME;
	case 161:		return K_KP_UPARROW;
	case 162:		return K_KP_PGUP;
	case 163:		return K_KP_LEFTARROW;
	case 164:		return K_KP_5;
	case 165:		return K_KP_RIGHTARROW;
	case 166:		return K_KP_END;
	case 167:		return K_KP_DOWNARROW;
	case 168:		return K_KP_PGDN;
	case 169:		return K_KP_ENTER;
	case 170:		return K_KP_INS;
	case 171:		return K_KP_DEL;
	case 172:		return K_KP_SLASH;
	case 173:		return K_KP_MINUS;
	case 174:		return K_KP_PLUS;
	case 255:		return K_PAUSE;

	case 768:		return K_JOY1;
	case 769:		return K_JOY2;
	case 770:		return K_JOY3;
	case 771:		return K_JOY4;
	case 772:		return K_AUX1;
	case 773:		return K_AUX2;
	case 774:		return K_AUX3;
	case 775:		return K_AUX4;
	case 776:		return K_AUX5;
	case 777:		return K_AUX6;
	case 778:		return K_AUX7;
	case 779:		return K_AUX8;
	case 780:		return K_AUX9;
	case 781:		return K_AUX10;
	case 782:		return K_AUX11;
	case 783:		return K_AUX12;
	case 784:		return K_AUX13;
	case 785:		return K_AUX14;
	case 786:		return K_AUX15;
	case 787:		return K_AUX16;
	case 788:		return K_AUX17;
	case 789:		return K_AUX18;
	case 790:		return K_AUX19;
	case 791:		return K_AUX20;
	case 792:		return K_AUX21;
	case 793:		return K_AUX22;
	case 794:		return K_AUX23;
	case 795:		return K_AUX24;
	case 796:		return K_AUX25;
	case 797:		return K_AUX26;
	case 798:		return K_AUX27;
	case 799:		return K_AUX28;
	case 800:		return K_AUX29;
	case 801:		return K_AUX30;
	case 802:		return K_AUX31;
	case 803:		return K_AUX32;
	case 512:		return K_MOUSE1;
	case 513:		return K_MOUSE2;
	case 514:		return K_MOUSE3;
//	case 515:		return K_MOUSE4;
//	case 516:		return K_MOUSE5;
	case 517:		return K_MOUSE6;
	case 518:		return K_MOUSE7;
	case 519:		return K_MOUSE8;
//	case 520:		return K_MOUSE9;
//	case 521:		return K_MOUSE10;
	case 515:		return K_MWHEELDOWN;//K_MOUSE4;
	case 516:		return K_MWHEELUP;//K_MOUSE5;
	default:		return code;
	}
}

void MP_Keydown(int key, int unicode)
{
	extern qboolean	keydown[K_MAX];
	if (setjmp(mp_abort))
		return;

	if (key == 'c')
	{
		if (keydown[K_CTRL])
		{
			MP_Shutdown();
			return;
		}
	}
	if (key == K_ESCAPE)
	{
		if (keydown[K_SHIFT])
		{
			Con_ToggleConsole_f();
			return;
		}
	}

	menutime = Sys_DoubleTime();
	if (mp_time)
		*mp_time = menutime;

	inmenuprogs++;
	if (mp_keydown_function)
	{
		void *pr_globals = PR_globals(menuprogs, PR_CURRENT);
		G_FLOAT(OFS_PARM0) = MP_TranslateFTEtoDPCodes(key);
		G_FLOAT(OFS_PARM1) = unicode;
		PR_ExecuteProgram(menuprogs, mp_keydown_function);
	}
	inmenuprogs--;
}

void MP_Keyup(int key, int unicode)
{
	if (setjmp(mp_abort))
		return;

	menutime = Sys_DoubleTime();
	if (mp_time)
		*mp_time = menutime;

	inmenuprogs++;
	if (mp_keyup_function)
	{
		void *pr_globals = PR_globals(menuprogs, PR_CURRENT);
		G_FLOAT(OFS_PARM0) = MP_TranslateFTEtoDPCodes(key);
		G_FLOAT(OFS_PARM1) = unicode;
		PR_ExecuteProgram(menuprogs, mp_keyup_function);
	}
	inmenuprogs--;
}

qboolean MP_Toggle(void)
{
	if (!menuprogs)
		return false;

	if (setjmp(mp_abort))
		return false;

	menutime = Sys_DoubleTime();
	if (mp_time)
		*mp_time = menutime;

	inmenuprogs++;
	if (mp_toggle_function)
		PR_ExecuteProgram(menuprogs, mp_toggle_function);
	inmenuprogs--;

	return true;
}
#endif
