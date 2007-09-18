#include "quakedef.h"
//#include "cg_public.h"
#ifdef VM_CG

#ifdef RGLQUAKE

#include "shader.h"

#if 1
#include "glquake.h"//hack
#else
typedef float m3by3_t[3][3];
#endif

#include "clq3defs.h"

//cl_ui.c
typedef struct q3refEntity_s q3refEntity_t;
void VQ3_AddEntity(const q3refEntity_t *q3);
typedef struct q3refdef_s q3refdef_t;
void VQ3_RenderView(const q3refdef_t *ref);
void CG_Command_f(void);

void GLDraw_ShaderImage (int x, int y, int w, int h, float s1, float t1, float s2, float t2, shader_t *pic);

#define	CGAME_IMPORT_API_VERSION	4

#define CGTAGNUM 5423

#define VM_TOSTRCACHE(a) VMQ3_StringToHandle(VM_POINTER(a))
#define VM_FROMSTRCACHE(a) VMQ3_StringFromHandle(a)
char *VMQ3_StringFromHandle(int handle);
int VMQ3_StringToHandle(char *str);

extern model_t mod_known[];
#define VM_FROMMHANDLE(a) (a?mod_known+a-1:NULL)
#define VM_TOMHANDLE(a) (a?a-mod_known+1:0)

extern shader_t r_shaders[];
#define VM_FROMSHANDLE(a) (a?r_shaders+a-1:NULL)
#define VM_TOSHANDLE(a) (a?a-r_shaders+1:0)

typedef enum {
	CG_PRINT,
	CG_ERROR,
	CG_MILLISECONDS,
	CG_CVAR_REGISTER,
	CG_CVAR_UPDATE,
	CG_CVAR_SET,
	CG_CVAR_VARIABLESTRINGBUFFER,
	CG_ARGC,
	CG_ARGV,
	CG_ARGS,
	CG_FS_FOPENFILE,				//10
	CG_FS_READ,
	CG_FS_WRITE,
	CG_FS_FCLOSEFILE,
	CG_SENDCONSOLECOMMAND,
	CG_ADDCOMMAND,
	CG_SENDCLIENTCOMMAND,
	CG_UPDATESCREEN,
	CG_CM_LOADMAP,
	CG_CM_NUMINLINEMODELS,
	CG_CM_INLINEMODEL,				//20
	CG_CM_LOADMODEL,
	CG_CM_TEMPBOXMODEL,
	CG_CM_POINTCONTENTS,
	CG_CM_TRANSFORMEDPOINTCONTENTS,
	CG_CM_BOXTRACE,
	CG_CM_TRANSFORMEDBOXTRACE,
	CG_CM_MARKFRAGMENTS,
	CG_S_STARTSOUND,
	CG_S_STARTLOCALSOUND,
	CG_S_CLEARLOOPINGSOUNDS,		//30
	CG_S_ADDLOOPINGSOUND,
	CG_S_UPDATEENTITYPOSITION,
	CG_S_RESPATIALIZE,
	CG_S_REGISTERSOUND,
	CG_S_STARTBACKGROUNDTRACK,
	CG_R_LOADWORLDMAP,
	CG_R_REGISTERMODEL,
	CG_R_REGISTERSKIN,
	CG_R_REGISTERSHADER,
	CG_R_CLEARSCENE,				//40
	CG_R_ADDREFENTITYTOSCENE,
	CG_R_ADDPOLYTOSCENE,
	CG_R_ADDLIGHTTOSCENE,
	CG_R_RENDERSCENE,
	CG_R_SETCOLOR,
	CG_R_DRAWSTRETCHPIC,
	CG_R_MODELBOUNDS,
	CG_R_LERPTAG,
	CG_GETGLCONFIG,
	CG_GETGAMESTATE,				//50
	CG_GETCURRENTSNAPSHOTNUMBER,
	CG_GETSNAPSHOT,
	CG_GETSERVERCOMMAND,
	CG_GETCURRENTCMDNUMBER,
	CG_GETUSERCMD,
	CG_SETUSERCMDVALUE,
	CG_R_REGISTERSHADERNOMIP,
	CG_MEMORY_REMAINING,
	CG_R_REGISTERFONT,
	CG_KEY_ISDOWN,					//60
	CG_KEY_GETCATCHER,
	CG_KEY_SETCATCHER,
	CG_KEY_GETKEY,
 	CG_PC_ADD_GLOBAL_DEFINE,
	CG_PC_LOAD_SOURCE,
	CG_PC_FREE_SOURCE,
	CG_PC_READ_TOKEN,
	CG_PC_SOURCE_FILE_AND_LINE,
	CG_S_STOPBACKGROUNDTRACK,
	CG_REAL_TIME,					//70
	CG_SNAPVECTOR,
	CG_REMOVECOMMAND,
	CG_R_LIGHTFORPOINT,				//73
	CG_CIN_PLAYCINEMATIC,			//64
	CG_CIN_STOPCINEMATIC,			//75
	CG_CIN_RUNCINEMATIC,			//76
	CG_CIN_DRAWCINEMATIC,			//77
	CG_CIN_SETEXTENTS,				//78
	CG_R_REMAP_SHADER,				//79
	CG_S_ADDREALLOOPINGSOUND,		//80
	CG_S_STOPLOOPINGSOUND,			//81

	CG_CM_TEMPCAPSULEMODEL,			//82
	CG_CM_CAPSULETRACE,				//83
	CG_CM_TRANSFORMEDCAPSULETRACE,	//84
	CG_R_ADDADDITIVELIGHTTOSCENE,	//85
	CG_GET_ENTITY_TOKEN,			//86
	CG_R_ADDPOLYSTOSCENE,			//87
	CG_R_INPVS,						//88
	// 1.32
	CG_FS_SEEK,						//89

	CG_MEMSET = 100,
	CG_MEMCPY,
	CG_STRNCPY,
	CG_SIN,
	CG_COS,
	CG_ATAN2,
	CG_SQRT,
	CG_FLOOR,
	CG_CEIL,
	CG_TESTPRINTINT,
	CG_TESTPRINTFLOAT,
	CG_ACOS
} cgameImport_t;



/*
==================================================================

functions exported to the main executable

==================================================================
*/

typedef enum {
	CG_INIT,
//	void CG_Init( int serverMessageNum, int serverCommandSequence, int clientNum )
	// called when the level loads or when the renderer is restarted
	// all media should be registered at this time
	// cgame will display loading status by calling SCR_Update, which
	// will call CG_DrawInformation during the loading process
	// reliableCommandSequence will be 0 on fresh loads, but higher for
	// demos, tourney restarts, or vid_restarts

	CG_SHUTDOWN,
//	void (*CG_Shutdown)( void );
	// oportunity to flush and close any open files

	CG_CONSOLE_COMMAND,
//	qboolean (*CG_ConsoleCommand)( void );
	// a console command has been issued locally that is not recognized by the
	// main game system.
	// use Cmd_Argc() / Cmd_Argv() to read the command, return qfalse if the
	// command is not known to the game

	CG_DRAW_ACTIVE_FRAME,
//	void (*CG_DrawActiveFrame)( int serverTime, stereoFrame_t stereoView, qboolean demoPlayback );
	// Generates and draws a game scene and status information at the given time.
	// If demoPlayback is set, local movement prediction will not be enabled

	CG_CROSSHAIR_PLAYER,
//	int (*CG_CrosshairPlayer)( void );

	CG_LAST_ATTACKER,
//	int (*CG_LastAttacker)( void );

	CG_KEY_EVENT, 
//	void	(*CG_KeyEvent)( int key, qboolean down );

	CG_MOUSE_EVENT,
//	void	(*CG_MouseEvent)( int dx, int dy );
	CG_EVENT_HANDLING
//	void (*CG_EventHandling)(int type);
} cgameExport_t;








unsigned int Contents_To_Q3(unsigned int fte)
{
	int ret = 0;

	if (fte & FTECONTENTS_SOLID)	//should use q3 constants.
		ret |= 1;
	if (fte & FTECONTENTS_WATER)	//should use q3 constants.
		ret |= 32;
	if (fte & FTECONTENTS_SLIME)	//should use q3 constants.
		ret |= 16;
	if (fte & FTECONTENTS_LAVA)	//should use q3 constants.
		ret |= 8;
	if (fte & FTECONTENTS_SKY)	//should use q3 constants.
		ret |= 0x80000000;

	return ret;
}
unsigned int Contents_From_Q3(unsigned int Q3)
{
	int ret = 0;

	if (Q3 & 1)	//should use q3 constants.
		ret |= FTECONTENTS_SOLID;
	if (Q3 & 32)	//should use q3 constants.
		ret |= FTECONTENTS_WATER;
	if (Q3 & 16)	//should use q3 constants.
		ret |= FTECONTENTS_SLIME;
	if (Q3 & 8)	//should use q3 constants.
		ret |= FTECONTENTS_LAVA;
	if (Q3 & 0x80000000)	//should use q3 constants.
		ret |= FTECONTENTS_SKY;

	return ret;
}

#define	MAX_GAMESTATE_CHARS	16000
#define MAX_CONFIGSTRINGS 1024
typedef struct {
	int			stringOffsets[MAX_CONFIGSTRINGS];
	char		stringData[MAX_GAMESTATE_CHARS];
	int			dataCount;
} gameState_t;
gameState_t cggamestate;

void CG_InsertIntoGameState(int num, char *str)
{
	if (num < 5)
	{
		Con_DPrintf("%i: %s", num, str);
	}

	if (cggamestate.dataCount + strlen(str)+1 > MAX_GAMESTATE_CHARS)
	{
		char oldstringData[MAX_GAMESTATE_CHARS];
		int i;
		char *oldstr;
		//copy the old strings to a temporary buffer
		memcpy(oldstringData, cggamestate.stringData, MAX_GAMESTATE_CHARS);
		cggamestate.dataCount = 0;
		for (i = 0; i < MAX_CONFIGSTRINGS; i++)
		{
			oldstr = oldstringData+cggamestate.stringOffsets[i];
			if (*oldstr)
			{
				if (cggamestate.dataCount + strlen(oldstr)+1 > MAX_GAMESTATE_CHARS)
					Host_EndGame("Too much configstring text\n");

				cggamestate.dataCount+=1;
				strcpy(cggamestate.stringData+cggamestate.dataCount, oldstr);
				cggamestate.stringOffsets[i] = cggamestate.dataCount;
				cggamestate.dataCount += strlen(oldstr);
			}
			else
				cggamestate.stringOffsets[i] = 0;
		}
	}

	if (!*str)
	{
		cggamestate.stringOffsets[num] = 0;
		return;
	}

	cggamestate.dataCount+=1;
	strcpy(cggamestate.stringData+cggamestate.dataCount, str);
	cggamestate.stringOffsets[num] = cggamestate.dataCount;
	cggamestate.dataCount += strlen(str);
}

char *CG_GetConfigString(int num)
{
	if ((unsigned)num >= MAX_CONFIGSTRINGS)
		return "";
	return cggamestate.stringData + cggamestate.stringOffsets[num];
}

int CG_GetGameState(gameState_t *gs)
{
	memcpy(gs, &cggamestate, sizeof(gameState_t));
	return sizeof(gameState_t);
}

typedef struct {
	int				serverTime;
	int				angles[3];
	int 			buttons;
	qbyte			weapon;           // weapon 
	signed char	forwardmove, rightmove, upmove;
} q3usercmd_t;
#define CMD_MASK Q3UPDATE_MASK
qboolean CGQ3_GetUserCmd(int cmdNumber, q3usercmd_t *ucmd)
{
	usercmd_t *cmd;
	cmdNumber--;

	if (cmdNumber > ccs.currentUserCmdNumber)
		Host_EndGame("CL_GetUserCmd: cmdNumber > ccs.currentUserCmdNumber");

	if (ccs.currentUserCmdNumber - cmdNumber > CMD_MASK)
		return false; // too old

	cmd = &cl.frames[(cmdNumber) & CMD_MASK].cmd[0];
	ucmd->angles[0] = cmd->angles[0];
	ucmd->angles[1] = cmd->angles[1];
	ucmd->angles[2] = cmd->angles[2];
	ucmd->serverTime = cmd->servertime;
	ucmd->forwardmove = cmd->forwardmove;
	ucmd->rightmove = cmd->sidemove;
	ucmd->upmove = cmd->upmove;
	ucmd->buttons = cmd->buttons;
	ucmd->weapon = cmd->weapon;

	return true;
}

static vm_t *cgvm;

extern int keycatcher;

qboolean CG_GetServerCommand(int cmdnum)
{
	//quote from cgame code:
	// get the gamestate from the client system, which will have the
	// new configstring already integrated

	char *str = ccs.serverCommands[cmdnum & TEXTCMD_MASK];

	Con_DPrintf("Dispaching %s\n", str);
	Cmd_TokenizeString(str, false, false);

	if (!strcmp(Cmd_Argv(0), "cs"))
		CG_InsertIntoGameState(atoi(Cmd_Argv(1)), Cmd_Argv(2));
	return true;
}


typedef struct {
	int		firstPoint;
	int		numPoints;
} markFragment_t;
int CG_MarkFragments( int numPoints, const vec3_t *points, const vec3_t projection,
				   int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragmentBuffer )
{
#if 1	//FIXME: make work
	return 0;
#else
	vec3_t center;
	vec3_t axis[3];
	vec3_t p[4];
	int i;
	float radius;

	if (numPoints != 4)
		return 0;

	/*
	q3 gamecode includes something like this

	originalPoints[0][i] = origin[i] - radius * axis[1][i] - radius * axis[2][i];
	originalPoints[1][i] = origin[i] + radius * axis[1][i] - radius * axis[2][i];
	originalPoints[2][i] = origin[i] + radius * axis[1][i] + radius * axis[2][i];
	originalPoints[3][i] = origin[i] - radius * axis[1][i] + radius * axis[2][i];

	We want that origional axis and the origin
	axis[0] is given in the 'projection' parameter.

	Yes, reversing this stuff means that we'll have no support for triangles.
	*/

	VectorClear(center);
	VectorAdd(center, points[0], center);
	VectorAdd(center, points[1], center);
	VectorAdd(center, points[2], center);
	VectorAdd(center, points[3], center);

	VectorSubtract(points[0], center, p[0]);
	VectorSubtract(points[1], center, p[1]);
	VectorSubtract(points[2], center, p[2]);
	VectorSubtract(points[3], center, p[3]);

	for (i = 0; i < 3; i++)
	{
		axis[1][i] = (p[2][i]+p[1][i])/2;
		axis[2][i] = (p[2][i]+p[3][i])/2;
	}

	radius = VectorNormalize(axis[1]);
	VectorNormalize(axis[2]);
	VectorNormalize(projection);

	


	Q1BSP_ClipDecal(center, axis[0], axis[1], axis[2], radius, pointBuffer, maxPoints);
	fragmentBuffer->firstPoint = 0;
	fragmentBuffer->numPoints = 0;

	return 1;
#endif
}


void GLDraw_Image(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qpic_t *pic);
int VM_LerpTag(void *out, model_t *model, int f1, int f2, float l2, char *tagname);


#define VALIDATEPOINTER(o,l) if ((int)o + l >= mask || VM_POINTER(o) < offset) Host_EndGame("Call to cgame trap %i passes invalid pointer\n", fn);	//out of bounds.

static int CG_SystemCallsEx(void *offset, unsigned int mask, int fn, const int *arg)
{
	int ret=0;

	//Remember to range check pointers.
	//The QVM must not be allowed to write to anything outside it's memory.
	//This includes getting the exe to copy it for it.

	//don't bother with reading, as this isn't a virus risk.
	//could be a cheat risk, but hey.

	//make sure that any called functions are also range checked.
	//like reading from files copies names into alternate buffers, allowing stack screwups.
//OutputDebugString(va("cl_cg: %i\n", fn));
	switch(fn)
	{
	case CG_PRINT:
		Con_Printf("%s", VM_POINTER(arg[0]));
		break;
	case CG_ERROR:
		Host_EndGame("cgame: %s", VM_POINTER(arg[0]));
		break;

	case CG_ARGC:
		VM_LONG(ret) = Cmd_Argc();
		break;
	case CG_ARGV:
		VALIDATEPOINTER(arg[1], arg[2]);
		Q_strncpyz(VM_POINTER(arg[1]), Cmd_Argv(VM_LONG(arg[0])), VM_LONG(arg[2]));
		break;
	case CG_ARGS:
		VALIDATEPOINTER(arg[0], arg[1]);
		Q_strncpyz(VM_POINTER(arg[0]), Cmd_Args(), VM_LONG(arg[1]));
		break;
	case CG_CVAR_REGISTER:
		if (arg[0])
			VALIDATEPOINTER(arg[0], sizeof(vmcvar_t));
		return VMQ3_Cvar_Register(VM_POINTER(arg[0]), VM_POINTER(arg[1]), VM_POINTER(arg[2]), VM_LONG(arg[3]));
	case CG_CVAR_UPDATE:
		VALIDATEPOINTER(arg[0], sizeof(vmcvar_t));
		return VMQ3_Cvar_Update(VM_POINTER(arg[0]));

	case CG_CVAR_SET:
		{
			cvar_t *var;
			var = Cvar_FindVar(VM_POINTER(arg[0]));
			if (var)
				Cvar_Set(var, VM_POINTER(arg[1])?VM_POINTER(arg[1]):"");	//set it
			else
				Cvar_Get(VM_POINTER(arg[0]), VM_POINTER(arg[1]), 0, "Q3CG created");	//create one
		}
		break;
	case CG_CVAR_VARIABLESTRINGBUFFER:
		{
			cvar_t *var;
			var = Cvar_FindVar(VM_POINTER(arg[0]));
			if (!VM_LONG(arg[2]))
				VM_LONG(ret) = 0;
			else if (!var)
			{
				VALIDATEPOINTER(arg[1], 1);
				*(char *)VM_POINTER(arg[1]) = '\0';
				VM_LONG(ret) = -1;
			}
			else
			{
				VALIDATEPOINTER(arg[1], arg[2]);
				Q_strncpyz(VM_POINTER(arg[1]), var->string, VM_LONG(arg[2]));
			}
		}
		break;

	case CG_SENDCONSOLECOMMAND:
		Con_DPrintf("CG_SENDCONSOLECOMMAND: %s", VM_POINTER(arg[0]));
		Cbuf_AddText(VM_POINTER(arg[0]), RESTRICT_SERVER);
		break;
	case CG_ADDCOMMAND:
		Cmd_AddRemCommand(VM_POINTER(arg[0]), NULL);
		break;
	case CG_SENDCLIENTCOMMAND:
		Con_DPrintf("CG_SENDCLIENTCOMMAND: %s", VM_POINTER(arg[0]));
		CL_SendClientCommand(true, "%s", VM_POINTER(arg[0]));
		break;

	case CG_UPDATESCREEN:	//force a buffer swap cos loading won't refresh it soon.
		SCR_BeginLoadingPlaque();
		SCR_UpdateScreen();
		SCR_EndLoadingPlaque();
//		GL_EndRendering();
//		GL_DoSwap();
		break;

	case CG_FS_FOPENFILE: //fopen
		if (arg[1])
			VALIDATEPOINTER(arg[1], 4);
		VM_LONG(ret) = VM_fopen(VM_POINTER(arg[0]), VM_POINTER(arg[1]), VM_LONG(arg[2]), 1);
		break;

	case CG_FS_READ:	//fread
		VALIDATEPOINTER(arg[1], 4);
		VM_LONG(ret) = VM_FRead(VM_POINTER(arg[0]), VM_LONG(arg[1]), VM_LONG(arg[2]), 1);
		break;
	case CG_FS_WRITE:	//fwrite
		break;
	case CG_FS_FCLOSEFILE:	//fclose
		VM_fclose(VM_LONG(arg[0]), 1);
		break;

	case CG_CM_POINTCONTENTS: //int			trap_CM_PointContents( const vec3_t p, clipHandle_t model );
		{
			unsigned int pc;
			model_t *mod = VM_FROMMHANDLE(arg[1]);
			if (!mod)
				mod = cl.worldmodel;
			if (mod)
				pc = cl.worldmodel->funcs.NativeContents(mod, 0, 0, VM_POINTER(arg[0]), vec3_origin, vec3_origin);
			else
				pc = 1;//FTECONTENTS_SOLID;
			VM_LONG(ret) = pc;//Contents_To_Q3(pc);
		}
		break;

	case CG_CM_TRANSFORMEDPOINTCONTENTS: //int		trap_CM_TransformedPointContents( const vec3_t p, clipHandle_t model, const vec3_t origin, const vec3_t angles ) {
		{
			unsigned int pc;
			float *p = VM_POINTER(arg[0]);
			model_t *mod = VM_FROMMHANDLE(arg[1]);
			float *origin = VM_POINTER(arg[2]);
			float *angles = VM_POINTER(arg[3]);

			if (!mod)
				mod = cl.worldmodel;

			{
				vec3_t		p_l;
				vec3_t		temp;
				vec3_t		forward, right, up;

				// subtract origin offset
				VectorSubtract (p, origin, p_l);

				// rotate start and end into the models frame of reference
				if (angles[0] || angles[1] || angles[2])
				{
					AngleVectors (angles, forward, right, up);

					VectorCopy (p_l, temp);
					p_l[0] = DotProduct (temp, forward);
					p_l[1] = -DotProduct (temp, right);
					p_l[2] = DotProduct (temp, up);
				}

				if (mod)
					pc = mod->funcs.NativeContents(mod, 0, 0, VM_POINTER(arg[0]), vec3_origin, vec3_origin);
				else
					pc = 1;//FTECONTENTS_SOLID;
			}
			VM_LONG(ret) = pc;//Contents_To_Q3(pc);
		}
		break;

	case CG_CM_TRANSFORMEDBOXTRACE:
//		void		trap_CM_BoxTrace( trace_t *results, const vec3_t start, const vec3_t end,
//					  const vec3_t mins, const vec3_t maxs,
//					  clipHandle_t model, int brushmask );
		{
//FIXME: no protection of result trace.
			trace_t tr;
			q3trace_t *results	= VM_POINTER(arg[0]);
			float *start		= VM_POINTER(arg[1]);
			float *end			= VM_POINTER(arg[2]);
			float *mins			= VM_POINTER(arg[3]);
			float *maxs			= VM_POINTER(arg[4]);
			model_t *mod		= VM_FROMMHANDLE(arg[5]);
			int brushmask			= VM_LONG(arg[6]);
			float *origin		= VM_POINTER(arg[7]);
			float *angles		= VM_POINTER(arg[8]);
			if (!mod)
				mod = cl.worldmodel;
			if (!mins)
				mins = vec3_origin;
			if (!maxs)
				maxs = vec3_origin;
			if (!origin)
				origin = vec3_origin;
			if (!angles)
				angles = vec3_origin;
			if (mod)
				TransformedNativeTrace(mod, 0, 0, start, end, mins, maxs, brushmask, &tr, origin, angles);
			else
			{
				memset(&tr, 0, sizeof(tr));
				tr.allsolid = tr.startsolid = true;
				tr.contents = 1;
			}
			results->allsolid = tr.allsolid;
			results->contents = tr.contents;
			results->fraction = tr.fraction;
			results->entityNum = 0;
			results->startsolid = tr.startsolid;
			results->surfaceFlags = tr.surface?tr.surface->flags:0;
			memcpy(results->endpos, tr.endpos, sizeof(vec3_t));
			memcpy(&results->plane, &tr.plane, sizeof(cplane_t));
		}
		break;
	case CG_CM_BOXTRACE:
//		void		trap_CM_BoxTrace( trace_t *results, const vec3_t start, const vec3_t end,
//					  const vec3_t mins, const vec3_t maxs,
//					  clipHandle_t model, int brushmask );
		{
//FIXME: no protection of result trace.
			trace_t tr;
			q3trace_t *results	= VM_POINTER(arg[0]);
			float *start		= VM_POINTER(arg[1]);
			float *end			= VM_POINTER(arg[2]);
			float *mins			= VM_POINTER(arg[3]);
			float *maxs			= VM_POINTER(arg[4]);
			model_t *mod		= VM_FROMMHANDLE(arg[5]);
			int brushmask			= VM_LONG(arg[6]);
			if (!mod)
				mod = cl.worldmodel;
			if (!mins)
				mins = vec3_origin;
			if (!maxs)
				maxs = vec3_origin;
			if (mod)
			{
				mod->funcs.NativeTrace(mod, 0, 0, start, end, mins, maxs, brushmask, &tr);
			}
			else
			{
				memset(&tr, 0, sizeof(tr));
				tr.allsolid = tr.startsolid = true;
				tr.contents = 1;
			}
			results->allsolid = tr.allsolid;
			results->contents = tr.contents;
			results->fraction = tr.fraction;
			results->entityNum = 0;
			results->startsolid = tr.startsolid;
			results->surfaceFlags = tr.surface?tr.surface->flags:0;
			memcpy(results->endpos, tr.endpos, sizeof(vec3_t));
			memcpy(&results->plane, &tr.plane, sizeof(cplane_t));
		}
		break;

	case CG_R_LOADWORLDMAP:	//FTE can't distinguish. :/
		break;				//So long as noone has one collision model with a different rendering one, we'll be fine

	case CG_CM_LOADMAP:
		{
			int i;
			char *mapname = VM_POINTER(arg[0]);
			strcpy(cl.model_name[1], mapname);
			cl.worldmodel = cl.model_precache[1] = Mod_ForName(mapname, false);
			if (cl.worldmodel->needload)
				Host_EndGame("Couldn't load map");

			for (i=1 ; i<cl.model_precache[1]->numsubmodels ; i++)
			{
				strcpy(cl.model_name[1+i], va("*%i", i));
				cl.model_precache[i+1] = Mod_ForName (cl.model_name[i+1], false);
			}
		}

		break;

	case CG_CM_INLINEMODEL:
		VM_LONG(ret) = VM_TOMHANDLE(cl.model_precache[VM_LONG(arg[0])+1]);
		break;
	case CG_CM_NUMINLINEMODELS:
		VM_LONG(ret) = cl.worldmodel?cl.worldmodel->numsubmodels:0;
		break;

	case CG_CM_TEMPBOXMODEL:
		VM_LONG(ret) = VM_TOMHANDLE(CM_TempBoxModel(VM_POINTER(arg[0]), VM_POINTER(arg[1])));
		break;

	case CG_R_MODELBOUNDS:
		VALIDATEPOINTER(arg[1], sizeof(vec3_t));
		VALIDATEPOINTER(arg[2], sizeof(vec3_t));
		{
			model_t *mod = VM_FROMMHANDLE(arg[0]);
			if (mod)
			{
				VectorCopy(mod->mins, ((float*)VM_POINTER(arg[1])));
				VectorCopy(mod->maxs, ((float*)VM_POINTER(arg[2])));
			}
		}
		break;

	case CG_R_REGISTERMODEL:	//precache model
		{
			model_t *mod;
			mod = Mod_ForName(VM_POINTER(arg[0]), false);
			if (mod->needload || mod->type == mod_dummy)
				return 0;
			VM_LONG(ret) = VM_TOMHANDLE(mod);
		}
		break;

	case CG_R_REGISTERSKIN:
		VM_LONG(ret) = VM_TOSTRCACHE(arg[0]);
		break;

	case CG_R_REGISTERSHADER:
		if (!*(char*)VM_POINTER(arg[0]))
			VM_LONG(ret) = 0;
		else if (qrenderer == QR_OPENGL)
			VM_LONG(ret) = VM_TOSHANDLE(R_RegisterPic(VM_POINTER(arg[0])));
//FIXME: 64bit		else
//			VM_LONG(ret) = VM_TOHANDLE(Draw_SafeCachePic(VM_POINTER(arg[0])));
		break;
	case CG_R_REGISTERSHADERNOMIP:
		if (!*(char*)VM_POINTER(arg[0]))
			VM_LONG(ret) = 0;
		else if (qrenderer == QR_OPENGL)
			VM_LONG(ret) = VM_TOSHANDLE(R_RegisterPic(VM_POINTER(arg[0])));
//FIXME: 64bit		else
//			VM_LONG(ret) = VM_TOHANDLE(Draw_SafeCachePic(VM_POINTER(arg[0])));
		break;

	case CG_R_CLEARSCENE:	//clear scene
		cl_numvisedicts=0;
		dlights_running=0;
		dlights_software=0;
		break;
	case CG_R_ADDPOLYTOSCENE:
		// ...
		break;
	case CG_R_ADDREFENTITYTOSCENE:	//add ent to scene
		VQ3_AddEntity(VM_POINTER(arg[0]));
		break;
	case CG_R_ADDADDITIVELIGHTTOSCENE:
	case CG_R_ADDLIGHTTOSCENE:	//add light to scene.
		{
			float *org = VM_POINTER(arg[0]);
			CL_NewDlightRGB(-1, org[0], org[1], org[2], VM_FLOAT(arg[1]), 0, VM_FLOAT(arg[2]), VM_FLOAT(arg[3]), VM_FLOAT(arg[4]));
		}
		break;
	case CG_R_RENDERSCENE:	//render scene
		GLR_PushDlights();
		VQ3_RenderView(VM_POINTER(arg[0]));
		break;

	case CG_R_SETCOLOR:	//setcolour float*
		{
			float *f = VM_POINTER(arg[0]);
			if (f)
				Draw_ImageColours(f[0], f[1], f[2], f[3]);
			else
				Draw_ImageColours(1, 1, 1, 1);
		}
		break;

	case CG_R_DRAWSTRETCHPIC:
		if (qrenderer == QR_OPENGL)
			GLDraw_ShaderImage(VM_FLOAT(arg[0]), VM_FLOAT(arg[1]), VM_FLOAT(arg[2]), VM_FLOAT(arg[3]), VM_FLOAT(arg[4]), VM_FLOAT(arg[5]), VM_FLOAT(arg[6]), VM_FLOAT(arg[7]), VM_FROMSHANDLE(arg[8]));
//		else
//			Draw_Image(VM_FLOAT(arg[0]), VM_FLOAT(arg[1]), VM_FLOAT(arg[2]), VM_FLOAT(arg[3]), VM_FLOAT(arg[4]), VM_FLOAT(arg[5]), VM_FLOAT(arg[6]), VM_FLOAT(arg[7]), (mpic_t *)VM_LONG(arg[8]));
		break;

	case CG_R_LERPTAG:	//Lerp tag...
		VALIDATEPOINTER(arg[0], sizeof(float)*12);
		VM_LONG(ret) = VM_LerpTag(VM_POINTER(arg[0]), VM_FROMMHANDLE(arg[1]), VM_LONG(arg[2]), VM_LONG(arg[3]), VM_FLOAT(arg[4]), VM_POINTER(arg[5]));
		break;

	case CG_S_REGISTERSOUND:
		{
			sfx_t *sfx;
			sfx = S_PrecacheSound(VM_POINTER(arg[0]));
			if (sfx)
				VM_LONG(ret) = VM_TOSTRCACHE(arg[0]);
			else
				VM_LONG(ret) = -1;
		}
		break;

	case CG_S_STARTLOCALSOUND:
		if (VM_LONG(arg[0]) != -1 && arg[0])
			S_LocalSound(VM_FROMSTRCACHE(arg[0]));
		break;

	case CG_S_STARTSOUND:// ( vec3_t origin, int entityNum, int entchannel, sfxHandle_t sfx )
		S_StartSound(VM_LONG(arg[1]), VM_LONG(arg[2]), S_PrecacheSound(VM_FROMSTRCACHE(arg[3])), VM_POINTER(arg[0]), 1, 1);
		break;

	case CG_S_ADDLOOPINGSOUND:
		break;
	case CG_S_STOPLOOPINGSOUND:
		break;

	case CG_S_STARTBACKGROUNDTRACK:
	case CG_S_STOPBACKGROUNDTRACK:
	case CG_S_CLEARLOOPINGSOUNDS:
		break;

	case CG_S_UPDATEENTITYPOSITION://void		trap_S_UpdateEntityPosition( int entityNum, const vec3_t origin );
		break;
	case CG_S_RESPATIALIZE://void		trap_S_Respatialize( int entityNum, const vec3_t origin, vec3_t axis[3], int inwater );
		{
			float *org = VM_POINTER(arg[1]);
			float *axis = VM_POINTER(arg[2]);
/*
vec3_t		listener_origin;
vec3_t		listener_forward;
vec3_t		listener_right;
vec3_t		listener_up;
*/
			VectorCopy(org, listener_origin);
			VectorCopy(axis+0, listener_forward);
			VectorCopy(axis+3, listener_right);
			VectorCopy(axis+6, listener_up);

//			S_Update(origin, axis[0], axis[1], axis[2], false);
		}
		break;

	case CG_S_ADDREALLOOPINGSOUND:
		break;

	case CG_KEY_ISDOWN:
		{
			extern qboolean	keydown[256];
			if (keydown[VM_LONG(arg[0])])
				VM_LONG(ret) = 1;
			else
				VM_LONG(ret) = 0;
		}
		break;

	case CG_KEY_GETKEY:
		{
			int ret[2];
			M_FindKeysForCommand (VM_POINTER(arg[0]), ret);
			return ret[0];
		}
		break;

	case CG_KEY_GETCATCHER:
		VM_LONG(ret) = keycatcher;
		break;
	case CG_KEY_SETCATCHER:
		keycatcher = VM_LONG(arg[0]);
		break;

	case CG_GETGLCONFIG:
		VALIDATEPOINTER(arg[0], 11332);

		{	//FIXME: Clean this shit up
			//do any needed work
			unsigned char *glconfig = VM_POINTER(arg[0]);
			memset(glconfig, 0, 11304);
			*(int *)(glconfig+11304) = vid.width;
			*(int *)(glconfig+11308) = vid.height;
			*(float *)(glconfig+11312) = (float)vid.width/vid.height;
			memset((glconfig+11316), 0, 11332-11316);
		}
		break;

	case CG_GETGAMESTATE:
		VALIDATEPOINTER(arg[0], sizeof(gameState_t));
		VM_LONG(ret) = CG_GetGameState(VM_POINTER(arg[0]));
		break;

	case CG_CM_MARKFRAGMENTS:
		VM_LONG(ret) = CG_MarkFragments( VM_LONG(arg[0]), VM_POINTER(arg[1]), VM_POINTER(arg[2]), VM_LONG(arg[3]), VM_POINTER(arg[4]), VM_LONG(arg[5]), VM_POINTER(arg[6]) );
		break;

	case CG_GETCURRENTSNAPSHOTNUMBER:
		VALIDATEPOINTER(arg[0], sizeof(int));
		VALIDATEPOINTER(arg[1], sizeof(int));
		*(int *)VM_POINTER(arg[0]) = ccs.snap.serverMessageNum;
		*(int *)VM_POINTER(arg[1]) = ccs.snap.serverTime;// + Sys_DoubleTime()*1000-ccs.snap.localTime;
		break;

	case CG_GETSNAPSHOT:
		VALIDATEPOINTER(arg[1], sizeof(snapshot_t));
		VM_LONG(ret) = CG_FillQ3Snapshot(VM_LONG(arg[0]), VM_POINTER(arg[1]));
		break;

	case CG_GETCURRENTCMDNUMBER:
		VM_LONG(ret) = ccs.currentUserCmdNumber;
		break;
	case CG_GETUSERCMD:
		VALIDATEPOINTER(arg[1], sizeof(q3usercmd_t));
		VM_LONG(ret) = CGQ3_GetUserCmd(VM_LONG(arg[0]), VM_POINTER(arg[1]));
		break;
	case CG_SETUSERCMDVALUE:	//weaponselect, zoomsensativity.
		ccs.selected_weapon = VM_LONG(arg[0]);
		in_sensitivityscale = VM_FLOAT(arg[1]);
		break;

	case CG_GETSERVERCOMMAND:
		VM_LONG(ret) = CG_GetServerCommand(VM_LONG(arg[0]));
		break;

	case CG_MEMORY_REMAINING:
		VM_LONG(ret) = Hunk_LowMemAvailable();
		break;

	case CG_MILLISECONDS:
		VM_LONG(ret) = Sys_Milliseconds();
		break;
	case CG_REAL_TIME:
		VM_FLOAT(ret) = realtime;
		break;

	case CG_SNAPVECTOR: // ( float *v )
		VALIDATEPOINTER(arg[0], sizeof(vec3_t));
		{
			float *fp = (float *)VM_POINTER(arg[0]);
#define rint(x) (int)((x > 0)?(x + 0.5f):(x-0.5f))
			fp[0] = rint(fp[0]);
			fp[1] = rint(fp[1]);
			fp[2] = rint(fp[2]);
		}
		break;

	case CG_PC_ADD_GLOBAL_DEFINE:
		Con_Printf("CG_PC_ADD_GLOBAL_DEFINE not supported\n");
		break;
	case CG_PC_SOURCE_FILE_AND_LINE:
		Script_Get_File_And_Line(arg[0], VM_POINTER(arg[1]), VM_POINTER(arg[2]));
		break;

	case CG_PC_LOAD_SOURCE:
		return Script_LoadFile(VM_POINTER(arg[0]));
	case CG_PC_FREE_SOURCE:
		Script_Free(arg[0]);
		break;
	case CG_PC_READ_TOKEN:
		//fixme: memory protect.
		return Script_Read(arg[0], VM_POINTER(arg[1]));

// standard Q3
	case CG_MEMSET:
		VALIDATEPOINTER(arg[0], arg[2]);
		{
			void *dst = VM_POINTER(arg[0]);
			memset(dst, arg[1], arg[2]);
		}
		break;
	case CG_MEMCPY:
		VALIDATEPOINTER(arg[0], arg[2]);
		{
			void *dst = VM_POINTER(arg[0]);
			void *src = VM_POINTER(arg[1]);
			memcpy(dst, src, arg[2]);
		}
		break;
	case CG_STRNCPY:
		VALIDATEPOINTER(arg[0], arg[2]);
		{
			void *dst = VM_POINTER(arg[0]);
			void *src = VM_POINTER(arg[1]);
			strncpy(dst, src, arg[2]);
		}
		break;
	case CG_SIN:
		VM_FLOAT(ret)=(float)sin(VM_FLOAT(arg[0]));
		break;
	case CG_COS:
		VM_FLOAT(ret)=(float)cos(VM_FLOAT(arg[0]));
		break;
	case CG_ACOS:
		VM_FLOAT(ret)=(float)acos(VM_FLOAT(arg[0]));
		break;
	case CG_ATAN2:
		VM_FLOAT(ret)=(float)atan2(VM_FLOAT(arg[0]), VM_FLOAT(arg[1]));
		break;
	case CG_SQRT:
		VM_FLOAT(ret)=(float)sqrt(VM_FLOAT(arg[0]));
		break;
	case CG_FLOOR:
		VM_FLOAT(ret)=(float)floor(VM_FLOAT(arg[0]));
		break;
	case CG_CEIL:
		VM_FLOAT(ret)=(float)ceil(VM_FLOAT(arg[0]));
		break;

	case CG_R_REGISTERFONT:
		VALIDATEPOINTER(arg[2], sizeof(fontInfo_t));
		UI_RegisterFont(VM_POINTER(arg[0]), VM_LONG(arg[1]), VM_POINTER(arg[2]));
		break;
	default:
		Con_Printf("Q3CG: Bad system trap: %d\n", fn);
	}

	return ret;
}
#ifdef _DEBUG
static int CG_SystemCallsExWrapper(void *offset, unsigned int mask, int fn, const int *arg)
{	//this is so we can use edit and continue properly (vc doesn't like function pointers for edit+continue)
	return CG_SystemCallsEx(offset, mask, fn, arg);
}
#define CG_SystemCallsEx CG_SystemCallsExWrapper
#endif

//I'm not keen on this.
//but dlls call it without saying what sort of vm it comes from, so I've got to have them as specifics
static int EXPORT_FN CG_SystemCalls(int arg, ...)
{
	int args[10];
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
	args[9]=va_arg(argptr, int);
	va_end(argptr);

	return CG_SystemCallsEx(NULL, (unsigned)~0, arg, args);
}

#endif

int CG_Refresh(void)
{
#ifdef RGLQUAKE
	int time;
	if (!cgvm)
		return false;

	time = ccs.serverTime;
	VM_Call(cgvm, CG_DRAW_ACTIVE_FRAME, time, 0, false);

	Draw_ImageColours(1, 1, 1, 1);

	return true;
#else
	return false;
#endif
}



void CG_Stop (void)
{
#ifdef RGLQUAKE
	keycatcher &= ~2;
	if (cgvm)
	{
		VM_Call(cgvm, CG_SHUTDOWN);
		VM_Destroy(cgvm);
		VM_fcloseall(1);
		cgvm = NULL;
	}
#endif
}

void CG_Start (void)
{
	if (cls.protocol != CP_QUAKE3)
	{	//q3 clients only.
		CG_Stop();
		return;
	}

#if defined(RGLQUAKE) || defined(DIRECT3D)
	if (!Draw_SafeCachePic)	//no renderer loaded
	{
		CG_Stop();
		return;
	}

	if (qrenderer != QR_OPENGL && qrenderer != QR_DIRECT3D)
	{	//sorry.
		CG_Stop();
		Host_EndGame("Unable to connect to q3 servers without opengl or d3d.\n");
		return;
	}

	Z_FreeTags(CGTAGNUM);
	SCR_BeginLoadingPlaque();

	cgvm = VM_Create(NULL, "vm/cgame", CG_SystemCalls, CG_SystemCallsEx);
	if (cgvm)
	{	//hu... cgame doesn't appear to have a query version call!
		VM_Call(cgvm, CG_INIT, ccs.serverMessageNum, ccs.lastServerCommandNum, cl.playernum[0]);
		SCR_EndLoadingPlaque();
	}
	else
	{
		SCR_EndLoadingPlaque();
		Host_EndGame("Failed to initialise cgame module\n");
	}
#else
	Host_EndGame("Unable to connect to q3 servers without opengl.\n");
#endif
}

qboolean CG_Command(void)
{
#ifdef RGLQUAKE
	if (!cgvm)
		return false;
	Con_DPrintf("CG_Command: %s %s\n", Cmd_Argv(0), Cmd_Args());
	return VM_Call(cgvm, CG_CONSOLE_COMMAND);
#else
	return false;
#endif
}

void CG_Command_f(void)
{
	if (cgvm)
	{
		Con_DPrintf("CG_Command_f: %s %s\n", Cmd_Argv(0), Cmd_Args());
		if (!VM_Call(cgvm, CG_CONSOLE_COMMAND))
		{
			Cmd_ForwardToServer();
		}
	}
}

qboolean CG_KeyPress(int key, int down)
{
	if (!cgvm || !(keycatcher&8))
		return false;
	return VM_Call(cgvm, CG_KEY_EVENT, key, down);
}

void CG_Restart_f(void)
{
	CG_Stop();
	CG_Start();
}

void CG_Init(void)
{
	Cmd_AddCommand("cg_restart", CG_Restart_f);
}

#endif
