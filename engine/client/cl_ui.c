#include "quakedef.h"
#ifdef VM_UI
#include "clq3defs.h"
#include "ui_public.h"
#include "cl_master.h"
#include "shader.h"

int keycatcher;

#define TT_STRING					1			// string
#define TT_LITERAL					2			// literal
#define TT_NUMBER					3			// number
#define TT_NAME						4			// name
#define TT_PUNCTUATION				5			// punctuation

#define SCRIPT_MAXDEPTH 64
#define SCRIPT_DEFINELENGTH 256
typedef struct {
	char *filestack[SCRIPT_MAXDEPTH];
	char *originalfilestack[SCRIPT_MAXDEPTH];
	char *lastreadptr;
	int lastreaddepth;
	char filename[MAX_QPATH][SCRIPT_MAXDEPTH];
	int stackdepth;

	char *defines;
	int numdefines;
} script_t;
static script_t *scripts;
static int maxscripts;
#define Q3SCRIPTPUNCTUATION "(,{})(\':;=!><&|+-\""
void StripCSyntax (char *s)
{
	while(*s)
	{
		if (*s == '\\')
		{
			memmove(s, s+1, strlen(s+1)+1);
			switch (*s)
			{
			case 'r':
				*s = '\r';
				break;
			case 'n':
				*s = '\n';
				break;
			case '\\':
				*s = '\\';
				break;
			default:
				*s = '?';
				break;
			}
		}
		s++;
	}
}
int Script_Read(int handle, struct pc_token_s *token)
{
	char *s;
	char readstring[8192];
	int i;
	script_t *sc = scripts+handle-1;

	for(;;)
	{
		if (!sc->stackdepth)
		{
			memset(token, 0, sizeof(*token));
			return 0;
		}

		s = sc->filestack[sc->stackdepth-1];
		sc->lastreadptr = s;
		sc->lastreaddepth = sc->stackdepth;

		s = (char *)COM_ParseToken(s, Q3SCRIPTPUNCTUATION);
		Q_strncpyz(readstring, com_token, sizeof(readstring));
		if (com_tokentype == TTP_STRING)
		{
			while(s)
			{
				while (*s > '\0' && *s <= ' ')
					s++;
				if (*s == '/' && s[1] == '/')
				{
					while(*s && *s != '\n')
						s++;
					continue;
				}
				while (*s > '\0' && *s <= ' ')
					s++;
				if (*s == '\"')
				{
					s = (char*)COM_ParseToken(s, Q3SCRIPTPUNCTUATION);
					Q_strncatz(readstring, com_token, sizeof(readstring));
				}
				else
					break;
			}
		}
		sc->filestack[sc->stackdepth-1] = s;
		

		if (!strcmp(readstring, "#include"))
		{
			sc->filestack[sc->stackdepth-1] = (char *)COM_ParseToken(sc->filestack[sc->stackdepth-1], Q3SCRIPTPUNCTUATION);

			if (sc->stackdepth == SCRIPT_MAXDEPTH)	//just don't enter it
				continue;

			if (sc->originalfilestack[sc->stackdepth])
				BZ_Free(sc->originalfilestack[sc->stackdepth]);
			sc->filestack[sc->stackdepth] = sc->originalfilestack[sc->stackdepth] = FS_LoadMallocFile(com_token, NULL);
			Q_strncpyz(sc->filename[sc->stackdepth], com_token, MAX_QPATH);
			sc->stackdepth++;
			continue;
		}
		if (!strcmp(readstring, "#define"))
		{
			sc->numdefines++;
			sc->defines = BZ_Realloc(sc->defines, sc->numdefines*SCRIPT_DEFINELENGTH*2);
			sc->filestack[sc->stackdepth-1] = (char *)COM_ParseToken(sc->filestack[sc->stackdepth-1], Q3SCRIPTPUNCTUATION);
			Q_strncpyz(sc->defines+SCRIPT_DEFINELENGTH*2*(sc->numdefines-1), com_token, SCRIPT_DEFINELENGTH);
			sc->filestack[sc->stackdepth-1] = (char *)COM_ParseToken(sc->filestack[sc->stackdepth-1], Q3SCRIPTPUNCTUATION);
			Q_strncpyz(sc->defines+SCRIPT_DEFINELENGTH*2*(sc->numdefines-1)+SCRIPT_DEFINELENGTH, com_token, SCRIPT_DEFINELENGTH);

			continue;
		}
		if (!*readstring && com_tokentype != TTP_STRING)
		{
			if (sc->stackdepth==0)
			{
				memset(token, 0, sizeof(*token));
				return 0;
			}

			sc->stackdepth--;
			continue;
		}
		break;
	}
	if (com_tokentype == TTP_STRING)
	{
		i = sc->numdefines;
	}
	else
	{
		for (i = 0; i < sc->numdefines; i++)
		{
			if (!strcmp(readstring, sc->defines+SCRIPT_DEFINELENGTH*2*i))
			{
				Q_strncpyz(token->string, sc->defines+SCRIPT_DEFINELENGTH*2*i+SCRIPT_DEFINELENGTH, sizeof(token->string));
				break;
			}
		}
	}
	if (i == sc->numdefines)	//otherwise
		Q_strncpyz(token->string, readstring, sizeof(token->string));

	StripCSyntax(token->string);

	token->intvalue = atoi(token->string);
	token->floatvalue = atof(token->string);
	if (token->floatvalue || *token->string == '0' || *token->string == '.')
	{
		token->type = TT_NUMBER;
		token->subtype = 0;
	}
	else if (com_tokentype == TTP_STRING)
	{
		token->type = TT_STRING;
		token->subtype = strlen(token->string);
	}
	else
	{
		if (token->string[1] == '\0')
		{
			token->type = TT_PUNCTUATION;
			token->subtype = token->string[0];
		}
		else
		{
			token->type = TT_NAME;
			token->subtype = strlen(token->string);
		}
	}

//	Con_Printf("Found %s (%i, %i)\n", token->string, token->type, token->subtype);
	return !!*token->string || com_tokentype == TTP_STRING;
}

int Script_LoadFile(char *filename)
{
	int i;
	script_t *sc;
	for (i = 0; i < maxscripts; i++)
		if (!scripts[i].stackdepth)
			break;
	if (i == maxscripts)
	{
		maxscripts++;
		scripts = BZ_Realloc(scripts, sizeof(script_t)*maxscripts); 
	}
	
	sc = scripts+i;
	memset(sc, 0, sizeof(*sc));
	sc->filestack[0] = sc->originalfilestack[0] = FS_LoadMallocFile(filename, NULL);
	Q_strncpyz(sc->filename[sc->stackdepth], filename, MAX_QPATH);
	sc->stackdepth = 1;

	return i+1;
}

void Script_Free(int handle)
{
	int i;
	script_t *sc = scripts+handle-1;
	if (sc->defines)
		BZ_Free(sc->defines);

	for (i = 0; i < sc->stackdepth; i++)
		BZ_Free(sc->originalfilestack[i]);

	sc->stackdepth = 0;
}

void Script_Get_File_And_Line(int handle, char *filename, int *line)
{
	script_t *sc = scripts+handle-1;
	char *src;
	char *start;

	if (!sc->lastreaddepth)
	{
		*line = 0;
		Q_strncpyz(filename, sc->filename[0], MAX_QPATH);
		return;
	}
	*line = 1;

	src = sc->lastreadptr;
	start = sc->originalfilestack[sc->lastreaddepth-1];

	while(start < src)
	{
		if (*start == '\n')
			(*line)++;
		start++;
	}

	Q_strncpyz(filename, sc->filename[sc->lastreaddepth-1], MAX_QPATH);

}










#ifdef GLQUAKE
#include "glquake.h"//hack
#else
typedef float m3by3_t[3][3];
#endif

static vm_t *uivm;

static char *scr_centerstring;

char *Get_Q2ConfigString(int i);


#define MAX_PINGREQUESTS 32

netadr_t ui_pings[MAX_PINGREQUESTS];

#define UITAGNUM 2452

extern model_t *mod_known;
#define VM_FROMMHANDLE(a) (a?mod_known+a-1:NULL)
#define VM_TOMHANDLE(a) (a?a-mod_known+1:0)

#define VM_FROMSHANDLE(a) (a?r_shaders[a-1]:NULL)
#define VM_TOSHANDLE(a) (a?a->id+1:0)


struct q3refEntity_s {
	refEntityType_t	reType;
	int			renderfx;

	int hModel;				// opaque type outside refresh

	// most recent data
	vec3_t		lightingOrigin;		// so multi-part models can be lit identically (RF_LIGHTING_ORIGIN)
	float		shadowPlane;		// projection shadows go here, stencils go slightly lower

	vec3_t		axis[3];			// rotation vectors
	qboolean	nonNormalizedAxes;	// axis are not normalized, i.e. they have scale
	float		origin[3];			// also used as MODEL_BEAM's "from"
	int			frame;				// also used as MODEL_BEAM's diameter

	// previous data for frame interpolation
	float		oldorigin[3];		// also used as MODEL_BEAM's "to"
	int			oldframe;
	float		backlerp;			// 0.0 = current, 1.0 = old

	// texturing
	int			skinNum;			// inline skin index
	int		customSkin;			// NULL for default skin
	int		customShader;		// use one image for the entire thing

	// misc
	qbyte		shaderRGBA[4];		// colors used by rgbgen entity shaders
	float		shaderTexCoord[2];	// texture coordinates used by tcMod entity modifiers
	float		shaderTime;			// subtracted from refdef time to control effect start times

	// extra sprite information
	float		radius;
	float		rotation;
};

struct q3polyvert_s
{
	vec3_t org;
	vec2_t tcoord;
	qbyte colours[4];
};

#define Q3RF_MINLIGHT			1
#define	Q3RF_THIRD_PERSON		2		// don't draw through eyes, only mirrors (player bodies, chat sprites)
#define	Q3RF_FIRST_PERSON		4		// only draw through eyes (view weapon, damage blood blob)
#define	Q3RF_DEPTHHACK			8		// for view weapon Z crunching
#define Q3RF_NOSHADOW			64
#define Q3RF_LIGHTING_ORIGIN	128

#define MAX_VMQ3_CACHED_STRINGS 1024
char *stringcache[1024];

void VMQ3_FlushStringHandles(void)
{
	int i;
	for (i = 0; i < MAX_VMQ3_CACHED_STRINGS; i++)
	{
		if (stringcache[i])
		{
			Z_Free(stringcache[i]);
			stringcache[i] = NULL;
		}
	}
}

char *VMQ3_StringFromHandle(int handle)
{
	if (!handle)
		return "";
	handle--;
	if ((unsigned) handle >= MAX_VMQ3_CACHED_STRINGS)
		return "";
	return stringcache[handle];
}

int VMQ3_StringToHandle(char *str)
{
	int i;
	for (i = 0; i < MAX_VMQ3_CACHED_STRINGS; i++)
	{
		if (!stringcache[i])
			break;
		if (!strcmp(str, stringcache[i]))
			return i+1;
	}
	if (i == MAX_VMQ3_CACHED_STRINGS)
	{
		Con_Printf("Q3VM out of string handle space\n");
		return 0;
	}
	stringcache[i] = Z_Malloc(strlen(str)+1);
	strcpy(stringcache[i], str);
	return i+1;
}

#define VM_TOSTRCACHE(a) VMQ3_StringToHandle(VM_POINTER(a))
#define VM_FROMSTRCACHE(a) VMQ3_StringFromHandle(a)

void VQ3_AddEntity(const q3refEntity_t *q3)
{
	entity_t ent;
	memset(&ent, 0, sizeof(ent));
	ent.model = VM_FROMMHANDLE(q3->hModel);
	ent.framestate.g[FS_REG].frame[0] = q3->frame;
	ent.framestate.g[FS_REG].frame[1] = q3->oldframe;
	memcpy(ent.axis, q3->axis, sizeof(q3->axis));
	ent.framestate.g[FS_REG].lerpweight[1] = q3->backlerp;
	ent.framestate.g[FS_REG].lerpweight[0] = 1 - ent.framestate.g[FS_REG].lerpweight[1];
	if (q3->reType == RT_SPRITE)
	{
		ent.scale = q3->radius;
		ent.rotation = q3->rotation;
	}
	else
		ent.scale = 1;
	ent.rtype = q3->reType;

	ent.customskin = q3->customSkin;
	ent.skinnum = q3->skinNum;

	ent.shaderRGBAf[0] = q3->shaderRGBA[0]/255.0f;
	ent.shaderRGBAf[1] = q3->shaderRGBA[1]/255.0f;
	ent.shaderRGBAf[2] = q3->shaderRGBA[2]/255.0f;
	ent.shaderRGBAf[3] = q3->shaderRGBA[3]/255.0f;

	/*don't set translucent, the shader is meant to already be correct*/
//	if (ent.shaderRGBAf[3] <= 0)
//		return;

	ent.forcedshader = VM_FROMSHANDLE(q3->customShader);
	ent.shaderTime = q3->shaderTime;
	if (q3->renderfx & Q3RF_FIRST_PERSON)
		ent.flags |= RF_WEAPONMODEL;
	if (q3->renderfx & Q3RF_DEPTHHACK)
		ent.flags |= RF_DEPTHHACK;
	if (q3->renderfx & Q3RF_THIRD_PERSON)
		ent.flags |= RF_EXTERNALMODEL;
	if (q3->renderfx & Q3RF_NOSHADOW)
		ent.flags |= RF_NOSHADOW;

	ent.topcolour = TOP_DEFAULT;
	ent.bottomcolour = BOTTOM_DEFAULT;
	ent.playerindex = -1;

	VectorCopy(q3->origin, ent.origin);
	VectorCopy(q3->oldorigin, ent.oldorigin);
	V_AddAxisEntity(&ent);
}

void VQ3_AddPoly(shader_t *s, int num, q3polyvert_t *verts)
{
	unsigned int v;
	scenetris_t *t;
	/*reuse the previous trigroup if its the same shader*/
	if (cl_numstris && cl_stris[cl_numstris-1].shader == s && cl_stris[cl_numstris-1].flags == (BEF_NODLIGHT|BEF_NOSHADOWS))
		t = &cl_stris[cl_numstris-1];
	else
	{
		if (cl_numstris == cl_maxstris)
		{
			cl_maxstris += 8;
			cl_stris = BZ_Realloc(cl_stris, sizeof(*cl_stris)*cl_maxstris);
		}
		t = &cl_stris[cl_numstris++];
		t->shader = s;
		t->flags = BEF_NODLIGHT|BEF_NOSHADOWS;
		t->numidx = 0;
		t->numvert = 0;
		t->firstidx = cl_numstrisidx;
		t->firstvert = cl_numstrisvert;
	}

	if (cl_maxstrisvert < cl_numstrisvert+num)
	{
		cl_maxstrisvert = cl_numstrisvert+num + 64;
		cl_strisvertv = BZ_Realloc(cl_strisvertv, sizeof(*cl_strisvertv)*cl_maxstrisvert);
		cl_strisvertt = BZ_Realloc(cl_strisvertt, sizeof(vec2_t)*cl_maxstrisvert);
		cl_strisvertc = BZ_Realloc(cl_strisvertc, sizeof(vec4_t)*cl_maxstrisvert);
	}
	if (cl_maxstrisidx < cl_numstrisidx+(num-2)*3)
	{
		cl_maxstrisidx = cl_numstrisidx+(num-2)*3 + 64;
		cl_strisidx = BZ_Realloc(cl_strisidx, sizeof(*cl_strisidx)*cl_maxstrisidx);
	}

	for (v = 0; v < num; v++)
	{
		VectorCopy(verts[v].org, cl_strisvertv[cl_numstrisvert+v]);
		Vector2Copy(verts[v].tcoord, cl_strisvertt[cl_numstrisvert+v]);
		Vector4Scale(verts[v].colours, (1/255.0f), cl_strisvertc[cl_numstrisvert+v]);
	}
	for (v = 2; v < num; v++)
	{
		cl_strisidx[cl_numstrisidx++] = cl_numstrisvert - t->firstvert;
		cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+(v-1) - t->firstvert;
		cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+v - t->firstvert;
	}

	t->numvert += num;
	t->numidx += (num-2)*3;
	cl_numstrisvert += num;
	//we already increased idx
}

int VM_LerpTag(void *out, model_t *model, int f1, int f2, float l2, char *tagname)
{
	int tagnum;
	float *ang;
	float *org;

	float tr[12];
	qboolean found;
	framestate_t fstate;

	org = (float*)out;
	ang = ((float*)out+3);

	memset(&fstate, 0, sizeof(fstate));
	fstate.g[FS_REG].frame[0] = f1;
	fstate.g[FS_REG].frame[1] = f2;
	fstate.g[FS_REG].lerpweight[0] = 1 - l2;
	fstate.g[FS_REG].lerpweight[1] = l2;

	tagnum = Mod_TagNumForName(model, tagname);
	found = Mod_GetTag(model, tagnum, &fstate, tr);

	if (found && tagnum)
	{
		ang[0] = tr[0];
		ang[1] = tr[1];
		ang[2] = tr[2];
		org[0] = tr[3];

		ang[3] = tr[4];
		ang[4] = tr[5];
		ang[5] = tr[6];
		org[1] = tr[7];

		ang[6] = tr[8];
		ang[7] = tr[9];
		ang[8] = tr[10];
		org[2] = tr[11];

		return true;
	}
	else
	{
		org[0] = 0;
		org[1] = 0;
		org[2] = 0;

		ang[0] = 1;
		ang[1] = 0;
		ang[2] = 0;

		ang[3] = 0;
		ang[4] = 1;
		ang[5] = 0;

		ang[6] = 0;
		ang[7] = 0;
		ang[8] = 1;

		return false;
	}
}

#define	MAX_RENDER_STRINGS			8
#define	MAX_RENDER_STRING_LENGTH	32

struct q3refdef_s {
	int			x, y, width, height;
	float		fov_x, fov_y;
	vec3_t		vieworg;
	vec3_t		viewaxis[3];		// transformation matrix

	// time in milliseconds for shader effects and other time dependent rendering issues
	int			time;

	int			rdflags;			// RDF_NOWORLDMODEL, etc

	// 1 bits will prevent the associated area from rendering at all
	qbyte		areamask[MAX_MAP_AREA_BYTES];

	// text messages for deform text shaders
	char		text[MAX_RENDER_STRINGS][MAX_RENDER_STRING_LENGTH];
};
void R_DrawNameTags(void);
void VQ3_RenderView(const q3refdef_t *ref)
{
	int i;
	extern cvar_t r_torch;
	VectorCopy(ref->vieworg, r_refdef.vieworg);
	r_refdef.viewangles[0] = -(atan2(ref->viewaxis[0][2], sqrt(ref->viewaxis[0][1]*ref->viewaxis[0][1]+ref->viewaxis[0][0]*ref->viewaxis[0][0])) * 180 / M_PI);
	r_refdef.viewangles[1] = (atan2(ref->viewaxis[0][1], ref->viewaxis[0][0]) * 180 / M_PI);
	r_refdef.viewangles[2] = 0;
	VectorCopy(ref->viewaxis[0], r_refdef.viewaxis[0]);
	VectorCopy(ref->viewaxis[1], r_refdef.viewaxis[1]);
	VectorCopy(ref->viewaxis[2], r_refdef.viewaxis[2]);
	if (ref->rdflags & 1)
		r_refdef.flags |= RDF_NOWORLDMODEL;
	else
		r_refdef.flags &= ~RDF_NOWORLDMODEL;
	r_refdef.fov_x = ref->fov_x;
	r_refdef.fov_y = ref->fov_y;
	r_refdef.vrect.x = ref->x;
	r_refdef.vrect.y = ref->y;
	r_refdef.vrect.width = ref->width;
	r_refdef.vrect.height = ref->height;
	r_refdef.time = ref->time/1000.0f;
	r_refdef.useperspective = true;
	r_refdef.mindist = bound(0.1, gl_mindist.value, 4);
	r_refdef.maxdist = gl_maxdist.value;
	r_refdef.playerview = &cl.playerview[0];

	memset(&r_refdef.globalfog, 0, sizeof(r_refdef.globalfog));

	if (r_torch.ival)
	{
		dlight_t *dl;
		dl = CL_NewDlight(0, ref->vieworg, 300, r_torch.ival, 0.5, 0.5, 0.2);
		dl->flags |= LFLAG_SHADOWMAP|LFLAG_FLASHBLEND;
		dl->fov = 60;
		VectorCopy(ref->viewaxis[0], dl->axis[0]);
		VectorCopy(ref->viewaxis[1], dl->axis[1]);
		VectorCopy(ref->viewaxis[2], dl->axis[2]);
	}

	r_refdef.areabitsknown = true;
	for (i = 0; i < MAX_MAP_AREA_BYTES/sizeof(int); i++)
		((int*)r_refdef.areabits)[i] = ((int*)ref->areamask)[i] ^ ~0;
	R_RenderView();
	R_DrawNameTags();
	r_refdef.playerview = NULL;
#ifdef GLQUAKE
	if (qrenderer == QR_OPENGL)
	{
//		GL_Set2D (false);
	}
#endif

	r_refdef.time = 0;
}




void UI_RegisterFont(char *fontName, int pointSize, fontInfo_t *font)
{
	union 
	{
		char *c;
		int *i;
		float *f;
	} in;
	int i;
	char name[MAX_QPATH];
	size_t sz;
	#define readInt() LittleLong(*in.i++)
	#define readFloat() LittleFloat(*in.f++)

	snprintf(name, sizeof(name), "fonts/fontImage_%i.dat",pointSize);

	in.c = COM_LoadTempFile(name, &sz);
	if (sz == sizeof(fontInfo_t))
	{
		for(i=0; i<GLYPHS_PER_FONT; i++)
		{
			font->glyphs[i].height		= readInt();
			font->glyphs[i].top			= readInt();
			font->glyphs[i].bottom		= readInt();
			font->glyphs[i].pitch		= readInt();
			font->glyphs[i].xSkip		= readInt();
			font->glyphs[i].imageWidth	= readInt();
			font->glyphs[i].imageHeight = readInt();
			font->glyphs[i].s			= readFloat();
			font->glyphs[i].t			= readFloat();
			font->glyphs[i].s2			= readFloat();
			font->glyphs[i].t2			= readFloat();
			font->glyphs[i].glyph		= readInt();
			memcpy(font->glyphs[i].shaderName, in.i, 32);
			in.c += 32;
		}
		font->glyphScale = readFloat();
		memcpy(font->name, in.i, MAX_QPATH);

//		Com_Memcpy(font, faceData, sizeof(fontInfo_t));
		Q_strncpyz(font->name, name, sizeof(font->name));
		for (i = GLYPH_START; i < GLYPH_END; i++)
		{
			font->glyphs[i].glyph = VM_TOSHANDLE(R_RegisterPic(font->glyphs[i].shaderName, NULL));
		}
	}
}



#define VALIDATEPOINTER(o,l) if ((quintptr_t)o + l >= mask || VM_POINTER(o) < offset) Host_EndGame("Call to ui trap %i passes invalid pointer\n", (int)fn);	//out of bounds.

static qintptr_t UI_SystemCalls(void *offset, quintptr_t mask, qintptr_t fn, const qintptr_t *arg)
{
	int ret=0;
	char adrbuf[MAX_ADR_SIZE];

	//Remember to range check pointers.
	//The QVM must not be allowed to write to anything outside it's memory.
	//This includes getting the exe to copy it for it.

	//don't bother with reading, as this isn't a virus risk.
	//could be a cheat risk, but hey.

	//make sure that any called functions are also range checked.
	//like reading from files copies names into alternate buffers, allowing stack screwups.

	switch((uiImport_t)fn)
	{
	case UI_ERROR:
		Con_Printf("%s", (char*)VM_POINTER(arg[0]));
		break;
	case UI_PRINT:
		Con_Printf("%s", (char*)VM_POINTER(arg[0]));
		break;

	case UI_MILLISECONDS:
		VM_LONG(ret) = Sys_Milliseconds();
		break;

	case UI_ARGC:
		VM_LONG(ret) = Cmd_Argc();
		break;
	case UI_ARGV:
//		VALIDATEPOINTER(arg[1], arg[2]);
		Q_strncpyz(VM_POINTER(arg[1]), Cmd_Argv(VM_LONG(arg[0])), VM_LONG(arg[2]));
		break;
/*	case UI_ARGS:
		VALIDATEPOINTER(arg[0], arg[1]);
		Q_strncpyz(VM_POINTER(arg[0]), Cmd_Args(), VM_LONG(arg[1]));
		break;
*/

	case UI_CVAR_SET:
		{
			cvar_t *var;
			char *vname = VM_POINTER(arg[0]);
			char *vval = VM_POINTER(arg[1]);
			if (!strcmp(vname, "fs_game"))
			{
				Cbuf_AddText(va("gamedir %s\nui_restart\n", (char*)vval), RESTRICT_SERVER);
			}
			else
			{
				var = Cvar_FindVar(vname);
				if (var)
					Cvar_Set(var, vval);	//set it
				else
					Cvar_Get(vname, vval, 0, "UI created");	//create one
			}
		}
		break;
	case UI_CVAR_VARIABLEVALUE:
		{
			cvar_t *var;
			char *vname = VM_POINTER(arg[0]);
			var = Cvar_FindVar(vname);
			if (var)
				VM_FLOAT(ret) = var->value;
			else
				VM_FLOAT(ret) = 0;
		}
		break;
	case UI_CVAR_VARIABLESTRINGBUFFER:
		{
			cvar_t *var;
			char *vname = VM_POINTER(arg[0]);
			var = Cvar_FindVar(vname);
			if (!VM_LONG(arg[2]))
				VM_LONG(ret) = 0;
			else if (!var)
			{
				*(char *)VM_POINTER(arg[1]) = '\0';
				VM_LONG(ret) = -1;
			}
			else
			{
				if (arg[1] + arg[2] >= mask || VM_POINTER(arg[1]) < offset)
					VM_LONG(ret) = -2;	//out of bounds.
				else
					Q_strncpyz(VM_POINTER(arg[1]), var->string, VM_LONG(arg[2]));
			}
		}
		break;

	case UI_CVAR_SETVALUE:
		Cvar_SetValue(Cvar_FindVar(VM_POINTER(arg[0])), VM_FLOAT(arg[1]));
		break;

	case UI_CVAR_RESET:	//cvar reset
		{
			cvar_t *var;
			char *vname = VM_POINTER(arg[0]);
			var = Cvar_FindVar(vname);
			if (var)
				Cvar_Set(var, var->defaultstr);
		}
		break;

	case UI_CMD_EXECUTETEXT:
		{
			char *cmdtext = VM_POINTER(arg[1]);
#ifdef CL_MASTER
			if (!strncmp(cmdtext, "ping ", 5))
			{
				int i;
				for (i = 0; i < MAX_PINGREQUESTS; i++)
					if (ui_pings[i].type == NA_INVALID)
					{
						serverinfo_t *info;
						COM_Parse(cmdtext + 5);
						if (NET_StringToAdr(com_token, 0, &ui_pings[i]))
						{
							info = Master_InfoForServer(&ui_pings[i]);
							if (info)
							{
								info->special |= SS_KEEPINFO;
								info->sends++;
								Master_QueryServer(info);
							}
						}
						break;
					}
			}
			else if (!strncmp(cmdtext, "localservers", 12))
			{
				extern void NET_SendPollPacket(int len, void *data, netadr_t to);
				netadr_t na;
				MasterInfo_Refresh();

				if (NET_StringToAdr("255.255.255.255", PORT_Q3SERVER, &na))
					NET_SendPollPacket (14, va("%c%c%c%cgetstatus\n", 255, 255, 255, 255), na);
			}
			else
#endif
				Cbuf_AddText(cmdtext, RESTRICT_SERVER);
		}
		break;

	case UI_FS_FOPENFILE: //fopen
		if ((int)arg[1] + 4 >= mask || VM_POINTER(arg[1]) < offset)
			break;	//out of bounds.
		VM_LONG(ret) = VM_fopen(VM_POINTER(arg[0]), VM_POINTER(arg[1]), VM_LONG(arg[2]), 0);
		break;

	case UI_FS_READ:	//fread
		if ((int)arg[0] + VM_LONG(arg[1]) >= mask || VM_POINTER(arg[0]) < offset)
			break;	//out of bounds.

		VM_LONG(ret) = VM_FRead(VM_POINTER(arg[0]), VM_LONG(arg[1]), VM_LONG(arg[2]), 0);
		break;
	case UI_FS_WRITE:	//fwrite
		break;
	case UI_FS_FCLOSEFILE:	//fclose
		VM_fclose(VM_LONG(arg[0]), 0);
		break;

	case UI_FS_GETFILELIST:	//fs listing
		if ((int)arg[2] + arg[3] >= mask || VM_POINTER(arg[2]) < offset)
			break;	//out of bounds.
		return VM_GetFileList(VM_POINTER(arg[0]), VM_POINTER(arg[1]), VM_POINTER(arg[2]), VM_LONG(arg[3]));

	case UI_R_REGISTERMODEL:	//precache model
		{
			char *name = VM_POINTER(arg[0]);
			model_t *mod;
			mod = Mod_ForName(name, MLV_SILENT);
			if (mod && mod->loadstate == MLS_LOADING)
				COM_WorkerPartialSync(mod, &mod->loadstate, MLS_LOADING);
			if (!mod || mod->loadstate != MLS_LOADED || mod->type == mod_dummy)
				VM_LONG(ret) = 0;
			else
				VM_LONG(ret) = VM_TOMHANDLE(mod);
		}
		break;
	case UI_R_MODELBOUNDS:
		{
			VALIDATEPOINTER(arg[1], sizeof(vec3_t));
			VALIDATEPOINTER(arg[2], sizeof(vec3_t));
			{
				model_t *mod = VM_FROMMHANDLE(arg[0]);
				if (mod)
				{
					VectorCopy(mod->mins, ((float*)VM_POINTER(arg[1])));
					VectorCopy(mod->maxs, ((float*)VM_POINTER(arg[2])));
				}
				else
				{
					VectorClear(((float*)VM_POINTER(arg[1])));
					VectorClear(((float*)VM_POINTER(arg[2])));
				}
			}
		}
		break;

	case UI_R_REGISTERSKIN:
		VM_LONG(ret) = Mod_RegisterSkinFile(VM_POINTER(arg[0]));
		break;

	case UI_R_REGISTERFONT:	//register font
		UI_RegisterFont(VM_POINTER(arg[0]), arg[1], VM_POINTER(arg[2]));
		break;
	case UI_R_REGISTERSHADERNOMIP:
		if (!*(char*)VM_POINTER(arg[0]))
			VM_LONG(ret) = 0;
		else
			VM_LONG(ret) = VM_TOSHANDLE(R_RegisterPic(VM_POINTER(arg[0]), NULL));
		break;

	case UI_R_CLEARSCENE:	//clear scene
		CL_ClearEntityLists();
		break;
	case UI_R_ADDREFENTITYTOSCENE:	//add ent to scene
		VQ3_AddEntity(VM_POINTER(arg[0]));
		break;
	case UI_R_ADDLIGHTTOSCENE:	//add light to scene.
		break;
	case UI_R_RENDERSCENE:	//render scene
		VQ3_RenderView(VM_POINTER(arg[0]));
		break;

	case UI_R_SETCOLOR:	//setcolour float*
		{
			float *fl =VM_POINTER(arg[0]);
			if (!fl)
				R2D_ImageColours(1, 1, 1, 1);
			else
				R2D_ImageColours(fl[0], fl[1], fl[2], fl[3]);
		}
		break;

	case UI_R_DRAWSTRETCHPIC:
		R2D_Image(VM_FLOAT(arg[0]), VM_FLOAT(arg[1]), VM_FLOAT(arg[2]), VM_FLOAT(arg[3]), VM_FLOAT(arg[4]), VM_FLOAT(arg[5]), VM_FLOAT(arg[6]), VM_FLOAT(arg[7]), VM_FROMSHANDLE(VM_LONG(arg[8])));
		break;

	case UI_CM_LERPTAG:	//Lerp tag...
	//	tag, model, startFrame, endFrame, frac, tagName
		if ((int)arg[0] + sizeof(float)*12 >= mask || VM_POINTER(arg[0]) < offset)
			break;	//out of bounds.
		VM_LerpTag(VM_POINTER(arg[0]), VM_FROMMHANDLE(arg[1]), VM_LONG(arg[2]), VM_LONG(arg[3]), VM_FLOAT(arg[4]), VM_POINTER(arg[5]));
		break;

	case UI_S_REGISTERSOUND:
		{
			sfx_t *sfx;
			sfx = S_PrecacheSound(va("%s", (char*)VM_POINTER(arg[0])));
			if (sfx)
				VM_LONG(ret) = VM_TOSTRCACHE(arg[0]);	//return handle is the parameter they just gave
			else
				VM_LONG(ret) = -1;
		}
		break;
	case UI_S_STARTLOCALSOUND:
		if (VM_LONG(arg[0]) != -1 && arg[0])
			S_LocalSound(VM_FROMSTRCACHE(arg[0]));	//now we can fix up the sound name
		break;

	case UI_KEY_GETOVERSTRIKEMODE:
		return true;

	case UI_KEY_KEYNUMTOSTRINGBUF:
		if (VM_LONG(arg[0]) < 0 || VM_LONG(arg[0]) > 255 || (int)arg[1] + VM_LONG(arg[2]) >= mask || VM_POINTER(arg[1]) < offset || VM_LONG(arg[2]) < 1)
			break;	//out of bounds.

		Q_strncpyz(VM_POINTER(arg[1]), Key_KeynumToString(VM_LONG(arg[0]), 0), VM_LONG(arg[2]));
		break;

	case UI_KEY_GETBINDINGBUF:
		if (VM_LONG(arg[0]) < 0 || VM_LONG(arg[0]) > 255 || (int)arg[1] + VM_LONG(arg[2]) >= mask || VM_POINTER(arg[1]) < offset || VM_LONG(arg[2]) < 1)
			break;	//out of bounds.

		if (keybindings[VM_LONG(arg[0])][0])
			Q_strncpyz(VM_POINTER(arg[1]), keybindings[VM_LONG(arg[0])][0], VM_LONG(arg[2]));
		else
			*(char *)VM_POINTER(arg[1]) = '\0';
		break;

	case UI_KEY_SETBINDING:
		Key_SetBinding(VM_LONG(arg[0]), ~0, VM_POINTER(arg[1]), RESTRICT_LOCAL);
		break;

	case UI_KEY_ISDOWN:
		{
			extern qboolean	keydown[K_MAX];
			unsigned int k = VM_LONG(arg[0]);
			if (k < K_MAX && keydown[k])
				VM_LONG(ret) = 1;
			else
				VM_LONG(ret) = 0;
		}
		break;

	case UI_KEY_CLEARSTATES:
		Key_ClearStates();
		break;
	case UI_KEY_GETCATCHER:
		if (Key_Dest_Has(kdm_console))
			VM_LONG(ret) = keycatcher | 1;
		else
			VM_LONG(ret) = keycatcher;
		break;
	case UI_KEY_SETCATCHER:
		keycatcher = VM_LONG(arg[0]);
		break;

	case UI_GETGLCONFIG:	//get glconfig
		{
			char *cfg;
			if ((int)arg[0] + 11332/*sizeof(glconfig_t)*/ >= mask || VM_POINTER(arg[0]) < offset)
				break;	//out of bounds.
			cfg = VM_POINTER(arg[0]);
		

		//do any needed work
		memset(cfg, 0, 11304);
		*(int *)(cfg+11304) = vid.width;
		*(int *)(cfg+11308) = vid.height;
		*(float *)(cfg+11312) = (float)vid.width/vid.height;
		memset(cfg+11316, 0, 11332-11316);
		}
		break;

	case UI_GETCLIENTSTATE:	//get client state
		//fixme: we need to fill in a structure.
//		Con_Printf("ui_getclientstate\n");
		VALIDATEPOINTER(arg[0], sizeof(uiClientState_t));
		{
			uiClientState_t *state = VM_POINTER(arg[0]);
			state->connectPacketCount = 0;//clc.connectPacketCount;

			switch(cls.state)
			{
			case ca_disconnected:
				if (CL_TryingToConnect())
					state->connState = Q3CA_CONNECTING;
				else
					state->connState = Q3CA_DISCONNECTED;
				break;
			case ca_demostart:
				state->connState = Q3CA_CONNECTING;
				break;
			case ca_connected:
				state->connState = Q3CA_CONNECTED;
				break;
			case ca_onserver:
				state->connState = Q3CA_PRIMED;
				break;
			case ca_active:
				state->connState = Q3CA_ACTIVE;
				break;
			}
			Q_strncpyz( state->servername, cls.servername, sizeof( state->servername ) );
			Q_strncpyz( state->updateInfoString, "FTE!", sizeof( state->updateInfoString ) );	//warning/motd message from update server
			Q_strncpyz( state->messageString, "", sizeof( state->messageString ) );				//error message from game server
			state->clientNum = cl.playerview[0].playernum;
		}
		break;

	case UI_GETCONFIGSTRING:
		if (arg[1] + VM_LONG(arg[2]) >= mask || VM_POINTER(arg[1]) < offset || VM_LONG(arg[2]) < 1)
		{
			VM_LONG(ret) = 0;
			break;	//out of bounds.
		}
#ifdef VM_CG
		Q_strncpyz(VM_POINTER(arg[1]), CG_GetConfigString(VM_LONG(arg[0])), VM_LONG(arg[2]));
#endif
		break;

#ifdef CL_MASTER
	case UI_LAN_GETPINGQUEUECOUNT:	//these four are master server polling.
		{
			int i;
			for (i = 0; i < MAX_PINGREQUESTS; i++)
				if (ui_pings[i].type != NA_INVALID)
					VM_LONG(ret)++;
		}
		break;
	case UI_LAN_CLEARPING:	//clear ping
		//void (int pingnum)
		if (VM_LONG(arg[0])>= 0 && VM_LONG(arg[0]) < MAX_PINGREQUESTS)
			ui_pings[VM_LONG(arg[0])].type = NA_INVALID;
		break;
	case UI_LAN_GETPING:
		//void (int pingnum, char *buffer, int buflen, int *ping)
		if ((int)arg[1] + VM_LONG(arg[2]) >= mask || VM_POINTER(arg[1]) < offset)
			break;	//out of bounds.
		if ((int)arg[3] + sizeof(int) >= mask || VM_POINTER(arg[3]) < offset)
			break;	//out of bounds.

		Master_CheckPollSockets();
		if (VM_LONG(arg[0])>= 0 && VM_LONG(arg[0]) < MAX_PINGREQUESTS)
		{
			char *buf = VM_POINTER(arg[1]);
			char *adr;
			serverinfo_t *info = Master_InfoForServer(&ui_pings[VM_LONG(arg[0])]);
			if (info && info->ping != 0xffff)
			{
				adr = NET_AdrToString(adrbuf, sizeof(adrbuf), &info->adr);
				if (strlen(adr) < VM_LONG(arg[2]))
				{
					strcpy(buf, adr);
					VM_LONG(ret) = true;
					*(int *)VM_POINTER(arg[3]) = info->ping;
				}	
			}
			else
				strcpy(buf, "");
		}
		break;
	case UI_LAN_GETPINGINFO:
		//void (int pingnum, char *buffer, int buflen, )
		if ((int)arg[1] + VM_LONG(arg[2]) >= mask || VM_POINTER(arg[1]) < offset)
			break;	//out of bounds.
		if ((int)arg[3] + sizeof(int) >= mask || VM_POINTER(arg[3]) < offset)
			break;	//out of bounds.

		Master_CheckPollSockets();
		if (VM_LONG(arg[0])>= 0 && VM_LONG(arg[0]) < MAX_PINGREQUESTS)
		{
			char *buf = VM_POINTER(arg[1]);
			char *adr;
			serverinfo_t *info = Master_InfoForServer(&ui_pings[VM_LONG(arg[0])]);
			if (info && info->ping != 0xffff)
			{
				adr = info->moreinfo->info;
				if (!adr)
					adr = "";
				if (strlen(adr) < VM_LONG(arg[2]))
				{
					strcpy(buf, adr);
					if (!*Info_ValueForKey(buf, "mapname"))
					{
						Info_SetValueForKey(buf, "mapname", Info_ValueForKey(buf, "map"), VM_LONG(arg[2]));
						Info_RemoveKey(buf, "map");
					}
					Info_SetValueForKey(buf, "sv_maxclients", va("%i", info->maxplayers), VM_LONG(arg[2]));
					Info_SetValueForKey(buf, "clients", va("%i", info->players), VM_LONG(arg[2]));
					VM_LONG(ret) = true;
				}	
			}
			else
				strcpy(buf, "");
		}
		break;
#endif

	case UI_CVAR_REGISTER:
		if (VM_OOB(arg[0], sizeof(q3vmcvar_t)))
			break;	//out of bounds.
		return VMQ3_Cvar_Register(VM_POINTER(arg[0]), VM_POINTER(arg[1]), VM_POINTER(arg[2]), VM_LONG(arg[3]));

	case UI_CVAR_UPDATE:
		if (VM_OOB(arg[0], sizeof(q3vmcvar_t)))
			break;	//out of bounds.
		return VMQ3_Cvar_Update(VM_POINTER(arg[0]));

	case UI_MEMORY_REMAINING:
		VM_LONG(ret) = 1024*1024*8;//Hunk_LowMemAvailable();
		break;

	case UI_GET_CDKEY:	//get cd key
		{
			char *keydest = VM_POINTER(arg[0]);
			if ((int)arg[0] + VM_LONG(arg[1]) >= mask || VM_POINTER(arg[0]) < offset)
				break;	//out of bounds.
			strncpy(keydest, Cvar_VariableString("cl_cdkey"), VM_LONG(arg[1]));
		}
		break;
	case UI_SET_CDKEY:	//set cd key
		{
			char *keysrc = VM_POINTER(arg[0]);
			cvar_t *cvar;
			if ((int)arg[0] + strlen(keysrc) >= mask || VM_POINTER(arg[0]) < offset)
				break;	//out of bounds.
			cvar = Cvar_Get("cl_cdkey", "", 0, "Quake3 auth");
			Cvar_Set(cvar, keysrc);
		}
		break;

	case UI_REAL_TIME:
		VM_FLOAT(ret) = realtime;
		break;

#ifdef CL_MASTER
	case UI_LAN_GETSERVERCOUNT:	//LAN Get server count
		//int (int source)
		VM_LONG(ret) = Master_TotalCount();
		break;
	case UI_LAN_GETSERVERADDRESSSTRING:	//LAN get server address
		//void (int source, int svnum, char *buffer, int buflen)
		if ((int)arg[2] + VM_LONG(arg[3]) >= mask || VM_POINTER(arg[2]) < offset)
			break;	//out of bounds.
		{
			char *buf = VM_POINTER(arg[2]);
			char *adr;
			serverinfo_t *info = Master_InfoForNum(VM_LONG(arg[1]));
			if (info)
			{
				adr = NET_AdrToString(adrbuf, sizeof(adrbuf), &info->adr);
				if (strlen(adr) < VM_LONG(arg[3]))
				{
					strcpy(buf, adr);
					VM_LONG(ret) = true;
				}	
			}
			else
				strcpy(buf, "");
		}
		break;
	case UI_LAN_LOADCACHEDSERVERS:
		break;
	case UI_LAN_SAVECACHEDSERVERS:
		break;
	case UI_LAN_GETSERVERPING:
		return 50;
	case UI_LAN_GETSERVERINFO:
		break;
	case UI_LAN_SERVERISVISIBLE:
		return 1;
		break;
#endif

	case UI_VERIFY_CDKEY:
		VM_LONG(ret) = true;
		break;

	case UI_SET_PBCLSTATUS:
		break;

// standard Q3
	case UI_MEMSET:
		{
			void *dest = VM_POINTER(arg[0]);
			if ((int)arg[0] + arg[2] >= mask || dest < offset)
				break;	//out of bounds.
			memset(dest, arg[1], arg[2]);
		}
		break;
	case UI_MEMCPY:
		{
			void *dest = VM_POINTER(arg[0]);
			void *src = VM_POINTER(arg[1]);
			if ((int)arg[0] + arg[2] >= mask || VM_POINTER(arg[0]) < offset)
				break;	//out of bounds.
			memcpy(dest, src, arg[2]);
		}
		break;
	case UI_STRNCPY:
		{
			void *dest = VM_POINTER(arg[0]);
			void *src = VM_POINTER(arg[1]);
			if (arg[0] + arg[2] >= mask || VM_POINTER(arg[0]) < offset)
				break;	//out of bounds.
			Q_strncpyS(dest, src, arg[2]);
		}
		break;
	case UI_SIN:
		VM_FLOAT(ret)=(float)sin(VM_FLOAT(arg[0]));
		break;
	case UI_COS:
		VM_FLOAT(ret)=(float)cos(VM_FLOAT(arg[0]));
		break;
	case UI_ATAN2:
		VM_FLOAT(ret)=(float)atan2(VM_FLOAT(arg[0]), VM_FLOAT(arg[1]));
		break;
	case UI_SQRT:
		VM_FLOAT(ret)=(float)sqrt(VM_FLOAT(arg[0]));
		break;
	case UI_FLOOR:
		VM_FLOAT(ret)=(float)floor(VM_FLOAT(arg[0]));
		break;
	case UI_CEIL:
		VM_FLOAT(ret)=(float)ceil(VM_FLOAT(arg[0]));
		break;
/*
	case UI_GETPLAYERINFO:
		if (arg[1] + sizeof(vmuiclientinfo_t) >= mask || VM_POINTER(arg[1]) < offset)
			break;	//out of bounds.
		if (VM_LONG(arg[0]) < -1 || VM_LONG(arg[0] ) >= MAX_CLIENTS)
			break;
		{
			int i = VM_LONG(arg[0]);
			vmuiclientinfo_t *vci = VM_POINTER(arg[1]);
			if (i == -1)
			{
				i = cl.playernum[0];
				if (i < 0)
				{
					memset(vci, 0, sizeof(*vci));
					return 0;
				}	
			}
			vci->bottomcolour = cl.players[i].rbottomcolor;
			vci->frags = cl.players[i].frags;
			Q_strncpyz(vci->name, cl.players[i].name, UIMAX_SCOREBOARDNAME);
			vci->ping = cl.players[i].ping;
			vci->pl = cl.players[i].pl;
			vci->starttime = cl.players[i].entertime;
			vci->topcolour = cl.players[i].rtopcolor;
			vci->userid = cl.players[i].userid;
			Q_strncpyz(vci->userinfo, cl.players[i].userinfo, sizeof(vci->userinfo));
		}
		break;
	case UI_GETSTAT:
		if (VM_LONG(arg[0]) < 0 || VM_LONG(arg[0]) >= MAX_CL_STATS)
			VM_LONG(ret) = 0;	//invalid stat num.
		else
			VM_LONG(ret) = cl.stats[0][VM_LONG(arg[0])];
		break;
	case UI_GETVIDINFO:
		{
			vidinfo_t *vi;
			if (arg[0] + VM_LONG(arg[1]) >= mask || VM_POINTER(arg[1]) < offset)
			{
				VM_LONG(ret) = 0;
				break;	//out of bounds.
			}
			vi = VM_POINTER(arg[0]);
			if (VM_LONG(arg[1]) < sizeof(vidinfo_t))
			{
				VM_LONG(ret) = 0;
				break;
			}
			VM_LONG(ret) = sizeof(vidinfo_t);
			vi->width = vid.width;
			vi->height = vid.height;
			vi->refreshrate = 60;
			vi->fullscreen = 1;
			Q_strncpyz(vi->renderername, q_renderername, sizeof(vi->renderername));
		}
		break;
	case UI_GET_STRING:
		{
			char *str = NULL;
			switch (arg[0])
			{
			case SID_Q2STATUSBAR:
				str = cl.q2statusbar;
				break;
			case SID_Q2LAYOUT:
				str = cl.q2layout;
				break;
			case SID_CENTERPRINTTEXT:
				str = scr_centerstring;
				break;
			case SID_SERVERNAME:
				str = cls.servername;
				break;

			default:
				str = Get_Q2ConfigString(arg[0]);
				break;
			}
			if (!str)
				return -1;

			if (arg[1] + arg[2] >= mask || VM_POINTER(arg[1]) < offset)
				return -1;	//out of bounds.

			if (strlen(str)>= arg[2])
				return -1;

			strcpy(VM_POINTER(arg[1]), str);	//already made sure buffer is big enough

			return strlen(str);
		}
*/

	case UI_PC_ADD_GLOBAL_DEFINE:
		Con_Printf("UI_PC_ADD_GLOBAL_DEFINE not supported\n");
		break;
	case UI_PC_SOURCE_FILE_AND_LINE:
		Script_Get_File_And_Line(arg[0], VM_POINTER(arg[1]), VM_POINTER(arg[2]));
		break;

	case UI_PC_LOAD_SOURCE:
		return Script_LoadFile(VM_POINTER(arg[0]));
	case UI_PC_FREE_SOURCE:
		Script_Free(arg[0]);
		break;
	case UI_PC_READ_TOKEN:
		//fixme: memory protect.
		return Script_Read(arg[0], VM_POINTER(arg[1]));

	case UI_CIN_PLAYCINEMATIC:
		//handle(name, x, y, w, h, looping)
	case UI_CIN_STOPCINEMATIC:
		//(handle)
	case UI_CIN_RUNCINEMATIC:
		//(handle)
	case UI_CIN_DRAWCINEMATIC:
		//(handle)
	case UI_CIN_SETEXTENTS:
		//(handle, x, y, w, h)
		break;

	default:
		Con_Printf("Q3UI: Not implemented system trap: %i\n", (int)fn);
		return 0;
	}

	return ret;
}

static int UI_SystemCallsVM(void *offset, quintptr_t mask, int fn, const int *arg)
{	//this is so we can use edit and continue properly (vc doesn't like function pointers for edit+continue)
#if __WORDSIZE == 32
	return UI_SystemCalls(offset, mask, fn, (qintptr_t*)arg);
#else
	qintptr_t args[9];

	args[0]=arg[0];
	args[1]=arg[1];
	args[2]=arg[2];
	args[3]=arg[3];
	args[4]=arg[4];
	args[5]=arg[5];
	args[6]=arg[6];
	args[7]=arg[7];
	args[8]=arg[8];

	return UI_SystemCalls(offset, mask, fn, args);
#endif
}

//I'm not keen on this.
//but dlls call it without saying what sort of vm it comes from, so I've got to have them as specifics
static qintptr_t EXPORT_FN UI_SystemCallsNative(qintptr_t arg, ...)
{
	qintptr_t args[9];
	va_list argptr;

	va_start(argptr, arg);
	args[0]=va_arg(argptr, qintptr_t);
	args[1]=va_arg(argptr, qintptr_t);
	args[2]=va_arg(argptr, qintptr_t);
	args[3]=va_arg(argptr, qintptr_t);
	args[4]=va_arg(argptr, qintptr_t);
	args[5]=va_arg(argptr, qintptr_t);
	args[6]=va_arg(argptr, qintptr_t);
	args[7]=va_arg(argptr, qintptr_t);
	args[8]=va_arg(argptr, qintptr_t);
	va_end(argptr);

	return UI_SystemCalls(NULL, ~(quintptr_t)0, arg, args);
}

qboolean UI_DrawStatusBar(int scores)
{
	return false;
/*
	if (!uivm)
		return false;

	return VM_Call(uivm, UI_DRAWSTATUSBAR, scores);
*/
}

qboolean UI_DrawIntermission(void)
{
	return false;
/*
	if (!uivm)
		return false;

	return VM_Call(uivm, UI_INTERMISSION);
*/
}

void UI_DrawMenu(void)
{
	if (uivm)
	{
		VM_Call(uivm, UI_REFRESH, (int)(realtime * 1000));
	}
}

qboolean UI_CenterPrint(char *text, qboolean finale)
{
	scr_centerstring = text;
	return false;
/*
	if (!uivm)
		return false;

	return VM_Call(uivm, UI_STRINGCHANGED, SID_CENTERPRINTTEXT);
*/
}

qboolean UI_Q2LayoutChanged(void)
{
	return false;
/*
	if (!uivm)
		return false;

	return VM_Call(uivm, UI_STRINGCHANGED, SID_CENTERPRINTTEXT);
*/
}

void UI_StringChanged(int num)
{
/*	if (uivm)
		VM_Call(uivm, UI_STRINGCHANGED, num);
*/
}

void UI_Reset(void)
{
	keycatcher &= ~2;

	if (qrenderer == QR_NONE)	//no renderer loaded
		UI_Stop();
	else if (uivm)
		VM_Call(uivm, UI_INIT);
}

int UI_MenuState(void)
{
	if (Key_Dest_Has(kdm_gmenu) || Key_Dest_Has(kdm_emenu))
	{	//engine's menus take precedence over q3's ui
		return false;
	}
	if (!uivm)
		return false;
	if (VM_Call(uivm, UI_IS_FULLSCREEN))
		return 2;
	else if (keycatcher&2)
		return 3;
	else
		return 0;
}

qboolean UI_KeyPress(int key, int unicode, qboolean down)
{
	extern qboolean	keydown[K_MAX];
	extern int		keyshift[K_MAX];		// key to map to if shift held down in console
//	qboolean result;
	if (!uivm)
		return false;
//	if (key_dest == key_menu)
//		return false;
	if (!(keycatcher&2))
	{
		if (key == K_ESCAPE && down)
		{
			if (Media_PlayingFullScreen())
			{
				Media_StopFilm(true);
			}

			UI_OpenMenu();

			scr_conlines = 0;
			return true;
		}
		return false;
	}

	if (keydown[K_SHIFT])
		key = keyshift[key];
	if (key < K_BACKSPACE && key >= ' ')
		key |= 1024;

	/*result = */VM_Call(uivm, UI_KEY_EVENT, key, down);
/*
	if (!keycatcher && !cls.state && key == K_ESCAPE && down)
	{
		M_Menu_Main_f();
		return true;
	}*/

	return true;

//	return result;
}

qboolean UI_MousePosition(int xpos, int ypos)
{
	if (uivm && (keycatcher&2))
	{
		VM_Call(uivm, UI_MOUSE_EVENT, (xpos)*640/(int)vid.width, (ypos)*480/(int)vid.height);
		return true;
	}
	return false;
}

void UI_Stop (void)
{
	keycatcher &= ~2;
	if (uivm)
	{
		VM_Call(uivm, UI_SHUTDOWN);
		VM_Destroy(uivm);
		VM_fcloseall(0);
		uivm = NULL;

		//mimic Q3 and save the config if anything got changed.
		//note that q3 checks every frame. we only check when the ui is closed.
		if (Cvar_UnsavedArchive())
			Cmd_ExecuteString("cfg_save", RESTRICT_LOCAL);
	}
}

void UI_Start (void)
{
	int i;
	int apiversion;
	if (qrenderer == QR_NONE)
		return;

	UI_Stop();

	for (i = 0; i < MAX_PINGREQUESTS; i++)
		ui_pings[i].type = NA_INVALID;

	uivm = VM_Create("vm/ui", com_nogamedirnativecode.ival?NULL:UI_SystemCallsNative, UI_SystemCallsVM);
	if (uivm)
	{
		apiversion = VM_Call(uivm, UI_GETAPIVERSION, 6);
		if (apiversion != 4 && apiversion != 6)	//make sure we can run the thing
		{
			Con_Printf("User-Interface VM uses incompatible API version (%i)\n", apiversion);
			VM_Destroy(uivm);
			VM_fcloseall(0);
			uivm = NULL;
			return;
		}
		VM_Call(uivm, UI_INIT);

		UI_OpenMenu();
	}
}

void UI_Restart_f(void)
{
	UI_Stop();
	UI_Start();

	if (uivm)
	{
		if (cls.state)
			VM_Call(uivm, UI_SET_ACTIVE_MENU, 2);
		else
			VM_Call(uivm, UI_SET_ACTIVE_MENU, 1);
	}
}

qboolean UI_OpenMenu(void)
{
	if (uivm)
	{
		if (cls.state)
			VM_Call(uivm, UI_SET_ACTIVE_MENU, 2);
		else
			VM_Call(uivm, UI_SET_ACTIVE_MENU, 1);
		return true;
	}
	return false;
}

qboolean UI_Command(void)
{
	if (uivm)
		return VM_Call(uivm, UI_CONSOLE_COMMAND, (int)(realtime * 1000));
	return false;
}

void UI_Init (void)
{
	Cmd_AddCommand("ui_restart", UI_Restart_f);
}
#endif

