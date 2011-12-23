#include "progtype.h"
#include "progslib.h"

#ifdef CLIENTONLY
typedef struct edict_s {
	pbool	isfree;

	float		freetime;			// realtime when the object was freed
	unsigned int entnum;
	pbool	readonly;	//causes error when QC tries writing to it. (quake's world entity)
	void	*v;
} edict_t;
#endif

struct wedict_s
{
	qboolean	isfree;
	float		freetime; // sv.time when the object was freed
	int			entnum;
	qboolean	readonly;	//world
#ifdef VM_Q1
	comentvars_t	*v;
	comextentvars_t	*xv;
#else
	union {
		comentvars_t	*v;
		comentvars_t	*xv;
	};
#endif
	/*the above is shared with qclib*/
	link_t	area;
	pvscache_t pvsinfo;

#ifdef USEODE
	entityode_t ode;
#endif
	/*the above is shared with ssqc*/
};

#define PF_cin_open PF_Fixme
#define PF_cin_close PF_Fixme
#define PF_cin_setstate PF_Fixme
#define PF_cin_getstate PF_Fixme
#define PF_cin_restart PF_Fixme
#define PF_drawline PF_Fixme
#define PF_gecko_create PF_Fixme
#define PF_gecko_destroy PF_Fixme
#define PF_gecko_navigate PF_Fixme
#define PF_gecko_keyevent PF_Fixme
#define PF_gecko_movemouse PF_Fixme
#define PF_gecko_resize PF_Fixme
#define PF_gecko_get_texture_extent PF_Fixme

#define PF_pointsound PF_Fixme
#define PF_getsurfacepointattribute PF_Fixme
#define PF_gecko_mousemove PF_Fixme
#define PF_numentityfields PF_Fixme
#define PF_entityfieldname PF_Fixme
#define PF_entityfieldtype PF_Fixme
#define PF_getentityfieldstring PF_Fixme
#define PF_putentityfieldstring PF_Fixme
#define PF_WritePicture PF_Fixme
#define PF_ReadPicture PF_Fixme

#define G_PROG G_FLOAT

//the lh extension system asks for a name for the extension.
//the ebfs version is a function that returns a builtin number.
//thus lh's system requires various builtins to exist at specific numbers.
typedef struct lh_extension_s {
	char *name;
	int numbuiltins;
	qboolean *queried;
	char *builtinnames[21];	//extend freely
} lh_extension_t;

extern lh_extension_t QSG_Extensions[];
extern unsigned int QSG_Extensions_count;

pbool QC_WriteFile(const char *name, void *data, int len);
void *VARGS PR_CB_Malloc(int size);	//these functions should be tracked by the library reliably, so there should be no need to track them ourselves.
void VARGS PR_CB_Free(void *mem);


void PF_InitTempStrings(progfuncs_t *prinst);
string_t PR_TempString(progfuncs_t *prinst, char *str);	//returns a tempstring containing str
char *PF_TempStr(progfuncs_t *prinst);	//returns a tempstring which can be filled in with whatever junk you want.

#define	RETURN_SSTRING(s) (((int *)pr_globals)[OFS_RETURN] = PR_SetString(prinst, s))	//static - exe will not change it.
#define	RETURN_TSTRING(s) (((int *)pr_globals)[OFS_RETURN] = PR_TempString(prinst, s))	//temp (static but cycle buffers)
extern cvar_t pr_tempstringsize;
extern cvar_t pr_tempstringcount;

int MP_TranslateFTEtoDPCodes(int code);
int MP_TranslateDPtoFTECodes(int code);

//pr_cmds.c builtins that need to be moved to a common.
void VARGS PR_BIError(progfuncs_t *progfuncs, char *format, ...) LIKEPRINTF(2);
void QCBUILTIN PF_print (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_dprint (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_error (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_rint (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_floor (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_ceil (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_Tokenize  (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_tokenizebyseparator  (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_tokenize_console  (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_ArgV  (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_argv_start_index  (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_argv_end_index  (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_FindString (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_nextent (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_Sin (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_Cos (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_Sqrt (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_bound (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_strlen(progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_strcat (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_ftos (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_fabs (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_vtos (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_etos (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_stof (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_mod (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_substring (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_stov (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_dupstring(progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_forgetstring(progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_Spawn (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_min (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_max (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_registercvar (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_pow (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_asin (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_acos (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_atan (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_atan2 (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_tan (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_localcmd (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_sprintf (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_random (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_fclose (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_fputs (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_fgets (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_normalize (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_vlen (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_vectoyaw (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_vectoangles (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_findchain (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_findchainfloat (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_coredump (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_traceon (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_traceoff (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_eprint (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void search_close_progs(progfuncs_t *prinst, qboolean complain);
void QCBUILTIN PF_search_begin (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_search_end (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_search_getsize (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_search_getfilename (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_WasFreed (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_break (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_crc16 (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_cvar_type (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_uri_escape  (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_uri_unescape  (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_uri_get  (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_itos (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_stoi (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_stoh (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_htos (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PR_fclose_progs (progfuncs_t *prinst);
char *PF_VarString (progfuncs_t *prinst, int	first, struct globalvars_s *pr_globals);
void PR_AutoCvarSetup(progfuncs_t *prinst);
void PR_AutoCvar(progfuncs_t *prinst, cvar_t *var);


void QCBUILTIN PF_getsurfacenumpoints(progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_getsurfacepoint(progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_getsurfacenormal(progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_getsurfacetexture(progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_getsurfacenearpoint(progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_getsurfaceclippedpoint(progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_skel_set_bone_world (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_skel_mmap(progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_skel_ragedit(progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_skel_create (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_skel_build (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_skel_get_numbones (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_skel_get_bonename (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_skel_get_boneparent (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_skel_find_bone (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_skel_get_bonerel (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_skel_get_boneabs (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_skel_set_bone (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_skel_mul_bone (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_skel_mul_bones (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_skel_copybones (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_skel_delete (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void skel_lookup(progfuncs_t *prinst, int skelidx, framestate_t *out);
void skel_dodelete(progfuncs_t *prinst);
void skel_reset(progfuncs_t *prinst);
void QCBUILTIN PF_gettaginfo (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_gettagindex (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_terrain_edit(progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_touchtriggers(progfuncs_t *prinst, struct globalvars_s *pr_globals);

//pr_cmds.c builtins that need to be moved to a common.
void VARGS PR_BIError(progfuncs_t *progfuncs, char *format, ...) LIKEPRINTF(2);
void QCBUILTIN PF_cvar_string (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_cvar_set (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_cvar_setf (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_ArgC (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_randomvec (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_strreplace (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_strireplace (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_randomvector (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_fopen (progfuncs_t *prinst, struct globalvars_s *pr_globals);

void QCBUILTIN PF_FindString (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_FindFloat (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_FindFlags (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_findchain (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_findchainfloat (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_findchainflags (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_bitshift(progfuncs_t *prinst, struct globalvars_s *pr_globals);

void QCBUILTIN PF_Abort(progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_externcall (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_externrefcall (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_externvalue (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_externset (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_instr (progfuncs_t *prinst, struct globalvars_s *pr_globals);

void QCBUILTIN PF_strlennocol (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_strdecolorize (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_strtolower (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_strtoupper (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_strftime (progfuncs_t *prinst, struct globalvars_s *pr_globals);

void QCBUILTIN PF_strstrofs (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_str2chr (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_chr2str (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_strconv (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_infoadd (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_infoget (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_strncmp (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_strcasecmp (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_strncasecmp (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_strpad (progfuncs_t *prinst, struct globalvars_s *pr_globals);

void QCBUILTIN PF_edict_for_num (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_num_for_edict (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_cvar_defstring (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_cvar_description (progfuncs_t *prinst, struct globalvars_s *pr_globals);

//these functions are from pr_menu.dat
void QCBUILTIN PF_CL_is_cached_pic (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_CL_precache_pic (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_CL_free_pic (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_CL_drawcharacter (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_CL_drawrawstring (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_CL_drawcolouredstring (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_CL_drawpic (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_CL_drawline (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_CL_drawfill (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_CL_drawsetcliparea (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_CL_drawresetcliparea (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_CL_drawgetimagesize (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_CL_stringwidth (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_CL_drawsubpic (progfuncs_t *prinst, struct globalvars_s *pr_globals);

void QCBUILTIN PF_cl_getmousepos (progfuncs_t *prinst, struct globalvars_s *pr_globals);

void QCBUILTIN PF_cl_keynumtostring (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_cl_findkeysforcommand (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_cl_stringtokeynum(progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_cl_getkeybind (progfuncs_t *prinst, struct globalvars_s *pr_globals);

void search_close_progs(progfuncs_t *prinst, qboolean complain);

void QCBUILTIN PF_buf_create  (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_buf_del  (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_buf_getsize  (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_buf_copy  (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_buf_sort  (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_buf_implode  (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_bufstr_get  (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_bufstr_set  (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_bufstr_add  (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_bufstr_free  (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_buf_cvarlist  (progfuncs_t *prinst, struct globalvars_s *pr_globals);

void QCBUILTIN PF_calltimeofday (progfuncs_t *prinst, struct globalvars_s *pr_globals);

void QCBUILTIN PF_whichpack (progfuncs_t *prinst, struct globalvars_s *pr_globals);

void PF_fclose_progs (progfuncs_t *prinst);
int QCEditor (progfuncs_t *prinst, char *filename, int line, int nump, char **parms);
void PF_Common_RegisterCvars(void);





/*these are server ones, provided by pr_cmds.c, as required by pr_q1qvm.c*/
#ifdef VM_Q1
model_t *SVPR_GetCModel(world_t *w, int modelindex);
void SVPR_Event_Touch(world_t *w, wedict_t *s, wedict_t *o);
void QCBUILTIN PF_WriteByte (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_WriteChar (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_WriteShort (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_WriteLong (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_WriteAngle (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_WriteCoord (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_WriteFloat (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_WriteEntity (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_multicast (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_svtraceline (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_changelevel (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_applylightstyle(int style, char *val, int col);
void PF_ambientsound_Internal (float *pos, char *samp, float vol, float attenuation);
void QCBUILTIN PF_makestatic (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_logfrag (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_ExecuteCommand  (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_setspawnparms (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_ForceInfoKey(progfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_precache_vwep_model(progfuncs_t *prinst, struct globalvars_s *pr_globals);
int PF_checkclient_Internal (progfuncs_t *prinst);
void PF_precache_sound_Internal (progfuncs_t *prinst, char *s);
int PF_precache_model_Internal (progfuncs_t *prinst, char *s, qboolean queryonly);
void PF_setmodel_Internal (progfuncs_t *prinst, edict_t *e, char *m);
char *PF_infokey_Internal (int entnum, char *value);
void PF_centerprint_Internal (int entnum, qboolean plaque, char *s);
void PF_WriteString_Internal (int target, char *str);
pbool ED_CanFree (edict_t *ed);
#endif

#define	MOVETYPE_NONE			0		// never moves
#define	MOVETYPE_ANGLENOCLIP	1
#define	MOVETYPE_ANGLECLIP		2
#define	MOVETYPE_WALK			3		// gravity
#define	MOVETYPE_STEP			4		// gravity, special edge handling
#define	MOVETYPE_FLY			5
#define	MOVETYPE_TOSS			6		// gravity
#define	MOVETYPE_PUSH			7		// no clip to world, push and crush
#define	MOVETYPE_NOCLIP			8
#define	MOVETYPE_FLYMISSILE		9		// extra size to monsters
#define	MOVETYPE_BOUNCE			10
#define MOVETYPE_BOUNCEMISSILE	11		// bounce w/o gravity
#define MOVETYPE_FOLLOW			12		// track movement of aiment
#define MOVETYPE_H2PUSHPULL		13		// pushable/pullable object
#define MOVETYPE_H2SWIM			14		// should keep the object in water
#define MOVETYPE_PHYSICS		32

// edict->solid values
#define	SOLID_NOT				0		// no interaction with other objects
#define	SOLID_TRIGGER			1		// touch on edge, but not blocking
#define	SOLID_BBOX				2		// touch on edge, block
#define	SOLID_SLIDEBOX			3		// touch on edge, but not an onground
#define	SOLID_BSP				4		// bsp clip, touch on edge, block
#define	SOLID_PHASEH2			5		// hexen2 flag - these ents can be freely walked through or something
#define	SOLID_CORPSE			5		// non-solid to solid_slidebox entities and itself.
#define SOLID_LADDER			20		//dmw. touch on edge, not blocking. Touching players have different physics. Otherwise a SOLID_TRIGGER
#define	SOLID_PHYSICS_BOX		32		///< physics object (mins, maxs, mass, origin, axis_forward, axis_left, axis_up, velocity, spinvelocity)
#define	SOLID_PHYSICS_SPHERE	33		///< physics object (mins, maxs, mass, origin, axis_forward, axis_left, axis_up, velocity, spinvelocity)
#define	SOLID_PHYSICS_CAPSULE	34		///< physics object (mins, maxs, mass, origin, axis_forward, axis_left, axis_up, velocity, spinvelocity)


#define JOINTTYPE_POINT 1
#define JOINTTYPE_HINGE 2
#define JOINTTYPE_SLIDER 3
#define JOINTTYPE_UNIVERSAL 4
#define JOINTTYPE_HINGE2 5
#define JOINTTYPE_FIXED -1

#define	DAMAGE_NO				0
#define	DAMAGE_YES				1
#define	DAMAGE_AIM				2

//shared constants
typedef enum
{
	VF_MIN = 1,
	VF_MIN_X = 2,
	VF_MIN_Y = 3,
	VF_SIZE = 4,
	VF_SIZE_X = 5,
	VF_SIZE_Y = 6,
	VF_VIEWPORT = 7,
	VF_FOV = 8,
	VF_FOVX = 9,
	VF_FOVY = 10,
	VF_ORIGIN = 11,
	VF_ORIGIN_X = 12,
	VF_ORIGIN_Y = 13,
	VF_ORIGIN_Z = 14,
	VF_ANGLES = 15,
	VF_ANGLES_X = 16,
	VF_ANGLES_Y = 17,
	VF_ANGLES_Z = 18,
	VF_DRAWWORLD = 19,
	VF_ENGINESBAR = 20,
	VF_DRAWCROSSHAIR = 21,
	VF_CARTESIAN_ANGLES = 22,

	//this is a DP-compatibility hack.
	VF_CL_VIEWANGLES_V = 33,
	VF_CL_VIEWANGLES_X = 34,
	VF_CL_VIEWANGLES_Y = 35,
	VF_CL_VIEWANGLES_Z = 36,


	//33-36 used by DP...
	VF_PERSPECTIVE = 200,
	//201 used by DP... WTF? CLEARSCREEN
	VF_LPLAYER = 202,
	VF_AFOV = 203,	//aproximate fov (match what the engine would normally use for the fov cvar). p0=fov, p1=zoom
} viewflags;

/*FIXME: this should be changed*/
#define CSQC_API_VERSION 1.0f

#define CSQCRF_VIEWMODEL		1 //Not drawn in mirrors
#define CSQCRF_EXTERNALMODEL	2 //drawn ONLY in mirrors
#define CSQCRF_DEPTHHACK		4 //fun depthhack
#define CSQCRF_ADDITIVE			8 //add instead of blend
#define CSQCRF_USEAXIS			16 //use v_forward/v_right/v_up as an axis/matrix - predraw is needed to use this properly
#define CSQCRF_NOSHADOW			32 //don't cast shadows upon other entities (can still be self shadowing, if the engine wishes, and not additive)
#define CSQCRF_FRAMETIMESARESTARTTIMES 64 //EXT_CSQC_1: frame times should be read as (time-frametime).
#define CSQCRF_NOAUTOADD		128 //EXT_CSQC_1: don't automatically add after predraw was called

/*only read+append+write are standard frik_file*/
#define FRIK_FILE_READ		0 /*read-only*/
#define FRIK_FILE_APPEND	1 /*append (write-only, but offset begins at end of previous file)*/
#define FRIK_FILE_WRITE		2 /*write-only*/
#define FRIK_FILE_INVALID	3 /*no idea what this is for, presume placeholder*/
#define FRIK_FILE_READNL	4 /*fgets ignores newline chars, returning the entire thing in one lump*/
#define FRIK_FILE_MMAP_READ	5 /*fgets returns a pointer. memory is not guarenteed to be released.*/
#define FRIK_FILE_MMAP_RW	6 /*fgets returns a pointer. file is written upon close. memory is not guarenteed to be released.*/

#define MASK_DELTA 1
#define MASK_STDVIEWMODEL 2

enum lightfield_e
{
	lfield_origin=0,
	lfield_colour=1,
	lfield_radius=2,
	lfield_flags=3,
	lfield_style=4,
	lfield_angles=5,
	lfield_fov=6
};