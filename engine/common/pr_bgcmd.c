#include "quakedef.h"

#if !defined(CLIENTONLY) || defined(CSQC_DAT) || defined(MENU_DAT)

#include "pr_common.h"

#include <ctype.h>

//fixme
#define Z_QC_TAG 2
#define PRSTR	0xa6ffb3d7

static char *cvargroup_progs = "Progs variables";

cvar_t pr_brokenfloatconvert = SCVAR("pr_brokenfloatconvert", "0");
cvar_t pr_tempstringcount = SCVAR("pr_tempstringcount", "");//"16");
cvar_t pr_tempstringsize = SCVAR("pr_tempstringsize", "4096");
cvar_t  dpcompat_stats = CVAR("dpcompat_stats", "0");

static char *strtoupper(char *s)
{
	char *p;

	p = s;
	while(*p)
	{
		*p = toupper(*p);
		p++;
	}

	return s;
}

static char *strtolower(char *s)
{
	char *p;

	p = s;
	while(*p)
	{
		*p = tolower(*p);
		p++;
	}

	return s;
}

void PF_Common_RegisterCvars(void)
{
	static qboolean alreadydone;
	if (alreadydone)
		return;
	alreadydone = true;

	Cvar_Register (&pr_brokenfloatconvert, cvargroup_progs);
	Cvar_Register (&pr_tempstringcount, cvargroup_progs);
	Cvar_Register (&pr_tempstringsize, cvargroup_progs);
	Cvar_Register (&dpcompat_stats, cvargroup_progs);
}

char *Translate(char *message);
char *PF_VarString (progfuncs_t *prinst, int	first, struct globalvars_s *pr_globals)
{
#define VARSTRINGLEN 16384+8
	int		i;
	static char buffer[2][VARSTRINGLEN];
	static int bufnum;
	char *s, *out;

	out = buffer[(bufnum++)&1];

	out[0] = 0;
	for (i=first ; i<*prinst->callargc ; i++)
	{
//		if (G_INT(OFS_PARM0+i*3) < 0 || G_INT(OFS_PARM0+i*3) >= 1024*1024);
//			break;

		s = PR_GetStringOfs(prinst, OFS_PARM0+i*3);
		if (s)
		{
			s = Translate(s);
			if (strlen(out)+strlen(s)+1 >= VARSTRINGLEN)
				Con_DPrintf("VarString (builtin call ending with strings) exceeded maximum string length of %i chars", VARSTRINGLEN);

			Q_strncatz (out, s, VARSTRINGLEN);
		}
	}
	return out;
}

#define MAX_TEMPSTRS	((int)pr_tempstringcount.value)
#define MAXTEMPBUFFERLEN	((int)pr_tempstringsize.value)
string_t PR_TempString(progfuncs_t *prinst, char *str)
{
	char *tmp;
	if (!prinst->tempstringbase)
		return prinst->TempString(prinst, str);

	if (!str || !*str)
		return 0;

	if (prinst->tempstringnum == MAX_TEMPSTRS)
		prinst->tempstringnum = 0;
	tmp = prinst->tempstringbase + (prinst->tempstringnum++)*MAXTEMPBUFFERLEN;

	Q_strncpyz(tmp, str, MAXTEMPBUFFERLEN);
	return tmp - prinst->stringtable;
}

void PF_InitTempStrings(progfuncs_t *prinst)
{
	if (pr_tempstringcount.value > 0 && pr_tempstringcount.value < 2)
		pr_tempstringcount.value = 2;
	if (pr_tempstringsize.value < 256)
		pr_tempstringsize.value = 256;
	pr_tempstringcount.flags |= CVAR_NOSET;
	pr_tempstringsize.flags |= CVAR_NOSET;

	if (pr_tempstringcount.value >= 2)
		prinst->tempstringbase = prinst->AddString(prinst, "", MAXTEMPBUFFERLEN*MAX_TEMPSTRS);
	else
		prinst->tempstringbase = 0;
	prinst->tempstringnum = 0;
}

//#define	RETURN_EDICT(pf, e) (((int *)pr_globals)[OFS_RETURN] = EDICT_TO_PROG(pf, e))
#define	RETURN_SSTRING(s) (((int *)pr_globals)[OFS_RETURN] = PR_SetString(prinst, s))	//static - exe will not change it.
#define	RETURN_TSTRING(s) (((int *)pr_globals)[OFS_RETURN] = PR_TempString(prinst, s))	//temp (static but cycle buffers)
#define	RETURN_CSTRING(s) (((int *)pr_globals)[OFS_RETURN] = PR_SetString(prinst, s))	//semi-permanant. (hash tables?)
#define	RETURN_PSTRING(s) (((int *)pr_globals)[OFS_RETURN] = PR_NewString(prinst, s, 0))	//permanant



void VARGS PR_BIError(progfuncs_t *progfuncs, char *format, ...)
{
	va_list		argptr;
	static char		string[2048];

	va_start (argptr, format);
	vsnprintf (string,sizeof(string)-1, format,argptr);
	va_end (argptr);

	if (developer.value)
	{
		struct globalvars_s *pr_globals = PR_globals(progfuncs, PR_CURRENT);
		Con_Printf("%s\n", string);
		*progfuncs->pr_trace = 1;
		G_INT(OFS_RETURN)=0;	//just in case it was a float and should be an ent...
		G_INT(OFS_RETURN+1)=0;
		G_INT(OFS_RETURN+2)=0;
	}
	else
	{
		PR_StackTrace(progfuncs);
		PR_AbortStack(progfuncs);
		progfuncs->parms->Abort ("%s", string);
	}
}


pbool QC_WriteFile(const char *name, void *data, int len)
{
	char buffer[256];
	Q_snprintfz(buffer, sizeof(buffer), "%s", name);
	COM_WriteFile(buffer, data, len);
	return true;
}

//a little loop so we can keep track of used mem
void *VARGS PR_CB_Malloc(int size)
{
	return BZ_Malloc(size);//Z_TagMalloc (size, 100);
}
void VARGS PR_CB_Free(void *mem)
{
	BZ_Free(mem);
}

////////////////////////////////////////////////////
//Finding

/*
//entity(string field, float match) findchainflags = #450
//chained search for float, int, and entity reference fields
void PF_findchainflags (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int i, f;
	int s;
	edict_t	*ent, *chain;

	chain = (edict_t *) *prinst->parms->sv_edicts;

	f = G_INT(OFS_PARM0)+prinst->fieldadjust;
	s = G_FLOAT(OFS_PARM1);

	for (i = 1; i < *prinst->parms->sv_num_edicts; i++)
	{
		ent = EDICT_NUM(prinst, i);
		if (ent->isfree)
			continue;
		if (!((int)((float *)ent->v)[f] & s))
			continue;

		ent->v->chain = EDICT_TO_PROG(prinst, chain);
		chain = ent;
	}

	RETURN_EDICT(prinst, chain);
}
*/

/*
//entity(string field, float match) findchainfloat = #403
void PF_findchainfloat (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int i, f;
	float s;
	edict_t	*ent, *chain;

	chain = (edict_t *) *prinst->parms->sv_edicts;

	f = G_INT(OFS_PARM0)+prinst->fieldadjust;
	s = G_FLOAT(OFS_PARM1);

	for (i = 1; i < *prinst->parms->sv_num_edicts; i++)
	{
		ent = EDICT_NUM(prinst, i);
		if (ent->isfree)
			continue;
		if (((float *)ent->v)[f] != s)
			continue;

		ent->v->chain = EDICT_TO_PROG(prinst, chain);
		chain = ent;
	}

	RETURN_EDICT(prinst, chain);
}
*/

/*
//entity(string field, string match) findchain = #402
//chained search for strings in entity fields
void PF_findchain (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int i, f;
	char *s;
	string_t t;
	edict_t *ent, *chain;

	chain = (edict_t *) *prinst->parms->sv_edicts;

	f = G_INT(OFS_PARM0)+prinst->fieldadjust;
	s = PR_GetStringOfs(prinst, OFS_PARM1);

	for (i = 1; i < *prinst->parms->sv_num_edicts; i++)
	{
		ent = EDICT_NUM(prinst, i);
		if (ent->isfree)
			continue;
		t = *(string_t *)&((float*)ent->v)[f];
		if (!t)
			continue;
		if (strcmp(PR_GetString(prinst, t), s))
			continue;

		ent->v->chain = EDICT_TO_PROG(prinst, chain);
		chain = ent;
	}

	RETURN_EDICT(prinst, chain);
}
*/

//EXTENSION: DP_QC_FINDFLAGS
//entity(entity start, float fld, float match) findflags = #449
void QCBUILTIN PF_FindFlags (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int e, f;
	int s;
	edict_t *ed;

	e = G_EDICTNUM(prinst, OFS_PARM0);
	f = G_INT(OFS_PARM1)+prinst->fieldadjust;
	s = G_FLOAT(OFS_PARM2);

	for (e++; e < *prinst->parms->sv_num_edicts; e++)
	{
		ed = EDICT_NUM(prinst, e);
		if (ed->isfree)
			continue;
		if ((int)((float *)ed->v)[f] & s)
		{
			RETURN_EDICT(prinst, ed);
			return;
		}
	}

	RETURN_EDICT(prinst, *prinst->parms->sv_edicts);
}

//entity(entity start, float fld, float match) findfloat = #98
void QCBUILTIN PF_FindFloat (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int e, f;
	int s;
	edict_t *ed;

	if (*prinst->callargc != 3)	//I can hate mvdsv if I want to.
	{
		PR_BIError(prinst, "PF_FindFloat (#98): callargc != 3\nDid you mean to set pr_imitatemvdsv to 1?");
		return;
	}

	e = G_EDICTNUM(prinst, OFS_PARM0);
	f = G_INT(OFS_PARM1)+prinst->fieldadjust;
	s = G_INT(OFS_PARM2);

	for (e++; e < *prinst->parms->sv_num_edicts; e++)
	{
		ed = EDICT_NUM(prinst, e);
		if (ed->isfree)
			continue;
		if (((int *)ed->v)[f] == s)
		{
			RETURN_EDICT(prinst, ed);
			return;
		}
	}

	RETURN_EDICT(prinst, *prinst->parms->sv_edicts);
}

// entity (entity start, .string field, string match) find = #5;
void QCBUILTIN PF_FindString (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int		e;
	int		f;
	char	*s;
	string_t t;
	edict_t	*ed;

	e = G_EDICTNUM(prinst, OFS_PARM0);
	f = G_INT(OFS_PARM1)+prinst->fieldadjust;
	s = PR_GetStringOfs(prinst, OFS_PARM2);
	if (!s)
	{
		PR_BIError (prinst, "PF_FindString: bad search string");
		return;
	}

	for (e++ ; e < *prinst->parms->sv_num_edicts ; e++)
	{
		ed = EDICT_NUM(prinst, e);
		if (ed->isfree)
			continue;
		t = ((string_t *)ed->v)[f];
		if (!t)
			continue;
		if (!strcmp(PR_GetString(prinst, t),s))
		{
			RETURN_EDICT(prinst, ed);
			return;
		}
	}

	RETURN_EDICT(prinst, *prinst->parms->sv_edicts);
}

//Finding
////////////////////////////////////////////////////
//Cvars

//string(string cvarname) cvar_string
void QCBUILTIN PF_cvar_string (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*str = PR_GetStringOfs(prinst, OFS_PARM0);
	cvar_t *cv = Cvar_Get(str, "", 0, "QC variables");
	RETURN_CSTRING(cv->string);
}

//string(string cvarname) cvar_defstring
void QCBUILTIN PF_cvar_defstring (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*str = PR_GetStringOfs(prinst, OFS_PARM0);
	cvar_t *cv = Cvar_Get(str, "", 0, "QC variables");
	RETURN_CSTRING(cv->defaultstr);
}

//string(string cvarname) cvar_description
void QCBUILTIN PF_cvar_description (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*str = PR_GetStringOfs(prinst, OFS_PARM0);
	cvar_t *cv = Cvar_Get(str, "", 0, "QC variables");
	RETURN_CSTRING(cv->description);
}

//float(string name) cvar_type
void QCBUILTIN PF_cvar_type (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*str = PR_GetStringOfs(prinst, OFS_PARM0);
	int ret = 0;
	cvar_t *v;

	v = Cvar_FindVar(str);
	if (v)
	{
		ret |= 1; // CVAR_EXISTS
		if(v->flags & CVAR_ARCHIVE)
			ret |= 2; // CVAR_TYPE_SAVED
		if(v->flags & CVAR_NOTFROMSERVER)
			ret |= 4; // CVAR_TYPE_PRIVATE
		if(!(v->flags & CVAR_USERCREATED))
			ret |= 8; // CVAR_TYPE_ENGINE
		
		//fte cvars don't support this
		//	ret |= 16; // CVAR_TYPE_HASDESCRIPTION
	}
	G_FLOAT(OFS_RETURN) = ret;
}

//void(string cvarname, string newvalue) cvar
void QCBUILTIN PF_cvar_set (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*var_name, *val;
	cvar_t *var;

	var_name = PR_GetStringOfs(prinst, OFS_PARM0);
	val = PR_GetStringOfs(prinst, OFS_PARM1);

	var = Cvar_Get(var_name, val, 0, "QC variables");
	if (!var)
		return;
	Cvar_Set (var, val);
}

void QCBUILTIN PF_cvar_setf (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*var_name;
	float	val;
	cvar_t *var;

	var_name = PR_GetStringOfs(prinst, OFS_PARM0);
	val = G_FLOAT(OFS_PARM1);

	var = Cvar_FindVar(var_name);
	if (!var)
		Con_Printf("PF_cvar_set: variable %s not found\n", var_name);
	else
		Cvar_SetValue (var, val);
}

//float(string name, string value) registercvar
void QCBUILTIN PF_registercvar (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *name, *value;
	value = PR_GetStringOfs(prinst, OFS_PARM0);

	if (Cvar_FindVar(value))
		G_FLOAT(OFS_RETURN) = 0;
	else
	{
		name = BZ_Malloc(strlen(value)+1);
		strcpy(name, value);
		if (*prinst->callargc > 1)
			value = PR_GetStringOfs(prinst, OFS_PARM1);
		else
			value = "";

	// archive?
		if (Cvar_Get(name, value, CVAR_USERCREATED, "QC created vars"))
			G_FLOAT(OFS_RETURN) = 1;
		else
			G_FLOAT(OFS_RETURN) = 0;
	}
}

//Cvars
////////////////////////////////////////////////////
//File access

#define MAX_QC_FILES 256

#define FIRST_QC_FILE_INDEX 1000

typedef struct {
	char name[256];
	char *data;
	int bufferlen;
	int len;
	int ofs;
	int accessmode;
	progfuncs_t *prinst;
} pf_fopen_files_t;
pf_fopen_files_t pf_fopen_files[MAX_QC_FILES];

/*only read+append+write are standard frik_file*/
#define FRIK_FILE_READ		0 /*read-only*/
#define FRIK_FILE_APPEND	1 /*append (write-only, but offset begins at end of previous file)*/
#define FRIK_FILE_WRITE		2 /*write-only*/
#define FRIK_FILE_INVALID	3 /*no idea what this is for, presume placeholder*/
#define FRIK_FILE_READNL	4 /*fgets ignores newline chars, returning the entire thing in one lump*/
#define FRIK_FILE_MMAP_READ	5 /*fgets returns a pointer. memory is not guarenteed to be released.*/
#define FRIK_FILE_MMAP_RW	6 /*fgets returns a pointer. file is written upon close. memory is not guarenteed to be released.*/

void QCBUILTIN PF_fopen (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *name = PR_GetStringOfs(prinst, OFS_PARM0);
	int fmode = G_FLOAT(OFS_PARM1);
	int fsize = G_FLOAT(OFS_PARM2);
	int i;

	for (i = 0; i < MAX_QC_FILES; i++)
		if (!pf_fopen_files[i].data)
			break;

	if (i == MAX_QC_FILES)	//too many already open
	{
		Con_Printf("qcfopen: too many files open (trying %s)\n", name);
		G_FLOAT(OFS_RETURN) = -1;
		return;
	}

	if (name[1] == ':' ||	//dos filename absolute path specified - reject.
		strchr(name, '\\') || *name == '/' ||	//absolute path was given - reject
		strstr(name, ".."))	//someone tried to be cleaver.
	{
		G_FLOAT(OFS_RETURN) = -1;
		return;
	}

	Q_strncpyz(pf_fopen_files[i].name, va("data/%s", name), sizeof(pf_fopen_files[i].name));

	pf_fopen_files[i].accessmode = fmode;
	switch (fmode)
	{
	case FRIK_FILE_MMAP_READ:
	case FRIK_FILE_MMAP_RW:
		{
			vfsfile_t *f = FS_OpenVFS(pf_fopen_files[i].name, "rb", FS_GAME);
			if (f)
			{
				pf_fopen_files[i].bufferlen = pf_fopen_files[i].len = VFS_GETLEN(f);
				if (pf_fopen_files[i].bufferlen < fsize)
					pf_fopen_files[i].bufferlen = fsize;
				pf_fopen_files[i].data = PR_AddressableAlloc(prinst, pf_fopen_files[i].bufferlen);
				VFS_READ(f, pf_fopen_files[i].data, pf_fopen_files[i].len);
				VFS_CLOSE(f);
			}
			else
			{
				pf_fopen_files[i].bufferlen = fsize;
				pf_fopen_files[i].data = PR_AddressableAlloc(prinst, pf_fopen_files[i].bufferlen);
			}

			if (!pf_fopen_files[i].data)
			{
				G_FLOAT(OFS_RETURN) = -1;
				break;
			}

			pf_fopen_files[i].len = pf_fopen_files[i].bufferlen;
			pf_fopen_files[i].ofs = 0;
			G_FLOAT(OFS_RETURN) = i + FIRST_QC_FILE_INDEX;
			pf_fopen_files[i].prinst = prinst;
		}
		break;
	case FRIK_FILE_READ:	//read
	case FRIK_FILE_READNL:	//read whole file
		pf_fopen_files[i].data = FS_LoadMallocFile(pf_fopen_files[i].name);
		if (!pf_fopen_files[i].data)
		{
			Q_strncpyz(pf_fopen_files[i].name, name, sizeof(pf_fopen_files[i].name));
			pf_fopen_files[i].data = FS_LoadMallocFile(pf_fopen_files[i].name);
		}

		if (pf_fopen_files[i].data)
		{
			G_FLOAT(OFS_RETURN) = i + FIRST_QC_FILE_INDEX;
			pf_fopen_files[i].prinst = prinst;
		}
		else
			G_FLOAT(OFS_RETURN) = -1;

		pf_fopen_files[i].bufferlen = pf_fopen_files[i].len = com_filesize;
		pf_fopen_files[i].ofs = 0;
		break;
	case FRIK_FILE_APPEND:	//append
		pf_fopen_files[i].data = FS_LoadMallocFile(pf_fopen_files[i].name);
		pf_fopen_files[i].ofs = pf_fopen_files[i].bufferlen = pf_fopen_files[i].len = com_filesize;
		if (pf_fopen_files[i].data)
		{
			G_FLOAT(OFS_RETURN) = i + FIRST_QC_FILE_INDEX;
			pf_fopen_files[i].prinst = prinst;
			break;
		}
		//file didn't exist - fall through
	case FRIK_FILE_WRITE:	//write
		pf_fopen_files[i].bufferlen = 8192;
		pf_fopen_files[i].data = BZ_Malloc(pf_fopen_files[i].bufferlen);
		pf_fopen_files[i].len = 0;
		pf_fopen_files[i].ofs = 0;
		G_FLOAT(OFS_RETURN) = i + FIRST_QC_FILE_INDEX;
		pf_fopen_files[i].prinst = prinst;
		break;
	case FRIK_FILE_INVALID:
		pf_fopen_files[i].bufferlen = 0;
		pf_fopen_files[i].data = "";
		pf_fopen_files[i].len = 0;
		pf_fopen_files[i].ofs = 0;
		G_FLOAT(OFS_RETURN) = i + FIRST_QC_FILE_INDEX;
		pf_fopen_files[i].prinst = prinst;
		break;
	default: //bad
		G_FLOAT(OFS_RETURN) = -1;
		break;
	}
}

void PF_fclose_i (int fnum)
{
	if (fnum < 0 || fnum >= MAX_QC_FILES)
	{
		Con_Printf("PF_fclose: File out of range\n");
		return;	//out of range
	}

	if (!pf_fopen_files[fnum].data)
	{
		Con_Printf("PF_fclose: File is not open\n");
		return;	//not open
	}

	switch(pf_fopen_files[fnum].accessmode)
	{
	case FRIK_FILE_MMAP_RW:
		COM_WriteFile(pf_fopen_files[fnum].name, pf_fopen_files[fnum].data, pf_fopen_files[fnum].len);
		/*fall through*/
	case FRIK_FILE_MMAP_READ:
		/*cannot free accessible mem*/
		break;

	case FRIK_FILE_READ:
	case 4:
		BZ_Free(pf_fopen_files[fnum].data);
		break;
	case 1:
	case 2:
		COM_WriteFile(pf_fopen_files[fnum].name, pf_fopen_files[fnum].data, pf_fopen_files[fnum].len);
		BZ_Free(pf_fopen_files[fnum].data);
		break;
	case 3:
		break;
	}
	pf_fopen_files[fnum].data = NULL;
	pf_fopen_files[fnum].prinst = NULL;
}

void QCBUILTIN PF_fclose (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int fnum = G_FLOAT(OFS_PARM0)-FIRST_QC_FILE_INDEX;

	if (fnum < 0 || fnum >= MAX_QC_FILES)
	{
		Con_Printf("PF_fclose: File out of range\n");
		return;	//out of range
	}

	if (pf_fopen_files[fnum].prinst != prinst)
	{
		Con_Printf("PF_fclose: File is from wrong instance\n");
		return;	//this just isn't ours.
	}

	PF_fclose_i(fnum);
}

void QCBUILTIN PF_fgets (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char c, *s, *o, *max, *eof;
	int fnum = G_FLOAT(OFS_PARM0) - FIRST_QC_FILE_INDEX;
	char pr_string_temp[4096];

	*pr_string_temp = '\0';
	G_INT(OFS_RETURN) = 0;	//EOF
	if (fnum < 0 || fnum >= MAX_QC_FILES)
	{
		PR_BIError(prinst, "PF_fgets: File out of range\n");
		return;	//out of range
	}

	if (!pf_fopen_files[fnum].data)
	{
		PR_BIError(prinst, "PF_fgets: File is not open\n");
		return;	//not open
	}

	if (pf_fopen_files[fnum].prinst != prinst)
	{
		PR_BIError(prinst, "PF_fgets: File is from wrong instance\n");
		return;	//this just isn't ours.
	}

	if (pf_fopen_files[fnum].accessmode == FRIK_FILE_MMAP_READ || pf_fopen_files[fnum].accessmode == FRIK_FILE_MMAP_RW)
	{
		G_INT(OFS_RETURN) = PR_SetString(prinst, pf_fopen_files[fnum].data);
		return;
	}

	//read up to the next \n, ignoring any \rs.
	o = pr_string_temp;
	max = o + sizeof(pr_string_temp)-1;
	s = pf_fopen_files[fnum].data+pf_fopen_files[fnum].ofs;
	eof = pf_fopen_files[fnum].data+pf_fopen_files[fnum].len;
	while(s < eof)
	{
		c = *s++;
		if (c == '\n' && pf_fopen_files[fnum].accessmode != FRIK_FILE_READNL)
			break;
		if (c == '\r' && pf_fopen_files[fnum].accessmode != FRIK_FILE_READNL)
			continue;

		if (o == max)
			break;
		*o++ = c;
	}
	*o = '\0';

	pf_fopen_files[fnum].ofs = s - pf_fopen_files[fnum].data;

	if (!pr_string_temp[0] && s == eof)
		G_INT(OFS_RETURN) = 0;	//EOF
	else
		RETURN_TSTRING(pr_string_temp);
}

void QCBUILTIN PF_fputs (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int fnum = G_FLOAT(OFS_PARM0) - FIRST_QC_FILE_INDEX;
	char *msg = PF_VarString(prinst, 1, pr_globals);
	int len = strlen(msg);
	if (fnum < 0 || fnum >= MAX_QC_FILES)
	{
		Con_Printf("PF_fgets: File out of range\n");
		return;	//out of range
	}

	if (!pf_fopen_files[fnum].data)
	{
		Con_Printf("PF_fgets: File is not open\n");
		return;	//not open
	}

	if (pf_fopen_files[fnum].prinst != prinst)
	{
		Con_Printf("PF_fgets: File is from wrong instance\n");
		return;	//this just isn't ours.
	}

	switch(pf_fopen_files[fnum].accessmode)
	{
	default:
		break;
	case FRIK_FILE_APPEND:
	case FRIK_FILE_WRITE:
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
		break;
	}
}

void PF_fcloseall (progfuncs_t *prinst)
{
	int i;
	for (i = 0; i < MAX_QC_FILES; i++)
	{
		if (pf_fopen_files[i].prinst != prinst)
			continue;
		Con_Printf("qc file %s was still open\n", pf_fopen_files[i].name);
		PF_fclose_i(i);
	}
}


//DP_QC_WHICHPACK
void QCBUILTIN PF_whichpack (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *srcname = PR_GetStringOfs(prinst, OFS_PARM0);
	flocation_t loc;

	if (FS_FLocateFile(srcname, FSLFRT_IFFOUND, &loc))
	{
		srcname = FS_WhichPackForLocation(&loc);
		if (srcname == NULL)
			srcname = "";
		RETURN_TSTRING(srcname);
	}
	else
	{
		G_INT(OFS_RETURN) = 0;	//null/empty
	}

}


typedef struct prvmsearch_s {
	int handle;
	progfuncs_t *fromprogs;	//share across menu/server
	int entries;
	char **names;
	int *sizes;

	struct prvmsearch_s *next;
} prvmsearch_t;
prvmsearch_t *prvmsearches;
int prvm_nextsearchhandle;

void search_close (progfuncs_t *prinst, int handle)
{
	int i;
	prvmsearch_t *prev, *s;

	prev = NULL;
	for (s = prvmsearches; s; )
	{
		if (s->handle == handle)
		{	//close it down.
			if (s->fromprogs != prinst)
			{
				Con_Printf("Handle wasn't valid with that progs\n");
				return;
			}
			if (prev)
				prev->next = s->next;
			else
				prvmsearches = s->next;

			for (i = 0; i < s->entries; i++)
			{
				BZ_Free(s->names[i]);
			}
			BZ_Free(s->names);
			BZ_Free(s->sizes);
			BZ_Free(s);

			return;
		}

		prev = s;
		s = s->next;
	}
}
//a progs was closed... hunt down it's searches, and warn about any searches left open.
void search_close_progs(progfuncs_t *prinst, qboolean complain)
{
	int i;
	prvmsearch_t *prev, *s;

	prev = NULL;
	for (s = prvmsearches; s; )
	{
		if (s->fromprogs == prinst)
		{	//close it down.

			if (complain)
				Con_Printf("Warning: Progs search was still active\n");
			if (prev)
				prev->next = s->next;
			else
				prvmsearches = s->next;

			for (i = 0; i < s->entries; i++)
			{
				BZ_Free(s->names[i]);
			}
			BZ_Free(s->names);
			BZ_Free(s->sizes);
			BZ_Free(s);

			if (prev)
				s = prev->next;
			else
				s = prvmsearches;
			continue;
		}

		prev = s;
		s = s->next;
	}

	if (!prvmsearches)
		prvm_nextsearchhandle = 0;	//might as well.
}

int search_enumerate(const char *name, int fsize, void *parm)
{
	prvmsearch_t *s = parm;

	s->names = BZ_Realloc(s->names, ((s->entries+64)&~63) * sizeof(char*));
	s->sizes = BZ_Realloc(s->sizes, ((s->entries+64)&~63) * sizeof(int));
	s->names[s->entries] = BZ_Malloc(strlen(name)+1);
	strcpy(s->names[s->entries], name);
	s->sizes[s->entries] = fsize;

	s->entries++;
	return true;
}

//float	search_begin(string pattern, float caseinsensitive, float quiet) = #74;
void QCBUILTIN PF_search_begin (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{	//< 0 for error, > 0 for handle.
	char *pattern = PR_GetStringOfs(prinst, OFS_PARM0);
//	qboolean caseinsensative = G_FLOAT(OFS_PARM1);
//	qboolean quiet = G_FLOAT(OFS_PARM2);
	prvmsearch_t *s;

	s = Z_Malloc(sizeof(*s));
	s->fromprogs = prinst;
	s->handle = prvm_nextsearchhandle++;

	COM_EnumerateFiles(pattern, search_enumerate, s);

	if (s->entries==0)
	{
		BZ_Free(s);
		G_FLOAT(OFS_RETURN) = -1;
		return;
	}
	s->next = prvmsearches;
	prvmsearches = s;
	G_FLOAT(OFS_RETURN) = s->handle;
}
//void	search_end(float handle) = #75;
void QCBUILTIN PF_search_end (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int handle = G_FLOAT(OFS_PARM0);
	search_close(prinst, handle);
}
//float	search_getsize(float handle) = #76;
void QCBUILTIN PF_search_getsize (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int handle = G_FLOAT(OFS_PARM0);
	prvmsearch_t *s;
	G_FLOAT(OFS_RETURN) = -1;
	for (s = prvmsearches; s; s = s->next)
	{
		if (s->handle == handle)
		{	//close it down.
			if (s->fromprogs != prinst)
			{
				Con_Printf("Handle wasn't valid with that progs\n");
				return;
			}

			G_FLOAT(OFS_RETURN) = s->entries;
			return;
		}
	}
}
//string	search_getfilename(float handle, float num) = #77;
void QCBUILTIN PF_search_getfilename (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int handle = G_FLOAT(OFS_PARM0);
	int num = G_FLOAT(OFS_PARM1);
	prvmsearch_t *s;
	G_INT(OFS_RETURN) = 0;

	for (s = prvmsearches; s; s = s->next)
	{
		if (s->handle == handle)
		{	//close it down.
			if (s->fromprogs != prinst)
			{
				Con_Printf("Search handle wasn't valid with that progs\n");
				return;
			}

			if (num < 0 || num >= s->entries)
				return;
			RETURN_TSTRING(s->names[num]);
			return;
		}
	}

	Con_Printf("Search handle wasn't valid\n");
}

//closes filesystem type stuff for when a progs has stopped needing it.
void PR_fclose_progs (progfuncs_t *prinst)
{
	PF_fcloseall(prinst);
	search_close_progs(prinst, true);
}

//File access
////////////////////////////////////////////////////
//Entities

void QCBUILTIN PF_WasFreed (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t	*ent;
	ent = (edict_t*)G_EDICT(prinst, OFS_PARM0);
	G_FLOAT(OFS_RETURN) = ent->isfree;
}

void QCBUILTIN PF_num_for_edict (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t	*ent;
	ent = (edict_t*)G_EDICT(prinst, OFS_PARM0);
	G_FLOAT(OFS_RETURN) = ent->entnum;
}

void QCBUILTIN PF_edict_for_num(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t	*ent;
	ent = (edict_t*)EDICT_NUM(prinst, G_FLOAT(OFS_PARM0));

	RETURN_EDICT(prinst, ent);
}

//entity nextent(entity)
void QCBUILTIN PF_nextent (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int		i;
	edict_t	*ent;

	i = G_EDICTNUM(prinst, OFS_PARM0);
	while (1)
	{
		i++;
		if (i == *prinst->parms->sv_num_edicts)
		{
			RETURN_EDICT(prinst, *prinst->parms->sv_edicts);
			return;
		}
		ent = EDICT_NUM(prinst, i);
		if (!ent->isfree)
		{
			RETURN_EDICT(prinst, ent);
			return;
		}
	}
}

//entity() spawn
void QCBUILTIN PF_Spawn (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	struct edict_s	*ed;
	ed = ED_Alloc(prinst);
	RETURN_EDICT(prinst, ed);
}

//Entities
////////////////////////////////////////////////////
//String functions

//PF_dprint
void QCBUILTIN PF_dprint (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	Con_DPrintf ("%s",PF_VarString(prinst, 0, pr_globals));
}

//PF_print
void QCBUILTIN PF_print (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	Con_Printf ("%s",PF_VarString(prinst, 0, pr_globals));
}

//FTE_STRINGS
//C style strncasecmp (compare first n characters - case insensative)
void QCBUILTIN PF_strncasecmp (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *a = PR_GetStringOfs(prinst, OFS_PARM0);
	char *b = PR_GetStringOfs(prinst, OFS_PARM1);
	float len = G_FLOAT(OFS_PARM2);

	G_FLOAT(OFS_RETURN) = strnicmp(a, b, len);
}

//FTE_STRINGS
//C style strcasecmp (case insensative string compare)
void QCBUILTIN PF_strcasecmp (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *a = PR_GetStringOfs(prinst, OFS_PARM0);
	char *b = PR_GetStringOfs(prinst, OFS_PARM1);

	G_FLOAT(OFS_RETURN) = stricmp(a, b);
}

//FTE_STRINGS
//C style strncmp (compare first n characters - case sensative. Note that there is no strcmp provided)
void QCBUILTIN PF_strncmp (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *a = PR_GetStringOfs(prinst, OFS_PARM0);
	char *b = PR_GetStringOfs(prinst, OFS_PARM1);
	float len = G_FLOAT(OFS_PARM2);

	G_FLOAT(OFS_RETURN) = strncmp(a, b, len);
}

//uses qw style \key\value strings
void QCBUILTIN PF_infoget (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *info = PR_GetStringOfs(prinst, OFS_PARM0);
	char *key = PR_GetStringOfs(prinst, OFS_PARM1);

	key = Info_ValueForKey(info, key);

	RETURN_TSTRING(key);
}

//uses qw style \key\value strings
void QCBUILTIN PF_infoadd (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *info = PR_GetStringOfs(prinst, OFS_PARM0);
	char *key = PR_GetStringOfs(prinst, OFS_PARM1);
	char *value = PF_VarString(prinst, 2, pr_globals);
	char temp[8192];

	Q_strncpyz(temp, info, MAXTEMPBUFFERLEN);

	Info_SetValueForStarKey(temp, key, value, MAXTEMPBUFFERLEN);

	RETURN_TSTRING(temp);
}

//string(float pad, string str1, ...) strpad
void QCBUILTIN PF_strpad (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char destbuf[4096];
	char *dest = destbuf;
	int pad = G_FLOAT(OFS_PARM0);
	char *src = PF_VarString(prinst, 1, pr_globals);

	if (pad < 0)
	{	//pad left
		pad = -pad - strlen(src);
		if (pad>=MAXTEMPBUFFERLEN)
			pad = MAXTEMPBUFFERLEN-1;
		if (pad < 0)
			pad = 0;

		Q_strncpyz(dest+pad, src, MAXTEMPBUFFERLEN-pad);
		while(pad--)
		{
			pad--;
			dest[pad] = ' ';
		}
	}
	else
	{	//pad right
		if (pad>=MAXTEMPBUFFERLEN)
			pad = MAXTEMPBUFFERLEN-1;
		pad -= strlen(src);
		if (pad < 0)
			pad = 0;

		Q_strncpyz(dest, src, MAXTEMPBUFFERLEN);
		dest+=strlen(dest);

		while(pad-->0)
			*dest++ = ' ';
		*dest = '\0';
	}

	RETURN_TSTRING(destbuf);
}

//part of PF_strconv
static int chrconv_number(int i, int base, int conv)
{
	i -= base;
	switch (conv)
	{
	default:
	case 5:
	case 6:
	case 0:
		break;
	case 1:
		base = '0';
		break;
	case 2:
		base = '0'+128;
		break;
	case 3:
		base = '0'-30;
		break;
	case 4:
		base = '0'+128-30;
		break;
	}
	return i + base;
}
//part of PF_strconv
static int chrconv_punct(int i, int base, int conv)
{
	i -= base;
	switch (conv)
	{
	default:
	case 0:
		break;
	case 1:
		base = 0;
		break;
	case 2:
		base = 128;
		break;
	}
	return i + base;
}
//part of PF_strconv
static int chrchar_alpha(int i, int basec, int baset, int convc, int convt, int charnum)
{
	//convert case and colour seperatly...

	i -= baset + basec;
	switch (convt)
	{
	default:
	case 0:
		break;
	case 1:
		baset = 0;
		break;
	case 2:
		baset = 128;
		break;

	case 5:
	case 6:
		baset = 128*((charnum&1) == (convt-5));
		break;
	}

	switch (convc)
	{
	default:
	case 0:
		break;
	case 1:
		basec = 'a';
		break;
	case 2:
		basec = 'A';
		break;
	}
	return i + basec + baset;
}
//FTE_STRINGS
//bulk convert a string. change case or colouring.
void QCBUILTIN PF_strconv (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int ccase = G_FLOAT(OFS_PARM0);		//0 same, 1 lower, 2 upper
	int redalpha = G_FLOAT(OFS_PARM1);	//0 same, 1 white, 2 red,  5 alternate, 6 alternate-alternate
	int rednum = G_FLOAT(OFS_PARM2);	//0 same, 1 white, 2 red, 3 redspecial, 4 whitespecial, 5 alternate, 6 alternate-alternate
	unsigned char *string = PF_VarString(prinst, 3, pr_globals);
	int len = strlen(string);
	int i;
	unsigned char resbuf[8192];
	unsigned char *result = resbuf;

	if (len >= MAXTEMPBUFFERLEN)
		len = MAXTEMPBUFFERLEN-1;

	for (i = 0; i < len; i++, string++, result++)	//should this be done backwards?
	{
		if (*string >= '0' && *string <= '9')	//normal numbers...
			*result = chrconv_number(*string, '0', rednum);
		else if (*string >= '0'+128 && *string <= '9'+128)
			*result = chrconv_number(*string, '0'+128, rednum);
		else if (*string >= '0'+128-30 && *string <= '9'+128-30)
			*result = chrconv_number(*string, '0'+128-30, rednum);
		else if (*string >= '0'-30 && *string <= '9'-30)
			*result = chrconv_number(*string, '0'-30, rednum);

		else if (*string >= 'a' && *string <= 'z')	//normal numbers...
			*result = chrchar_alpha(*string, 'a', 0, ccase, redalpha, i);
		else if (*string >= 'A' && *string <= 'Z')	//normal numbers...
			*result = chrchar_alpha(*string, 'A', 0, ccase, redalpha, i);
		else if (*string >= 'a'+128 && *string <= 'z'+128)	//normal numbers...
			*result = chrchar_alpha(*string, 'a', 128, ccase, redalpha, i);
		else if (*string >= 'A'+128 && *string <= 'Z'+128)	//normal numbers...
			*result = chrchar_alpha(*string, 'A', 128, ccase, redalpha, i);

		else if ((*string & 127) < 16 || !redalpha)	//special chars..
			*result = *string;
		else if (*string < 128)
			*result = chrconv_punct(*string, 0, redalpha);
		else
			*result = chrconv_punct(*string, 128, redalpha);
	}
	*result = '\0';

	RETURN_TSTRING(((char*)resbuf));
}

//FTE_STRINGS
//returns a string containing one character per parameter (up to the qc max params of 8).
void QCBUILTIN PF_chr2str (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int i;

	char string[16];
	for (i = 0; i < *prinst->callargc; i++)
		string[i] = G_FLOAT(OFS_PARM0 + i*3);
	string[i] = '\0';
	RETURN_TSTRING(string);
}

//FTE_STRINGS
//returns character at position X
void QCBUILTIN PF_str2chr (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *instr = PR_GetStringOfs(prinst, OFS_PARM0);
	int ofs = (*prinst->callargc>1)?G_FLOAT(OFS_PARM1):0;

	if (ofs < 0)
		ofs = strlen(instr)+ofs;

	if (ofs && (ofs < 0 || ofs > strlen(instr)))
		G_FLOAT(OFS_RETURN) = '\0';
	else
		G_FLOAT(OFS_RETURN) = instr[ofs];
}

//FTE_STRINGS
//strstr, without generating a new string. Use in conjunction with FRIK_FILE's substring for more similar strstr.
void QCBUILTIN PF_strstrofs (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *instr = PR_GetStringOfs(prinst, OFS_PARM0);
	char *match = PR_GetStringOfs(prinst, OFS_PARM1);

	int firstofs = (*prinst->callargc>2)?G_FLOAT(OFS_PARM2):0;

	if (firstofs && (firstofs < 0 || firstofs > strlen(instr)))
	{
		G_FLOAT(OFS_RETURN) = -1;
		return;
	}

	match = strstr(instr+firstofs, match);
	if (!match)
		G_FLOAT(OFS_RETURN) = -1;
	else
		G_FLOAT(OFS_RETURN) = match - instr;
}

//float(string input) stof
void QCBUILTIN PF_stof (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*s;

	s = PR_GetStringOfs(prinst, OFS_PARM0);

	G_FLOAT(OFS_RETURN) = atof(s);
}

//tstring(float input) ftos
void QCBUILTIN PF_ftos (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float	v;
	char pr_string_temp[64];
	v = G_FLOAT(OFS_PARM0);

	if (v == (int)v)
		sprintf (pr_string_temp, "%d",(int)v);
	else if (pr_brokenfloatconvert.value)
		sprintf (pr_string_temp, "%5.1f",v);
	else
		Q_ftoa (pr_string_temp, v);
	RETURN_TSTRING(pr_string_temp);
}

//tstring(integer input) itos
void QCBUILTIN PF_itos (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int	v;
	char pr_string_temp[64];
	v = G_INT(OFS_PARM0);

	sprintf (pr_string_temp, "%d",v);
	RETURN_TSTRING(pr_string_temp);
}

//int(string input) stoi
void QCBUILTIN PF_stoi (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *input = PR_GetStringOfs(prinst, OFS_PARM0);

	G_INT(OFS_RETURN) = atoi(input);
}

//tstring(integer input) htos
void QCBUILTIN PF_htos (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	unsigned int	v;
	char pr_string_temp[64];
	v = G_INT(OFS_PARM0);

	sprintf (pr_string_temp, "%08x",v);
	RETURN_TSTRING(pr_string_temp);
}

//int(string input) stoh
void QCBUILTIN PF_stoh (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *input = PR_GetStringOfs(prinst, OFS_PARM0);

	G_INT(OFS_RETURN) = strtoul(input, NULL, 16);
}

//vector(string s) stov = #117
//returns vector value from a string
void QCBUILTIN PF_stov (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int i;
	char *s;
	float *out;

	s = PF_VarString(prinst, 0, pr_globals);
	out = G_VECTOR(OFS_RETURN);
	out[0] = out[1] = out[2] = 0;

	if (*s == '\'')
		s++;

	for (i = 0; i < 3; i++)
	{
		while (*s == ' ' || *s == '\t')
			s++;
		out[i] = atof (s);
		if (!out[i] && *s != '-' && *s != '+' && (*s < '0' || *s > '9'))
			break; // not a number
		while (*s && *s != ' ' && *s !='\t' && *s != '\'')
			s++;
		if (*s == '\'')
			break;
	}
}

//tstring(vector input) vtos
void QCBUILTIN PF_vtos (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char pr_string_temp[64];
	//sprintf (pr_string_temp, "'%5.1f %5.1f %5.1f'", G_VECTOR(OFS_PARM0)[0], G_VECTOR(OFS_PARM0)[1], G_VECTOR(OFS_PARM0)[2]);
	sprintf (pr_string_temp, "'%f %f %f'", G_VECTOR(OFS_PARM0)[0], G_VECTOR(OFS_PARM0)[1], G_VECTOR(OFS_PARM0)[2]);
	RETURN_TSTRING(pr_string_temp);
}

void QCBUILTIN PF_forgetstring(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *s=PR_RemoveProgsString(prinst, G_INT(OFS_PARM0));
	if (!s)
	{
		Con_Printf("string was not strzoned\n");
		(*prinst->pr_trace) = 1;
		return;
	}
//	char *s=PR_GetStringOfs(prinst, OFS_PARM0);
	s-=8;
	if (((int *)s)[0] != PRSTR)
	{
		Con_Printf("QC tried to free a non dynamic string: ");
		Con_Printf("%s\n", s+8);	//two prints, so that logged prints ensure the first is written.
		(*prinst->pr_trace) = 1;
		PR_StackTrace(prinst);
		return;
	}
	((int *)s)[0] = 0xabcd1234;
	Z_TagFree(s);

}

void QCBUILTIN PF_dupstring(progfuncs_t *prinst, struct globalvars_s *pr_globals)	//frik_file
{
	char *s, *in;
	int len;
	in = PF_VarString(prinst, 0, pr_globals);
	len = strlen(in)+1;
	s = Z_TagMalloc(len+8, Z_QC_TAG);
	((int *)s)[0] = PRSTR;
	((int *)s)[1] = len;
	strcpy(s+8, in);
	RETURN_SSTRING(s+8);
}

//returns a section of a string as a tempstring
void QCBUILTIN PF_substring (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int i, start, length;
	char *s;
	char string[4096];

	s = PR_GetStringOfs(prinst, OFS_PARM0);
	start = G_FLOAT(OFS_PARM1);
	length = G_FLOAT(OFS_PARM2);

	if (start < 0)
		start = strlen(s)-start;
	if (length < 0)
		length = strlen(s)-start+(length+1);
	if (start < 0)
	{
	//	length += start;
		start = 0;
	}

	if (start >= strlen(s) || length<=0 || !*s)
	{
		RETURN_TSTRING("");
		return;
	}

	if (length >= MAXTEMPBUFFERLEN)
		length = MAXTEMPBUFFERLEN-1;

	for (i = 0; i < start && *s; i++, s++)
		;

	for (i = 0; *s && i < length; i++, s++)
		string[i] = *s;
	string[i] = 0;

	RETURN_TSTRING(string);
}

//string(string str1, string str2) strcat
void QCBUILTIN PF_strcat (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char dest[4096];
	char *src = PF_VarString(prinst, 0, pr_globals);
	Q_strncpyz(dest, src, MAXTEMPBUFFERLEN);
	RETURN_TSTRING(dest);
}

void QCBUILTIN PF_strlen(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = strlen(PR_GetStringOfs(prinst, OFS_PARM0));
}

//float(string input, string token) instr
void QCBUILTIN PF_instr (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *sub;
	char *s1;
	char *s2;

	s1 = PR_GetStringOfs(prinst, OFS_PARM0);
	s2 = PF_VarString(prinst, 1, pr_globals);

	if (!s1 || !s2)
	{
		PR_BIError(prinst, "Null string in \"instr\"\n");
		return;
	}

	sub = strstr(s1, s2);

	if (sub == NULL)
		G_INT(OFS_RETURN) = 0;
	else
		RETURN_SSTRING(sub);	//last as long as the original string
}

void QCBUILTIN PF_strreplace (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char resultbuf[4096];
	char *result = resultbuf;
	char *search = PR_GetStringOfs(prinst, OFS_PARM0);
	char *replace = PR_GetStringOfs(prinst, OFS_PARM1);
	char *subject = PR_GetStringOfs(prinst, OFS_PARM2);
	int searchlen = strlen(search);
	int replacelen = strlen(replace);

	if (searchlen)
	{
		while (*subject && result < resultbuf + sizeof(resultbuf) - replacelen - 2)
		{
			if (!strncmp(subject, search, searchlen))
			{
				subject += searchlen;
				memcpy(result, replace, replacelen);
				result += replacelen;
			}
			else
				*result++ = *subject++;
		}
		*result = 0;
		RETURN_TSTRING(resultbuf);
	}
	else
		RETURN_TSTRING(subject);
}
void QCBUILTIN PF_strireplace (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char resultbuf[4096];
	char *result = resultbuf;
	char *search = PR_GetStringOfs(prinst, OFS_PARM0);
	char *replace = PR_GetStringOfs(prinst, OFS_PARM1);
	char *subject = PR_GetStringOfs(prinst, OFS_PARM2);
	int searchlen = strlen(search);
	int replacelen = strlen(replace);

	if (searchlen)
	{
		while (*subject && result < resultbuf + sizeof(resultbuf) - replacelen - 2)
		{
			if (!strnicmp(subject, search, searchlen))
			{
				subject += searchlen;
				memcpy(result, replace, replacelen);
				result += replacelen;
			}
			else
				*result++ = *subject++;
		}
		*result = 0;
		RETURN_TSTRING(resultbuf);
	}
	else
		RETURN_TSTRING(subject);
}

//string(entity ent) etos = #65
void QCBUILTIN PF_etos (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char s[64];
	snprintf (s, sizeof(s), "entity %i", G_EDICTNUM(prinst, OFS_PARM0));
	RETURN_TSTRING(s);
}

//DP_QC_STRINGCOLORFUNCTIONS
// #476 float(string s) strlennocol - returns how many characters are in a string, minus color codes
void QCBUILTIN PF_strlennocol (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *in = PR_GetStringOfs(prinst, OFS_PARM0);
	G_FLOAT(OFS_RETURN) = COM_FunStringLength(in);
}

//DP_QC_STRINGCOLORFUNCTIONS
// string (string s) strdecolorize - returns the passed in string with color codes stripped
void QCBUILTIN PF_strdecolorize (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *in = PR_GetStringOfs(prinst, OFS_PARM0);
	char result[8192];
	unsigned int flagged[8192];
	COM_ParseFunString(CON_WHITEMASK, in, flagged, sizeof(flagged), false);
	COM_DeFunString(flagged, result, sizeof(result), true);

	RETURN_TSTRING(result);
}

//DP_QC_STRING_CASE_FUNCTIONS
void QCBUILTIN PF_strtolower (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *in = PR_GetStringOfs(prinst, OFS_PARM0);
	char result[8192];

	Q_strncpyz(result, in, sizeof(result));
	strtolower(result);

	RETURN_TSTRING(result);
}

//DP_QC_STRING_CASE_FUNCTIONS
void QCBUILTIN PF_strtoupper (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *in = PR_GetStringOfs(prinst, OFS_PARM0);
	char result[8192];

	Q_strncpyz(result, in, sizeof(result));
	strtoupper(result);

	RETURN_TSTRING(result);
}

//DP_QC_STRFTIME
void QCBUILTIN PF_strftime (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *in = PF_VarString(prinst, 1, pr_globals);
	char result[8192];

	time_t ctime;
	struct tm *tm;

	ctime = time(NULL);

	if (G_FLOAT(OFS_PARM0))
		tm = localtime(&ctime);
	else
		tm = gmtime(&ctime);
	strftime(result, sizeof(result), in, tm);
	Q_strncpyz(result, in, sizeof(result));
	strtoupper(result);

	RETURN_TSTRING(result);
}

//String functions
////////////////////////////////////////////////////
//515's String functions

struct strbuf {
	progfuncs_t *prinst;
	char **strings;
	int used;
	int allocated;
};

#define NUMSTRINGBUFS 16
struct strbuf strbuflist[NUMSTRINGBUFS];

// #440 float() buf_create (DP_QC_STRINGBUFFERS)
void QCBUILTIN PF_buf_create  (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	
	int i;

	for (i = 0; i < NUMSTRINGBUFS; i++)
	{
		if (!strbuflist[i].prinst)
		{
			strbuflist[i].prinst = prinst;
			strbuflist[i].used = 0;
			strbuflist[i].allocated = 0;
			strbuflist[i].strings = NULL;
			G_FLOAT(OFS_RETURN) = i+1;
			return;
		}
	}
	G_FLOAT(OFS_RETURN) = 0;
}
// #441 void(float bufhandle) buf_del (DP_QC_STRINGBUFFERS)
void QCBUILTIN PF_buf_del  (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int i;
	int bufno = G_FLOAT(OFS_PARM0)-1;

	if ((unsigned int)bufno >= NUMSTRINGBUFS)
		return;
	if (strbuflist[bufno].prinst != prinst)
		return;

	for (i = 0; i < strbuflist[bufno].used; i++)
		Z_Free(strbuflist[bufno].strings[i]);
	Z_Free(strbuflist[bufno].strings);

	strbuflist[bufno].strings = NULL;
	strbuflist[bufno].used = 0;
	strbuflist[bufno].allocated = 0;

	strbuflist[bufno].prinst = NULL;
}
// #442 float(float bufhandle) buf_getsize (DP_QC_STRINGBUFFERS)
void QCBUILTIN PF_buf_getsize  (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int bufno = G_FLOAT(OFS_PARM0)-1;

	if ((unsigned int)bufno >= NUMSTRINGBUFS)
		return;
	if (strbuflist[bufno].prinst != prinst)
		return;

	G_FLOAT(OFS_RETURN) = strbuflist[bufno].used;
}
// #443 void(float bufhandle_from, float bufhandle_to) buf_copy (DP_QC_STRINGBUFFERS)
void QCBUILTIN PF_buf_copy  (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int buffrom = G_FLOAT(OFS_PARM0)-1;
	int bufto = G_FLOAT(OFS_PARM1)-1;

	if ((unsigned int)buffrom >= NUMSTRINGBUFS)
		return;
	if (strbuflist[buffrom].prinst != prinst)
		return;

	if ((unsigned int)bufto >= NUMSTRINGBUFS)
		return;
	if (strbuflist[bufto].prinst != prinst)
		return;

	Con_Printf("PF_buf_copy: stub\n");
}
// #444 void(float bufhandle, float sortpower, float backward) buf_sort (DP_QC_STRINGBUFFERS)
void QCBUILTIN PF_buf_sort  (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int bufno = G_FLOAT(OFS_PARM0)-1;
	//int sortpower = G_FLOAT(OFS_PARM1);
	//int backwards = G_FLOAT(OFS_PARM2);

	if ((unsigned int)bufno >= NUMSTRINGBUFS)
		return;
	if (strbuflist[bufno].prinst != prinst)
		return;

	Con_Printf("PF_buf_sort: stub\n");
}
// #445 string(float bufhandle, string glue) buf_implode (DP_QC_STRINGBUFFERS)
void QCBUILTIN PF_buf_implode  (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int bufno = G_FLOAT(OFS_PARM0)-1;
	//char *glue = PR_GetStringOfs(prinst, OFS_PARM1);

	if ((unsigned int)bufno >= NUMSTRINGBUFS)
		return;
	if (strbuflist[bufno].prinst != prinst)
		return;

	Con_Printf("PF_buf_implode: stub\n");

	RETURN_TSTRING("");
}
// #446 string(float bufhandle, float string_index) bufstr_get (DP_QC_STRINGBUFFERS)
void QCBUILTIN PF_bufstr_get  (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int bufno = G_FLOAT(OFS_PARM0)-1;
	int index = G_FLOAT(OFS_PARM1);

	if ((unsigned int)bufno >= NUMSTRINGBUFS)
	{
		RETURN_CSTRING("");
		return;
	}
	if (strbuflist[bufno].prinst != prinst)
	{
		RETURN_CSTRING("");
		return;
	}

	if (index >= strbuflist[bufno].used)
	{
		RETURN_CSTRING("");
		return;
	}

	RETURN_TSTRING(strbuflist[bufno].strings[index]);
}
// #447 void(float bufhandle, float string_index, string str) bufstr_set (DP_QC_STRINGBUFFERS)
void QCBUILTIN PF_bufstr_set  (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int bufno = G_FLOAT(OFS_PARM0)-1;
	int index = G_FLOAT(OFS_PARM1);
	char *string = PR_GetStringOfs(prinst, OFS_PARM2);
	int oldcount;

	if ((unsigned int)bufno >= NUMSTRINGBUFS)
		return;
	if (strbuflist[bufno].prinst != prinst)
		return;

	if (index >= strbuflist[bufno].allocated)
	{
		oldcount = strbuflist[bufno].allocated;
		strbuflist[bufno].allocated = (index + 256);
		strbuflist[bufno].strings = BZ_Realloc(strbuflist[bufno].strings, strbuflist[bufno].allocated*sizeof(char*));
		memset(strbuflist[bufno].strings+oldcount, 0, (strbuflist[bufno].allocated - oldcount) * sizeof(char*));
	}
	strbuflist[bufno].strings[index] = Z_Malloc(strlen(string)+1);
	strcpy(strbuflist[bufno].strings[index], string);

	if (index >= strbuflist[bufno].used)
		strbuflist[bufno].used = index+1;
}
// #448 float(float bufhandle, string str, float order) bufstr_add (DP_QC_STRINGBUFFERS)
void QCBUILTIN PF_bufstr_add  (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int bufno = G_FLOAT(OFS_PARM0)-1;
	//char *string = PR_GetStringOfs(prinst, OFS_PARM1);
	//int order = G_FLOAT(OFS_PARM2);

	if ((unsigned int)bufno >= NUMSTRINGBUFS)
		return;
	if (strbuflist[bufno].prinst != prinst)
		return;

	Con_Printf("PF_bufstr_add: stub\n");

	G_FLOAT(OFS_RETURN) = 0;
}
// #449 void(float bufhandle, float string_index) bufstr_free (DP_QC_STRINGBUFFERS)
void QCBUILTIN PF_bufstr_free  (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int bufno = G_FLOAT(OFS_PARM0)-1;
	//int index = G_FLOAT(OFS_PARM1);

	if ((unsigned int)bufno >= NUMSTRINGBUFS)
		return;
	if (strbuflist[bufno].prinst != prinst)
		return;

	Con_Printf("PF_bufstr_free: stub\n");
}

void QCBUILTIN PF_buf_cvarlist  (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int bufno = G_FLOAT(OFS_PARM0)-1;
	//char *pattern = PR_GetStringOfs(prinst, OFS_PARM1);
	//char *antipattern = PR_GetStringOfs(prinst, OFS_PARM2);

	if ((unsigned int)bufno >= NUMSTRINGBUFS)
		return;
	if (strbuflist[bufno].prinst != prinst)
		return;

	Con_Printf("PF_buf_cvarlist: stub\n");
}

//515's String functions
////////////////////////////////////////////////////

//float(float caseinsensitive, string s, ...) crc16 = #494 (DP_QC_CRC16)
void QCBUILTIN PF_crc16 (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int insens = G_FLOAT(OFS_PARM0);
	char *str = PF_VarString(prinst, 1, pr_globals);
	int len = strlen(str);

	if (insens)
		G_FLOAT(OFS_RETURN) = QCRC_Block_AsLower(str, len);	
	else
		G_FLOAT(OFS_RETURN) = QCRC_Block(str, len);
}

// #510 string(string in) uri_escape = #510;
void QCBUILTIN PF_uri_escape  (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	static const char *hex = "0123456789ABCDEF";

	unsigned char result[8192];
	unsigned char *o = result;
	unsigned char *s = PR_GetStringOfs(prinst, OFS_PARM0);
	*result = 0;
	while (*s && o < result+sizeof(result)-4)
	{
		if ((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9')
				|| *s == '.' || *s == '-' || *s == '_')
			*o++ = *s++;
		else
		{
			*o++ = '%';
			*o++ = hex[*s>>4];
			*o++ = hex[*s&0xf];
			s++;
		}
	}
	*o = 0;
	RETURN_TSTRING(result);
}

// #511 string(string in) uri_unescape = #511;
void QCBUILTIN PF_uri_unescape  (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	unsigned char *s = (unsigned char*)PR_GetStringOfs(prinst, OFS_PARM0);
	unsigned char resultbuf[8192];
	unsigned char *i, *o;
	unsigned char hex;
	i = s; o = resultbuf;
	while (*i)
	{
		if (*i == '%')
		{
			hex = 0;
			if (i[1] >= 'A' && i[1] <= 'F')
				hex += i[1]-'A'+10;
			else if (i[1] >= 'a' && i[1] <= 'f')
				hex += i[1]-'a'+10;
			else if (i[1] >= '0' && i[1] <= '9')
				hex += i[1]-'0';
			else
			{
				*o++ = *i++;
				continue;
			}
			hex <<= 4;
			if (i[2] >= 'A' && i[2] <= 'F')
				hex += i[2]-'A'+10;
			else if (i[2] >= 'a' && i[2] <= 'f')
				hex += i[2]-'a'+10;
			else if (i[2] >= '0' && i[2] <= '9')
				hex += i[2]-'0';
			else
			{
				*o++ = *i++;
				continue;
			}
			*o++ = hex;
			i += 3;
		}
		else
			*o++ = *i++;
	}
	*o = 0;
	RETURN_TSTRING(resultbuf);
}

// uri_get() gets content from an URL and calls a callback "uri_get_callback" with it set as string; an unique ID of the transfer is returned
// returns 1 on success, and then calls the callback with the ID, 0 or the HTTP status code, and the received data in a string
void QCBUILTIN PF_uri_get  (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	Con_Printf("PF_uri_get: stub\n");

	G_FLOAT(OFS_RETURN) = 0;
}

////////////////////////////////////////////////////
//Console functions

#define MAXQCTOKENS 64
static struct {
	char *token;
	unsigned int start;
	unsigned int end;
} qctoken[MAXQCTOKENS];
unsigned int qctoken_count;

void QCBUILTIN PF_ArgC  (progfuncs_t *prinst, struct globalvars_s *pr_globals)				//85			//float() argc;
{
	G_FLOAT(OFS_RETURN) = qctoken_count;
}

int tokenizeqc(char *str, qboolean dpfuckage)
{
	char *start = str;
	while(qctoken_count > 0)
	{
		qctoken_count--;
		free(qctoken[qctoken_count].token);
	}
	qctoken_count = 0;
	while (qctoken_count < MAXQCTOKENS)
	{
		/*skip whitespace here so the token's start is accurate*/
		while (*str && *(unsigned char*)str <= ' ')
			str++;

		if (!*str)
			break;

		qctoken[qctoken_count].start = str - start;
		str = COM_StringParse (str, com_token, sizeof(com_token), false, dpfuckage);
		if (!str)
			break;

		qctoken[qctoken_count].token = strdup(com_token);

		qctoken[qctoken_count].end = str - start;
		qctoken_count++;
	}
	return qctoken_count;
}

/*KRIMZON_SV_PARSECLIENTCOMMAND added these two - note that for compatibility with DP, this tokenize builtin is veeery vauge and doesn't match the console*/
void QCBUILTIN PF_Tokenize  (progfuncs_t *prinst, struct globalvars_s *pr_globals)			//84			//void(string str) tokanize;
{
	G_FLOAT(OFS_RETURN) = tokenizeqc(PR_GetStringOfs(prinst, OFS_PARM0), true);
}

void QCBUILTIN PF_tokenize_console  (progfuncs_t *prinst, struct globalvars_s *pr_globals)			//84			//void(string str) tokanize;
{
	G_FLOAT(OFS_RETURN) = tokenizeqc(PR_GetStringOfs(prinst, OFS_PARM0), false);
}

void QCBUILTIN PF_tokenizebyseparator  (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *str = PR_GetStringOfs(prinst, OFS_PARM0);
	char *sep[7];
	int seplen[7];
	int seps = 0, s;
	char *start = str;
	int tlen;
	qboolean found = true;

	while (seps < *prinst->callargc - 1 && seps < 7)
	{
		sep[seps] = PR_GetStringOfs(prinst, OFS_PARM1 + seps*3);
		seplen[seps] = strlen(sep[seps]);
		seps++;
	}

	/*flush the old lot*/
	while(qctoken_count > 0)
	{
		qctoken_count--;
		free(qctoken[qctoken_count].token);
	}
	qctoken_count = 0;

	qctoken[qctoken_count].start = 0;
	if (*str)
	for(;;)
	{
		found = false;
		/*see if its a separator*/
		if (!*str)
		{
			qctoken[qctoken_count].end = str - start;
			found = true;
		}
		else
		{
			for (s = 0; s < seps; s++)
			{
				if (!strncmp(str, sep[s], seplen[s]))
				{
					qctoken[qctoken_count].end = str - start;
					str += seplen[s];
					found = true;
					break;
				}
			}
		}
		/*it was, split it out*/
		if (found)
		{
			tlen = qctoken[qctoken_count].end - qctoken[qctoken_count].start;
			qctoken[qctoken_count].token = malloc(tlen + 1);
			memcpy(qctoken[qctoken_count].token, start + qctoken[qctoken_count].start, tlen);
			qctoken[qctoken_count].token[tlen] = 0;

			qctoken_count++;

			if (*str && qctoken_count < MAXQCTOKENS)
				qctoken[qctoken_count].start = str - start;
			else
				break;
		}
		str++;
	}
	G_FLOAT(OFS_RETURN) = qctoken_count;
}

void QCBUILTIN PF_argv_start_index  (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int idx = G_FLOAT(OFS_PARM0);

	/*negative indexes are relative to the end*/
	if (idx < 0)
		idx += qctoken_count;	

	if ((unsigned int)idx >= qctoken_count)
		G_FLOAT(OFS_RETURN) = -1;
	else
		G_FLOAT(OFS_RETURN) = qctoken[idx].start;
}

void QCBUILTIN PF_argv_end_index  (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int idx = G_FLOAT(OFS_PARM0);

	/*negative indexes are relative to the end*/
	if (idx < 0)
		idx += qctoken_count;	

	if ((unsigned int)idx >= qctoken_count)
		G_FLOAT(OFS_RETURN) = -1;
	else
		G_FLOAT(OFS_RETURN) = qctoken[idx].end;
}

void QCBUILTIN PF_ArgV  (progfuncs_t *prinst, struct globalvars_s *pr_globals)				//86			//string(float num) argv;
{
	int idx = G_FLOAT(OFS_PARM0);

	/*negative indexes are relative to the end*/
	if (idx < 0)
		idx += qctoken_count;	

	if ((unsigned int)idx >= qctoken_count)
		G_INT(OFS_RETURN) = 0;
	else
		RETURN_TSTRING(qctoken[idx].token);
}

//Console functions
////////////////////////////////////////////////////
//Maths functions

void QCBUILTIN PF_random (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float		num;

	num = (rand ()&0x7fff) / ((float)0x7fff);

	G_FLOAT(OFS_RETURN) = num;
}

//float(float number, float quantity) bitshift = #218;
void QCBUILTIN PF_bitshift(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int bitmask;
	int shift;

	bitmask = G_FLOAT(OFS_PARM0);
	shift = G_FLOAT(OFS_PARM1);

	if (shift < 0)
		bitmask >>= -shift;
	else
		bitmask <<= shift;

	G_FLOAT(OFS_RETURN) = bitmask;
}

//float(float a, floats) min = #94
void QCBUILTIN PF_min (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int i;
	float f;

	if (*prinst->callargc == 2)
	{
		G_FLOAT(OFS_RETURN) = min(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1));
	}
	else if (*prinst->callargc >= 3)
	{
		f = G_FLOAT(OFS_PARM0);
		for (i = 1; i < *prinst->callargc; i++)
		{
			if (G_FLOAT((OFS_PARM0 + i * 3)) < f)
				f = G_FLOAT((OFS_PARM0 + i * 3));
		}
		G_FLOAT(OFS_RETURN) = f;
	}
	else
		PR_BIError(prinst, "PF_min: must supply at least 2 floats\n");
}

//float(float a, floats) max = #95
void QCBUILTIN PF_max (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int i;
	float f;

	if (*prinst->callargc == 2)
	{
		G_FLOAT(OFS_RETURN) = max(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1));
	}
	else if (*prinst->callargc >= 3)
	{
		f = G_FLOAT(OFS_PARM0);
		for (i = 1; i < *prinst->callargc; i++) {
			if (G_FLOAT((OFS_PARM0 + i * 3)) > f)
				f = G_FLOAT((OFS_PARM0 + i * 3));
		}
		G_FLOAT(OFS_RETURN) = f;
	}
	else
	{
		PR_BIError(prinst, "PF_min: must supply at least 2 floats\n");
	}
}

//float(float minimum, float val, float maximum) bound = #96
void QCBUILTIN PF_bound (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (G_FLOAT(OFS_PARM1) > G_FLOAT(OFS_PARM2))
		G_FLOAT(OFS_RETURN) = G_FLOAT(OFS_PARM2);
	else if (G_FLOAT(OFS_PARM1) < G_FLOAT(OFS_PARM0))
		G_FLOAT(OFS_RETURN) = G_FLOAT(OFS_PARM0);
	else
		G_FLOAT(OFS_RETURN) = G_FLOAT(OFS_PARM1);
}

void QCBUILTIN PF_Sin (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = sin(G_FLOAT(OFS_PARM0));
}
void QCBUILTIN PF_Cos (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = cos(G_FLOAT(OFS_PARM0));
}
void QCBUILTIN PF_Sqrt (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = sqrt(G_FLOAT(OFS_PARM0));
}
void QCBUILTIN PF_pow (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = pow(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1));
}
void QCBUILTIN PF_asin (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = asin(G_FLOAT(OFS_PARM0));
}
void QCBUILTIN PF_acos (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = acos(G_FLOAT(OFS_PARM0));
}
void QCBUILTIN PF_atan (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = atan(G_FLOAT(OFS_PARM0));
}
void QCBUILTIN PF_atan2 (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = atan2(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1));
}
void QCBUILTIN PF_tan (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = tan(G_FLOAT(OFS_PARM0));
}

void QCBUILTIN PF_fabs (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float	v;
	v = G_FLOAT(OFS_PARM0);
	G_FLOAT(OFS_RETURN) = fabs(v);
}

void QCBUILTIN PF_rint (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float	f;
	f = G_FLOAT(OFS_PARM0);
	if (f > 0)
		G_FLOAT(OFS_RETURN) = (int)(f + 0.5);
	else
		G_FLOAT(OFS_RETURN) = (int)(f - 0.5);
}

void QCBUILTIN PF_floor (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = floor(G_FLOAT(OFS_PARM0));
}

void QCBUILTIN PF_ceil (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = ceil(G_FLOAT(OFS_PARM0));
}

//Maths functions
////////////////////////////////////////////////////
//Vector functions

//vector() randomvec = #91
void QCBUILTIN PF_randomvector (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	vec3_t temp;
	do
	{
		temp[0] = (rand() & 32767) * (2.0 / 32767.0) - 1.0;
		temp[1] = (rand() & 32767) * (2.0 / 32767.0) - 1.0;
		temp[2] = (rand() & 32767) * (2.0 / 32767.0) - 1.0;
	} while (DotProduct(temp, temp) >= 1);
	VectorCopy (temp, G_VECTOR(OFS_RETURN));
}

//float vectoyaw(vector)
void QCBUILTIN PF_vectoyaw (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float	*value1;
	float	yaw;

	value1 = G_VECTOR(OFS_PARM0);

	if (value1[1] == 0 && value1[0] == 0)
		yaw = 0;
	else
	{
		yaw = (int) (atan2(value1[1], value1[0]) * 180 / M_PI);
		if (yaw < 0)
			yaw += 360;
	}

	G_FLOAT(OFS_RETURN) = yaw;
}

//float(vector) vlen
void QCBUILTIN PF_vlen (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float	*value1;
	float	newv;

	value1 = G_VECTOR(OFS_PARM0);

	newv = value1[0] * value1[0] + value1[1] * value1[1] + value1[2]*value1[2];
	newv = sqrt(newv);

	G_FLOAT(OFS_RETURN) = newv;
}

//vector vectoangles(vector)
void QCBUILTIN PF_vectoangles (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float	*value1, *up;
	float	yaw, pitch, roll;

	value1 = G_VECTOR(OFS_PARM0);
	if (*prinst->callargc >= 2)
		up = G_VECTOR(OFS_PARM1);
	else
		up = NULL;

	

	if (value1[1] == 0 && value1[0] == 0)
	{
		if (value1[2] > 0)
		{
			pitch = -M_PI*0.5;
			yaw = up ? atan2(-up[1], -up[0]) : 0;
		}
		else
		{
			pitch = M_PI*0.5;
			yaw = up ? atan2(up[1], up[0]) : 0;
		}
		roll = 0;
	}
	else
	{
		yaw = atan2(value1[1], value1[0]);
		pitch = -atan2(value1[2], sqrt (value1[0]*value1[0] + value1[1]*value1[1]));

		if (up)
		{
			vec_t cp = cos(pitch), sp = sin(pitch);
			vec_t cy = cos(yaw), sy = sin(yaw);
			vec3_t tleft, tup;
			tleft[0] = -sy;
			tleft[1] = cy;
			tleft[2] = 0;
			tup[0] = sp*cy;
			tup[1] = sp*sy;
			tup[2] = cp;
			roll = -atan2(DotProduct(up, tleft), DotProduct(up, tup));
		}
		else
			roll = 0;
	}

	pitch *= -180 / M_PI;
	yaw *= 180 / M_PI;
	roll *= 180 / M_PI;
	if (pitch < 0)
		pitch += 360;
	if (yaw < 0)
		yaw += 360;
	if (roll < 0)
		roll += 360;
	G_FLOAT(OFS_RETURN+0) = pitch;
	G_FLOAT(OFS_RETURN+1) = yaw;
	G_FLOAT(OFS_RETURN+2) = roll;
}

//vector normalize(vector)
void QCBUILTIN PF_normalize (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float	*value1;
	vec3_t	newvalue;
	float	newf;

	value1 = G_VECTOR(OFS_PARM0);

	newf = value1[0] * value1[0] + value1[1] * value1[1] + value1[2]*value1[2];
	newf = sqrt(newf);

	if (newf == 0)
		newvalue[0] = newvalue[1] = newvalue[2] = 0;
	else
	{
		newf = 1/newf;
		newvalue[0] = value1[0] * newf;
		newvalue[1] = value1[1] * newf;
		newvalue[2] = value1[2] * newf;
	}

	VectorCopy (newvalue, G_VECTOR(OFS_RETURN));
}

//Vector functions
////////////////////////////////////////////////////
//Progs internals

void QCBUILTIN PF_Abort(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	prinst->AbortStack(prinst);
}

//this func calls a function in annother progs
//it works in the same way as the above func, except that it calls by reference to a function, as opposed to by it's name
//used for entity function variables - not actually needed anymore
void QCBUILTIN PF_externrefcall (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int progsnum;
	func_t f;
	progsnum = G_PROG(OFS_PARM0);
	f = G_INT(OFS_PARM1);

	(*prinst->pr_trace)++;	//continue debugging.
	PR_ExecuteProgram(prinst, f);
}

void QCBUILTIN PF_externset (progfuncs_t *prinst, struct globalvars_s *pr_globals)	//set a value in annother progs
{
	int n = G_PROG(OFS_PARM0);
	int v = G_INT(OFS_PARM1);
	char *varname = PF_VarString(prinst, 2, pr_globals);
	eval_t *var;

	var = PR_FindGlobal(prinst, varname, n, NULL);

	if (var)
		var->_int = v;
}

void QCBUILTIN PF_externvalue (progfuncs_t *prinst, struct globalvars_s *pr_globals)	//return a value in annother progs
{
	int n = G_PROG(OFS_PARM0);
	char *varname = PF_VarString(prinst, 1, pr_globals);
	eval_t *var;

	var = prinst->FindGlobal(prinst, varname, n, NULL);

	if (var)
	{
		G_INT(OFS_RETURN+0) = ((int*)&var->_int)[0];
		G_INT(OFS_RETURN+1) = ((int*)&var->_int)[1];
		G_INT(OFS_RETURN+2) = ((int*)&var->_int)[2];
	}
	else
		G_INT(OFS_RETURN) = 0;
}

void QCBUILTIN PF_externcall (progfuncs_t *prinst, struct globalvars_s *pr_globals)	//this func calls a function in annother progs (by name)
{
	int progsnum;
	char *funcname;
	int i;
	string_t failedst = G_INT(OFS_PARM1);
	func_t f;

	progsnum = G_PROG(OFS_PARM0);
	funcname = PR_GetStringOfs(prinst, OFS_PARM1);

	f = PR_FindFunction(prinst, funcname, progsnum);
	if (f)
	{
		for (i = OFS_PARM0; i < OFS_PARM5; i+=3)
			VectorCopy(G_VECTOR(i+(2*3)), G_VECTOR(i));

		(*prinst->pr_trace)++;	//continue debugging
		PR_ExecuteProgram(prinst, f);
	}
	else if (!f)
	{
		f = PR_FindFunction(prinst, "MissingFunc", progsnum);
		if (!f)
		{
			PR_BIError(prinst, "Couldn't find function %s", funcname);
			return;
		}

		for (i = OFS_PARM0; i < OFS_PARM6; i+=3)
			VectorCopy(G_VECTOR(i+(1*3)), G_VECTOR(i));
		G_INT(OFS_PARM0) = failedst;

		(*prinst->pr_trace)++;	//continue debugging
		PR_ExecuteProgram(prinst, f);
	}
}

void QCBUILTIN PF_traceon (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	(*prinst->pr_trace) = true;
}

void QCBUILTIN PF_traceoff (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	(*prinst->pr_trace) = false;
}
void QCBUILTIN PF_coredump (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int size = 1024*1024*8;
	char *buffer = BZ_Malloc(size);
	prinst->save_ents(prinst, buffer, &size, 3);
	COM_WriteFile("core.txt", buffer, size);
	BZ_Free(buffer);
}
void QCBUILTIN PF_eprint (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int size = 1024*1024;
	char *buffer = BZ_Malloc(size);
	char *buf;
	buf = prinst->saveent(prinst, buffer, &size, G_EDICT(prinst, OFS_PARM0));
	Con_Printf("Entity %i:\n%s\n", G_EDICTNUM(prinst, OFS_PARM0), buf);
	BZ_Free(buffer);
}

void QCBUILTIN PF_break (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
#ifdef SERVERONLY	//new break code
	char *s;

	//I would like some sort of network activity here,
	//but I don't want to mess up the sequence and stuff
	//It should be possible, but would mean that I would
	//need to alter the client, or rewrite a bit of the server..

	if (pr_globals)
		Con_TPrintf(STL_BREAKSTATEMENT);
	else if (developer.value!=2)
		return;	//non developers cann't step.
	for(;;)
	{
		s=Sys_ConsoleInput();
		if (s)
		{
			if (!*s)
				break;
			else
				Con_Printf("%s\n", svprogfuncs->EvaluateDebugString(svprogfuncs, s));
		}
	}
#elif defined(TEXTEDITOR)
	(*prinst->pr_trace)++;
#else	//old break code
Con_Printf ("break statement\n");
*(int *)-4 = 0;	// dump to debugger
//	PR_RunError ("break statement");
#endif
}

void QCBUILTIN PF_error (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*s;

	s = PF_VarString(prinst, 0, pr_globals);
/*	Con_Printf ("======SERVER ERROR in %s:\n%s\n", PR_GetString(pr_xfunction->s_name) ,s);
	ed = PROG_TO_EDICT(pr_global_struct->self);
	ED_Print (ed);
*/

	PR_StackTrace(prinst);

	Con_Printf("%s\n", s);

	if (developer.value)
	{
//		SV_Error ("Program error: %s", s);
		PF_break(prinst, pr_globals);
		(*prinst->pr_trace) = 2;
	}
	else
	{
		PR_AbortStack(prinst);
		PR_BIError (prinst, "Program error: %s", s);
	}
}

//Progs internals
////////////////////////////////////////////////////
//System

//Sends text over to the client's execution buffer
void QCBUILTIN PF_localcmd (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*str;

	str = PF_VarString(prinst, 0, pr_globals);
	if (!strcmp(str, "host_framerate 0\n"))
		Cbuf_AddText ("sv_mintic 0\n", RESTRICT_INSECURE);	//hmm... do this better...
	else
		Cbuf_AddText (str, RESTRICT_INSECURE);
}

void QCBUILTIN PF_sprintf (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char result[1024];
	char *fmt = PR_GetStringOfs(prinst, OFS_PARM0);

	Con_Printf("PF_sprintf: stub\n");
	Q_strncpyz(result, fmt, sizeof(result));

	RETURN_TSTRING(result);
}

#define DEF_SAVEGLOBAL (1u<<15)
static void PR_AutoCvarApply(progfuncs_t *prinst, eval_t *val, etype_t type, cvar_t *var)
{
	switch(type & ~DEF_SAVEGLOBAL)
	{
	case ev_float:
		val->_float = var->value;
		break;
	case ev_integer:
		val->_int = var->ival;
		break;
	case ev_string:
		PR_RemoveProgsString(prinst, val->_int);
		val->_int = PR_SetString(prinst, var->string);
		break;
	case ev_vector:
		{
			char res[128];
			char *vs = var->string;
			vs = COM_ParseOut(vs, res, sizeof(res));
			val->_vector[0] = atof(com_token);
			vs = COM_ParseOut(vs, res, sizeof(res));
			val->_vector[1] = atof(com_token);
			vs = COM_ParseOut(vs, res, sizeof(res));
			val->_vector[2] = atof(com_token);
		}
		break;
	}
}
/*called when a var has changed*/
void PR_AutoCvar(progfuncs_t *prinst, cvar_t *var)
{
	char *gname;
	eval_t *val;
	etype_t type;
	int n, p;
	for (n = 0; n < 2; n++)
	{
		gname = n?var->name2:var->name;
		if (!gname)
			continue;
		gname = va("autocvar_%s", gname);
		
		for (p = 0; p < prinst->numprogs; p++)
		{
			val = PR_FindGlobal(prinst, gname, p, &type);
			if (val)
				PR_AutoCvarApply(prinst, val, type, var);
		}
	}
}

void PR_FoundPrefixedGlobals(progfuncs_t *progfuncs, char *name, eval_t *val, etype_t type)
{
	cvar_t *var;
	char *vals;
	name += 9; //autocvar_
	
	switch(type & ~DEF_SAVEGLOBAL)
	{
	case ev_float:
		vals = va("%f", val->_float);
		break;
	case ev_integer:
		vals = va("%i", val->_int);
		break;
	case ev_vector:
		vals = va("%f %f %f", val->_vector[0], val->_vector[1], val->_vector[2]);
		break;
	case ev_string:
		vals = PR_GetString(progfuncs, val->string);
		break;
	default:
		return;
	}
	var = Cvar_Get(name, vals, 0, "autocvars");
	if (!var)
		return;

	var->flags |= CVAR_TELLGAMECODE;

	PR_AutoCvarApply(progfuncs, val, type, var);
}

void PR_AutoCvarSetup(progfuncs_t *prinst)
{
	prinst->FindPrefixGlobals (prinst, "autocvar_", PR_FoundPrefixedGlobals);
}

lh_extension_t QSG_Extensions[] = {

//as a special hack, the first 32 entries are PEXT features.
//some of these are overkill yes, but they are all derived from the fteextensions flags and document the underlaying protocol available.
//(which is why there are two lists of extensions here)
//note: not all of these are actually supported. This list mearly reflects the values of the PEXT_ constants.
//Check protocol.h to make sure that the related PEXT is enabled. The engine will only accept if they are actually supported.
	{"FTE_PEXT_SETVIEW"},		//nq setview works.
	{"DP_ENT_SCALE"},			//entities may be rescaled
	{"FTE_PEXT_LIGHTSTYLECOL"},	//lightstyles may have colours.
	{"DP_ENT_ALPHA"},			//transparent entites
	{"FTE_PEXT_VIEW2"},		//secondary view.
	{"FTE_PEXT_ACURATETIMINGS"},		//allows full interpolation
	{"FTE_PEXT_SOUNDDBL"},	//twice the sound indexes
	{"FTE_PEXT_FATNESS"},		//entities may be expanded along their vertex normals
	{"DP_HALFLIFE_MAP"},		//entitiy can visit a hl bsp
	{"FTE_PEXT_TE_BULLET"},	//additional particle effect. Like TE_SPIKE and TE_SUPERSPIKE
	{"FTE_PEXT_HULLSIZE"},	//means we can tell a client to go to crouching hull
	{"FTE_PEXT_MODELDBL"},	//max of 512 models
	{"FTE_PEXT_ENTITYDBL"},	//max of 1024 ents
	{"FTE_PEXT_ENTITYDBL2"},	//max of 2048 ents
	{"FTE_PEXT_ORIGINDBL"},	//-8k to +8k map size.
	{"FTE_PEXT_VWEAP"},
	{"FTE_PEXT_Q2BSP"},		//supports q2 maps. No bugs are apparent.
	{"FTE_PEXT_Q3BSP"},		//quake3 bsp support. dp probably has an equivelent, but this is queryable per client.
	{"DP_ENT_COLORMOD"},
	{NULL},	//splitscreen - not queryable.
	{"FTE_HEXEN2"},				//client can use hexen2 maps. server can use hexen2 progs
	{"FTE_PEXT_SPAWNSTATIC"},	//means that static entities can have alpha/scale and anything else the engine supports on normal ents. (Added for >256 models, while still being compatible - previous system failed with -1 skins)
	{"FTE_PEXT_CUSTOMTENTS",					2,	NULL, {"RegisterTempEnt", "CustomTempEnt"}},
	{"FTE_PEXT_256PACKETENTITIES"},	//client is able to receive unlimited packet entities (server caps itself to 256 to prevent insanity).
	{"FTE_PEXT_64PLAYERS"},
	{"TEI_SHOWLMP2",					6,	NULL, {"showpic", "hidepic", "movepic", "changepic", "showpicent", "hidepicent"}},	//telejano doesn't actually export the moveent/changeent (we don't want to either cos it would stop frik_file stuff being autoregistered)
	{"DP_GFX_QUAKE3MODELTAGS",			1,	NULL, {"setattachment"}},
	{"FTE_PK3DOWNLOADS"},
	{"PEXT_CHUNKEDDOWNLOADS"},

	{"EXT_CSQC_SHARED"},				//this is a separate extension because it requires protocol modifications. note: this is also the extension that extends the allowed stats.

	{NULL},

	//{"EXT_CSQC"},	//this is the base csqc extension. I'm not sure what needs to be separate and what does not.
	//{"EXT_CSQC_DELTAS"},//this is a separate extension because the feature may be banned in a league due to cheat protection.

//the rest are generic extensions
	{"??TOMAZ_STRINGS",					6, NULL, {"tq_zone", "tq_unzone",  "tq_strcat", "tq_substring", "tq_stof", "tq_stov"}},
	{"??TOMAZ_FILE",					4, NULL, {"tq_fopen", "tq_fclose", "tq_fgets", "tq_fputs"}},
	{"??MVDSV_BUILTINS",				21, NULL, {"executecommand", "mvdtokenize", "mvdargc", "mvdargv",
												"teamfield", "substr", "mvdstrcat", "mvdstrlen", "str2byte",
												"str2short", "mvdnewstr", "mvdfreestr", "conprint", "readcmd",
												"mvdstrcpy", "strstr", "mvdstrncpy", "log", "redirectcmd",
												"mvdcalltimeofday", "forcedemoframe"}},
//end of mvdsv
// Tomaz - QuakeC File System End

	{"BX_COLOREDTEXT"},
	{"DP_CON_SET"},
#ifndef SERVERONLY
	{"DP_CON_SETA"},		//because the server doesn't write configs.
#endif
	{"DP_EF_BLUE"},						//hah!! This is QuakeWorld!!!
	{"DP_EF_FULLBRIGHT"},				//Rerouted to hexen2 support.
	{"DP_EF_NODRAW"},					//implemented by sending it with no modelindex
	{"DP_EF_RED"},
	{"DP_ENT_COLORMOD"},
	{"DP_ENT_EXTERIORMODELTOCLIENT"},
	//only in dp6 currently {"DP_ENT_GLOW"},
	{"DP_ENT_VIEWMODEL"},
	{"DP_GECKO_SUPPORT",				7,	NULL, {"gecko_create", "gecko_destroy", "gecko_navigate", "gecko_keyevent", "gecko_mousemove", "gecko_resize", "gecko_get_texture_extent"}},
	{"DP_GFX_QUAKE3MODELTAGS"},
	{"DP_GFX_SKINFILES"},
	{"DP_GFX_SKYBOX"},	//according to the spec. :)
	{"DP_HALFLIFE_MAP_CVAR"},
	//to an extend {"DP_HALFLIFE_SPRITE"},
	{"DP_INPUTBUTTONS"},
	{"DP_LITSUPPORT"},
	{"DP_MONSTERWALK"},
	{"DP_MOVETYPEBOUNCEMISSILE"},		//I added the code for hexen2 support.
	{"DP_MOVETYPEFOLLOW"},
	{"DP_QC_ASINACOSATANATAN2TAN",		5,	NULL, {"asin", "acos", "atan", "atan2", "tan"}},
	{"DP_QC_CHANGEPITCH",				1,	NULL, {"changepitch"}},
	{"DP_QC_COPYENTITY",				1,	NULL, {"copyentity"}},
	{"DP_QC_CRC16",						1,	NULL, {"crc16"}},
	{"DP_QC_CVAR_DEFSTRING",			1,	NULL, {"cvar_defstring"}},
	{"DP_QC_CVAR_STRING",				1,	NULL, {"dp_cvar_string"}},	//448 builtin.
	{"DP_QC_CVAR_TYPE",					1,	NULL, {"cvar_type"}},
	{"DP_QC_EDICT_NUM",					1,	NULL, {"edict_num"}},
	{"DP_QC_ENTITYDATA",				5,	NULL, {"numentityfields", "entityfieldname", "entityfieldtype", "getentityfieldstring", "putentityfieldstring"}},
	{"DP_QC_ETOS",						1,	NULL, {"etos"}},
	{"DP_QC_FINDCHAIN",					1,	NULL, {"findchain"}},
	{"DP_QC_FINDCHAINFLOAT",			1,	NULL, {"findchainfloat"}},
	{"DP_QC_FINDFLAGS",					1,	NULL, {"findflags"}},
	{"DP_QC_FINDCHAINFLAGS",			1,	NULL, {"findchainflags"}},
	{"DP_QC_FINDFLOAT",					1,	NULL, {"findfloat"}},
	{"DP_QC_FS_SEARCH",					4,	NULL, {"search_begin", "search_end", "search_getsize", "search_getfilename"}},
	{"DP_QC_GETSURFACEPOINTATTRIBUTE",	1,	NULL, {"getsurfacepointattribute"}},
	{"DP_QC_MINMAXBOUND",				3,	NULL, {"min", "max", "bound"}},
	{"DP_QC_MULTIPLETEMPSTRINGS"},
	{"DP_QC_RANDOMVEC",					1,	NULL, {"randomvec"}},
	{"DP_QC_SINCOSSQRTPOW",				4,	NULL, {"sin", "cos", "sqrt", "pow"}},
	{"DP_QC_STRFTIME",					1,	NULL, {"strftime"}},
	{"DP_QC_STRING_CASE_FUNCTIONS",		2,	NULL, {"strtolower", "strtoupper"}},
	{"DP_QC_STRINGBUFFERS",				10,	NULL, {"buf_create", "buf_del", "buf_getsize", "buf_copy", "buf_sort", "buf_implode", "bufstr_get", "bufstr_set", "bufstr_add", "bufstr_free"}},
	{"DP_QC_STRINGCOLORFUNCTIONS",		2,	NULL, {"strlennocol", "strdecolorize"}},
	{"DP_QC_STRREPLACE",				2,	NULL, {"strreplace", "strireplace"}},
	{"DP_QC_TOKENIZEBYSEPARATOR",		1,	NULL, {"tokenizebyseparator"}},
	{"DP_QC_TRACEBOX",					1,	NULL, {"tracebox"}},
	{"DP_QC_TRACETOSS"},
	{"DP_QC_TRACE_MOVETYPE_HITMODEL"},
	{"DP_QC_TRACE_MOVETYPE_WORLDONLY"},
	{"DP_QC_TRACE_MOVETYPES"},		//this one is just a lame excuse to add annother extension...
	{"DP_QC_UNLIMITEDTEMPSTRINGS"},
	{"DP_QC_URI_ESCAPE",				2,	NULL, {"uri_escape", "uri_unescape"}},
	{"DP_QC_URI_GET",					1,	NULL, {"uri_get"}},
	{"DP_QC_VECTOANGLES_WITH_ROLL"},
	{"DP_QC_VECTORVECTORS",				1,	NULL, {"vectorvectors"}},
	{"DP_QC_WHICHPACK",					1,	NULL, {"whichpack"}},
	{"DP_QUAKE2_MODEL"},
	{"DP_QUAKE2_SPRITE"},
	{"DP_QUAKE3_MODEL"},
	{"DP_REGISTERCVAR",					1,	NULL, {"registercvar"}},
	{"DP_SND_STEREOWAV"},
	{"DP_SND_OGGVORBIS"},
	{"DP_SOLIDCORPSE"},
	{"DP_SPRITE32"},				//hmm... is it legal to advertise this one?
	{"DP_SV_BOTCLIENT",					2,	NULL, {"spawnclient", "clienttype"}},
	{"DP_SV_CLIENTCOLORS"},
	{"DP_SV_CLIENTNAME"},
	{"DP_SV_DRAWONLYTOCLIENT"},
	{"DP_SV_DROPCLIENT",				1,	NULL, {"dropclient"}},
	{"DP_SV_EFFECT",					1,	NULL, {"effect"}},
	{"DP_SV_EXTERIORMODELFORCLIENT"},
	{"DP_SV_NODRAWTOCLIENT"},		//I prefer my older system. Guess I might as well remove that older system at some point.
	{"DP_SV_PLAYERPHYSICS"},
	{"DP_SV_POINTSOUND",				1,	NULL, {"pointsound"}},
	{"DP_SV_PRECACHEANYTIME"},
	{"DP_SV_SETCOLOR"},
	{"DP_SV_SPAWNFUNC_PREFIX"},
	{"DP_SV_WRITEPICTURE",				1,	NULL, {"WritePicture"}},
	{"DP_SV_WRITEUNTERMINATEDSTRING",	1,	NULL, {"WriteUnterminatedString"}},
	{"DP_TE_BLOOD",						1,	NULL, {"te_blood"}},
	{"DP_TE_BLOODSHOWER",				1,	NULL, {"te_bloodshower"}},
	{"DP_TE_CUSTOMFLASH",				1,	NULL, {"te_customflash"}},
	{"DP_TE_EXPLOSIONRGB"},
	{"DP_TE_FLAMEJET",					1,	NULL, {"te_flamejet"}},
	{"DP_TE_PARTICLECUBE",				1,	NULL, {"te_particlecube"}},
	//particlerain
	//particlesnow
	{"DP_TE_PLASMABURN",				1,	NULL, {"te_plasmaburn"}},
	{"DP_TE_QUADEFFECTS1"},
	{"DP_TE_SMALLFLASH",				1,	NULL, {"te_smallflash"}},
	{"DP_TE_SPARK",						1,	NULL, {"te_spark"}},
	{"DP_TE_STANDARDEFFECTBUILTINS",	14,	NULL, {"te_gunshot", "te_spike", "te_superspike", "te_explosion", "te_tarexplosion", "te_wizspike", "te_knightspike", "te_lavasplash", "te_teleport", "te_explosion2", "te_lightning1", "te_lightning2", "te_lightning3", "te_beam"}},
	{"DP_VIEWZOOM"},
	{"EXT_BITSHIFT",					1,	NULL, {"bitshift"}},
	{"EXT_DIMENSION_VISIBILITY"},
	{"EXT_DIMENSION_PHYSICS"},
	{"EXT_DIMENSION_GHOST"},
	{"FRIK_FILE",						11, NULL, {"stof", "fopen","fclose","fgets","fputs","strlen","strcat","substring","stov","strzone","strunzone"}},
	{"FTE_CALLTIMEOFDAY",				1,	NULL, {"calltimeofday"}},
	{"FTE_CSQC_HALFLIFE_MODELS"},		//hl-specific skeletal model control
	{"FTE_CSQC_BASEFRAME"},				//control for all skeletal models
	{"FTE_ENT_UNIQUESPAWNID"},
	{"FTE_EXTENDEDTEXTCODES"},
	{"FTE_FORCEINFOKEY",				1,	NULL, {"forceinfokey"}},
	{"FTE_GFX_QUAKE3SHADERS"},
	{"FTE_ISBACKBUFFERED",				1,	NULL, {"isbackbuffered"}},
#ifndef NOMEDIA
	{"FTE_MEDIA_AVI"},	//playfilm supports avi files.
	{"FTE_MEDIA_CIN"},	//playfilm command supports q2 cin files.
	{"FTE_MEDIA_ROQ"},	//playfilm command supports q3 roq files
#endif
	{"FTE_MULTIPROGS"},	//multiprogs functions are available.
	{"FTE_MULTITHREADED",				3,	NULL, {"sleep", "fork", "abort"}},
#ifdef SERVER_DEMO_PLAYBACK
	{"FTE_MVD_PLAYBACK"},
#endif
#ifdef SVCHAT
	{"FTE_NPCCHAT",						1,	NULL, {"chat"}},	//server looks at chat files. It automagically branches through calling qc functions as requested.
#endif
	{"FTE_QC_CHECKPVS",					1,	NULL, {"checkpvs"}},
	{"FTE_QC_MATCHCLIENTNAME",				1,	NULL, {"matchclientname"}},
	{"FTE_QC_PAUSED"},
	{"FTE_QC_SENDPACKET",				1,	NULL, {"sendpacket"}},
	{"FTE_QC_TRACETRIGGER"},
	{"FTE_SOLID_LADDER"},	//part of a worthy hl implementation. Allows a simple trigger to remove effects of gravity (solid 20)

#ifdef SQL
	// serverside SQL functions for managing an SQL database connection
	{"FTE_SQL",							9, NULL, {"sqlconnect","sqldisconnect","sqlopenquery","sqlclosequery","sqlreadfield","sqlerror","sqlescape","sqlversion",
												  "sqlreadfloat"}},
#endif
	//eperimental advanced strings functions.
	//reuses the FRIK_FILE builtins (with substring extension)
	{"FTE_STRINGS",						16, NULL, {"stof", "strlen","strcat","substring","stov","strzone","strunzone",
												   "strstrofs", "str2chr", "chr2str", "strconv", "infoadd", "infoget", "strncmp", "strcasecmp", "strncasecmp"}},
	{"FTE_SV_REENTER"},
	{"FTE_TE_STANDARDEFFECTBUILTINS",	14,	NULL, {"te_gunshot", "te_spike", "te_superspike", "te_explosion", "te_tarexplosion", "te_wizspike", "te_knightspike", "te_lavasplash",
												   "te_teleport", "te_lightning1", "te_lightning2", "te_lightning3", "te_lightningblood", "te_bloodqw"}},

	{"KRIMZON_SV_PARSECLIENTCOMMAND",	3,	NULL, {"clientcommand", "tokenize", "argv"}},	//very very similar to the mvdsv system.
	{"NEH_CMD_PLAY2"},
	{"NEH_RESTOREGAME"},
	//{"PRYDON_CLIENTCURSOR"},
	{"QSG_CVARSTRING",					1,	NULL, {"qsg_cvar_string"}},
	{"QW_ENGINE",						3,	NULL, {"infokey", "stof", "logfrag"}},	//warning: interpretation of .skin on players can be dodgy, as can some other QW features that differ from NQ.
	{"QWE_MVD_RECORD"},	//Quakeworld extended get the credit for this one. (mvdsv)
	{"TEI_MD3_MODEL"},
//	{"TQ_RAILTRAIL"},	//treat this as the ZQ style railtrails which the client already supports, okay so the preparse stuff needs strengthening.
	{"ZQ_MOVETYPE_FLY"},
	{"ZQ_MOVETYPE_NOCLIP"},
	{"ZQ_MOVETYPE_NONE"},
//	{"ZQ_QC_PARTICLE"},	//particle builtin works in QW ( we don't mimic ZQ fully though)
	{"ZQ_VWEP",							1,	NULL, {"precache_vwep_model"}},


	{"ZQ_QC_STRINGS",					7, NULL, {"stof", "strlen","strcat","substring","stov","strzone","strunzone"}}	//a trimmed down FRIK_FILE.
};
unsigned int QSG_Extensions_count = sizeof(QSG_Extensions)/sizeof(QSG_Extensions[0]);
#endif
