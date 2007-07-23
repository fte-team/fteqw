#include "quakedef.h"
#ifdef VM_UI
#include "ui_public.h"
#include "cl_master.h"
#include "shader.h"

int keycatcher;

#include "clq3defs.h"

void GLDraw_ShaderImage (int x, int y, int w, int h, float s1, float t1, float s2, float t2, struct shader_s *pic);

#define MAX_TOKENLENGTH		1024
typedef struct pc_token_s
{
	int type;
	int subtype;
	int intvalue;
	float floatvalue;
	char string[MAX_TOKENLENGTH];
} pc_token_t;
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
script_t *scripts;
int maxscripts;
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
		strcpy(readstring, com_token);
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
					strcat(readstring, com_token);
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
			sc->filestack[sc->stackdepth] = sc->originalfilestack[sc->stackdepth] = COM_LoadMallocFile(com_token);
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
	sc->filestack[0] = sc->originalfilestack[0] = COM_LoadMallocFile(filename);
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










#ifdef RGLQUAKE
#include "glquake.h"//hack
#else
typedef float m3by3_t[3][3];
#endif

static vm_t *uivm;

static char *scr_centerstring;

static int ox, oy;

void GLDraw_Image(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qpic_t *pic);
void SWDraw_Image (float xp, float yp, float wp, float hp, float s1, float t1, float s2, float t2, qpic_t *pic);
char *Get_Q2ConfigString(int i);
void SWDraw_ImageColours (float r, float g, float b, float a);


#define MAX_PINGREQUESTS 16

netadr_t ui_pings[MAX_PINGREQUESTS];

#define UITAGNUM 2452


#define MAX_VMUI_FILES 8

typedef struct {
	char name[256];
	char *data;
	int bufferlen;
	int len;
	int ofs;
	int accessmode;
	int owner;
} vmui_fopen_files_t;
vmui_fopen_files_t vmui_fopen_files[MAX_VMUI_FILES];

int VMUI_fopen (char *name, int *handle, int fmode, int owner)
{
	int i;

	if (!handle)
		return FS_FLocateFile(name, FSLFRT_IFFOUND, NULL);

	*handle = 0;

	for (i = 0; i < MAX_VMUI_FILES; i++)
		if (!vmui_fopen_files[i].data)
			break;

	if (i == MAX_VMUI_FILES)	//too many already open
	{
		return -1;
	}

	if (name[1] == ':' ||	//dos filename absolute path specified - reject.
		*name == '\\' || *name == '/' ||	//absolute path was given - reject
		strstr(name, ".."))	//someone tried to be cleaver.
	{
		return -1;
	}

	Q_strncpyz(vmui_fopen_files[i].name, name, sizeof(vmui_fopen_files[i].name));

	vmui_fopen_files[i].accessmode = fmode;
	vmui_fopen_files[i].owner = owner;
	switch (fmode)
	{
	case 0:	//read
		vmui_fopen_files[i].data = COM_LoadMallocFile(name);
		vmui_fopen_files[i].bufferlen = vmui_fopen_files[i].len = com_filesize;
		vmui_fopen_files[i].ofs = 0;
		if (vmui_fopen_files[i].data)
			break;
		else
			return -1;
		break;
		/*
	case 2:	//append
	case 3:	//append
		vmui_fopen_files[i].data = COM_LoadMallocFile(name);
		vmui_fopen_files[i].ofs = vmui_fopen_files[i].bufferlen = vmui_fopen_files[i].len = com_filesize;
		if (vmui_fopen_files[i].data)
			break;
		//fall through
	case 1:	//write
		vmui_fopen_files[i].bufferlen = 8192;
		vmui_fopen_files[i].data = BZ_Malloc(vmui_fopen_files[i].bufferlen);
		vmui_fopen_files[i].len = 0;
		vmui_fopen_files[i].ofs = 0;
		break;
		*/
	default: //bad
		return -1;
	}

	*handle = i+1;
	return vmui_fopen_files[i].len;
}

void VMUI_fclose (int fnum, int owner)
{
	fnum--;

	if (fnum < 0 || fnum >= MAX_VMUI_FILES)
		return;	//out of range

	if (vmui_fopen_files[fnum].owner != owner)
		return;	//cgs?

	if (!vmui_fopen_files[fnum].data)
		return;	//not open

	switch(vmui_fopen_files[fnum].accessmode)
	{
	case 0:
		BZ_Free(vmui_fopen_files[fnum].data);
		break;
	case 1:
	case 2:
	case 3:
		COM_WriteFile(vmui_fopen_files[fnum].name, vmui_fopen_files[fnum].data, vmui_fopen_files[fnum].len);
		BZ_Free(vmui_fopen_files[fnum].data);
		break;
	}
	vmui_fopen_files[fnum].data = NULL;
}

int VMUI_FRead (char *dest, int quantity, int fnum, int owner)
{
	fnum--;
	if (fnum < 0 || fnum >= MAX_VMUI_FILES)
		return 0;	//out of range

	if (vmui_fopen_files[fnum].owner != owner)
		return 0;	//cgs?

	if (!vmui_fopen_files[fnum].data)
		return 0;	//not open

	if (quantity > vmui_fopen_files[fnum].len - vmui_fopen_files[fnum].ofs)
		quantity = vmui_fopen_files[fnum].len - vmui_fopen_files[fnum].ofs;
	memcpy(dest, vmui_fopen_files[fnum].data + vmui_fopen_files[fnum].ofs, quantity);
	vmui_fopen_files[fnum].ofs += quantity;

	return quantity;
}
/*
void VMUI_fputs (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int fnum = G_FLOAT(OFS_PARM0);
	char *msg = PF_VarString(prinst, 1, pr_globals);
	int len = strlen(msg);
	if (fnum < 0 || fnum >= MAX_QC_FILES)
		return;	//out of range

	if (!pf_fopen_files[fnum].data)
		return;	//not open

	if (pf_fopen_files[fnum].prinst != prinst)
		return;	//this just isn't ours.

	if (pf_fopen_files[fnum].bufferlen < pf_fopen_files[fnum].ofs + len)
	{
		char *newbuf;
		pf_fopen_files[fnum].bufferlen = pf_fopen_files[fnum].bufferlen*2 + len;
		newbuf = BZF_Malloc(pf_fopen_files[fnum].bufferlen);
		memcpy(newbuf, pf_fopen_files[fnum].data, pf_fopen_files[fnum].len);
		BZ_Free(pf_fopen_files[fnum].data);
		pf_fopen_files[fnum].data = newbuf;
	}

	memcpy(pf_fopen_files[fnum].data + pf_fopen_files[fnum].ofs, msg, len);
	if (pf_fopen_files[fnum].len < pf_fopen_files[fnum].ofs + len)
		pf_fopen_files[fnum].len = pf_fopen_files[fnum].ofs + len;
	pf_fopen_files[fnum].ofs+=len;
}
*/
void VMUI_fcloseall (int owner)
{
	int i;
	for (i = 1; i <= MAX_VMUI_FILES; i++)
	{
		VMUI_fclose(i, owner);
	}
}



typedef struct {
	char *initialbuffer;
	char *buffer;
	char *dir;
	int found;
	int bufferleft;
	int skip;
} vmsearch_t;
int VMEnum(char *match, int size, void *args)
{
	char *check;
	int newlen;
	match += ((vmsearch_t *)args)->skip;
	newlen = strlen(match)+1;
	if (newlen > ((vmsearch_t *)args)->bufferleft)
		return false;	//too many files for the buffer

	check = ((vmsearch_t *)args)->initialbuffer;
	while(check < ((vmsearch_t *)args)->buffer)
	{
		if (!stricmp(check, match))
			return true;	//we found this one already
		check += strlen(check)+1;
	}

	memcpy(((vmsearch_t *)args)->buffer, match, newlen);
	((vmsearch_t *)args)->buffer+=newlen;
	((vmsearch_t *)args)->bufferleft-=newlen;
	((vmsearch_t *)args)->found++;
	return true;
}

static int IfFound(char *match, int size, void *args)
{
	*(qboolean*)args = true;
	return true;
}

int VMEnumMods(char *match, int size, void *args)
{
	char *check;
	char desc[1024];
	int newlen;
	int desclen;
	qboolean foundone;
	vfsfile_t *f;

	newlen = strlen(match)+1;

	if (*match && match[newlen-2] != '/')
		return true;
	match[newlen-2] = '\0';
	newlen--;

	if (!stricmp(match, "baseq3"))
		return true;	//we don't want baseq3

	foundone = false;
	Sys_EnumerateFiles(va("%s/%s/", ((vmsearch_t *)args)->dir, match), "*.pk3", IfFound, &foundone);
	if (foundone == false)
		return true;	//we only count directories with a pk3 file

	Q_strncpyz(desc, match, sizeof(desc));
	f = FS_OpenVFS(va("%s/description.txt", match), "rb", FS_BASE);
	if (f)
	{
		VFS_GETS(f, desc, sizeof(desc));
		VFS_CLOSE(f);
	}

	desclen = strlen(desc)+1;

	if (newlen+desclen+5 > ((vmsearch_t *)args)->bufferleft)
		return false;	//too many files for the buffer

	check = ((vmsearch_t *)args)->initialbuffer;
	while(check < ((vmsearch_t *)args)->buffer)
	{
		if (!stricmp(check, match))
			return true;	//we found this one already
		check += strlen(check)+1;
		check += strlen(check)+1;
	}

	memcpy(((vmsearch_t *)args)->buffer, match, newlen);
	((vmsearch_t *)args)->buffer+=newlen;
	((vmsearch_t *)args)->bufferleft-=newlen;

	memcpy(((vmsearch_t *)args)->buffer, desc, desclen);
	((vmsearch_t *)args)->buffer+=desclen;
	((vmsearch_t *)args)->bufferleft-=desclen;

	((vmsearch_t *)args)->found++;
	return true;
}

int VMQ3_GetFileList(char *path, char *ext, char *output, int buffersize)
{
	vmsearch_t vms;
	vms.initialbuffer = vms.buffer = output;
	vms.skip = strlen(path)+1;
	vms.bufferleft = buffersize;
	vms.found=0;
	if (!strcmp(path, "$modlist"))
	{
		vms.skip=0;
		Sys_EnumerateFiles((vms.dir=com_quakedir), "*", VMEnumMods, &vms);
		if (*com_homedir)
			Sys_EnumerateFiles((vms.dir=com_homedir), "*", VMEnumMods, &vms);
	}
	else if (*(char *)ext == '.' || *(char *)ext == '/')
		COM_EnumerateFiles(va("%s/*%s", path, ext), VMEnum, &vms);
	else
		COM_EnumerateFiles(va("%s/*.%s", path, ext), VMEnum, &vms);
	return vms.found;
}





typedef struct q3refEntity_s {
	refEntityType_t	reType;
	int			renderfx;

	struct model_s	*hModel;				// opaque type outside refresh

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
	//int		customSkin;			// NULL for default skin
	char		*customSkin; //the function that references this wants a char* not an int
	int		customShader;		// use one image for the entire thing

	// misc
	qbyte		shaderRGBA[4];		// colors used by rgbgen entity shaders
	float		shaderTexCoord[2];	// texture coordinates used by tcMod entity modifiers
	float		shaderTime;			// subtracted from refdef time to control effect start times

	// extra sprite information
	float		radius;
	float		rotation;
} q3refEntity_t;

#define	Q2RF_VIEWERMODEL		2		// don't draw through eyes, only mirrors
#define	Q2RF_WEAPONMODEL		4		// only draw through eyes
#define	Q2RF_DEPTHHACK			16		// for view weapon Z crunching


#define	Q3RF_THIRD_PERSON		2		// don't draw through eyes, only mirrors (player bodies, chat sprites)
#define	Q3RF_FIRST_PERSON		4		// only draw through eyes (view weapon, damage blood blob)
#define	Q3RF_DEPTHHACK			8		// for view weapon Z crunching
void VQ3_AddEntity(const q3refEntity_t *q3)
{
	entity_t ent;
	if (!cl_visedicts)
		cl_visedicts = cl_visedicts_list[0];
	memset(&ent, 0, sizeof(ent));
	ent.model = q3->hModel;
	ent.frame = q3->frame;
	ent.oldframe = q3->oldframe;
	memcpy(ent.axis, q3->axis, sizeof(q3->axis));
	ent.lerpfrac = q3->backlerp;
	ent.scale = q3->radius;
	ent.rtype = q3->reType;
	ent.rotation = q3->rotation;

	if (q3->customSkin)
		ent.skinnum = Mod_SkinForName(ent.model, q3->customSkin);

	ent.shaderRGBAf[0] = q3->shaderRGBA[0]/255.0f;
	ent.shaderRGBAf[1] = q3->shaderRGBA[1]/255.0f;
	ent.shaderRGBAf[2] = q3->shaderRGBA[2]/255.0f;
	ent.shaderRGBAf[3] = q3->shaderRGBA[3]/255.0f;
#ifdef Q3SHADERS
	ent.forcedshader = (void*)q3->customShader;
	ent.shaderTime = q3->shaderTime;
#endif
	if (q3->renderfx & Q3RF_FIRST_PERSON)
		ent.flags |= Q2RF_WEAPONMODEL;
	if (q3->renderfx & Q3RF_DEPTHHACK)
		ent.flags |= Q2RF_DEPTHHACK;
	if (q3->renderfx & Q3RF_THIRD_PERSON)
		ent.flags |= Q2RF_VIEWERMODEL;
	VectorCopy(q3->origin, ent.origin);
	VectorCopy(q3->oldorigin, ent.oldorigin);
	V_AddAxisEntity(&ent);
}

int VM_LerpTag(void *out, model_t *model, int f1, int f2, float l2, char *tagname)
{
	int tagnum;
	float *ang;
	float *org;

	float tr[12];
	qboolean found;

	org = (float*)out;
	ang = ((float*)out+3);

	if (Mod_GetTag)
	{
		if (Mod_TagNumForName)
			tagnum = Mod_TagNumForName(model, tagname);
		else
			tagnum = 0;
		found = Mod_GetTag(model, tagnum, f1, f2, l2, 0, 0, tr);
	}
	else
		found = false;

	if (found)
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

typedef struct q3refdef_s {
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
} q3refdef_t;

void VQ3_RenderView(const q3refdef_t *ref)
{
	VectorCopy(ref->vieworg, r_refdef.vieworg);
	r_refdef.viewangles[0] = -(atan2(ref->viewaxis[0][2], sqrt(ref->viewaxis[0][1]*ref->viewaxis[0][1]+ref->viewaxis[0][0]*ref->viewaxis[0][0])) * 180 / M_PI);
	r_refdef.viewangles[1] = (atan2(ref->viewaxis[0][1], ref->viewaxis[0][0]) * 180 / M_PI);
	r_refdef.viewangles[2] = 0;
	if (ref->rdflags & 1)
		r_refdef.flags |= Q2RDF_NOWORLDMODEL;
	else
		r_refdef.flags &= ~Q2RDF_NOWORLDMODEL;
	r_refdef.fov_x = ref->fov_x;
	r_refdef.fov_y = ref->fov_y;
	r_refdef.vrect.x = ref->x;
	r_refdef.vrect.y = ref->y;
	r_refdef.vrect.width = ref->width;
	r_refdef.vrect.height = ref->height;
	r_refdef.time = ref->time/1000.0f;
	r_refdef.useperspective = true;
	r_refdef.currentplayernum = -1;

	memcpy(cl.q2frame.areabits, ref->areamask, sizeof(cl.q2frame.areabits));
#ifdef RGLQUAKE
	if (qrenderer == QR_OPENGL)
	{
		gl_ztrickdisabled|=16;
		qglDisable(GL_ALPHA_TEST);
		qglDisable(GL_BLEND);
	}
#endif
	R_RenderView();
#ifdef RGLQUAKE
	if (qrenderer == QR_OPENGL)
	{
		gl_ztrickdisabled&=~16;
		GL_Set2D ();
		qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		GL_TexEnv(GL_MODULATE);
	}
#endif

#ifdef RGLQUAKE
	if (qrenderer == QR_OPENGL)
	{
		qglDisable(GL_ALPHA_TEST);
		qglEnable(GL_BLEND);
	}
#endif

	vid.recalc_refdef = 1;
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
	#define readInt() LittleLong(*in.i++)
	#define readFloat() LittleFloat(*in.f++)

	snprintf(name, sizeof(name), "fonts/fontImage_%i.dat",pointSize);

	in.c = COM_LoadTempFile(name);
	if (com_filesize == sizeof(fontInfo_t))
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
			font->glyphs[i].glyph = (int)R_RegisterPic(font->glyphs[i].shaderName);
		}
	}
}



#define VM_FROMHANDLE(a) ((void*)a)
#define VM_TOHANDLE(a) ((int)a)
#define VALIDATEPOINTER(o,l) if ((int)o + l >= mask || VM_POINTER(o) < offset) SV_Error("Call to ui trap %i passes invalid pointer\n", fn);	//out of bounds.

#ifndef _DEBUG
static
#endif
long UI_SystemCallsEx(void *offset, unsigned int mask, int fn, const long *arg)
{
	int ret=0;

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
		Con_Printf("%s", VM_POINTER(arg[0]));
		break;
	case UI_PRINT:
		Con_Printf("%s", VM_POINTER(arg[0]));
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
			if (!strcmp(VM_POINTER(arg[0]), "fs_game"))
			{
				Cbuf_AddText(va("gamedir %s\nui_restart\n", VM_POINTER(arg[1])), RESTRICT_SERVER);
			}
			else
			{
				var = Cvar_FindVar(VM_POINTER(arg[0]));
				if (var)
					Cvar_Set(var, VM_POINTER(arg[1]));	//set it
				else
					Cvar_Get(VM_POINTER(arg[0]), VM_POINTER(arg[1]), 0, "UI created");	//create one
			}
		}
		break;
	case UI_CVAR_VARIABLEVALUE:
		{
			cvar_t *var;
			var = Cvar_FindVar(VM_POINTER(arg[0]));
			if (var)
				VM_FLOAT(ret) = var->value;
			else
				VM_FLOAT(ret) = 0;
		}
		break;
	case UI_CVAR_VARIABLESTRINGBUFFER:
		{
			cvar_t *var;
			var = Cvar_FindVar(VM_POINTER(arg[0]));
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
			var = Cvar_FindVar((char *)VM_POINTER(arg[0]));
			if (var)
				Cvar_Set(var, var->defaultstr);
		}
		break;

	case UI_CMD_EXECUTETEXT:
		if (!strncmp(VM_POINTER(arg[1]), "ping ", 5))
		{
			int i;
			for (i = 0; i < MAX_PINGREQUESTS; i++)
				if (ui_pings[i].type == NA_INVALID)
				{
					serverinfo_t *info;
					NET_StringToAdr((char *)VM_POINTER(arg[1]) + 5, &ui_pings[i]);
					info = Master_InfoForServer(ui_pings[i]);
					if (info)
					{
						info->special |= SS_KEEPINFO;
						Master_QueryServer(info);
					}
					break;
				}
		}
		else if (!strncmp(VM_POINTER(arg[1]), "localservers", 12))
		{
			MasterInfo_Begin();
		}
/*		else if (!strncmp(VM_POINTER(arg[1]), "r_vidmode", 12))
		{
			MasterInfo_Begin();
		}
*/		else
			Cbuf_AddText(VM_POINTER(arg[1]), RESTRICT_SERVER);
		break;

	case UI_FS_FOPENFILE: //fopen
		if ((int)arg[1] + 4 >= mask || VM_POINTER(arg[1]) < offset)
			break;	//out of bounds.
		VM_LONG(ret) = VMUI_fopen(VM_POINTER(arg[0]), VM_POINTER(arg[1]), VM_LONG(arg[2]), 0);
		break;

	case UI_FS_READ:	//fread
		if ((int)arg[0] + VM_LONG(arg[1]) >= mask || VM_POINTER(arg[0]) < offset)
			break;	//out of bounds.

		VM_LONG(ret) = VMUI_FRead(VM_POINTER(arg[0]), VM_LONG(arg[1]), VM_LONG(arg[2]), 0);
		break;
	case UI_FS_WRITE:	//fwrite
		break;
	case UI_FS_FCLOSEFILE:	//fclose
		VMUI_fclose(VM_LONG(arg[0]), 0);
		break;

	case UI_FS_GETFILELIST:	//fs listing
		if ((int)arg[2] + arg[3] >= mask || VM_POINTER(arg[2]) < offset)
			break;	//out of bounds.
		return VMQ3_GetFileList(VM_POINTER(arg[0]), VM_POINTER(arg[1]), VM_POINTER(arg[2]), VM_LONG(arg[3]));

	case UI_R_REGISTERMODEL:	//precache model
		{
			char *name = VM_POINTER(arg[0]);
			VM_LONG(ret) = VM_TOHANDLE(Mod_ForName(name, false));
		}
		break;
	case UI_R_MODELBOUNDS:
		{
			VALIDATEPOINTER(arg[1], sizeof(vec3_t));
			VALIDATEPOINTER(arg[2], sizeof(vec3_t));
			{
				model_t *mod = VM_FROMHANDLE(arg[0]);
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
		{
			char *buf;
			char *skinname = VM_POINTER(arg[0]);
			buf = Z_TagMalloc(strlen(skinname)+1, UITAGNUM);
			strcpy(buf, skinname);
			VM_LONG(ret) = (int)buf;	//precache skin - engine ignores these anyway... (for now)
		}
		break;

	case UI_R_REGISTERFONT:	//register font
		UI_RegisterFont(VM_POINTER(arg[0]), arg[1], VM_POINTER(arg[2]));
		break;
	case UI_R_REGISTERSHADERNOMIP:
		if (!*(char*)VM_POINTER(arg[0]))
			VM_LONG(ret) = 0;
		else if (qrenderer == QR_OPENGL)
			VM_LONG(ret) = (long)R_RegisterPic(VM_POINTER(arg[0]));
		else
			VM_LONG(ret) = (long)Draw_SafeCachePic(VM_POINTER(arg[0]));
		break;

	case UI_R_CLEARSCENE:	//clear scene
		cl_numvisedicts=0;
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
		if (Draw_ImageColours)
		{
			float *fl =VM_POINTER(arg[0]);
			if (!fl)
				Draw_ImageColours(1, 1, 1, 1);
			else
				Draw_ImageColours(fl[0], fl[1], fl[2], fl[3]);
		}
		break;

	case UI_R_DRAWSTRETCHPIC:
//		qglDisable(GL_ALPHA_TEST);
//		qglEnable(GL_BLEND);
//		GL_TexEnv(GL_MODULATE);
		if (qrenderer == QR_OPENGL)
			GLDraw_ShaderImage(VM_FLOAT(arg[0]), VM_FLOAT(arg[1]), VM_FLOAT(arg[2]), VM_FLOAT(arg[3]), VM_FLOAT(arg[4]), VM_FLOAT(arg[5]), VM_FLOAT(arg[6]), VM_FLOAT(arg[7]), (void *)VM_LONG(arg[8]));
		else
			Draw_Image(VM_FLOAT(arg[0]), VM_FLOAT(arg[1]), VM_FLOAT(arg[2]), VM_FLOAT(arg[3]), VM_FLOAT(arg[4]), VM_FLOAT(arg[5]), VM_FLOAT(arg[6]), VM_FLOAT(arg[7]), (mpic_t *)VM_LONG(arg[8]));
		break;

	case UI_CM_LERPTAG:	//Lerp tag...
	//	tag, model, startFrame, endFrame, frac, tagName
		if ((int)arg[0] + sizeof(float)*12 >= mask || VM_POINTER(arg[0]) < offset)
			break;	//out of bounds.
		VM_LerpTag(VM_POINTER(arg[0]), (model_t*)VM_LONG(arg[1]), VM_LONG(arg[2]), VM_LONG(arg[3]), VM_FLOAT(arg[4]), VM_POINTER(arg[5]));
		break;

	case UI_S_REGISTERSOUND:
		{
			sfx_t *sfx;
			sfx = S_PrecacheSound(va("../%s", VM_POINTER(arg[0])));
			if (sfx)
				VM_LONG(ret) = sfx->name - (char *)offset;
			else
				VM_LONG(ret) = -1;
		}
		break;
	case UI_S_STARTLOCALSOUND:
		if (VM_LONG(arg[0]) != -1 && arg[0])
			S_LocalSound(VM_LONG(arg[0])+(char *)offset);
		break;

	case UI_KEY_GETOVERSTRIKEMODE:
		return true;

	case UI_KEY_KEYNUMTOSTRINGBUF:
		if (VM_LONG(arg[0]) < 0 || VM_LONG(arg[0]) > 255 || (int)arg[1] + VM_LONG(arg[2]) >= mask || VM_POINTER(arg[1]) < offset || VM_LONG(arg[2]) < 1)
			break;	//out of bounds.

		Q_strncpyz(VM_POINTER(arg[1]), Key_KeynumToString(VM_LONG(arg[0])), VM_LONG(arg[2]));
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
			extern qboolean	keydown[256];
			if (keydown[VM_LONG(arg[0])])
				VM_LONG(ret) = 1;
			else
				VM_LONG(ret) = 0;
		}
		break;

	case UI_KEY_CLEARSTATES:
		Key_ClearStates();
		break;
	case UI_KEY_GETCATCHER:
		if (key_dest == key_console)
			VM_LONG(ret) = keycatcher | 1;
		else
			VM_LONG(ret) = keycatcher;
		break;
	case UI_KEY_SETCATCHER:
		keycatcher = VM_LONG(arg[0]);
		break;

	case UI_GETGLCONFIG:	//get glconfig
		if ((int)arg[0] + 11332/*sizeof(glconfig_t)*/ >= mask || VM_POINTER(arg[0]) < offset)
			break;	//out of bounds.

		//do any needed work
		memset(VM_POINTER(arg[0]), 0, 11304);
		*(int *)VM_POINTER(arg[0]+11304) = vid.width;
		*(int *)VM_POINTER(arg[0]+11308) = vid.height;
		*(float *)VM_POINTER(arg[0]+11312) = (float)vid.width/vid.height;
		memset(VM_POINTER(arg[0]+11316), 0, 11332-11316);
		break;

	case UI_GETCLIENTSTATE:	//get client state
		//fixme: we need to fill in a structure.
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
		if (VM_LONG(arg[0])>= 0 && VM_LONG(arg[0]) <= MAX_PINGREQUESTS)
			ui_pings[VM_LONG(arg[0])].type = NA_INVALID;
		break;
	case UI_LAN_GETPING:
		//void (int pingnum, char *buffer, int buflen, int *ping)
		if ((int)arg[1] + VM_LONG(arg[2]) >= mask || VM_POINTER(arg[1]) < offset)
			break;	//out of bounds.
		if ((int)arg[3] + sizeof(int) >= mask || VM_POINTER(arg[3]) < offset)
			break;	//out of bounds.

		NET_CheckPollSockets();
		if (VM_LONG(arg[0])>= 0 && VM_LONG(arg[0]) <= MAX_PINGREQUESTS)
		{
			char *buf = VM_POINTER(arg[1]);
			char *adr;
			serverinfo_t *info = Master_InfoForServer(ui_pings[VM_LONG(arg[0])]);
			if (info)
			{
				adr = NET_AdrToString(info->adr);
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

		NET_CheckPollSockets();
		if (VM_LONG(arg[0])>= 0 && VM_LONG(arg[0]) <= MAX_PINGREQUESTS)
		{
			char *buf = VM_POINTER(arg[1]);
			char *adr;
			serverinfo_t *info = Master_InfoForServer(ui_pings[VM_LONG(arg[0])]);
			if (info)
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

	case UI_CVAR_REGISTER:
		if ((int)arg[0] + sizeof(vmcvar_t) >= mask || VM_POINTER(arg[0]) < offset)
			break;	//out of bounds.
		{
			vmcvar_t *vmc;
			cvar_t *var;
			vmc = VM_POINTER(arg[0]);
			var = Cvar_Get(VM_POINTER(arg[1]), VM_POINTER(arg[2]), 0/*VM_LONG(arg[3])*/, "UI cvar");
			if (!vmc)
				break;
			vmc->handle = (char *)var - (char *)offset;

			vmc->integer = var->value;
			vmc->value = var->value;
			vmc->modificationCount = var->modified;
			Q_strncpyz(vmc->string, var->string, sizeof(vmc->string));
		}
	case UI_CVAR_UPDATE:
		if ((int)arg[0] + sizeof(vmcvar_t) >= mask || VM_POINTER(arg[0]) < offset)
			break;	//out of bounds.
		{
			cvar_t *var;
			vmcvar_t *vmc;
			vmc = VM_POINTER(arg[0]);
			var = (cvar_t *)((int)vmc->handle + (char *)offset);

			vmc->integer = var->value;
			vmc->value = var->value;
			vmc->modificationCount = var->modified;
			Q_strncpyz(vmc->string, var->string, sizeof(vmc->string));
		}
		break;

	case UI_MEMORY_REMAINING:
		VM_LONG(ret) = Hunk_LowMemAvailable();
		break;

	case UI_GET_CDKEY:	//get cd key
		if ((int)arg[0] + VM_LONG(arg[1]) >= mask || VM_POINTER(arg[0]) < offset)
			break;	//out of bounds.
		strncpy(VM_POINTER(arg[0]), Cvar_VariableString("cl_cdkey"), VM_LONG(arg[1]));
		break;
	case UI_SET_CDKEY:	//set cd key
		if ((int)arg[0] + strlen(VM_POINTER(arg[0])) >= mask || VM_POINTER(arg[0]) < offset)
			break;	//out of bounds.
		{
			cvar_t *cvar;
			cvar = Cvar_Get("cl_cdkey", "", 0, "Quake3 auth");
			Cvar_Set(cvar, VM_POINTER(arg[0]));
		}
		break;

	case UI_REAL_TIME:
		VM_FLOAT(ret) = realtime;
		break;

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
				adr = NET_AdrToString(info->adr);
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

	case UI_VERIFY_CDKEY:
		VM_LONG(ret) = true;
		break;

	case UI_SET_PBCLSTATUS:
		break;

// standard Q3
	case UI_MEMSET:
		if ((int)arg[0] + arg[2] >= mask || VM_POINTER(arg[0]) < offset)
			break;	//out of bounds.
		memset(VM_POINTER(arg[0]), arg[1], arg[2]);
		break;
	case UI_MEMCPY:
		if ((int)arg[0] + arg[2] >= mask || VM_POINTER(arg[0]) < offset)
			break;	//out of bounds.
		memcpy(VM_POINTER(arg[0]), VM_POINTER(arg[1]), arg[2]);
		break;
	case UI_STRNCPY:
		if (arg[0] + arg[2] >= mask || VM_POINTER(arg[0]) < offset)
			break;	//out of bounds.
		Q_strncpyS(VM_POINTER(arg[0]), VM_POINTER(arg[1]), arg[2]);
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

	case UI_CACHE_PIC:
		if (!Draw_SafeCachePic)
			VM_LONG(ret) = 0;
		else
		{
			/*VM_LONG(ret) = (long)*/Draw_SafeCachePic(VM_POINTER(arg[0]));
			VM_LONG(ret) = (long)R_RegisterPic(VM_POINTER(arg[0]));
		}
		break;
	case UI_PICFROMWAD:
		if (!Draw_SafePicFromWad)
			VM_LONG(ret) = 0;
		else
		{
			/*VM_LONG(ret) = (long)*/Draw_SafePicFromWad(VM_POINTER(arg[0]));
			VM_LONG(ret) = (long)R_RegisterPic(VM_POINTER(arg[0]));
		}
		break;
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
#ifdef SWQUAKE
			vi->bpp = r_pixbytes;
#endif
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
		Con_Printf("Q3UI: Not implemented system trap: %d\n", fn);
		return 0;
	}

	return ret;
}

#ifdef _DEBUG
static long UI_SystemCallsExWrapper(void *offset, unsigned int mask, int fn, const long *arg)
{	//this is so we can use edit and continue properly (vc doesn't like function pointers for edit+continue)
	return UI_SystemCallsEx(offset, mask, fn, arg);
}
#define UI_SystemCallsEx UI_SystemCallsExWrapper
#endif

//I'm not keen on this.
//but dlls call it without saying what sort of vm it comes from, so I've got to have them as specifics
static int EXPORT_FN UI_SystemCalls(int arg, ...)
{
	long args[9];
	va_list argptr;

	va_start(argptr, arg);
	args[0]=va_arg(argptr, int);
	args[1]=va_arg(argptr, int);
	args[2]=va_arg(argptr, int);
	args[3]=va_arg(argptr, int);
	args[4]=va_arg(argptr, int);
	args[5]=va_arg(argptr, int);
	args[6]=va_arg(argptr, int);
	args[7]=va_arg(argptr, int);
	args[8]=va_arg(argptr, int);
	va_end(argptr);

	return UI_SystemCallsEx(NULL, ~0, arg, args);
}

qboolean UI_DrawStatusBar(int scores)
{
	if (!uivm)
		return false;

	return VM_Call(uivm, UI_DRAWSTATUSBAR, scores);
}

qboolean UI_DrawFinale(void)
{
	if (!uivm)
		return false;

	return VM_Call(uivm, UI_FINALE);
}

qboolean UI_DrawIntermission(void)
{
	if (!uivm)
		return false;

	return VM_Call(uivm, UI_INTERMISSION);
}

void UI_DrawMenu(void)
{
	if (uivm)
	{
		VM_Call(uivm, UI_REFRESH, (int)(realtime * 1000));
		if (keycatcher&2 && key_dest != key_console)
			key_dest = key_game;
	}
}

qboolean UI_CenterPrint(char *text, qboolean finale)
{
	scr_centerstring = text;

	if (!uivm)
		return false;

	return VM_Call(uivm, UI_STRINGCHANGED, SID_CENTERPRINTTEXT);
}

qboolean UI_Q2LayoutChanged(void)
{
	if (!uivm)
		return false;

	return VM_Call(uivm, UI_STRINGCHANGED, SID_CENTERPRINTTEXT);
}

void UI_StringChanged(int num)
{
	if (uivm)
		VM_Call(uivm, UI_STRINGCHANGED, num);
}

void UI_Reset(void)
{
	keycatcher &= ~2;

	if (!Draw_SafeCachePic || qrenderer != QR_OPENGL)	//no renderer loaded
		UI_Stop();
	else if (uivm)
		VM_Call(uivm, UI_INIT);
}

int UI_MenuState(void)
{
	if (key_dest == key_menu)
	{
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

qboolean UI_KeyPress(int key, qboolean down)
{
	extern qboolean	keydown[256];
	extern int		keyshift[256];		// key to map to if shift held down in console
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
				Media_PlayFilm("");
			}

			if (cls.state)
				VM_Call(uivm, UI_SET_ACTIVE_MENU, 2)>0;
			else
				VM_Call(uivm, UI_SET_ACTIVE_MENU, 1)>0;

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

void UI_MousePosition(int xpos, int ypos)
{
	if (uivm && (ox != xpos || oy != ypos))
	{
		if (xpos < 0)
			xpos = 0;
		if (ypos < 0)
			ypos = 0;
		if (xpos > vid.width)
			xpos = vid.width;
		if (ypos > vid.height)
			ypos = vid.height;
		ox=0;oy=0;
		//force a cap
		VM_Call(uivm, UI_MOUSE_EVENT, -32767, -32767);
		VM_Call(uivm, UI_MOUSE_EVENT, (xpos-ox)*640/vid.width, (ypos-oy)*480/vid.height);
		ox = xpos;
		oy = ypos;

	}
}

void UI_Stop (void)
{
	keycatcher &= ~2;
	if (uivm)
	{
		VM_Call(uivm, UI_SHUTDOWN);
		VM_Destroy(uivm);
		VMUI_fcloseall(0);
		uivm = NULL;
	}
}

void UI_Start (void)
{
	int apiversion;
	if (!Draw_SafeCachePic)	//no renderer loaded
		return;

	if (qrenderer != QR_OPENGL && qrenderer != QR_DIRECT3D)
		return;

	uivm = VM_Create(NULL, "vm/qwui", UI_SystemCalls, UI_SystemCallsEx);
	if (!uivm)	//broken currently, I believe.
		uivm = VM_Create(NULL, "vm/ui", UI_SystemCalls, UI_SystemCallsEx);
	if (uivm)
	{
		apiversion = VM_Call(uivm, UI_GETAPIVERSION, UI_API_VERSION);
		if (apiversion == UI_API_VERSION)
			keycatcher = 0;
		else if (apiversion != 4 && apiversion != 6)	//make sure we can run the thing
		{
			Con_Printf("User-Interface VM uses incompatable API version (%i)\n", apiversion);
			VM_Destroy(uivm);
			VMUI_fcloseall(0);
			uivm = NULL;
			return;
		}
		VM_Call(uivm, UI_INIT);

		VM_Call(uivm, UI_MOUSE_EVENT, -32767, -32767);
		ox = 0;
		oy = 0;

		VM_Call(uivm, UI_SET_ACTIVE_MENU, 1);
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

qboolean UI_Command(void)
{
	if (uivm)
		return VM_Call(uivm, UI_CONSOLE_COMMAND);
	return false;
}

void UI_Init (void)
{
	Cmd_AddRemCommand("ui_restart", UI_Restart_f);
	UI_Start();
}
#endif

