#include "quakedef.h"

#include "glquake.h"

#ifdef HLCLIENT

#define CLIENTDLLNAME "cl_dlls/client"

#define notimp(l) Con_Printf("halflife cl builtin not implemented on line %i\n", l)

//#define HLCL_API_VERSION 6
#define HLCL_API_VERSION 7

#define HLPIC model_t*

typedef struct
{
	int	l;
	int r;
	int t;
	int b;
} hlsubrect_t;

typedef struct
{
	vec3_t origin;

#if HLCL_API_VERSION >= 7
	vec3_t viewangles;
	int weapons;
	float fov;
#else
	float viewheight;
	float maxspeed;
	vec3_t viewangles;
	vec3_t punchangles;
	int keys;
	int weapons;
	float fov;
	float idlescale;
	float mousesens;
#endif
} hllocalclientdata_t;

typedef struct
{
	short lerpmsecs;
	qbyte msec;
	vec3_t viewangles;

	float forwardmove;
	float sidemove;
	float upmove;

	qbyte lightlevel;
	unsigned short buttons;
	qbyte impulse;
	qbyte weaponselect;

	int impact_index;
	vec3_t impact_position;
} hlusercmd_t;

typedef struct
{
	char name[64];
	char sprite[64];
	int unk;
	int forscrwidth;
	hlsubrect_t rect;
} hlspriteinf_t;

typedef struct 
{
	char *name;
	short ping;
	qbyte isus;
	qbyte isspec;
	qbyte pl;
	char *model;
	short tcolour;
	short bcolour;
} hlplayerinfo_t;

typedef struct
{
	int size;
	int width;
	int height;
	int flags;
	int charheight;
	short charwidths[256];
} hlscreeninfo_t;

typedef struct
{
	int effect;
	byte_vec4_t c1;
	byte_vec4_t c2;
	float x;
	float y;
	float fadein;
	float fadeout;
	float holdtime;
	float fxtime;
	char *name;
	char *message;
} hlmsginfo_t;

typedef struct
{
	HLPIC (*pic_load) (char *picname);
	int (*pic_getnumframes) (HLPIC pic);
	int (*pic_getheight) (HLPIC pic, int frame);
	int (*pic_getwidth) (HLPIC pic, int frame);
	void (*pic_select) (HLPIC pic, int r, int g, int b);
	void (*pic_drawcuropaque) (int frame, int x, int y, void *loc);
	void (*pic_drawcuralphatest) (int frame, int x, int y, void *loc);
	void (*pic_drawcuradditive) (int frame, int x, int y, void *loc);
	void (*pic_enablescissor) (int x, int y, int width, int height);
	void (*pic_disablescissor) (void);
	hlspriteinf_t *(*pic_parsepiclist) (char *filename, int *numparsed);

	void (*fillrgba) (int x, int y, int width, int height, int r, int g, int b, int a);
	int (*getscreeninfo) (hlscreeninfo_t *info);
	void (*setcrosshair) (HLPIC pic, hlsubrect_t rect, int r, int g, int b);	//I worry about stuff like this
	
	int (*cvar_register) (char *name, char *defvalue, int flags);
	float (*cvar_getfloat) (char *name);
	char *(*cvar_getstring) (char *name);

	void (*cmd_register) (char *name, void (*func) (void));
	void (*hooknetmsg) (char *msgname, void *func);
	void (*forwardcmd) (char *command);
	void (*localcmd) (char *command);

	void (*getplayerinfo) (int entnum, hlplayerinfo_t *result);

	void (*startsound_name) (char *name, float vol);
	void (*startsound_idx) (int idx, float vol);

	void (*anglevectors) (float *ina, float *outf, float *outr, float *outu);

	hlmsginfo_t *(*get_message_info) (char *name);	//translated+scaled+etc intro stuff
	int (*drawchar) (int x, int y, int charnum, int r, int g, int b);
	int (*drawstring) (int x, int y, char *string);
#if HLCL_API_VERSION >= 7
	void (*settextcolour) (float r, float b, float g);
#endif
	void (*drawstring_getlen) (char *string, int *outlen, int *outheight);
	void (*consoleprint) (char *str);
	void (*centerprint) (char *str);

#if HLCL_API_VERSION >= 7
	int (*getwindowcenterx)(void);	//yes, really, window center. for use with Get/SetCursorPos, the windows function.
	int (*getwindowcentery)(void);	//yes, really, window center. for use with Get/SetCursorPos, the windows function.
	void (*getviewnangles)(float*ang);
	void (*setviewnangles)(float*ang);
	void (*getmaxclients)(float*ang);
	void (*cvar_setvalue)(char *cvarname, char *value);

	int (*cmd_argc)(void);
	char *(*cmd_argv)(int i);
	void (*con_printf)(char *fmt, ...);
	void (*con_dprintf)(char *fmt, ...);
	void (*con_notificationprintf)(int pos, char *fmt, ...);
	void (*con_notificationprintfex)(void *info, char *fmt, ...);	//arg1 is of specific type
	char *(*physkey)(char *key);
	char *(*serverkey)(char *key);
	float (*getclientmaxspeed)(void);
	int (*checkparm)(char *str, char **next);
	int (*keyevent)(int key, int down);
	void (*getmousepos)(int *outx, int *outy);
	int (*movetypeisnoclip)(void);
	struct hlclent_s *(*getlocalplayer)(void);
	struct hlclent_s *(*getviewent)(void);
	struct hlclent_s *(*getentidx)(void);
	float (*getlocaltime)(void);
	void (*calcshake)(void);
	void (*applyshake)(void);
	int (*pointcontents)(float *point, float *truecon);
	int (*waterentity)(float *point);
	void (*traceline) (float *start, float *end, int flags, int hull, int forprediction);

	model_t *(*loadmodel)(char *modelname, int *mdlindex);
	int (*addrentity)(int type, void *ent);

	model_t *(*modelfrompic) (HLPIC pic);
	void (*soundatloc)(char*sound, float volume, float *org);

	unsigned short (*precacheevent)(int evtype, char *name);
	void (*playevent)(int flags, struct hledict_s *ent, unsigned short evindex, float delay, float *origin, float *angles, float f1, float f2, int i1, int i2, int b1, int b2);
	void (*weaponanimate)(int anim, int body);
	float (*randfloat) (float minv, float maxv);
	long (*randlong) (long minv, long maxv);
	void (*hookevent) (char *name, void (*func)(struct hlevent_s *event));
	int (*con_isshown) (void);
	char *(*getgamedir) (void);
	struct hlcvar_s *(*cvar_find) (char *name);
	char *(*lookupbinding) (char *command);
	char *(*getlevelname) (void);
	void (*getscreenfade) (struct hlsfade_s *fade);
	void (*setscreenfade) (struct hlsfade_s *fade);
	void *(*vgui_getpanel) (void);
	void (*vgui_paintback) (int extents[4]);

	void *(*loadfile) (char *path, int onhunk, int *length);
	char *(*parsefile) (char *data, char *token);
	void (*freefile) (void *file);

	struct hl_tri_api_s
	{
		int vers;
		int sentinal;
	} *triapi;
	struct hl_sfx_api_s
	{
		int vers;
		int sentinal;
	} *efxapi;
	struct hl_event_api_s 
	{
		int vers;
		int sentinal;
	} *eventapi;
	struct hl_demo_api_s 
	{
		int (*isrecording)(void);
		int (*isplaying)(void);
		int (*istimedemo)(void);
		void (*writedata)(int size, void *data);

		int sentinal;
	} *demoapi;
	struct hl_net_api_s 
	{
		int vers;
		int sentinal;
	} *netapi;

	struct hl_voicetweek_s
	{
		int sentinal;
	} *voiceapi;

	int (*forcedspectator) (void);
	model_t *(*loadmapsprite) (char *name);

	void (*fs_addgamedir) (char *basedir, char *appname);
	int (*expandfilename) (char *filename, char *outbuff, int outsize);

	char *(*player_key) (int pnum, char *key);
	void (*player_setkey) (char *key, char *value);	//wait, no pnum?

	qboolean (*getcdkey) (int playernum, char key[16]);
	int trackerfromplayer;
	int playerfromtracker;
	int (*sendcmd_unreliable) (char *cmd);
	void (*getsysmousepos) (long *xandy);
	void (*setsysmousepos) (int x, int y);
	void (*setmouseenable) (qboolean enable);
#endif

	int sentinal;
} CLHL_enginecgamefuncs_t;


typedef struct
{
	int (*HUD_VidInit) (void);
	int (*HUD_Init) (void);
	int (*HUD_Shutdown) (void);
	int (*HUD_Redraw) (float maptime, int inintermission);
	int (*HUD_UpdateClientData) (hllocalclientdata_t *localclientdata, float maptime);
	int (*HUD_Reset) (void);
#if HLCL_API_VERSION >= 7
	void (*CL_CreateMove) (float frametime, hlusercmd_t *cmd, int isplaying);
	void (*IN_ActivateMouse) (void);
	void (*IN_DeactivateMouse) (void);
	void (*IN_MouseEvent) (int buttonmask);
#endif
} CLHL_cgamefuncs_t;



//FIXME
typedef struct
{
	vec3_t	origin;
	vec3_t	oldorigin;

	int firstframe;
	int numframes;

	int		type;
	vec3_t	angles;
	int		flags;
	float	alpha;
	float	start;
	float	framerate;
	model_t	*model;
	int skinnum;
} explosion_t;





typedef struct
{
	char name[64];
	int (*hook) (char *name, int bufsize, void *bufdata);
} CLHL_UserMessages_t;
CLHL_UserMessages_t usermsgs[256];

int numnewhooks;
CLHL_UserMessages_t pendingusermsgs[256];


static HLPIC selectedpic;

float hl_viewmodelsequencetime;
int hl_viewmodelsequencecur;
int hl_viewmodelsequencebody;





HLPIC CLGHL_pic_load (char *picname)
{
	return Mod_ForName(picname, false);
//	return Draw_SafeCachePic(picname);
}
int CLGHL_pic_getnumframes (HLPIC pic)
{
	if (pic)
		return pic->numframes;
	else
		return 0;
}

static mspriteframe_t *getspriteframe(HLPIC pic, int frame)
{
	msprite_t		*psprite;
	mspritegroup_t *pgroup;
	if (!pic)
		return NULL;
	psprite = pic->cache.data;
	if (!psprite)
		return NULL;

	if (psprite->frames[frame].type == SPR_SINGLE)
		return psprite->frames[frame].frameptr;
	else
	{
		pgroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		return pgroup->frames[0];
	}
}
static mpic_t *getspritepic(HLPIC pic, int frame)
{
	mspriteframe_t *f;
	f = getspriteframe(pic, frame);
	if (f)
		return &f->p;
	return NULL;
}

int CLGHL_pic_getheight (HLPIC pic, int frame)
{
	mspriteframe_t *pframe;

	pframe = getspriteframe(pic, frame);
	if (!pframe)
		return 0;

	return pframe->p.width;
}
int CLGHL_pic_getwidth (HLPIC pic, int frame)
{
	mspriteframe_t *pframe;

	pframe = getspriteframe(pic, frame);
	if (!pframe)
		return 0;

	return pframe->p.height;
}
void CLGHL_pic_select (HLPIC pic, int r, int g, int b)
{
	selectedpic = pic;
	Draw_ImageColours(r/255.0f, g/255.0f, b/255.0f, 1);
}
void CLGHL_pic_drawcuropaque (int frame, int x, int y, hlsubrect_t *loc)
{
	mpic_t *pic = getspritepic(selectedpic, frame);
	if (!pic)
		return;

	//faster SW render: no blends/holes
	pic->flags &= ~1;

	Draw_Image(x, y,
		loc->r-loc->l, loc->b-loc->t,
		(float)loc->l/pic->width, (float)loc->t/pic->height,
		(float)loc->r/pic->width, (float)loc->b/pic->height,
		pic);
}
void CLGHL_pic_drawcuralphtest (int frame, int x, int y, hlsubrect_t *loc)
{
	mpic_t *pic = getspritepic(selectedpic, frame);
	if (!pic)
		return;
	//use some kind of alpha
	pic->flags |= 1;

	Draw_Image(x, y,
		loc->r-loc->l, loc->b-loc->t,
		(float)loc->l/pic->width, (float)loc->t/pic->height,
		(float)loc->r/pic->width, (float)loc->b/pic->height,
		pic);
}
void CLGHL_pic_drawcuradditive (int frame, int x, int y, hlsubrect_t *loc)
{
	mpic_t *pic = getspritepic(selectedpic, frame);
	if (!pic)
		return;

	qglEnable (GL_BLEND);
	qglBlendFunc(GL_ONE, GL_ONE);

	//use some kind of alpha
	pic->flags |= 1;
	if (loc)
	{
		Draw_Image(x, y,
			loc->r-loc->l, loc->b-loc->t,
			(float)loc->l/pic->width, (float)loc->t/pic->height,
			(float)loc->r/pic->width, (float)loc->b/pic->height,
			pic);
	}
	else
	{
		Draw_Image(x, y,
			pic->width, pic->height,
			0, 0,
			1, 1,
			pic);
	}
	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}
void CLGHL_pic_enablescissor (int x, int y, int width, int height)
{
}
void CLGHL_pic_disablescissor (void)
{
}
hlspriteinf_t *CLGHL_pic_parsepiclist (char *filename, int *numparsed)
{
	hlspriteinf_t *result;
	int entry;
	int entries;
	void *file;
	char *pos;

	*numparsed = 0;

	FS_LoadFile(filename, &file);
	if (!file)
		return NULL;
	pos = file;

	pos = COM_Parse(pos);
	entries = atoi(com_token);

	//name, res, pic, x, y, w, h

	result = Z_Malloc(sizeof(*result)*entries);
	for (entry = 0; entry < entries; entry++)
	{
		pos = COM_Parse(pos);
		Q_strncpyz(result[entry].name, com_token, sizeof(result[entry].name));

		pos = COM_Parse(pos);
		result[entry].forscrwidth = atoi(com_token);

		pos = COM_Parse(pos);
		Q_strncpyz(result[entry].sprite, com_token, sizeof(result[entry].name));

		pos = COM_Parse(pos);
		result[entry].rect.l = atoi(com_token);

		pos = COM_Parse(pos);
		result[entry].rect.t = atoi(com_token);

		pos = COM_Parse(pos);
		result[entry].rect.r = result[entry].rect.l+atoi(com_token);

		pos = COM_Parse(pos);
		result[entry].rect.b = result[entry].rect.t+atoi(com_token);

		if (pos)
			*numparsed = entry;
	}

	if (!pos || COM_Parse(pos))
		Con_Printf("unexpected end of file\n");

	FS_FreeFile(file);

	return result;
}

void CLGHL_fillrgba (int x, int y, int width, int height, int r, int g, int b, int a)
{
}
int CLGHL_getscreeninfo (hlscreeninfo_t *info)
{
	int i;
	if (info->size != sizeof(*info))
		return false;

	info->width = vid.width;
	info->height = vid.height;
	info->flags = 0;
	info->charheight = 8;
	for (i = 0; i < 256; i++)
		info->charwidths[i] = 8;

	return true;
}
void CLGHL_setcrosshair (HLPIC pic, hlsubrect_t rect, int r, int g, int b)
{
}

int CLGHL_cvar_register (char *name, char *defvalue, int flags)
{
	if (Cvar_Get(name, defvalue, 0, "Halflife cvars"))
		return GHL_CVarGetPointer(name);
	else
		return false;
}
float CLGHL_cvar_getfloat (char *name)
{
	cvar_t *var = Cvar_FindVar(name);
	if (var)
		return var->value;
	return 0;
}
char *CLGHL_cvar_getstring (char *name)
{
	cvar_t *var = Cvar_FindVar(name);
	if (var)
		return var->string;
	return "";
}

void CLGHL_cmd_register (char *name, xcommand_t func)
{
	Cmd_AddRemCommand(name, func);
}
void CLGHL_hooknetmsg (char *msgname, void *func)
{
	int i;
	//update the current list now.
	for (i = 0; i < sizeof(usermsgs)/sizeof(usermsgs[0]); i++)
	{
		if (!strcmp(usermsgs[i].name, msgname))
		{
			usermsgs[i].hook = func;
			break;	//one per name
		}
	}

	//we already asked for it perhaps?
	for (i = 0; i < numnewhooks; i++)
	{
		if (!strcmp(pendingusermsgs[i].name, msgname))
		{
			pendingusermsgs[i].hook = func;
			return;	//nothing to do
		}
	}

	Q_strncpyz(pendingusermsgs[numnewhooks].name, msgname, sizeof(pendingusermsgs[i].name));
	pendingusermsgs[numnewhooks].hook = func;
	numnewhooks++;
}
void CLGHL_forwardcmd (char *command)
{
	CL_SendClientCommand(true, "%s", command);
}
void CLGHL_localcmd (char *command)
{
	Cbuf_AddText(command, RESTRICT_SERVER);
}

void CLGHL_getplayerinfo (int entnum, hlplayerinfo_t *result)
{
	notimp(__LINE__);
}

void CLGHL_startsound_name (char *name, float vol)
{
	sfx_t *sfx = S_PrecacheSound (name);
	if (!sfx)
	{
		Con_Printf ("CLGHL_startsound_name: can't cache %s\n", name);
		return;
	}
	S_StartSound (-1, -1, sfx, vec3_origin, vol, 1);
}
void CLGHL_startsound_idx (int idx, float vol)
{
	sfx_t *sfx = cl.sound_precache[idx];
	if (!sfx)
	{
		Con_Printf ("CLGHL_startsound_name: index not precached %s\n", name);
		return;
	}
	S_StartSound (-1, -1, sfx, vec3_origin, vol, 1);
}

void CLGHL_anglevectors (float *ina, float *outf, float *outr, float *outu)
{
	AngleVectors(ina, outf, outr, outu);
}

hlmsginfo_t *CLGHL_get_message_info (char *name)
{
	//fixme: add parser
	return NULL;
}
int CLGHL_drawchar (int x, int y, int charnum, int r, int g, int b)
{
	return 0;
}
int CLGHL_drawstring (int x, int y, char *string)
{
	return 0;
}
void CLGHL_settextcolour(float r, float g, float b)
{
}
void CLGHL_drawstring_getlen (char *string, int *outlen, int *outheight)
{
	*outlen = strlen(string)*8;
	*outheight = 8;
}
void CLGHL_consoleprint (char *str)
{
	Con_Printf("%s", str);
}
void CLGHL_centerprint (char *str)
{
	SCR_CenterPrint(0, str, true);
}


int CLGHL_getwindowcenterx(void)
{
	return window_center_x;
}
int CLGHL_getwindowcentery(void)
{
	return window_center_y;
}
void CLGHL_getviewnangles(float*ang)
{
	VectorCopy(cl.viewangles[0], ang);
}
void CLGHL_setviewnangles(float*ang)
{
	VectorCopy(ang, cl.viewangles[0]);
}
void CLGHL_getmaxclients(float*ang){notimp(__LINE__);}
void CLGHL_cvar_setvalue(char *cvarname, char *value){notimp(__LINE__);}

int CLGHL_cmd_argc(void)
{
	return Cmd_Argc();
}
char *CLGHL_cmd_argv(int i)
{
	return Cmd_Argv(i);
}
#define CLGHL_con_printf Con_Printf//void CLGHL_con_printf(char *fmt, ...){notimp(__LINE__);}
#define CLGHL_con_dprintf Con_DPrintf//void CLGHL_con_dprintf(char *fmt, ...){notimp(__LINE__);}
void CLGHL_con_notificationprintf(int pos, char *fmt, ...){notimp(__LINE__);}
void CLGHL_con_notificationprintfex(void *info, char *fmt, ...){notimp(__LINE__);}
char *CLGHL_physkey(char *key){notimp(__LINE__);return NULL;}
char *CLGHL_serverkey(char *key){notimp(__LINE__);return NULL;}
float CLGHL_getclientmaxspeed(void)
{
	return 320;
}
int CLGHL_checkparm(char *str, const char **next)
{
	int i;
	i = COM_CheckParm(str);
	if (next)
	{
		if (i && i+1<com_argc)
			*next = com_argv[i+1];
		else
			*next = NULL;
	}
	return i;
}
int CLGHL_keyevent(int key, int down)
{
	if (key >= 241 && key <= 241+5)
		Key_Event(K_MOUSE1+key-241, down);
	else
		Con_Printf("CLGHL_keyevent: Unrecognised HL key code\n");
}
void CLGHL_getmousepos(int *outx, int *outy){notimp(__LINE__);}
int CLGHL_movetypeisnoclip(void){notimp(__LINE__);return 0;}
struct hlclent_s *CLGHL_getlocalplayer(void){notimp(__LINE__);return NULL;}
struct hlclent_s *CLGHL_getviewent(void){notimp(__LINE__);return NULL;}
struct hlclent_s *CLGHL_getentidx(void){notimp(__LINE__);return NULL;}
float CLGHL_getlocaltime(void){return cl.time;}
void CLGHL_calcshake(void){notimp(__LINE__);}
void CLGHL_applyshake(float *origin, float *angles, float factor){notimp(__LINE__);}
int CLGHL_pointcontents(float *point, float *truecon){notimp(__LINE__);return 0;}
int CLGHL_entcontents(float *point){notimp(__LINE__);return 0;}
void CLGHL_traceline(float *start, float *end, int flags, int hull, int forprediction){notimp(__LINE__);}

model_t *CLGHL_loadmodel(char *modelname, int *mdlindex){notimp(__LINE__);return Mod_ForName(modelname, false);}
int CLGHL_addrentity(int type, void *ent){notimp(__LINE__);return 0;}

model_t *CLGHL_modelfrompic(HLPIC pic){notimp(__LINE__);return NULL;}
void CLGHL_soundatloc(char*sound, float volume, float *org){notimp(__LINE__);}

unsigned short CLGHL_precacheevent(int evtype, char *name){notimp(__LINE__);return 0;}
void CLGHL_playevent(int flags, struct hledict_s *ent, unsigned short evindex, float delay, float *origin, float *angles, float f1, float f2, int i1, int i2, int b1, int b2){notimp(__LINE__);}
void CLGHL_weaponanimate(int newsequence, int body)
{
	hl_viewmodelsequencetime = cl.time;
	hl_viewmodelsequencecur = newsequence;
	hl_viewmodelsequencebody = body;
}
float CLGHL_randfloat(float minv, float maxv){notimp(__LINE__);return minv;}
long CLGHL_randlong(long minv, long maxv){notimp(__LINE__);return minv;}
void CLGHL_hookevent(char *name, void (*func)(struct hlevent_s *event)){notimp(__LINE__);}
int CLGHL_con_isshown(void)
{
	return scr_con_current > 0;
}
char *CLGHL_getgamedir(void)
{
	extern char	gamedirfile[];
	return gamedirfile;
}
struct hlcvar_s *CLGHL_cvar_find(char *name)
{
	return GHL_CVarGetPointer(name);
}
char *CLGHL_lookupbinding(char *command)
{
	return NULL;
}
char *CLGHL_getlevelname(void)
{
	return cl.levelname;
}
void CLGHL_getscreenfade(struct hlsfade_s *fade){notimp(__LINE__);}
void CLGHL_setscreenfade(struct hlsfade_s *fade){notimp(__LINE__);}
void *CLGHL_vgui_getpanel(void){notimp(__LINE__);return NULL;}
void CLGHL_vgui_paintback(int extents[4]){notimp(__LINE__);}

void *CLGHL_loadfile(char *path, int alloctype, int *length)
{
	void *ptr = NULL;
	int flen = -1;
	if (alloctype == 5)
	{
		flen = FS_LoadFile(path, &ptr);
	}
	else
		notimp(__LINE__);	//don't leak, just fail

	if (length)
		*length = flen;

	return ptr;
}
char *CLGHL_parsefile(char *data, char *token)
{
	return COM_ParseOut(data, token, 1024);
}
void CLGHL_freefile(void *file)
{
	//only valid for alloc type 5
	FS_FreeFile(file);
}


int CLGHL_forcedspectator(void)
{
	return cls.demoplayback;
}
model_t *CLGHL_loadmapsprite(char *name)
{
	notimp(__LINE__);return NULL;
}

void CLGHL_fs_addgamedir(char *basedir, char *appname){notimp(__LINE__);return NULL;}
int CLGHL_expandfilename(char *filename, char *outbuff, int outsize){notimp(__LINE__);return NULL;}

char *CLGHL_player_key(int pnum, char *key){notimp(__LINE__);return NULL;}
void CLGHL_player_setkey(char *key, char *value){notimp(__LINE__);return NULL;}

qboolean CLGHL_getcdkey(int playernum, char key[16]){notimp(__LINE__);return false;}
int CLGHL_trackerfromplayer(int pslot){notimp(__LINE__);return 0;}
int CLGHL_playerfromtracker(int tracker){notimp(__LINE__);return 0;}
int CLGHL_sendcmd_unreliable(char *cmd){notimp(__LINE__);return 0;}
void CLGHL_getsysmousepos(long *xandy)
{
#ifdef _WIN32
	GetCursorPos((LPPOINT)xandy);
#endif
}
void CLGHL_setsysmousepos(int x, int y)
{
#ifdef _WIN32
	SetCursorPos(x, y);
#endif
}
void CLGHL_setmouseenable(qboolean enable)
{
	extern cvar_t _windowed_mouse;
	Cvar_Set(&_windowed_mouse, enable?"1":"0");
}




int CLGHL_demo_isrecording(void)
{
	return cls.demorecording;
}
int CLGHL_demo_isplaying(void)
{
	return cls.demoplayback;
}
int CLGHL_demo_istimedemo(void)
{
	return cls.timedemo;
}
void CLGHL_demo_writedata(int size, void *data)
{
	notimp(__LINE__);
}

struct hl_demo_api_s hl_demo_api = 
{
		CLGHL_demo_isrecording,
		CLGHL_demo_isplaying,
		CLGHL_demo_istimedemo,
		CLGHL_demo_writedata,

		0xdeadbeef
};

CLHL_cgamefuncs_t CLHL_cgamefuncs;
CLHL_enginecgamefuncs_t CLHL_enginecgamefuncs =
{
	CLGHL_pic_load,
	CLGHL_pic_getnumframes,
	CLGHL_pic_getheight,
	CLGHL_pic_getwidth,
	CLGHL_pic_select,
	CLGHL_pic_drawcuropaque,
	CLGHL_pic_drawcuralphtest,
	CLGHL_pic_drawcuradditive,
	CLGHL_pic_enablescissor,
	CLGHL_pic_disablescissor,
	CLGHL_pic_parsepiclist,

	CLGHL_fillrgba,
	CLGHL_getscreeninfo,
	CLGHL_setcrosshair,

	CLGHL_cvar_register,
	CLGHL_cvar_getfloat,
	CLGHL_cvar_getstring,

	CLGHL_cmd_register,
	CLGHL_hooknetmsg,
	CLGHL_forwardcmd,
	CLGHL_localcmd,

	CLGHL_getplayerinfo,

	CLGHL_startsound_name,
	CLGHL_startsound_idx,

	CLGHL_anglevectors,

	CLGHL_get_message_info,
	CLGHL_drawchar,
	CLGHL_drawstring,
#if HLCL_API_VERSION >= 7
	CLGHL_settextcolour,
#endif
	CLGHL_drawstring_getlen,
	CLGHL_consoleprint,
	CLGHL_centerprint,

#if HLCL_API_VERSION >= 7
	CLGHL_getwindowcenterx,
	CLGHL_getwindowcentery,
	CLGHL_getviewnangles,
	CLGHL_setviewnangles,
	CLGHL_getmaxclients,
	CLGHL_cvar_setvalue,

	CLGHL_cmd_argc,
	CLGHL_cmd_argv,
	CLGHL_con_printf,
	CLGHL_con_dprintf,
	CLGHL_con_notificationprintf,
	CLGHL_con_notificationprintfex,
	CLGHL_physkey,
	CLGHL_serverkey,
	CLGHL_getclientmaxspeed,
	CLGHL_checkparm,
	CLGHL_keyevent,
	CLGHL_getmousepos,
	CLGHL_movetypeisnoclip,
	CLGHL_getlocalplayer,
	CLGHL_getviewent,
	CLGHL_getentidx,
	CLGHL_getlocaltime,
	CLGHL_calcshake,
	CLGHL_applyshake,
	CLGHL_pointcontents,
	CLGHL_entcontents,
	CLGHL_traceline,

	CLGHL_loadmodel,
	CLGHL_addrentity,

	CLGHL_modelfrompic,
	CLGHL_soundatloc,

	CLGHL_precacheevent,
	CLGHL_playevent,
	CLGHL_weaponanimate,
	CLGHL_randfloat,
	CLGHL_randlong,
	CLGHL_hookevent,
	CLGHL_con_isshown,
	CLGHL_getgamedir,
	CLGHL_cvar_find,
	CLGHL_lookupbinding,
	CLGHL_getlevelname,
	CLGHL_getscreenfade,
	CLGHL_setscreenfade,
	CLGHL_vgui_getpanel,
	CLGHL_vgui_paintback,

	CLGHL_loadfile,
	CLGHL_parsefile,
	CLGHL_freefile,

	NULL, //triapi;
	NULL, //efxapi;
	NULL, //eventapi;
	&hl_demo_api,
	NULL, //netapi;

//sdk 2.3+
	NULL, //voiceapi;

	CLGHL_forcedspectator,
	CLGHL_loadmapsprite,

	CLGHL_fs_addgamedir,
	CLGHL_expandfilename,

	CLGHL_player_key,
	CLGHL_player_setkey,

	CLGHL_getcdkey,
	(void*)0xdeaddead,//CLGHL_trackerfromplayer;
	(void*)0xdeaddead,//CLGHL_playerfromtracker;
	CLGHL_sendcmd_unreliable,
	CLGHL_getsysmousepos,
	CLGHL_setsysmousepos,
	CLGHL_setmouseenable,
#endif

	0xdeadbeef
};

dllhandle_t clg;

int CLHL_GamecodeDoesMouse(void)
{
	if (!clg || !CLHL_cgamefuncs.CL_CreateMove)
		return false;
	return true;
}

int CLHL_MouseEvent(unsigned int buttonmask)
{
	if (!CLHL_GamecodeDoesMouse())
		return false;

	CLHL_cgamefuncs.IN_MouseEvent(buttonmask);
	return true;
}

void CLHL_SetMouseActive(int activate)
{
	static int oldactive;
	if (!clg)
	{
		oldactive = false;
		return;
	}
	if (activate == oldactive)
		return;
	oldactive = activate;

	if (activate)
	{
		if (CLHL_cgamefuncs.IN_ActivateMouse)
			CLHL_cgamefuncs.IN_ActivateMouse();
	}
	else
	{
		if (CLHL_cgamefuncs.IN_DeactivateMouse)
			CLHL_cgamefuncs.IN_DeactivateMouse();
	}
}

void CLHL_UnloadClientGame(void)
{
	if (!clg)
		return;

	CLHL_SetMouseActive(false);
	if (CLHL_cgamefuncs.HUD_Shutdown)
		CLHL_cgamefuncs.HUD_Shutdown();
	Sys_CloseLibrary(clg);
	memset(&CLHL_cgamefuncs, 0, sizeof(CLHL_cgamefuncs));
	clg = NULL;

	hl_viewmodelsequencetime = 0;
}

void CLHL_LoadClientGame(void)
{
	char fullname[MAX_OSPATH];
	char *path;

	int (*initfunc)(CLHL_enginecgamefuncs_t *funcs, int version);
	dllfunction_t funcs[] =
	{
		{(void*)&initfunc, "Initialize"},
		{(void*)&CLHL_cgamefuncs.HUD_VidInit, "HUD_VidInit"},
		{(void*)&CLHL_cgamefuncs.HUD_Init, "HUD_Init"},
		{(void*)&CLHL_cgamefuncs.HUD_Shutdown, "HUD_Shutdown"},
		{(void*)&CLHL_cgamefuncs.HUD_Redraw, "HUD_Redraw"},
		{(void*)&CLHL_cgamefuncs.HUD_UpdateClientData, "HUD_UpdateClientData"},
		{(void*)&CLHL_cgamefuncs.HUD_Reset, "HUD_Reset"},
#if HLCL_API_VERSION >= 7
		{(void*)&CLHL_cgamefuncs.CL_CreateMove, "CL_CreateMove"},
		{(void*)&CLHL_cgamefuncs.IN_ActivateMouse, "IN_ActivateMouse"},
		{(void*)&CLHL_cgamefuncs.IN_DeactivateMouse, "IN_DeactivateMouse"},
		{(void*)&CLHL_cgamefuncs.IN_MouseEvent, "IN_MouseEvent"},
#endif
		{NULL}
	};

	CLHL_UnloadClientGame();

	memset(&CLHL_cgamefuncs, 0, sizeof(CLHL_cgamefuncs));

	path = NULL;
	while((path = COM_NextPath (path)))
	{
		if (!path)
			return NULL;		// couldn't find one anywhere
		snprintf (fullname, sizeof(fullname), "%s/%s", path, "cl_dlls/client");
		clg = Sys_LoadLibrary(fullname, funcs);
		if (clg)
			break;
	}

	if (!clg)
		return;

	if (!initfunc(&CLHL_enginecgamefuncs, HLCL_API_VERSION))
	{
		Sys_CloseLibrary(clg);
		clg = NULL;
		return;
	}

	CLHL_cgamefuncs.HUD_Init();
	CLHL_cgamefuncs.HUD_VidInit();
}

int CLHL_BuildUserInput(int msecs, usercmd_t *cmd)
{
	hlusercmd_t hlcmd;
	if (!clg || !CLHL_cgamefuncs.CL_CreateMove)
		return false;

	CLHL_cgamefuncs.CL_CreateMove(msecs/255.0f, &hlcmd, cls.state>=ca_active && !cls.demoplayback);

#define ANGLE2SHORT(x) (x) * (65536/360.0)
	cmd->msec = msecs;
	cmd->angles[0] = ANGLE2SHORT(hlcmd.viewangles[0]);
	cmd->angles[1] = ANGLE2SHORT(hlcmd.viewangles[1]);
	cmd->angles[2] = ANGLE2SHORT(hlcmd.viewangles[2]);
	cmd->forwardmove = hlcmd.forwardmove;
	cmd->sidemove = hlcmd.sidemove;
	cmd->upmove = hlcmd.upmove;
	cmd->weapon = hlcmd.weaponselect;
	cmd->impulse = hlcmd.impulse;
	cmd->buttons = hlcmd.buttons;
	cmd->lightlevel = hlcmd.lightlevel;
	return true;
}

int CLHL_DrawHud(void)
{
	extern kbutton_t in_attack;
	hllocalclientdata_t state;

	if (!clg || !CLHL_cgamefuncs.HUD_Redraw)
		return false;

//	state.origin;
//	state.viewangles;
#if HLCL_API_VERSION < 7
//	state.viewheight;
//	state.maxspeed;
//	state.punchangles;
	state.idlescale = 0;
	state.mousesens = 0;
	state.keys = (in_attack.state[0]&3)?1:0;
#endif
	state.weapons = cl.stats[0][STAT_ITEMS];
	state.fov = 90;

	V_StopPitchDrift(0);

	CLHL_cgamefuncs.HUD_UpdateClientData(&state, cl.time);

	return CLHL_cgamefuncs.HUD_Redraw(cl.time, cl.intermission);	
}

int CLHL_AnimateViewEntity(entity_t *ent)
{
	float time;
	if (!hl_viewmodelsequencetime)
		return false;

	time = cl.time - hl_viewmodelsequencetime;
	ent->framestate.g[FS_REG].frame[0] = hl_viewmodelsequencecur;
	ent->framestate.g[FS_REG].frame[1] = hl_viewmodelsequencecur;
	ent->framestate.g[FS_REG].frametime[0] = time;
	ent->framestate.g[FS_REG].frametime[1] = time;
	return true;
}

int CLHL_ParseGamePacket(void)
{
	int subcode;
	char *end;
	char *str;
	int tempi;

	end = net_message.data+msg_readcount+2+MSG_ReadShort();

	if (end > net_message.data+net_message.cursize)
		return false;

	subcode = MSG_ReadByte();

	if (usermsgs[subcode].hook)
		if (usermsgs[subcode].hook(usermsgs[subcode].name, end - (net_message.data+msg_readcount), net_message.data+msg_readcount))
		{
			msg_readcount = end - net_message.data;
			return true;
		}

	switch(subcode)
	{
	case 1:
		//register the server-sent code.
		tempi = MSG_ReadByte();
		str = MSG_ReadString();
		Q_strncpyz(usermsgs[tempi].name, str, sizeof(usermsgs[tempi].name));

		//get the builtin to reregister its hooks.
		for (tempi = 0; tempi < numnewhooks; tempi++)
			CLGHL_hooknetmsg(pendingusermsgs[tempi].name, pendingusermsgs[tempi].hook);
		break;
	case svc_temp_entity:
		{
			vec3_t startp, endp, vel;
			float ang;
			int midx;
			int mscale;
			int mrate;
			int flags;
			int lifetime;
			explosion_t *ef;
			extern cvar_t temp1;

			subcode = MSG_ReadByte();

			if (temp1.value)
				Con_Printf("Temp ent %i\n", subcode);
			switch(subcode)
			{
			case 3:
				MSG_ReadPos(startp);
				midx = MSG_ReadShort();
				mscale = MSG_ReadByte();
				mrate = MSG_ReadByte();
				flags = MSG_ReadByte();
				if (!(flags & 8))
					P_RunParticleEffectType(startp, NULL, 1, pt_explosion);
				if (!(flags & 4))
					S_StartSound(0, 0, S_PrecacheSound("explosion"), startp, 1, 1);
				if (!(flags & 2))
					CL_NewDlightRGB(0, startp[0], startp[1], startp[2], 200, 1, 0.2,0.2,0.2);

				ef = CL_AllocExplosion();
				VectorCopy(startp, ef->origin);
				ef->start = cl.time;
				ef->model = cl.model_precache[midx];
				ef->framerate = mrate;
				ef->firstframe = 0;
				ef->numframes = ef->model->numframes;
				if (!(flags & 1))
					ef->flags = Q2RF_ADDATIVE;
				else
					ef->flags = 0;
				break;
			case 4:
				MSG_ReadPos(startp);
				P_RunParticleEffectType(startp, NULL, 1, pt_tarexplosion);
				break;
			case 5:
				MSG_ReadPos(startp);
				MSG_ReadShort();
				MSG_ReadByte();
				MSG_ReadByte();
				break;
			case 6:
				MSG_ReadPos(startp);
				MSG_ReadPos(endp);
				break;
			case 9:
				MSG_ReadPos(startp);
				P_RunParticleEffectType(startp, NULL, 1, ptdp_spark);
				break;
			case 22:
				MSG_ReadShort();
				MSG_ReadShort();
				MSG_ReadByte();
				MSG_ReadByte();
				MSG_ReadByte();
				MSG_ReadByte();
				MSG_ReadByte();
				break;
			case 23:
				MSG_ReadPos(startp);
				MSG_ReadShort();
				MSG_ReadByte();
				MSG_ReadByte();
				MSG_ReadByte();
				break;
			case 106:
				MSG_ReadPos(startp);
				MSG_ReadPos(vel);
				ang = MSG_ReadAngle();
				midx = MSG_ReadShort();
				MSG_ReadByte();
				lifetime = MSG_ReadByte();

				ef = CL_AllocExplosion();
				VectorCopy(startp, ef->origin);
				ef->start = cl.time;
				ef->angles[1] = ang;
				ef->model = cl.model_precache[midx];
				ef->firstframe = 0;
				ef->numframes = lifetime;
				ef->flags = 0;
				ef->framerate = 10;
				break;
			case 108:
				MSG_ReadPos(startp);
				MSG_ReadPos(endp);
				MSG_ReadPos(vel);
				MSG_ReadByte();
				MSG_ReadShort();
				MSG_ReadByte();
				MSG_ReadByte();
				MSG_ReadByte();
				break;
			case 109:
				MSG_ReadPos(startp);
				MSG_ReadShort();
				MSG_ReadByte();
				break;
			case 116:
				MSG_ReadPos(startp);
				MSG_ReadByte();
				break;
			case 117:
				MSG_ReadPos(startp);
				MSG_ReadByte()+256;
				break;
			case 118:
				MSG_ReadPos(startp);
				MSG_ReadByte()+256;
				break;
			default:
				Con_Printf("CLHL_ParseGamePacket: Unable to parse gamecode tempent %i\n", subcode);
				msg_readcount = end - net_message.data;
				break;
			}
		}
		if (msg_readcount != end - net_message.data)
		{
			Con_Printf("CLHL_ParseGamePacket: Gamecode temp entity %i not parsed correctly read %i bytes too many\n", subcode, msg_readcount - (end - net_message.data));
			msg_readcount = end - net_message.data;
		}
		break;
	case svc_intermission:
		//nothing.
		cl.intermission = true;
		break;
	case svc_cdtrack:
		cl.cdtrack = MSG_ReadByte();
		MSG_ReadByte();
		CDAudio_Play ((qbyte)cl.cdtrack, true);
		break;

	case 35: //svc_weaponanimation:
		tempi = MSG_ReadByte();
		CLGHL_weaponanimate(tempi, MSG_ReadByte());
		break;
	case 37: //svc_roomtype
		tempi = MSG_ReadShort();
		SNDDMA_SetUnderWater(tempi==14||tempi==15||tempi==16);
		break;
	default:
		Con_Printf("Unrecognised gamecode packet %i (%s)\n", subcode, usermsgs[subcode].name);
		msg_readcount = end - net_message.data;
		break;
	}

	if (msg_readcount != end - net_message.data)
	{
		Con_Printf("CLHL_ParseGamePacket: Gamecode packet %i not parsed correctly read %i bytestoo many\n", subcode, msg_readcount - (end - net_message.data));
		msg_readcount = end - net_message.data;
	}
	return true;
}

#endif
