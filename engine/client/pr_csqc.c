#include "quakedef.h"

#ifdef CSQC_DAT

#ifdef RGLQUAKE
#include "glquake.h"	//evil to include this
#endif

progfuncs_t *csqcprogs;

typedef struct {
//These are the functions the engine will call to, found by name.
	func_t init_function;
	func_t shutdown_function;
	func_t draw_function;
	func_t keydown_function;
	func_t keyup_function;
	func_t parse_stuffcmd;
	func_t parse_centerprint;

	func_t ent_update;
	func_t ent_remove;

//These are pointers to the csqc's globals.
	float *time;				//float		Written before entering most qc functions
	int	*self;					//entity	Written before entering most qc functions

	float *forward;				//vector	written by anglevectors
	float *right;				//vector	written by anglevectors
	float *up;					//vector	written by anglevectors

	float *trace_allsolid;		//bool		written by traceline
	float *trace_startsolid;	//bool		written by traceline
	float *trace_fraction;		//float		written by traceline
	float *trace_inwater;		//bool		written by traceline
	float *trace_inopen;		//bool		written by traceline
	float *trace_endpos;		//vector	written by traceline
	float *trace_plane_normal;	//vector	written by traceline
	float *trace_plane_dist;	//float		written by traceline
	int *trace_ent;				//entity	written by traceline
} csqcglobals_t;
static csqcglobals_t csqcg;



void CSQC_FindGlobals(void)
{
	csqcg.time = (float*)PR_FindGlobal(csqcprogs, "time", 0);
	if (csqcg.time)
		*csqcg.time = Sys_DoubleTime();

	csqcg.self = (int*)PR_FindGlobal(csqcprogs, "self", 0);


	csqcg.forward = (float*)PR_FindGlobal(csqcprogs, "v_forward", 0);
	csqcg.right = (float*)PR_FindGlobal(csqcprogs, "v_right", 0);
	csqcg.up = (float*)PR_FindGlobal(csqcprogs, "v_up", 0);


	csqcg.init_function	= PR_FindFunction(csqcprogs, "CSQC_Init",	PR_ANY);
	csqcg.shutdown_function	= PR_FindFunction(csqcprogs, "CSQC_Shutdown",	PR_ANY);
	csqcg.draw_function	= PR_FindFunction(csqcprogs, "CSQC_UpdateView",	PR_ANY);
	csqcg.keydown_function	= PR_FindFunction(csqcprogs, "CSQC_KeyDown",	PR_ANY);
	csqcg.keyup_function	= PR_FindFunction(csqcprogs, "CSQC_KeyUp",	PR_ANY);

	csqcg.parse_stuffcmd	= PR_FindFunction(csqcprogs, "CSQC_Parse_StuffCmd",	PR_ANY);
	csqcg.parse_centerprint	= PR_FindFunction(csqcprogs, "CSQC_Parse_CenterPrint",	PR_ANY);

	csqcg.ent_update	= PR_FindFunction(csqcprogs, "CSQC_Ent_Update",	PR_ANY);
	csqcg.ent_remove	= PR_FindFunction(csqcprogs, "CSQC_Ent_Remove",	PR_ANY);
}



//this is the list for all the csqc fields.
//(the #define is so the list always matches the ones pulled out)
#define csqcfields	\
	fieldfloat(modelindex);	\
	fieldvector(origin);	\
	fieldvector(angles);	\
	fieldfloat(alpha);		/*transparency*/	\
	fieldfloat(scale);		/*model scale*/		\
	fieldfloat(fatness);	/*expand models X units along thier normals.*/	\
	fieldfloat(skin);		\
	fieldfloat(colormap);	\
	fieldfloat(frame);		\
	fieldfloat(oldframe);	\
	fieldfloat(lerpfrac);	\
							\
	fieldfloat(drawmask);	/*So that the qc can specify all rockets at once or all bannanas at once*/	\
	fieldfunction(predraw);	/*If present, is called just before it's drawn.*/	\
							\
	fieldstring(model);


//note: doesn't even have to match the clprogs.dat :)
typedef struct {
#define fieldfloat(name) float name
#define fieldvector(name) vec3_t name
#define fieldentity(name) int name
#define fieldstring(name) string_t name
#define fieldfunction(name) func_t name
csqcfields
#undef fieldfloat
#undef fieldvector
#undef fieldentity
#undef fieldstring
#undef fieldfunction
} csqcentvars_t;

typedef struct csqcedict_s
{
	qboolean	isfree;
	float		freetime; // sv.time when the object was freed
	int			entnum;
	qboolean	readonly;	//world
	
	csqcentvars_t	v;
} csqcedict_t;

csqcedict_t *csqc_edicts;	//consider this 'world'


void CSQC_InitFields(void)
{	//CHANGING THIS FUNCTION REQUIRES CHANGES TO csqcentvars_t
#define fieldfloat(name) PR_RegisterFieldVar(csqcprogs, ev_float, #name, (int)&((csqcedict_t*)0)->v.name - (int)&((csqcedict_t*)0)->v, -1)
#define fieldvector(name) PR_RegisterFieldVar(csqcprogs, ev_vector, #name, (int)&((csqcedict_t*)0)->v.name - (int)&((csqcedict_t*)0)->v, -1)
#define fieldentity(name) PR_RegisterFieldVar(csqcprogs, ev_entity, #name, (int)&((csqcedict_t*)0)->v.name - (int)&((csqcedict_t*)0)->v, -1)
#define fieldstring(name) PR_RegisterFieldVar(csqcprogs, ev_string, #name, (int)&((csqcedict_t*)0)->v.name - (int)&((csqcedict_t*)0)->v, -1)
#define fieldfunction(name) PR_RegisterFieldVar(csqcprogs, ev_function, #name, (int)&((csqcedict_t*)0)->v.name - (int)&((csqcedict_t*)0)->v, -1)
csqcfields
#undef fieldfloat
#undef fieldvector
#undef fieldentity
#undef fieldstring
#undef fieldfunction
}

csqcedict_t *csqcent[MAX_EDICTS];

#define	RETURN_SSTRING(s) (*(char **)&((int *)pr_globals)[OFS_RETURN] = PR_SetString(prinst, s))	//static - exe will not change it.
char *PF_TempStr(void);

int csqcentsize;

//pr_cmds.c builtins that need to be moved to a common.
void VARGS PR_BIError(progfuncs_t *progfuncs, char *format, ...);
void PF_cvar_string (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_cvar_set (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_dprint (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_error (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_rint (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_floor (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_ceil (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_Tokenize  (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_ArgV  (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_FindString (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_FindFloat (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_nextent (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_randomvec (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_Sin (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_Cos (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_Sqrt (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_bound (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_strlen(progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_strcat (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_ftos (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_fabs (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_vtos (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_etos (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_stof (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_mod (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_substring (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_stov (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_dupstring(progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_forgetstring(progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_Spawn (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_min (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_max (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_registercvar (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_pow (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_chr2str (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_localcmd (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_random (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_fopen (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_fclose (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_fputs (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_fgets (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_normalize (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_vlen (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_vectoyaw (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_vectoangles (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_findchain (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_findchainfloat (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_coredump (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_traceon (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_traceoff (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_eprint (progfuncs_t *prinst, struct globalvars_s *pr_globals);

void PF_strstrofs (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_str2chr (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_chr2str (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_strconv (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_infoadd (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_infoget (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_strncmp (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_strcasecmp (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_strncasecmp (progfuncs_t *prinst, struct globalvars_s *pr_globals);

//these functions are from pr_menu.dat
void PF_CL_is_cached_pic (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_CL_precache_pic (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_CL_free_pic (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_CL_drawcharacter (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_CL_drawstring (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_CL_drawpic (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_CL_drawfill (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_CL_drawsetcliparea (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_CL_drawresetcliparea (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_CL_drawgetimagesize (progfuncs_t *prinst, struct globalvars_s *pr_globals);

#define MAXTEMPBUFFERLEN	1024

void PF_fclose_progs (progfuncs_t *prinst);
char *PF_VarString (progfuncs_t *prinst, int	first, struct globalvars_s *pr_globals);


static void PF_Remove_ (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *ed;
	
	ed = (csqcedict_t*)G_EDICT(prinst, OFS_PARM0);

	if (ed->isfree)
	{
		Con_DPrintf("CSQC Tried removing free entity\n");
		return;
	}

	ED_Free (prinst, (void*)ed);
}

static void PF_cvar (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	cvar_t	*var;
	char	*str;

	str = PR_GetStringOfs(prinst, OFS_PARM0);
	{
		var = Cvar_Get(str, "", 0, "csqc cvars");
		if (var)
			G_FLOAT(OFS_RETURN) = var->value;
		else
			G_FLOAT(OFS_RETURN) = 0;
	}
}

//too specific to the prinst's builtins.
static void PF_Fixme (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	Con_Printf("\n");

	prinst->RunError(prinst, "\nBuiltin %i not implemented.\nMenu is not compatable.", prinst->lastcalledbuiltinnumber);
	PR_BIError (prinst, "bulitin not implemented");
}

static void PF_csqc_centerprint (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *str = PF_VarString(prinst, 0, pr_globals);
	SCR_CenterPrint(0, str);
}

static void PF_makevectors (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (!csqcg.forward || !csqcg.right || !csqcg.up)
		Host_EndGame("PF_makevectors: one of v_forward, v_right or v_up was not defined\n");
	AngleVectors (G_VECTOR(OFS_PARM0), csqcg.forward, csqcg.right, csqcg.up);
}

static void PF_R_AddEntity(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *in = (void*)G_EDICT(prinst, OFS_PARM0);
	entity_t ent;
	int i;
	model_t *model;

	if (in->v.predraw)
	{
		int oldself = *csqcg.self;
		*csqcg.self = EDICT_TO_PROG(prinst, (void*)in);
		PR_ExecuteProgram(prinst, in->v.predraw);
		*csqcg.self = oldself;
	}

	i = in->v.modelindex;
	if (i == 0)
		return;
	else if (i > 0 && i < MAX_MODELS)
		model = cl.model_precache[i];
	else if (i < 0 && i > -MAX_CSQCMODELS)
		model = cl.model_csqcprecache[-i];
	else
		return; //there might be other ent types later as an extension that stop this.

	memset(&ent, 0, sizeof(ent));
	ent.model = model;

	if (!ent.model)
	{
		Con_Printf("PF_R_AddEntity: model wasn't precached!\n");
		return;
	}

	
	ent.frame = in->v.frame;
	ent.oldframe = in->v.oldframe;
	ent.lerpfrac = in->v.lerpfrac;

	ent.angles[0] = in->v.angles[0];
	ent.angles[1] = in->v.angles[1];
	ent.angles[2] = in->v.angles[2];
	memcpy(ent.origin, in->v.origin, sizeof(vec3_t));
	AngleVectors(ent.angles, ent.axis[0], ent.axis[1], ent.axis[2]);
	VectorInverse(ent.axis[1]);

	ent.alpha = in->v.alpha;
	ent.scale = in->v.scale;

	V_AddEntity(&ent);
}

static void PF_R_AddDynamicLight(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *org = G_VECTOR(OFS_PARM0);
	float radius = G_FLOAT(OFS_PARM1);
	float *rgb = G_VECTOR(OFS_PARM2);
	V_AddLight(org, radius, rgb[0]/5, rgb[1]/5, rgb[2]/5);
}

#define MASK_ENGINE 1
static void PF_R_AddEntityMask(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int mask = G_FLOAT(OFS_PARM0);
	csqcedict_t *ent;
	int e;

	for (e=1; e < *prinst->parms->sv_num_edicts; e++)
	{
		ent = (void*)EDICT_NUM(prinst, e);
		if (ent->isfree)
			continue;

		if ((int)ent->v.drawmask & mask)
		{
			G_INT(OFS_PARM0) = EDICT_TO_PROG(prinst, (void*)ent);
			PF_R_AddEntity(prinst, pr_globals);
		}
	}

	if (mask & MASK_ENGINE && cl.worldmodel)
	{
		CL_LinkViewModel ();
		CL_LinkPlayers ();
		CL_LinkPacketEntities ();
		CL_LinkProjectiles ();
		CL_UpdateTEnts ();
	}
}

//float CalcFov (float fov_x, float width, float height);
//clear scene, and set up the default stuff.
static void PF_R_ClearScene (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	extern frame_t		*view_frame;
	extern player_state_t		*view_message;

	CL_DecayLights ();

	if (cl.worldmodel)
	{
		//work out which packet entities are solid
		CL_SetSolidEntities ();

		// Set up prediction for other players
		CL_SetUpPlayerPrediction(false);

		// do client side motion prediction
		CL_PredictMove ();

		// Set up prediction for other players
		CL_SetUpPlayerPrediction(true);
	}

	CL_SwapEntityLists();

	view_frame = &cl.frames[cls.netchan.incoming_sequence & UPDATE_MASK];
	view_message = &view_frame->playerstate[cl.playernum[0]];
	V_CalcRefdef(0);	//set up the defaults (for player 0)
	/*
	VectorCopy(cl.simangles[0], r_refdef.viewangles);
	VectorCopy(cl.simorg[0], r_refdef.vieworg);
	r_refdef.flags = 0;

	r_refdef.vrect.x = 0;
	r_refdef.vrect.y = 0;
	r_refdef.vrect.width = vid.width;
	r_refdef.vrect.height = vid.height;

	r_refdef.fov_x = scr_fov.value;
	r_refdef.fov_y = CalcFov (r_refdef.fov_x, r_refdef.vrect.width, r_refdef.vrect.height);
	*/
}

static void PF_R_SetViewFlag(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *s = PR_GetStringOfs(prinst, OFS_PARM0);
	float *p = G_VECTOR(OFS_PARM1);
	switch(*s)
	{
	case 'F':
		if (!strcmp(s, "FOV"))	//set both fov numbers
		{
			r_refdef.fov_x = p[0];
			r_refdef.fov_y = p[1];
			return;
		}
		if (!strcmp(s, "FOV_X"))
		{
			r_refdef.fov_x = *p;
			return;
		}
		if (!strcmp(s, "FOV_Y"))
		{
			r_refdef.fov_y = *p;
			return;
		}
		break;
	case 'O':
		if (!strcmp(s, "ORIGIN"))
		{
			VectorCopy(p, r_refdef.vieworg);
			return;
		}
		if (!strcmp(s, "ORIGIN_X"))
		{
			r_refdef.vieworg[0] = *p;
			return;
		}
		if (!strcmp(s, "ORIGIN_Y"))
		{
			r_refdef.vieworg[1] = *p;
			return;
		}
		if (!strcmp(s, "ORIGIN_Z"))
		{
			r_refdef.vieworg[2] = *p;
			return;
		}
		break;
		
	case 'A':
		if (!strcmp(s, "ANGLES"))
		{
			VectorCopy(p, r_refdef.viewangles);
			return;
		}
		if (!strcmp(s, "ANGLES_X"))
		{
			r_refdef.viewangles[0] = *p;
			return;
		}
		if (!strcmp(s, "ANGLES_Y"))
		{
			r_refdef.viewangles[1] = *p;
			return;
		}
		if (!strcmp(s, "ANGLES_Z"))
		{
			r_refdef.viewangles[2] = *p;
			return;
		}
		break;
		
	case 'W':
		if (!strcmp(s, "WIDTH"))
		{
			r_refdef.vrect.width = *p;
			return;
		}
		break;
	case 'H':
		if (!strcmp(s, "HEIGHT"))
		{
			r_refdef.vrect.height = *p;
			return;
		}
		break;
	case 'S':
		if (!strcmp(s, "SIZE"))
		{
			r_refdef.vrect.width = p[0];
			r_refdef.vrect.height = p[1];
			return;
		}
		break;
	case 'M':
		if (!strcmp(s, "MIN_X"))
		{
			r_refdef.vrect.x = *p;
			return;
		}
		if (!strcmp(s, "MIN_Y"))
		{
			r_refdef.vrect.y = *p;
			return;
		}
		if (!strcmp(s, "MIN"))
		{
			r_refdef.vrect.x = p[0];
			r_refdef.vrect.y = p[1];
			return;
		}
		break;
	default:
		break;
	}
	Con_DPrintf("SetViewFlag: %s not recognised\n", s);
}

static void PF_R_RenderScene(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (cl.worldmodel)
		R_PushDlights ();

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
}

static void PF_cs_getstatf(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int stnum = G_FLOAT(OFS_PARM0);
	float val = *(float*)&cl.stats[0][stnum];	//copy float into the stat
	G_FLOAT(OFS_RETURN) = val;
}
static void PF_cs_getstati(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{	//convert an int stat into a qc float.

	int stnum = G_FLOAT(OFS_PARM0);
	int val = cl.stats[0][stnum];
	if (*prinst->callargc > 1)
	{
		int first, count;
		first = G_FLOAT(OFS_PARM1);
		count = G_FLOAT(OFS_PARM2);
		G_FLOAT(OFS_RETURN) = (((unsigned int)val)&(((1<<count)-1)<<first))>>first;
	}
	else
		G_FLOAT(OFS_RETURN) = val;
}
static void PF_cs_getstats(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int stnum = G_FLOAT(OFS_PARM0);
	char *out;

	out = PF_TempStr();

	//the network protocol byteswaps

	((unsigned int*)out)[0] = LittleLong(cl.stats[0][stnum+0]);
	((unsigned int*)out)[1] = LittleLong(cl.stats[0][stnum+1]);
	((unsigned int*)out)[2] = LittleLong(cl.stats[0][stnum+2]);
	((unsigned int*)out)[3] = LittleLong(cl.stats[0][stnum+3]);
	((unsigned int*)out)[4] = 0;	//make sure it's null terminated

	RETURN_SSTRING(out);
}

static void PF_CSQC_SetOrigin(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *ent = (void*)G_EDICT(prinst, OFS_PARM0);
	float *org = G_VECTOR(OFS_PARM1);
	VectorCopy(org, ent->v.origin);

	//fixme: add some sort of fast area grid
}

//FIXME: Not fully functional
static void PF_CSQC_traceline(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float	*v1, *v2, *mins, *maxs;
	trace_t	trace;
	int		nomonsters;
	edict_t	*ent;
//	int savedhull;

	v1 = G_VECTOR(OFS_PARM0);
	v2 = G_VECTOR(OFS_PARM1);
	nomonsters = G_FLOAT(OFS_PARM2);
	ent = G_EDICT(prinst, OFS_PARM3);

//	if (*prinst->callargc == 6)
//	{
//		mins = G_VECTOR(OFS_PARM4);
//		maxs = G_VECTOR(OFS_PARM5);
//	}
//	else
	{
		mins = vec3_origin;
		maxs = vec3_origin;
	}
/*
	savedhull = ent->v.hull;
	ent->v.hull = 0;
	trace = SV_Move (v1, mins, maxs, v2, nomonsters, ent);
	ent->v.hull = savedhull;
*/
	
	memset(&trace, 0, sizeof(trace));
	trace.fraction = 1;
	cl.worldmodel->hulls->funcs.RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, v1, v2, &trace);

	*csqcg.trace_allsolid = trace.allsolid;
	*csqcg.trace_startsolid = trace.startsolid;
	*csqcg.trace_fraction = trace.fraction;
	*csqcg.trace_inwater = trace.inwater;
	*csqcg.trace_inopen = trace.inopen;
	VectorCopy (trace.endpos, csqcg.trace_endpos);
	VectorCopy (trace.plane.normal, csqcg.trace_plane_normal);
	*csqcg.trace_plane_dist =  trace.plane.dist;	
//	if (trace.ent)
//		*csqcg.trace_ent = EDICT_TO_PROG(prinst, trace.ent);
//	else
		*csqcg.trace_ent = EDICT_TO_PROG(prinst, (void*)csqc_edicts);
}

static void PF_CSQC_pointcontents(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float	*v;
	int cont;
	
	v = G_VECTOR(OFS_PARM0);

	cont = cl.worldmodel->hulls[0].funcs.HullPointContents(&cl.worldmodel->hulls[0], v);
	if (cont & FTECONTENTS_SOLID)
		G_FLOAT(OFS_RETURN) = Q1CONTENTS_SOLID;
	else if (cont & FTECONTENTS_SKY)
		G_FLOAT(OFS_RETURN) = Q1CONTENTS_SKY;
	else if (cont & FTECONTENTS_LAVA)
		G_FLOAT(OFS_RETURN) = Q1CONTENTS_LAVA;
	else if (cont & FTECONTENTS_SLIME)
		G_FLOAT(OFS_RETURN) = Q1CONTENTS_SLIME;
	else if (cont & FTECONTENTS_WATER)
		G_FLOAT(OFS_RETURN) = Q1CONTENTS_WATER;
	else
		G_FLOAT(OFS_RETURN) = Q1CONTENTS_EMPTY;
}

static int FindModel(char *name, int *free)
{
	int i;

	*free = 0;

	for (i = 1; i < MAX_CSQCMODELS; i++)
	{
		if (!*cl.model_csqcname[i])
		{
			*free = -i;
			break;
		}
		if (!strcmp(cl.model_csqcname[i], name))
			return -i;
	}
	for (i = 1; i < MAX_MODELS; i++)
	{
		if (!strcmp(cl.model_name[i], name))
			return i;
	}
	return 0;
}
static void PF_CSQC_SetModel(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *ent = (void*)G_EDICT(prinst, OFS_PARM0);
	char *modelname = PR_GetStringOfs(prinst, OFS_PARM1);
	int freei;
	int modelindex = FindModel(modelname, &freei);

	if (!modelindex)
	{
		if (!freei)
			Host_EndGame("CSQC ran out of model slots\n");
		Con_DPrintf("Late caching model \"%s\"\n", modelname);
		Q_strncpyz(cl.model_csqcname[-freei], modelname, sizeof(cl.model_csqcname[-freei]));	//allocate a slot now
		modelindex = freei;

		cl.model_csqcprecache[-freei] = Mod_ForName(cl.model_csqcname[-freei], false);
	}

	ent->v.modelindex = modelindex;
	if (modelindex < 0)
		ent->v.model = PR_SetString(prinst, cl.model_csqcname[-modelindex]);
	else
		ent->v.model = PR_SetString(prinst, cl.model_name[modelindex]);
}
static void PF_CSQC_SetModelIndex(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *ent = (void*)G_EDICT(prinst, OFS_PARM0);
	int modelindex = G_FLOAT(OFS_PARM1);

	ent->v.modelindex = modelindex;
	if (modelindex < 0)
		ent->v.model = PR_SetString(prinst, cl.model_csqcname[-modelindex]);
	else
		ent->v.model = PR_SetString(prinst, cl.model_name[modelindex]);
}
static void PF_CSQC_PrecacheModel(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int modelindex, freei;
	char *modelname = PR_GetStringOfs(prinst, OFS_PARM0);
	int i;

	for (i = 1; i < MAX_MODELS; i++)	//Make sure that the server specified model is loaded..
	{
		if (!*cl.model_name[i])
			break;
		if (!strcmp(cl.model_name[i], modelname))
		{
			cl.model_precache[i] = Mod_ForName(cl.model_name[i], false);
			break;
		}
	}

	modelindex = FindModel(modelname, &freei);	//now load it

	if (!modelindex)
	{
		if (!freei)
			Host_EndGame("CSQC ran out of model slots\n");
		Q_strncpyz(cl.model_csqcname[-freei], modelname, sizeof(cl.model_csqcname[-freei]));	//allocate a slot now
		modelindex = freei;

		cl.model_csqcprecache[-freei] = Mod_ForName(cl.model_csqcname[-freei], false);
	}
}
static void PF_CSQC_PrecacheSound(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *soundname = PR_GetStringOfs(prinst, OFS_PARM0);
	S_PrecacheSound(soundname);
}

static void PF_CSQC_ModelnameForIndex(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int modelindex = G_FLOAT(OFS_PARM0);

	if (modelindex < 0)
		G_INT(OFS_RETURN) = (int)PR_SetString(prinst, cl.model_csqcname[-modelindex]);
	else
		G_INT(OFS_RETURN) = (int)PR_SetString(prinst, cl.model_name[modelindex]);
}

static void PF_ReadByte(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = MSG_ReadByte();
}

static void PF_ReadChar(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = MSG_ReadChar();
}

static void PF_ReadShort(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = MSG_ReadShort();
}

static void PF_ReadLong(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = MSG_ReadLong();
}

static void PF_ReadCoord(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = MSG_ReadCoord();
}

static void PF_ReadString(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *str = PF_TempStr();
	char *read = MSG_ReadString();

	Q_strncpyz(str, read, MAXTEMPBUFFERLEN);
}

static void PF_ReadAngle(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = MSG_ReadAngle();
}


static void PF_objerror (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*s;
	edict_t	*ed;
	
	s = PF_VarString(prinst, 0, pr_globals);
/*	Con_Printf ("======OBJECT ERROR in %s:\n%s\n", PR_GetString(pr_xfunction->s_name),s);
*/	ed = PROG_TO_EDICT(prinst, pr_global_struct->self);
/*	ED_Print (ed);
*/
	ED_Print(prinst, ed);
	Con_Printf("%s", s);

	if (developer.value)
		(*prinst->pr_trace) = 2;
	else
	{
		ED_Free (prinst, ed);

		prinst->AbortStack(prinst);
	
		PR_BIError (prinst, "Program error: %s", s);

		if (sv.time > 10)
			Cbuf_AddText("restart\n", RESTRICT_LOCAL);
	}
}

static void PF_cs_setsensativityscaler (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	in_sensitivityscale = G_FLOAT(OFS_PARM0);
}

#define PF_FixTen PF_Fixme,PF_Fixme,PF_Fixme,PF_Fixme,PF_Fixme,PF_Fixme,PF_Fixme,PF_Fixme,PF_Fixme,PF_Fixme

//warning: functions that depend on globals are bad, mkay?
builtin_t csqc_builtins[] = {
//0
	PF_Fixme,
	PF_makevectors,
	PF_CSQC_SetOrigin, //PF_setorigin
	PF_CSQC_SetModel, //PF_setmodel
	PF_Fixme, //PF_setsize
	PF_Fixme,
	PF_Fixme, //PF_break,
	PF_random,
	PF_Fixme, //PF_sound,
	PF_normalize,
//10
	PF_error,
	PF_objerror,
	PF_vlen,
	PF_vectoyaw,
	PF_Spawn,
	PF_Remove_, //PF_Remove,
	PF_CSQC_traceline, //PF_traceline,
	PF_Fixme, //PF_checkclient, (don't support)
	PF_FindString,
	PF_CSQC_PrecacheSound, //PF_precache_sound,
//20
	PF_CSQC_PrecacheModel, //PF_precache_model,
	PF_Fixme, //PF_stuffcmd, (don't support)
	PF_Fixme, //PF_findradius,
	PF_Fixme, //PF_bprint, (don't support)
	PF_Fixme, //PF_sprint,
	PF_dprint,
	PF_ftos,
	PF_vtos,
	PF_coredump,
	PF_traceon,
//30
	PF_traceoff,
	PF_eprint,
	PF_Fixme, //PF_walkmove, (don't support yet)
	PF_Fixme,
PF_Fixme, //PF_droptofloor,
PF_Fixme, //PF_lightstyle,
PF_rint,
PF_floor,
PF_ceil,
PF_Fixme,
//40
PF_Fixme, //PF_checkbottom,
PF_CSQC_pointcontents, //PF_pointcontents,
PF_Fixme,
PF_fabs,
PF_Fixme, //PF_aim,	hehehe... (don't support)
PF_cvar,
PF_localcmd,
PF_nextent,
PF_Fixme, //PF_particle,
PF_Fixme, //PF_changeyaw,
//50
PF_Fixme,
PF_vectoangles,

PF_ReadByte,
PF_ReadChar,
PF_ReadShort,
PF_ReadLong,
PF_ReadCoord,
PF_ReadAngle,
PF_ReadString,
PF_Fixme,//PF_ReadEntity,

//60
PF_Fixme,

PF_Sin,
PF_Cos,
PF_Sqrt,

PF_Fixme,

PF_Fixme,
PF_Fixme,
SV_MoveToGoal,
PF_Fixme, //PF_precache_file, (don't support)
PF_Fixme, //PF_makestatic,
//70
PF_Fixme, //PF_changelevel, (don't support)
PF_Fixme,
PF_cvar_set,
PF_Fixme, //PF_centerprint,
PF_Fixme, //PF_ambientsound,

PF_CSQC_PrecacheModel, //PF_precache_model,
PF_Fixme, //PF_precache_sound,
PF_Fixme, //PF_precache_file,
PF_Fixme, //PF_setspawnparms,
PF_Fixme, //PF_logfrag, (don't support)
//80
PF_Fixme, //PF_infokey,
PF_stof,
PF_Fixme, //PF_multicast, (don't support)
PF_Fixme,
PF_Fixme,

PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,
//90
PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,

PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,
//100
PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,

PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,
//110
PF_fopen,
PF_fclose,
PF_fgets,
PF_fputs,
PF_strlen,

PF_strcat,
PF_substring,
PF_stov,
PF_dupstring,
PF_forgetstring,


//120
PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,

PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,

//130
PF_R_ClearScene,
PF_R_AddEntityMask,
PF_R_AddEntity,
PF_R_SetViewFlag,
PF_R_RenderScene,

PF_R_AddDynamicLight,
PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,

//140
PF_CL_is_cached_pic,//0
PF_CL_precache_pic,//1
PF_CL_free_pic,//2
PF_CL_drawcharacter,//3
PF_CL_drawstring,//4
PF_CL_drawpic,//5
PF_CL_drawfill,//6
PF_CL_drawsetcliparea,//7
PF_CL_drawresetcliparea,//8
PF_CL_drawgetimagesize,//9

//150
PF_cs_getstatf,
PF_cs_getstati,
PF_cs_getstats,
PF_CSQC_SetModelIndex,
PF_CSQC_ModelnameForIndex,

PF_cs_setsensativityscaler,
PF_csqc_centerprint,
PF_print,
PF_Fixme,
PF_Fixme,

//160
PF_FixTen,

//170
PF_FixTen,

//180
PF_FixTen,

//190
PF_FixTen,

//200
PF_FixTen,

//210
PF_FixTen,

//220
PF_Fixme,
PF_strstrofs,
PF_str2chr,
PF_chr2str,
PF_strconv,

PF_infoadd,
PF_infoget,
PF_strncmp,
PF_strcasecmp,
PF_strncasecmp,

//230
PF_FixTen,

//240
PF_FixTen,

//250
PF_FixTen,

PF_FixTen,

//260
PF_FixTen,

//270
PF_FixTen,

//280
PF_FixTen,

//290
PF_FixTen,

//300
PF_FixTen,

//310
PF_FixTen,

//320
PF_FixTen,

//330
PF_FixTen,

//340
PF_FixTen,

//350
PF_FixTen,

//360
PF_FixTen,

//370
PF_FixTen,

//380
PF_FixTen,

//390
PF_FixTen,

//400
PF_FixTen,

//410
PF_FixTen,

//420
PF_FixTen,

//430
PF_FixTen,

//440
PF_Fixme,
PF_Tokenize,
PF_ArgV,
PF_Fixme,
PF_Fixme,

PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,
};
int csqc_numbuiltins = sizeof(csqc_builtins)/sizeof(csqc_builtins[0]);





jmp_buf csqc_abort;
progparms_t csqcprogparms;
int num_csqc_edicts;



int COM_FileSize(char *path);
pbool QC_WriteFile(char *name, void *data, int len);
void *VARGS PR_Malloc(int size);	//these functions should be tracked by the library reliably, so there should be no need to track them ourselves.
void VARGS PR_Free(void *mem);

//Any menu builtin error or anything like that will come here.
void VARGS CSQC_Abort (char *format, ...)	//an error occured.
{
	va_list		argptr;
	char		string[1024];

	va_start (argptr, format);
	_vsnprintf (string,sizeof(string)-1, format,argptr);
	va_end (argptr);

	Con_Printf("CSQC_Abort: %s\nShutting down csqc\n", string);


{
	static char buffer[1024*1024*8];
	int size = sizeof buffer;
	csqcprogs->save_ents(csqcprogs, buffer, &size, 3);
	COM_WriteFile("csqccore.txt", buffer, size);
}

	Host_EndGame("csqc error");
}

void CSQC_Shutdown(void)
{
	if (csqcprogs)
	{
		CloseProgs(csqcprogs);
		Con_Printf("Closed csqc\n");
	}
	csqcprogs = NULL;

	in_sensitivityscale = 1;
}

double  csqctime;
void CSQC_Init (void)
{
	CSQC_Shutdown();

	if (!qrenderer)
	{
		return;
	}

	memset(cl.model_csqcname, 0, sizeof(cl.model_csqcname));
	memset(cl.model_csqcprecache, 0, sizeof(cl.model_csqcprecache));

	csqcprogparms.progsversion = PROGSTRUCT_VERSION;
	csqcprogparms.ReadFile = COM_LoadStackFile;//char *(*ReadFile) (char *fname, void *buffer, int *len);
	csqcprogparms.FileSize = COM_FileSize;//int (*FileSize) (char *fname);	//-1 if file does not exist
	csqcprogparms.WriteFile = QC_WriteFile;//bool (*WriteFile) (char *name, void *data, int len);
	csqcprogparms.printf = (void *)Con_Printf;//Con_Printf;//void (*printf) (char *, ...);
	csqcprogparms.Sys_Error = Sys_Error;
	csqcprogparms.Abort = CSQC_Abort;
	csqcprogparms.edictsize = sizeof(csqcedict_t)-sizeof(csqcentvars_t);

	csqcprogparms.entspawn = NULL;//void (*entspawn) (struct edict_s *ent);	//ent has been spawned, but may not have all the extra variables (that may need to be set) set
	csqcprogparms.entcanfree = NULL;//bool (*entcanfree) (struct edict_s *ent);	//return true to stop ent from being freed
	csqcprogparms.stateop = NULL;//StateOp;//void (*stateop) (float var, func_t func);
	csqcprogparms.cstateop = NULL;//CStateOp;
	csqcprogparms.cwstateop = NULL;//CWStateOp;
	csqcprogparms.thinktimeop = NULL;//ThinkTimeOp;

	//used when loading a game
	csqcprogparms.builtinsfor = NULL;//builtin_t *(*builtinsfor) (int num);	//must return a pointer to the builtins that were used before the state was saved.
	csqcprogparms.loadcompleate = NULL;//void (*loadcompleate) (int edictsize);	//notification to reset any pointers.

	csqcprogparms.memalloc = PR_Malloc;//void *(*memalloc) (int size);	//small string allocation	malloced and freed randomly
	csqcprogparms.memfree = PR_Free;//void (*memfree) (void * mem);


	csqcprogparms.globalbuiltins = csqc_builtins;//builtin_t *globalbuiltins;	//these are available to all progs
	csqcprogparms.numglobalbuiltins = csqc_numbuiltins;

	csqcprogparms.autocompile = PR_NOCOMPILE;//enum {PR_NOCOMPILE, PR_COMPILENEXIST, PR_COMPILECHANGED, PR_COMPILEALWAYS} autocompile;

	csqcprogparms.gametime = &csqctime;

	csqcprogparms.sv_edicts = (edict_t **)&csqc_edicts;
	csqcprogparms.sv_num_edicts = &num_csqc_edicts;

	csqcprogparms.useeditor = NULL;//sorry... QCEditor;//void (*useeditor) (char *filename, int line, int nump, char **parms);

	csqctime = Sys_DoubleTime();
	if (!csqcprogs)
	{
		in_sensitivityscale = 1;
		csqcprogs = InitProgs(&csqcprogparms);
		PR_Configure(csqcprogs, NULL, -1, 1);
		
		CSQC_InitFields();	//let the qclib know the field order that the engine needs.
		
		if (PR_LoadProgs(csqcprogs, "csprogs.dat", 0, NULL, 0) < 0) //no per-progs builtins.
		{
			CSQC_Shutdown();
			//failed to load or something
			return;
		}
		if (setjmp(csqc_abort))
		{
			CSQC_Shutdown();
			return;
		}

		memset(csqcent, 0, sizeof(csqcent));
		
		csqcentsize = PR_InitEnts(csqcprogs, 3072);
		
		CSQC_FindGlobals();

		ED_Alloc(csqcprogs);	//we need a word entity.
		//world edict becomes readonly
		EDICT_NUM(csqcprogs, 0)->readonly = true;
		EDICT_NUM(csqcprogs, 0)->isfree = false;

		if (csqcg.init_function)
			PR_ExecuteProgram(csqcprogs, csqcg.init_function);

		Con_Printf("Loaded csqc\n");
	}
}

qboolean CSQC_DrawView(void)
{
	if (!csqcg.draw_function || !csqcprogs)
		return false;

	r_secondaryview = 0;

	if (cl.worldmodel)
		R_LessenStains();

	if (csqcg.time)
		*csqcg.time = Sys_DoubleTime();

	PR_ExecuteProgram(csqcprogs, csqcg.draw_function);

	return true;
}

qboolean CSQC_StuffCmd(char *cmd)
{
	void *pr_globals;
	char *str;
	if (!csqcprogs || !csqcg.parse_stuffcmd)
		return false;

	str = PF_TempStr();
	Q_strncpyz(str, cmd, MAXTEMPBUFFERLEN);

	pr_globals = PR_globals(csqcprogs, PR_CURRENT);
	(*(char **)&((int *)pr_globals)[OFS_PARM0] = PR_SetString(csqcprogs, str));

	PR_ExecuteProgram (csqcprogs, csqcg.parse_stuffcmd);
	return true;
}
qboolean CSQC_CenterPrint(char *cmd)
{
	void *pr_globals;
	char *str;
	if (!csqcprogs || !csqcg.parse_centerprint)
		return false;

	str = PF_TempStr();
	Q_strncpyz(str, cmd, MAXTEMPBUFFERLEN);

	pr_globals = PR_globals(csqcprogs, PR_CURRENT);
	(*(char **)&((int *)pr_globals)[OFS_PARM0] = PR_SetString(csqcprogs, str));

	PR_ExecuteProgram (csqcprogs, csqcg.parse_centerprint);
	return G_FLOAT(OFS_RETURN);
}

//this protocol allows up to 32767 edicts.
#ifdef PEXT_CSQC
void CSQC_ParseEntities(void)
{
	csqcedict_t *ent;
	unsigned short entnum;
	void *pr_globals;

	if (!csqcprogs)
		Host_EndGame("CSQC needs to be initialized for this server.\n");

	if (!csqcg.ent_update)
		Host_EndGame("CSQC is unable to parse entities\n");

	pr_globals = PR_globals(csqcprogs, PR_CURRENT);

	if (csqcg.time)
		*csqcg.time = Sys_DoubleTime();

	for(;;)
	{
		entnum = MSG_ReadShort();
		if (!entnum)
			break;
		if (entnum & 0x8000)
		{	//remove
			entnum &= ~0x8000;

			if (!entnum)
				Host_EndGame("CSQC cannot remove world!\n");

			if (entnum >= MAX_EDICTS)
				Host_EndGame("CSQC recieved too many edicts!\n");

			ent = csqcent[entnum];

			if (!ent)	//hrm.
				continue;

			*csqcg.self = EDICT_TO_PROG(csqcprogs, (void*)ent);
			PR_ExecuteProgram(csqcprogs, csqcg.ent_remove);
			csqcent[entnum] = NULL;
			//the csqc is expected to call the remove builtin.
		}
		else
		{
			if (entnum >= MAX_EDICTS)
				Host_EndGame("CSQC recieved too many edicts!\n");

			ent = csqcent[entnum];
			if (!ent)
			{
				ent = (csqcedict_t*)ED_Alloc(csqcprogs);
				csqcent[entnum] = ent;
				G_FLOAT(OFS_PARM0) = true;
			}
			else
				G_FLOAT(OFS_PARM0) = false;

			*csqcg.self = EDICT_TO_PROG(csqcprogs, (void*)ent);
			PR_ExecuteProgram(csqcprogs, csqcg.ent_update);
		}
	}
}
#endif

#endif
