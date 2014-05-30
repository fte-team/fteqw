//DMW

/*
F1 will return to progs.src
F2 will try to open a file with the name of which is on that line. (excluding comments/tabs). Needs conditions.
F3 will give a prompt for typing in a value name to see the value.
F4 will save
F5 will run (unbreak).
F6 will list the stack.
F7 will compile.
F8 will move execution
F9 will set a break point.
F10 will apply code changes.
F11 will step through.
*/

#include "quakedef.h"
#ifdef TEXTEDITOR

#ifdef _WIN32
#define editaddcr_default "1"
#else
#define editaddcr_default "0"
#endif
//#if defined(ANDROID) || defined(SERVERONLY)
#define debugger_default "0"
//#else
//#define debugger_default "1"
//#endif

static cvar_t editstripcr = CVARD("edit_stripcr", "1", "remove \\r from eols (on load)");
static cvar_t editaddcr = CVARD("edit_addcr", editaddcr_default, "make sure that each line ends with a \\r (on save)");
static cvar_t edittabspacing = CVARD("edit_tabsize", "4", "How wide tab alignment is");
cvar_t pr_debugger = CVARAFD("pr_debugger", debugger_default, "debugger", CVAR_SAVE, "When enabled, QC errors and debug events will enable step-by-step tracing.");
extern cvar_t pr_sourcedir;

static pubprogfuncs_t *editprogfuncs;

typedef struct fileblock_s {
	struct fileblock_s *next;
	struct fileblock_s *prev;
	int allocatedlength;
	int datalength;
	int flags;
	char *data;
} fileblock_t;
#define FB_BREAK 1

static fileblock_t *cursorblock, *firstblock, *executionblock, *viewportystartblock;

static void *E_Malloc(int size)
{
	char *mem;
	mem = Z_Malloc(size);
	if (!mem)
		Sys_Error("Failed to allocate enough mem for editor\n");
	return mem;
}
static void E_Free(void *mem)
{
	Z_Free(mem);
}

#define GETBLOCK(s, ret) ret = (void *)E_Malloc(sizeof(fileblock_t) + s);ret->allocatedlength = s;ret->data = (char *)ret + sizeof(fileblock_t)

fileblock_t *GenAsm(int statement)
{
	char linebuf[256];
	fileblock_t *b;
	int l;
	if (!editprogfuncs)
		return NULL;
	editprogfuncs->GenerateStatementString(editprogfuncs, statement, linebuf, sizeof(linebuf));
	l = strlen(linebuf);
	b = E_Malloc(sizeof(fileblock_t) + l);
	b->allocatedlength = l;
	b->datalength = l;
	b->data = (char *)b + sizeof(fileblock_t);
	memcpy(b->data, linebuf, l);
	return b;
}

static char OpenEditorFile[256];


qboolean editoractive;	//(export)
qboolean editormodal;	//doesn't return. (export)
static qboolean madechanges;
static qboolean editenabled;
static qboolean insertkeyhit=true;
static qboolean useeval;
static qboolean stepasm;

static char evalstring[256];

static int executionlinenum;	//step by step debugger
static int cursorlinenum, cursorx;

static int viewportx;
static int viewporty;


static int VFS_GETC(vfsfile_t *fp)
{
	unsigned char c;
	VFS_READ(fp, &c, 1);
	return c;
}

									//newsize = number of chars, EXCLUDING terminator.
static void MakeNewSize(fileblock_t *block, int newsize)	//this is used to resize a block. It allocates a new one, copys the data frees the old one and links it into the right place
													//it is called when the user is typing
{
	fileblock_t *newblock;

	newsize = (newsize + 4)&~3;	//We allocate a bit extra, so we don't need to keep finding a new block for each and every character.

	if (block->allocatedlength >= newsize)
		return;	//Ignore. This block is too large already. Don't bother resizeing, cos that's pretty much pointless.

	GETBLOCK(newsize, newblock);
	memcpy(newblock->data, block->data, block->datalength);
	newblock->prev = block->prev;
	newblock->next = block->next;
	if (newblock->prev)
		newblock->prev->next = newblock;
	if (newblock->next)
		newblock->next->prev = newblock;

	newblock->datalength = block->datalength;
	newblock->flags = block->flags;

	E_Free(block);

	if (firstblock == block)
		firstblock = newblock;

	if (cursorblock == block)
		cursorblock = newblock;
}

static int positionacross;
static void GetCursorpos(void)
{
	int a;
	char *s;
	int ts = edittabspacing.value;
	if (ts < 1)
		ts = 4;
	for (a=0,positionacross=0,s=cursorblock->data;a < cursorx && *s;s++,a++)
	{
		if (*s == '\t')
		{
			positionacross += ts;
			positionacross -= cursorx%ts;
		}
		else
			positionacross++;
	}
//	positionacross = cursorofs;
}
static void SetCursorpos(void)
{
	int a=0;
	char *s;
	int ts = edittabspacing.value;
	if (ts < 1)
		ts = 4;
	for (cursorx=0,s=cursorblock->data;cursorx < positionacross && *s;s++,a++)
	{
		if (*s == '\t')
		{
			cursorx += ts;
			cursorx -= cursorx%ts;
		}
		else
			cursorx++;
	}
	cursorx=a;

//just in case
	if (cursorx > cursorblock->datalength)
		cursorx = cursorblock->datalength;
}


static void CloseEditor(void)
{
	fileblock_t *b;

	Key_Dest_Remove(kdm_editor);
	editoractive = false;
	editprogfuncs = NULL;
	cursorblock = NULL;

	if (!firstblock)
		return;
	OpenEditorFile[0] = '\0';

	while(firstblock)
	{
		b = firstblock;
		firstblock=firstblock->next;
		E_Free(b);
	}

	madechanges = false;
	editormodal = false;

	firstblock = NULL;

	executionlinenum = 0;
}

static qboolean EditorSaveFile(char *s)	//returns true if succesful
{

//	FILE *F;
	fileblock_t *b;

	int len=0;
	int pos=0;
	char *data;
	char native[MAX_OSPATH];

	if (!FS_NativePath(s, FS_GAMEONLY, native, sizeof(native)))
		Con_Printf("Not saving.\n");

	Con_Printf("Saving to \"%s\"\n", native);

	for (b = firstblock; b; b = b->next)	//find total length required.
	{
		len += b->datalength;
		len+=2;	//extra for \n
	}

	data = Hunk_TempAlloc(len);

	for (b = firstblock; b; b = b->next)	//find total length required.
	{
		memcpy(data + pos, b->data, b->datalength);
		pos += b->datalength;
		if (editaddcr.ival)
		{
			data[pos]='\r';
			pos++;
		}
		data[pos]='\n';
		pos++;
	}

	COM_WriteFile(s, data, len);

	madechanges = false;
	editenabled = true;
	executionlinenum = 0;

	return true;
}



static void EditorNewFile(void)
{
	fileblock_t *b;
	while(firstblock)
	{
		b = firstblock;
		firstblock=firstblock->next;
		E_Free(b);
	}

	GETBLOCK(64, firstblock);
	GETBLOCK(64, firstblock->next);
	firstblock->next->prev = firstblock;
	cursorblock = firstblock;
	cursorlinenum = 0;
	cursorx = 0;

	viewportystartblock = NULL;

	madechanges = true;
	executionlinenum = 0;

	Key_Dest_Add(kdm_editor);
	editoractive = true;
	editenabled = true;
}

static void EditorOpenFile(char *name, qboolean readonly)
{
	int i;
	char line[8192];
	int len, flen, pos=0;
	vfsfile_t *F;
	fileblock_t *b;
	char *prname;
	pubprogfuncs_t *epf = editprogfuncs;
	CloseEditor();

	strcpy(OpenEditorFile, name);

	if (!(F = FS_OpenVFS(OpenEditorFile, "rb", FS_GAME)))
	{
		Q_snprintfz(OpenEditorFile, sizeof(OpenEditorFile), "%s/%s", pr_sourcedir.string, name);
		if (!(F = FS_OpenVFS(OpenEditorFile, "rb", FS_GAME)))
		{
			Con_Printf("Couldn't open file \"%s\"\nA new file will be created\n", name);
			strcpy(OpenEditorFile, name);
			Key_Dest_Add(kdm_console);
			EditorNewFile();
			return;
		}
	}
	i=1;

	prname = OpenEditorFile;
	if (!strncmp(prname, "src/", 4))
		prname += 4;
	if (!strncmp(prname, "source/", 7))
		prname += 7;

	flen = VFS_GETLEN(F);

	while(pos < flen)
	{
		len = 0;
		for(;;)
		{
			if (pos+len >= flen || len > sizeof(line) - 16)
				break;
			line[len] = VFS_GETC(F);

			if (line[len] == '\n')
				break;
			len++;
		}
		pos+=len;
		pos++;	//and a \n

		if (editstripcr.ival)
		{
			if (line[len-1] == '\r')
				len--;
		}

		b = firstblock;

		GETBLOCK(len+1, firstblock);
		firstblock->prev = b;
		if (b)
			b->next = firstblock;

		firstblock->datalength = len;

		memcpy(firstblock->data, line, len);
		if (epf)
		{
			if (epf->ToggleBreak(epf, prname, i, 3))
			{
				firstblock->flags |= FB_BREAK;
			}
		}
		else
		{
			if (svprogfuncs)
			{
				if (svprogfuncs->ToggleBreak(svprogfuncs, prname, i, 3))
				{
					firstblock->flags |= FB_BREAK;
				}
			}
		}

		i++;
	}
	if (firstblock == NULL)
	{
		GETBLOCK(10, firstblock);
	}
	else
		for (; firstblock->prev; firstblock=firstblock->prev);
	VFS_CLOSE(F);

	cursorblock = firstblock;
	cursorx = 0;

	viewportystartblock = NULL;

	madechanges = false;
	executionlinenum = 0;
	editenabled = !readonly;

	Key_Dest_Add(kdm_editor);
	editoractive = true;
	editprogfuncs = epf;
}

extern qboolean	keydown[K_MAX];
void Editor_Key(int key, int unicode)
{
	int i;
	fileblock_t *nb;
	if (keybindings[key][0])
		if (!strcmp(keybindings[key][0], "toggleconsole"))
		{
			Key_Dest_Add(kdm_console);
			return;
		}

/*	if (CmdAfterSave)
	{
		switch (key)
		{
		case 'Y':
		case 'y':
			if (!EditorSaveFile(OpenEditorFile))
			{
				Con_Printf("Couldn't save file \"%s\"\n", OpenEditorFile);
				key_dest = key_console;
			}
			else if (!CmdAfterSaveCalled)
			{
				CmdAfterSaveCalled=true;
				(*CmdAfterSave)();
				CmdAfterSaveCalled=false;
			}
			CmdAfterSave = NULL;
			break;

		case 'N':
		case 'n':
			(*CmdAfterSave)();
			CmdAfterSave = NULL;
			break;

		case 'C':
		case 'c':
			CmdAfterSave = NULL;
			break;
		}

		return;
	}
*/
	if (key == K_SHIFT)
		return;

	if (useeval)
	{
		switch(key)
		{
		case K_ESCAPE:
			if (editprogfuncs)
				editprogfuncs->pr_trace = 0;
			useeval = false;
			return;
		case K_F3:
			useeval = false;
			return;
		case K_DEL:
			evalstring[0] = '\0';
			return;
		case K_BACKSPACE:
			i = strlen(evalstring);
			if (i < 1)
				return;
			evalstring[i-1] = '\0';
			return;
		default:
			if (unicode)
			{
				i = strlen(evalstring);
				evalstring[i] = unicode;
				evalstring[i+1] = '\0';
			}
			return;
		case K_F5:
		case K_F9:
		case K_F11:
		case K_MWHEELUP:
		case K_UPARROW:
		case K_PGUP:
		case K_MWHEELDOWN:
		case K_DOWNARROW:
		case K_PGDN:
		case K_LEFTARROW:
		case K_RIGHTARROW:
			break;
		}
	}

/*	if (ctrl_down && (key == 'c' || key == K_INS))
		key = K_F9;
	if ((ctrl_down && key == 'v') || (shift_down && key == K_INS))
		key = K_F10;
*/
	switch (key)
	{
	case K_LSHIFT:
	case K_RSHIFT:
		break;
	case K_LALT:
	case K_RALT:
		break;
	case K_LCTRL:
	case K_RCTRL:
		break;
	case K_MWHEELUP:
	case K_UPARROW:
	case K_PGUP:
		if (!cursorblock)
			break;
		GetCursorpos();
		{
			int a;
			if (key == K_PGUP)
				a =(vid.height/8)/2;
			else if (key == K_MWHEELUP)
				a = 5;
			else
				a = 1;
			while(a)
			{
				a--;
				if (cursorblock->prev)
				{
					cursorblock = cursorblock->prev;
					cursorlinenum--;
				}
				else if (cursorlinenum>1)
				{
					cursorlinenum--;
					nb = GenAsm(cursorlinenum);
					nb->next = cursorblock;
					cursorblock->prev = nb;
					firstblock = cursorblock = nb;
				}
			}
		}
		SetCursorpos();
		break;
	case K_MWHEELDOWN:
	case K_DOWNARROW:
	case K_PGDN:
		GetCursorpos();
		{
			int a;
			if (key == K_PGDN)
				a =(vid.height/8)/2;
			else if (key == K_MWHEELDOWN)
				a = 5;
			else
				a = 1;
			while(a)
			{
				a--;
				if (cursorblock->next)
				{
					cursorblock = cursorblock->next;
					cursorlinenum++;
				}
				else
				{
					nb = GenAsm(cursorlinenum+1);
					if (nb)
					{
						cursorlinenum++;
						nb->prev = cursorblock;
						cursorblock->next = nb;
						cursorblock = nb;
					}
				}
			}
		}
		SetCursorpos();
		break;

//	case K_BACK:
	case K_F1:
		Con_Printf(
			"Editor help:\n"
			"F1: Show help\n"
			"F2: Open file named on cursor line\n"
			"F3: Toggle expression evaluator\n"
			"F4: Save file\n"
			"F5: Stop tracing (resume)\n"
			"F6: Print stack trace\n"
			"F7: Save file and recompile\n"
			"F8: Change current point of execution\n"
			"F9: Set breakpoint\n"
			"F10: Save file, recompile, reload vm\n"
			"F11: Single step\n"
			"F12: \n"
			"Escape: Abort call, close editor\n"
			);
		Cbuf_AddText("toggleconsole\n", RESTRICT_LOCAL);
		break;
//	case K_FORWARD:
	case K_F2:
		{
			char file[1024];
			char *s;
			Q_strncpyz(file, cursorblock->data, sizeof(file));
			s = file;
			while (*s)
			{
				if ((*s == '/' && s[1] == '/') || (*s == '\t'))
				{
					*s = '\0';
					break;
				}
				s++;
			}
			if (*file)
				EditorOpenFile(file, false);
		}
		break;
	case K_F3:
		if (editprogfuncs)
			useeval = true;
		break;
	case K_F4:
		EditorSaveFile(OpenEditorFile);
		break;
	case K_F5:	/*stop debugging*/
		editormodal = false;
		if (editprogfuncs)
			editprogfuncs->pr_trace = false;
		break;
	case K_F6:
		if (editprogfuncs)
			PR_StackTrace(editprogfuncs, 2);
		break;
	case K_F7: /*save+recompile*/
		EditorSaveFile(OpenEditorFile);
		if (!editprogfuncs)
			Cbuf_AddText("compile; toggleconsole\n", RESTRICT_LOCAL);
		break;
	case K_F8:	/*move execution point to here - I hope you move to the same function!*/
		executionlinenum = cursorlinenum;
		executionblock = cursorblock;
		break;
	case K_F9: /*set breakpoint*/
		{
			int f = 0;
			char *fname = OpenEditorFile;
			if (!strncmp(fname, "src/", 4))
				fname += 4;
			if (!strncmp(fname, "source/", 7))
				fname += 7;

			if (editprogfuncs)
			{
				if (editprogfuncs->ToggleBreak(editprogfuncs, fname, cursorlinenum, 2))
					f |= 1;
				else
					f |= 2;
			}
#ifndef CLIENTONLY
			else if (svprogfuncs)
			{
				if (svprogfuncs->ToggleBreak(svprogfuncs, fname, cursorlinenum, 2))
					f |= 1;
				else
					f |= 2;
			}
#endif

			if (f & 1)
				cursorblock->flags |= FB_BREAK;
			else
				cursorblock->flags &= ~FB_BREAK;
		}
		break;
	case K_F10: //save+apply changes, supposedly
		EditorSaveFile(OpenEditorFile);
		Cbuf_AddText("applycompile\n", RESTRICT_LOCAL);
		break;
	case K_F11: //single step
		editormodal = false;
		break;
//	case K_STOP:
	case K_ESCAPE:
		if (editprogfuncs && editormodal)
		{
			editprogfuncs->AbortStack(editprogfuncs);
			CloseEditor();
			executionlinenum = 0;
			stepasm = true;
		}
		else
			CloseEditor();
		editormodal = false;
		break;

	case K_HOME:
		cursorx = 0;
		break;
	case K_END:
		cursorx = cursorblock->datalength;
		break;

	case K_LEFTARROW:
		cursorx--;
		if (keydown[K_LCTRL] || keydown[K_RCTRL])
		{
			//skip additional whitespace
			while(cursorx > 0 && (cursorblock->data[cursorx-1] == ' ' || cursorblock->data[cursorx-1] <= '\t'))
				cursorx--;
			//skip over the word, to the start of it
			while(cursorx > 0 && ((cursorblock->data[cursorx-1] >= 'a' && cursorblock->data[cursorx-1] <= 'z') ||
								  (cursorblock->data[cursorx-1] >= 'A' && cursorblock->data[cursorx-1] <= 'Z') ||
								  (cursorblock->data[cursorx-1] >= '0' && cursorblock->data[cursorx-1] <= '9')))
				cursorx--;
		}
		if (cursorx < 0)
			cursorx = 0;
		break;

	case K_RIGHTARROW:
		if (keydown[K_LCTRL] || keydown[K_RCTRL])
		{
			while(cursorx+1 < cursorblock->datalength && ((cursorblock->data[cursorx] >= 'a' && cursorblock->data[cursorx] <= 'z') ||
														  (cursorblock->data[cursorx] >= 'A' && cursorblock->data[cursorx] <= 'Z') ||
														  (cursorblock->data[cursorx] >= '0' && cursorblock->data[cursorx] <= '9')))
				cursorx++;
			cursorx++;
			while(cursorx+1 < cursorblock->datalength && (cursorblock->data[cursorx] == ' ' || cursorblock->data[cursorx] <= '\t'))
				cursorx++;
		}
		else
		{
			cursorx++;
			if (cursorx > cursorblock->datalength)
				cursorx = cursorblock->datalength;
		}

		break;

	case K_BACKSPACE:
		cursorx--;
		if (cursorx < 0)
		{
			fileblock_t *b = cursorblock;

			if (b == firstblock)	//no line above to remove to
			{
				cursorx=0;
				break;
			}

			if (editenabled)
			{
				cursorlinenum-=1;
				madechanges = true;

				cursorblock = b->prev;

				MakeNewSize(cursorblock, b->datalength + cursorblock->datalength+5);

				cursorx = cursorblock->datalength;
				memcpy(cursorblock->data + cursorblock->datalength, b->data, b->datalength);
				cursorblock->datalength += b->datalength;

				cursorblock->next = b->next;
				if (b->next)
					b->next->prev = cursorblock;
	//			cursorblock->prev->next = cursorblock->next;
	//			cursorblock->next->prev = cursorblock->prev;

				E_Free(b);
	//			cursorblock = b;
			}
			else
			{
				cursorblock = cursorblock->prev;
				cursorx = cursorblock->datalength;
			}

			break;
		}
	case K_DEL:	//bksp falls through.
		if (editenabled)
		{
			int a;
			fileblock_t *b;

			cursorlinenum=-1;
			madechanges = true;
//FIXME: does this work right?
			if (!cursorblock->datalength && cursorblock->next && cursorblock->prev)	//blank line
			{
				b = cursorblock;
				if (b->next)
					b->next->prev = b->prev;
				if (b->prev)
					b->prev->next = b->next;

				if (cursorblock->next)
					cursorblock = cursorblock->next;
				else
					cursorblock = cursorblock->prev;

				E_Free(b);
			}
			else
			{
				for (a = cursorx; a < cursorblock->datalength;a++)
					cursorblock->data[a] = cursorblock->data[a+1];
				cursorblock->datalength--;
			}

			if (cursorx > cursorblock->datalength)
				cursorx = cursorblock->datalength;
		}
		break;

	case K_ENTER:
	case K_KP_ENTER:
		if (editenabled)
		{
			fileblock_t *b = cursorblock;

			cursorlinenum=-1;

			madechanges = true;

			GETBLOCK(b->datalength - cursorx, cursorblock);
			cursorblock->next = b->next;
			cursorblock->prev = b;
			b->next = cursorblock;
			if (cursorblock->next)
				cursorblock->next->prev = cursorblock;
			if (cursorblock->prev)
				cursorblock->prev->next = cursorblock;

			cursorblock->datalength = b->datalength - cursorx;
			memcpy(cursorblock->data, b->data+cursorx, cursorblock->datalength);
			b->datalength = cursorx;

			cursorx = 0;
		}
		else if (cursorblock->next)
		{
			cursorblock = cursorblock->next;
			cursorlinenum++;
			cursorx = 0;
		}
		break;

	case K_INS:
		insertkeyhit = insertkeyhit?false:true;
		break;
	default:
		if (!editenabled)
			break;
		if (unicode < ' ' && unicode != '\t')	//we deem these as unprintable
			break;

		if (insertkeyhit)	//insert a char
		{
			char *s;

			madechanges = true;

			MakeNewSize(cursorblock, cursorblock->datalength+5);	//allocate a bit extra, so we don't need to keep resizing it and shifting loads a data about

			s = cursorblock->data + cursorblock->datalength;
			while (s >= cursorblock->data + cursorx)
			{
				s[1] = s[0];
				s--;
			}
			cursorx++;
			cursorblock->datalength++;
			*(s+1) = unicode;
		}
		else	//over write a char
		{
			MakeNewSize(cursorblock, cursorblock->datalength+5);	//not really needed

			cursorblock->data[cursorx] = unicode;
			cursorx++;
			if (cursorx > cursorblock->datalength)
				cursorblock->datalength = cursorx;
		}
		break;
	}
}

static void Draw_Line(int vy, fileblock_t *b, int cursorx)
{
	int nx = 0;
	int y;
	char *tooltip = NULL, *t;
	int nnx;
	qbyte *d = b->data;
	qbyte *c;
	int i;
	int smx = (mousecursor_x * vid.pixelwidth) / vid.width, smy = (mousecursor_y * vid.pixelheight) / vid.height;
	unsigned int colour;

	int ts = edittabspacing.value;
			char linebuf[128];

	if (cursorx >= 0)
		c = d + cursorx;
	else
		c = NULL;

	Font_BeginString(font_default, nx, vy, &nx, &y);

	if (ts < 1)
		ts = 4;
	ts*=8;

	//figure out the colour
	if (b->flags & (FB_BREAK))
	{
		if (executionblock == b)
			colour = COLOR_MAGENTA<<CON_FGSHIFT;
		else
			colour = COLOR_RED<<CON_FGSHIFT;
	}
	else
	{
		if (executionblock == b)
			colour = COLOR_YELLOW<<CON_FGSHIFT;	//yellow
		else
			colour = COLOR_WHITE<<CON_FGSHIFT;
	}

	//if this line currently holds the mouse cursor, figure out the word that is highighted, and evaluate that word for debugging.
	//self.ammo_shells is just 'self' if you highlight 'self', but if you highlight ammo_shells, it'll include the self, for easy debugging.
	//use the f3 evaulator for more explicit debugging.
	if (editprogfuncs && smy >= y && smy < y + Font_CharHeight())
	{
		int e, s;
		nx = -viewportx;
		for (i = 0; i < b->datalength; i++)
		{
			if (d[i] == '\t')
			{
				nnx=nx+ts;
				nnx-=(nnx - -viewportx)%ts;
			}
			else
				nnx = Font_CharEndCoord(font_default, nx, (int)d[i] | (colour));

			if (smx >= nx && smx <= nnx)
			{
				for(s = i; s > 0; )
				{
					if ((d[s-1] >= 'a' && d[s-1] <= 'z') ||
						(d[s-1] >= 'A' && d[s-1] <= 'Z') || 
						(d[s-1] >= '0' && d[s-1] <= '9') ||
						d[s-1] == '.' || d[s-1] == '_')
						s--;
					else
						break;
				}
				for (e = i; e < b->datalength; )
				{
					if ((d[e] >= 'a' && d[e] <= 'z') ||
						(d[e] >= 'A' && d[e] <= 'Z') || 
						(d[e] >= '0' && d[e] <= '9') ||
						/*d[e] == '.' ||*/ d[e] == '_')
						e++;
					else
						break;
				}
				if (e >= s+sizeof(linebuf))
					e = s+sizeof(linebuf) - 1;
				memcpy(linebuf, d+s, e - s);
				linebuf[e-s] = 0;
				if (*linebuf)
					tooltip = editprogfuncs->EvaluateDebugString(editprogfuncs, linebuf);
				break;
			}
			nx = nnx;
		}
	}
	nx = -viewportx;

	for (i = 0; i < b->datalength; i++)
	{
		if (*d == '\t')
		{
			if (d == c)
			{
				int e = Font_DrawChar(nx, y, (int)0xe00b | (CON_WHITEMASK|CON_BLINKTEXT));
				if (e >= vid.pixelwidth)
					viewportx += e - vid.pixelwidth;
				if (nx < 0)
				{
					viewportx -= -nx;
					if (viewportx < 0)
						viewportx = 0;
				}
			}
			nx+=ts;
			nx-=(nx - -viewportx)%ts;
			d++;
			continue;
		}
		if (nx <= (int)vid.pixelwidth || cursorx>=0)
			nnx = Font_DrawChar(nx, y, (int)*d | (colour));
		else nnx = vid.pixelwidth;

		if (d == c)
		{
			int e = Font_DrawChar(nx, y, (int)0xe00b | (CON_WHITEMASK|CON_BLINKTEXT));
			if (e >= vid.pixelwidth)
				viewportx += e - vid.pixelwidth;
			if (nx < 0)
			{
				viewportx -= -nx;
				if (viewportx < 0)
					viewportx = 0;
			}
		}
		nx = nnx;

		d++;
	}

	/*we didn't do the cursor! stick it at the end*/
	if (c && c >= d)
	{
		int e = Font_DrawChar(nx, y, (int)0xe00b | (CON_WHITEMASK|CON_BLINKTEXT));
		if (e >= vid.pixelwidth)
			viewportx += e - vid.pixelwidth;
		if (nx < 0)
		{
			viewportx -= -nx;
			if (viewportx < 0)
				viewportx = 0;
		}
	}

	if (tooltip)
	{
		nx = ((mousecursor_x+16) * vid.pixelwidth) / vid.width;
		while(*tooltip)
		{
			for (t = tooltip, smx = nx; *tooltip; tooltip++)
			{
				if (*tooltip == '\n')
					break;
				smx = Font_CharEndCoord(font_default, smx, *tooltip);
			}
			y = Font_CharHeight();
			Font_EndString(font_default);
			R2D_ImageColours(0, 0, 0, 1);
			R2D_FillBlock(((nx)*vid.width) / vid.pixelwidth, ((smy)*vid.height) / vid.pixelheight, ((smx - nx)*vid.width) / vid.pixelwidth, (y*vid.height) / vid.pixelheight);
			R2D_ImageColours(1, 1, 1, 1);
			Font_BeginString(font_default, nx, vy, &y, &y);
			for(smx = nx; t < tooltip; t++)
			{
				smx = Font_DrawChar(smx, smy, (COLOR_CYAN<<CON_FGSHIFT) | *t);
			}
			if (*tooltip == '\n')
				tooltip++;
			smy += Font_CharHeight();
		}
	}
	Font_EndString(font_default);
}

static fileblock_t *firstline(void)
{
	int lines;
	fileblock_t *b;
	lines = (vid.height/8)/2-1;
	b = cursorblock;
	if (!b)
		return NULL;
	while (1)
	{
		if (!b->prev)
			return b;
		b = b->prev;
		lines--;
		if (lines <= 0)
			return b;
	}
	return NULL;
}

void Editor_Draw(void)
{
	int y;
	int c;
	fileblock_t *b;

	if ((editoractive && cls.state == ca_disconnected) || editormodal)
		R2D_EditorBackground();

	if (cursorlinenum < 0)	//look for the cursor line num
	{
		cursorlinenum = 0;
		for (b = firstblock; b; b=b->next)
		{
			cursorlinenum++;
			if (b == cursorblock)
				break;
		}
	}

	if (!viewportystartblock)	//look for the cursor line num
	{
		y = 0;
		for (viewportystartblock = firstblock; viewportystartblock; viewportystartblock=viewportystartblock->prev)
		{
			y++;
			if (y == viewporty)
				break;
		}
	}

	if (madechanges)
		Draw_FunString (vid.width - 8, 0, "!");
	if (!insertkeyhit)
		Draw_FunString (vid.width - 16, 0, "O");
	if (!editenabled)
		Draw_FunString (vid.width - 24, 0, "R");
	Draw_FunString(0, 0, va("%6i:%4i:%s", cursorlinenum, cursorx+1, OpenEditorFile));

	if (useeval)
	{
		if (!editprogfuncs)
			useeval = false;
		else
		{
			char *eq, *term;
			Draw_FunString(0, 8, evalstring);

			eq = strchr(evalstring, '=');
			if (eq)
			{
				term = strchr(eq, ';');
				if (!term)
					term = strchr(eq, '\n');
				if (!term)
					term = strchr(eq, '\r');
				if (term)
				{
					*term = '\0';
					eq = NULL;
				}
				else
					*eq = '\0';
			}
			Draw_FunString(vid.width/2, 8, editprogfuncs->EvaluateDebugString(editprogfuncs, evalstring));
			if (eq)
				*eq = '=';
		}
		y = 16;
	}
	else
		y=8;
	b = firstline();
	for (; b; b=b->next)
	{
		c = -1;
		if (b == cursorblock)
			c = cursorx;
		Draw_Line(y, b, c);
		y+=8;

		if (y > vid.height)
			break;
	}

/*	if (CmdAfterSave)
	{
		if (madechanges)
		{
			M_DrawTextBox (0, 0, 36, 5);
			M_PrintWhite (16, 12, OpenEditorFile);
			M_PrintWhite (16, 28, "Do you want to save the open file?");
			M_PrintWhite (16, 36, "[Y]es/[N]o/[C]ancel");
		}
		else
		{
			if (!CmdAfterSaveCalled)
			{
				CmdAfterSaveCalled = true;
				(*CmdAfterSave) ();
				CmdAfterSaveCalled = false;
			}
			CmdAfterSave = NULL;
		}
	}
	*/
}

int QCLibEditor(pubprogfuncs_t *prfncs, char *filename, int line, int statement, int nump, char **parms)
{
	char *f1, *f2;
	if (editormodal || (line < 0 && !statement) || !pr_debugger.ival)
		return line;	//whoops

	if (qrenderer == QR_NONE)
	{
		int i;
		char buffer[8192];
		char *r;
		vfsfile_t *f;

		if (line == -1)
			return -1;
		f = FS_OpenVFS(filename, "rb", FS_GAME);
		if (!f)
			Con_Printf("%s - %i\n", filename, line);
		else
		{
			for (i = 0; i < line; i++)
			{
				VFS_GETS(f, buffer, sizeof(buffer));
			}
			if ((r = strchr(buffer, '\r')))
			{ r[0] = '\n';r[1]='\0';}
			Con_Printf("%s", buffer);
			VFS_CLOSE(f);
		}
	//PF_break(NULL);
		return line;
	}

	editprogfuncs = prfncs;

	f1 = OpenEditorFile;
	f2 = filename;
	if (!strncmp(f1, "src/", 4))
		f1 += 4;
	if (!strncmp(f2, "src/", 4))
		f2 += 4;
	if (!strncmp(f1, "source/", 7))
		f1 += 7;
	if (!strncmp(f2, "source/", 7))
		f2 += 7;

	stepasm = line < 0;

	if (stepasm)
	{
		fileblock_t *nb, *lb;
		int i;
		EditorNewFile();
		E_Free(firstblock->next);
		E_Free(firstblock);
		
		cursorlinenum = statement;
		firstblock = GenAsm(cursorlinenum);
		cursorblock = firstblock;

		for (i = cursorlinenum; i > 0 && i > cursorlinenum - 20; )
		{
			i--;
			firstblock->prev = GenAsm(i);
			firstblock->prev->next = firstblock;
			firstblock = firstblock->prev;
		}
		lb = cursorblock;
		for (i = cursorlinenum; i < cursorlinenum+20; )
		{
			i++;
			nb = GenAsm(i);
			lb->next = nb;
			nb->prev = lb;
			lb = nb;
		}
	}
	else
	{
		if (!editoractive || strcmp(f1, f2))
		{
			if (editoractive && madechanges)
				EditorSaveFile(OpenEditorFile);

			EditorOpenFile(filename, true);
		}

		for (cursorlinenum = 1, cursorblock = firstblock; cursorlinenum < line && cursorblock->next; cursorlinenum++)
			cursorblock=cursorblock->next;
	}

	executionlinenum = cursorlinenum;

	executionblock = cursorblock;

	if (!parms)
	{
		double oldrealtime = realtime;
		editormodal = true;

		while(editormodal && editoractive && editprogfuncs)
		{
			realtime = Sys_DoubleTime();
//			key_dest = key_editor;
			scr_disabled_for_loading=false;
			SCR_UpdateScreen();
			Sys_SendKeyEvents();
			IN_Commands ();
			S_ExtraUpdate();

			NET_Sleep(20, false);	//any os.
		}
		realtime = oldrealtime;

		editormodal = false;
	}

	if (stepasm)
		return -executionlinenum;
	else
		return executionlinenum;
}

void Editor_ProgsKilled(pubprogfuncs_t *dead)
{
	if (editprogfuncs == dead)
	{
		editprogfuncs = NULL;
		editormodal = false;
	}
}

static void Editor_f(void)
{
	int argc = Cmd_Argc();
	if (argc != 2 && argc != 3)
	{
		Con_Printf("edit <filename> [line]\n");
		return;
	}

	editprogfuncs = NULL;
	useeval = false;

	if (editoractive && madechanges)
		EditorSaveFile(OpenEditorFile);
	EditorOpenFile(Cmd_Argv(1), false);
//	EditorNewFile();

	if (argc == 3)
	{
		int line = atoi(Cmd_Argv(2));
		for (cursorlinenum = 1, cursorblock = firstblock; cursorlinenum < line && cursorblock->next; cursorlinenum++)
			cursorblock=cursorblock->next;
	}
}

void Editor_Init(void)
{
	Cmd_AddCommand("edit", Editor_f);

	Cvar_Register(&editstripcr, "Text editor");
	Cvar_Register(&editaddcr, "Text editor");
	Cvar_Register(&edittabspacing, "Text editor");
	Cvar_Register(&pr_debugger, "Text editor");
}
#endif
