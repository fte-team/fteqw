#define PROGSUSED
#include "progsint.h"

#define HunkAlloc BADGDFG sdfhhsf FHS

void PR_SetBuiltins(int type);
/*
progstate_t *pr_progstate;
progsnum_t pr_typecurrent;
int maxprogs;

progstate_t *current_progstate;
int numshares;

sharedvar_t *shares;	//shared globals, not including parms
int maxshares;
*/

pbool PR_SwitchProgs(progfuncs_t *progfuncs, progsnum_t type)
{	
	if (type >= maxprogs || type < 0)
		PR_RunError(progfuncs, "QCLIB: Bad prog type - %i", type);
//		Sys_Error("Bad prog type - %i", type);

	if (pr_progstate[(int)type].progs == NULL)	//we havn't loaded it yet, for some reason
		return false;	

	current_progstate = &pr_progstate[(int)type];

	pr_typecurrent = type;

	return true;
}

void PR_MoveParms(progfuncs_t *progfuncs, progsnum_t progs1, progsnum_t progs2)	//from 2 to 1
{
	unsigned int a;
	progstate_t *p1;
	progstate_t *p2;

	if (progs1 == progs2)
		return;	//don't bother coping variables to themselves...

	p1 = &pr_progstate[(int)progs1];
	p2 = &pr_progstate[(int)progs2];

	if (progs1 >= maxprogs || progs1 < 0 || !p1->globals)
		Sys_Error("QCLIB: Bad prog type - %i", progs1);
	if (progs2 >= maxprogs || progs2 < 0 || !p2->globals)
		Sys_Error("QCLIB: Bad prog type - %i", progs2);

	//copy parms.
	for (a = 0; a < MAX_PARMS;a++)
	{
		*(int *)&p1->globals[OFS_PARM0+3*a  ] = *(int *)&p2->globals[OFS_PARM0+3*a  ];
		*(int *)&p1->globals[OFS_PARM0+3*a+1] = *(int *)&p2->globals[OFS_PARM0+3*a+1];
		*(int *)&p1->globals[OFS_PARM0+3*a+2] = *(int *)&p2->globals[OFS_PARM0+3*a+2];
	}
	p1->globals[OFS_RETURN] = p2->globals[OFS_RETURN];
	p1->globals[OFS_RETURN+1] = p2->globals[OFS_RETURN+1];
	p1->globals[OFS_RETURN+2] = p2->globals[OFS_RETURN+2];

	//move the vars defined as shared.
	for (a = 0; a < numshares; a++)//fixme: make offset per progs
	{
		memmove(&((int *)p1->globals)[shares[a].varofs], &((int *)p2->globals)[shares[a].varofs], shares[a].size*4);
/*		((int *)p1->globals)[shares[a].varofs] = ((int *)p2->globals)[shares[a].varofs];
		if (shares[a].size > 1)
		{
			((int *)p1->globals)[shares[a].varofs+1] = ((int *)p2->globals)[shares[a].varofs+1];
			if (shares[a].size > 2)
				((int *)p1->globals)[shares[a].varofs+2] = ((int *)p2->globals)[shares[a].varofs+2];
		}
*/
	}
}

progsnum_t PR_LoadProgs(progfuncs_t *progfuncs, char *s, int headercrc, builtin_t *builtins, int numbuiltins)
{
	progsnum_t a;
	progsnum_t oldtype;
	oldtype = pr_typecurrent;	
	for (a = 0; a < maxprogs; a++)
	{
		if (pr_progstate[(int)a].progs == NULL)
		{
			pr_typecurrent = a;
			current_progstate = &pr_progstate[(int)a];
			if (PR_ReallyLoadProgs(progfuncs, s, headercrc, &pr_progstate[a], false))	//try and load it			
			{
				current_progstate->builtins = builtins;
				current_progstate->numbuiltins = numbuiltins;
				if (oldtype>=0)
				PR_SwitchProgs(progfuncs, oldtype);
				return a;	//we could load it. Yay!
			}
			if (oldtype!=-1)
				PR_SwitchProgs(progfuncs, oldtype);
			return -1; // loading failed.
		}
	}
	PR_SwitchProgs(progfuncs, oldtype);
	return -1;
}

void PR_ShiftParms(progfuncs_t *progfuncs, int amount)
{
	int a;
	for (a = 0; a < MAX_PARMS - amount;a++)
		*(int *)&pr_globals[OFS_PARM0+3*a] = *(int *)&pr_globals[OFS_PARM0+3*(amount+a)];
}

//forget a progs
void PR_Clear(progfuncs_t *progfuncs)
{
	int a;
	for (a = 0; a < maxprogs; a++)
	{
		pr_progstate[a].progs = NULL;
	}
}



void QC_StartShares(progfuncs_t *progfuncs)
{
	numshares = 0;
	maxshares = 32;
	if (shares)
		memfree(shares);
	shares = memalloc(sizeof(sharedvar_t)*maxshares);
}
void QC_AddSharedVar(progfuncs_t *progfuncs, int start, int size)	//fixme: make offset per progs and optional
{
	int ofs;
	unsigned int a;

	if (numshares >= maxshares)
	{
		void *buf;
		buf = shares;
		maxshares += 16;		
		shares = memalloc(sizeof(sharedvar_t)*maxshares);

		memcpy(shares, buf, sizeof(sharedvar_t)*numshares);

		memfree(buf);
	}
	ofs = start;
	for (a = 0; a < numshares; a++)
	{
		if (shares[a].varofs+shares[a].size == ofs)
		{
			shares[a].size += size;	//expand size.
			return;
		}
		if (shares[a].varofs == start)
			return;
	}


	shares[numshares].varofs = start;
	shares[numshares].size = size;
	numshares++;
}


//void ShowWatch(void);

void QC_InitShares(progfuncs_t *progfuncs)
{
//	ShowWatch();
	if (!field)	//don't make it so we will just need to remalloc everything
	{
		maxfields = 64;
		field = memalloc(sizeof(fdef_t) * maxfields);		
	}

	numfields = 0;
	progfuncs->fieldadjust = 0;
}


//called if a global is defined as a field
//returns offset.

//vectors must be added before any of thier corresponding _x/y/z vars
//in this way, even screwed up progs work.
int QC_RegisterFieldVar(progfuncs_t *progfuncs, unsigned int type, char *name, int requestedpos, int originalofs)
{
//	progstate_t *p;
//	int pnum;
	unsigned int i;
	int namelen;
	int ofs;

	int fnum;

	if (!name)	//engine can use this to offset all progs fields
	{			//which fixes constant field offsets (some ktpro arrays)
		progfuncs->fieldadjust = fields_size/4;
		return 0;
	}


	prinst->reorganisefields = true;

	//look for an existing match
	for (i = 0; i < numfields; i++)
	{		
		if (!strcmp(name, field[i].name))
		{
			if (field[i].type != type)
			{
				printf("Field type mismatch on %s\n", name);
				continue;
			}
			if (!progfuncs->fieldadjust && requestedpos>=0)
				if ((unsigned)requestedpos != field[i].ofs)
					Sys_Error("Field %s at wrong offset", name);

			if (field[i].requestedofs == -1)
				field[i].requestedofs = originalofs;
			return field[i].ofs;	//got a match			
		}
	}

	if (numfields+1>maxfields)
	{
		fdef_t *nf;
		i = maxfields;
		maxfields += 32;
		nf = memalloc(sizeof(fdef_t) * maxfields);
		memcpy(nf, field, sizeof(fdef_t) * i);
		memfree(field);
		field = nf;
	}

	//try to add a new one
	fnum = numfields;
	numfields++;
	field[fnum].name = name;	
	if (type == ev_vector)	//resize with the following floats (this is where I think I went wrong)
	{
		char *n;		
		namelen = strlen(name)+5;	

		n=PRHunkAlloc(progfuncs, namelen);
		sprintf(n, "%s_x", name);
		ofs = QC_RegisterFieldVar(progfuncs, ev_float, n, requestedpos, -1);
		field[fnum].ofs = ofs;

		n=PRHunkAlloc(progfuncs, namelen);
		sprintf(n, "%s_y", name);
		QC_RegisterFieldVar(progfuncs, ev_float, n, (requestedpos==-1)?-1:(requestedpos+4), -1);

		n=PRHunkAlloc(progfuncs, namelen);
		sprintf(n, "%s_z", name);
		QC_RegisterFieldVar(progfuncs, ev_float, n, (requestedpos==-1)?-1:(requestedpos+8), -1);
	}
	else if (requestedpos >= 0)
	{
		for (i = 0; i < numfields-1; i++)
		{
			if (field[i].ofs == (unsigned)requestedpos)
			{
				if (type == ev_float && field[i].type == ev_vector)	//check names
				{
					if (strncmp(field[i].name, name, strlen(field[i].name)))
						Sys_Error("Duplicated offset");
				}
				else
					Sys_Error("Duplicated offset");
			}
		}
		if (requestedpos&3)
			Sys_Error("field %s is %i&3", name, requestedpos);
		field[fnum].ofs = ofs = requestedpos/4;
	}
	else
		field[fnum].ofs = ofs = fields_size/4;
//	if (type != ev_vector)
		if (fields_size < (ofs+type_size[type])*4)
			fields_size = (ofs+type_size[type])*4;

	if (max_fields_size && fields_size > max_fields_size)
		Sys_Error("Allocated too many additional fields after ents were inited.");
	field[fnum].type = type;

	field[fnum].requestedofs = originalofs;
	
	//we've finished setting the structure	
	return ofs;
}


//called if a global is defined as a field
void QC_AddSharedFieldVar(progfuncs_t *progfuncs, int num, char *stringtable)
{
//	progstate_t *p;
//	int pnum;
	unsigned int i, o;

	char *s;

	//look for an existing match not needed, cos we look a little later too.
	/*
	for (i = 0; i < numfields; i++)
	{		
		if (!strcmp(pr_globaldefs[num].s_name, field[i].s_name))
		{
			//really we should look for a field def

			*(int *)&pr_globals[pr_globaldefs[num].ofs] = field[i].ofs;	//got a match

			return;
		}
	}
	*/
	
	switch(current_progstate->intsize)
	{
	case 24:
	case 16:
		for (i=1 ; i<pr_progs->numfielddefs; i++)
		{
			if (!strcmp(pr_fielddefs16[i].s_name+stringtable, pr_globaldefs16[num].s_name+stringtable))
			{
				*(int *)&pr_globals[pr_globaldefs16[num].ofs] = QC_RegisterFieldVar(progfuncs, pr_fielddefs16[i].type, pr_globaldefs16[num].s_name+stringtable, -1, *(int *)&pr_globals[pr_globaldefs16[num].ofs])-progfuncs->fieldadjust;
				return;
			}
		}

		s = pr_globaldefs16[num].s_name+stringtable;

		for (i = 0; i < numfields; i++)
		{
			o = field[i].requestedofs;
			if (o == *(unsigned int *)&pr_globals[pr_globaldefs16[num].ofs])
			{
				*(int *)&pr_globals[pr_globaldefs16[num].ofs] = field[i].ofs-progfuncs->fieldadjust;
				return;
			}
		}

		//oh well, must be a parameter.
		if (*(int *)&pr_globals[pr_globaldefs16[num].ofs])
			Sys_Error("QCLIB: Global field var with no matching field \"%s\", from offset %i", pr_globaldefs16[num].s_name, *(int *)&pr_globals[pr_globaldefs16[num].ofs]);
		return;
	case 32:
		for (i=1 ; i<pr_progs->numfielddefs; i++)
		{
			if (!strcmp(pr_fielddefs32[i].s_name+stringtable, pr_globaldefs32[num].s_name+stringtable))
			{
				*(int *)&pr_globals[pr_globaldefs32[num].ofs] = QC_RegisterFieldVar(progfuncs, pr_fielddefs32[i].type, pr_globaldefs32[num].s_name+stringtable, -1, *(int *)&pr_globals[pr_globaldefs32[num].ofs])-progfuncs->fieldadjust;
				return;
			}
		}

		s = pr_globaldefs32[num].s_name+stringtable;

		for (i = 0; i < numfields; i++)
		{
			o = field[i].requestedofs;
			if (o == *(unsigned int *)&pr_globals[pr_globaldefs32[num].ofs])
			{
				*(int *)&pr_globals[pr_globaldefs32[num].ofs] = field[i].ofs-progfuncs->fieldadjust;
				return;
			}
		}

		//oh well, must be a parameter.
		if (*(int *)&pr_globals[pr_globaldefs32[num].ofs])
			Sys_Error("QCLIB: Global field var with no matching field \"%s\", from offset %i", pr_globaldefs32[num].s_name+stringtable, *(int *)&pr_globals[pr_globaldefs32[num].ofs]);
		return;
	default:
		Sys_Error("Bad bits");
		break;
	}
	Sys_Error("Should be unreachable");	
}







/*
//Just a bit of code that makes a window appear with lots of variables listed.
//A little useless really.



static void WatchDraw(window_t *wnd);
static void WatchDead(window_t *wnd);
static bool WatchKeyDown(window_t *wnd, int k);

typedef struct {
	int progs;
	int globofs;
} watchinfo_t;

static window_t watchtemplate = {
	sizeof(window_t),	//int size;	//for possible later expansion
	"Watch",			//char *title;
	BORDER_RESIZE,		//void (*DrawBorder) (struct window_s *wnd);	//the border drawing func (use a borde type)
	WatchDraw,			//void (*DrawWindow) (struct window_s *);	//the drawing func
	NULL,				//void (*Draw3dWindow) (struct window_s *);	//the function to draw 3d stuff
	WatchDead,			//void (*CloseWindow) (struct window_s *);	//when it is closed
	WatchKeyDown,		//bool (*KeyDown) (struct window_s *, int key);	//return true to stop the main game from recieving the call
	NULL,				//void (*KeyUp) (struct window_s *, int key);	//sent to all
	NULL,				//void (*Think) (struct window_s *);
	NULL,				//void (*ReloadTex) (struct window_s *);	

	{320, 0, 640, 240},//float viewarea[4];	//l, t, r, b
	{1, 10, 1, 1},//float bordersize[4];	//l,t,r,b
	{0, 0, 0},//float vieworigin[3];	//3d view origin
	{0, 0, 0},//float viewangles[3];	//3d angles

	TRUE,//bool clear;	//should it be cleared first (for 3d rendering and default border routine)

	NULL,//void *data;	//use this to get unique windows of the same type
	0,//int classid;	//a randomly chosen number that is the same for each of this window's type
	0,//int subclass;	//a number if an app needs to identify between windows of the same class

	NULL//void *(*comunicate) (int type, void *info, void *moreinfo);	//later development for chatting between windows (like the 'SendMessage' function in the OS)

	//for multiple windows
	//struct window_s *next;
	//struct window_s *prev;
};

void ShowWatch(void)
{
	watchinfo_t *inf;
	window_t *wnd;
	wnd = memalloc(sizeof(window_t)+sizeof(watchinfo_t), "watch window");
	memcpy(wnd, &watchtemplate, sizeof(window_t));
	wnd->data = inf = (watchinfo_t *)(wnd+1);
	inf->globofs = 1;
	inf->progs = 0;
	AddWindow(wnd);
}

static void WatchDead(window_t *wnd)
{
	RemoveWindow(wnd);
	memfree(wnd);
}

static bool WatchKeyDown(window_t *wnd, int k)
{
	watchinfo_t *inf = wnd->data;
	int progs = inf->progs;
	if (progs < 0)
		progs = pr_typecurrent;

	switch(k)
	{
	case K_MOUSEWUP:
		inf->globofs-=8;
		if (inf->globofs < 1)
			inf->globofs = 1;
		break;
	case K_MOUSEWDOWN:
		inf->globofs+=8;
		if (inf->globofs > pr_progstate[progs].progs->numglobaldefs-1)
			inf->globofs = pr_progstate[progs].progs->numglobaldefs-1;
		break;
	case K_ESCAPE:
		RemoveWindow(wnd);
		break;
	}
	return true;
}

char *PR_ValueString (etype_t type, eval_t *val);
static void WatchDraw(window_t *wnd)
{	
	float yofs;
	int def=0;
	int progs = ((watchinfo_t *)wnd->data)->progs;

	if (progs < 0)
		progs = pr_typecurrent;

	if (!pr_progstate[progs].progs)
	{
		Draw_String(wnd->viewarea[0], wnd->viewarea[1]+8+def*8, "Progs not loaded", 1, 0);
		return;
	}

//	if (sv_edicts==NULL)
//		return;

	yofs=wnd->viewarea[1];
	Draw_String(wnd->viewarea[0], yofs, pr_progstate[progs].filename, 1, 0);yofs+=8;

	Draw_String(wnd->viewarea[0], yofs, Sva("%3i %16s %s", OFS_RETURN, "RETURN", PR_ValueString(ev_vector, (eval_t *)&pr_progstate[progs].globals[OFS_RETURN])), 1, 0);yofs+=8;
	Draw_String(wnd->viewarea[0], yofs, Sva("%3i %16s %s", OFS_PARM0, "PARM0", PR_ValueString(ev_vector, (eval_t *)&pr_progstate[progs].globals[OFS_PARM0])), 1, 0);yofs+=8;
	Draw_String(wnd->viewarea[0], yofs, Sva("%3i %16s %s", OFS_PARM1, "PARM1", PR_ValueString(ev_vector, (eval_t *)&pr_progstate[progs].globals[OFS_PARM1])), 1, 0);yofs+=8;
	Draw_String(wnd->viewarea[0], yofs, Sva("%3i %16s %s", OFS_PARM2, "PARM2", PR_ValueString(ev_vector, (eval_t *)&pr_progstate[progs].globals[OFS_PARM2])), 1, 0);yofs+=8;
	Draw_String(wnd->viewarea[0], yofs, Sva("%3i %16s %s", OFS_PARM3, "PARM3", PR_ValueString(ev_vector, (eval_t *)&pr_progstate[progs].globals[OFS_PARM3])), 1, 0);yofs+=8;
	Draw_String(wnd->viewarea[0], yofs, Sva("%3i %16s %s", OFS_PARM4, "PARM4", PR_ValueString(ev_vector, (eval_t *)&pr_progstate[progs].globals[OFS_PARM4])), 1, 0);yofs+=8;
	Draw_String(wnd->viewarea[0], yofs, Sva("%3i %16s %s", OFS_PARM5, "PARM5", PR_ValueString(ev_vector, (eval_t *)&pr_progstate[progs].globals[OFS_PARM5])), 1, 0);yofs+=8;
	Draw_String(wnd->viewarea[0], yofs, Sva("%3i %16s %s", OFS_PARM6, "PARM6", PR_ValueString(ev_vector, (eval_t *)&pr_progstate[progs].globals[OFS_PARM6])), 1, 0);yofs+=8;
	Draw_String(wnd->viewarea[0], yofs, Sva("%3i %16s %s", OFS_PARM7, "PARM7", PR_ValueString(ev_vector, (eval_t *)&pr_progstate[progs].globals[OFS_PARM7])), 1, 0);yofs+=8;
	for (def = ((watchinfo_t *)wnd->data)->globofs; def < pr_progstate[progs].progs->numglobaldefs; def++)
	{
		if ((pr_progstate[progs].globaldefs[def].type &~DEF_SAVEGLOBAL)== ev_entity && sv_edicts==NULL)
		{
			grColor4f(0.5, 0.5, 0.5, 1);
			Draw_String(wnd->viewarea[0], yofs, Sva("%3i %16s %s", pr_progstate[progs].globaldefs[def].ofs, pr_progstate[progs].globaldefs[def].s_name, "Entities not initialized"), 1, 0);
		}
		else
		{
			if (pr_progstate[progs].globaldefs[def].type == ev_void || pr_progstate[progs].globaldefs[def].type == ev_field || pr_progstate[progs].globaldefs[def].type == ev_function || !(pr_progstate[progs].globaldefs[def].type & DEF_SAVEGLOBAL))
				grColor4f(0.5, 0.5, 0.5, 1);
			else
				grColor4f(1, 1, 1, 1);
			Draw_String(wnd->viewarea[0], yofs, Sva("%3i %16s %s", pr_progstate[progs].globaldefs[def].ofs, pr_progstate[progs].globaldefs[def].s_name, PR_ValueString(pr_progstate[progs].globaldefs[def].type, (eval_t *)&pr_progstate[progs].globals[pr_progstate[progs].globaldefs[def].ofs])), 1, 0);
		}		
		yofs+=8;
	}
}

*/

