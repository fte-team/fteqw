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
cvar_t alloweditor = SCVAR("alloweditor", "1");	//disallow loading editor for stepbystep debugging.
cvar_t editstripcr = SCVAR("edit_stripcr", "1");	//remove \r from eols (on load).
cvar_t editaddcr = SCVAR("edit_addcr", "1");		//make sure that each line ends with a \r (on save).
cvar_t edittabspacing = SCVAR("edit_tabsize", "4");

#undef pr_trace

progfuncs_t *editprogfuncs;

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

void *E_Malloc(int size)
{
	char *mem;
	mem = Z_Malloc(size);
	if (!mem)
		Sys_Error("Failed to allocate enough mem for editor\n");
	return mem;
}
void E_Free(void *mem)
{
	Z_Free(mem);
}

#define GETBLOCK(s, ret) ret = (void *)E_Malloc(sizeof(fileblock_t) + s);ret->allocatedlength = s;ret->data = (char *)ret + sizeof(fileblock_t)


char OpenEditorFile[256];


qboolean editoractive;	//(export)
qboolean editormodal;	//doesn't return. (export)
qboolean editorblocking;
qboolean madechanges;
qboolean insertkeyhit=true;
qboolean useeval;

char evalstring[256];

int executionlinenum;	//step by step debugger
int cursorlinenum, cursorx;

int viewportx;
int viewporty;


static int VFS_GETC(vfsfile_t *fp)
{
	unsigned char c;
	VFS_READ(fp, &c, 1);
	return c;
}

									//newsize = number of chars, EXCLUDING terminator.
void MakeNewSize(fileblock_t *block, int newsize)	//this is used to resize a block. It allocates a new one, copys the data frees the old one and links it into the right place
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

int positionacross;
void GetCursorpos(void)
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
void SetCursorpos(void)
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


void CloseEditor(void)
{
	fileblock_t *b;

	key_dest = key_console;
	editoractive = false;
	editprogfuncs = NULL;

	if (!firstblock)
		return;
	OpenEditorFile[0] = '\0';

	for (b = firstblock; b;)
	{
		firstblock = b;
		b=b->next;
		E_Free(firstblock);
	}

	madechanges = false;
	editormodal = false;

	firstblock = NULL;

	executionlinenum = -1;
}

qboolean EditorSaveFile(char *s)	//returns true if succesful
{

//	FILE *F;
	fileblock_t *b;

	int len=0;
	int pos=0;
	char *data;

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
/*
	F = fopen(s, "wt");
	if (!F)
		return false;
	for (b = firstblock; b; b = b->next)
	{
		fprintf(F, "%s\n", b->data);
	}
	fclose(F);
*/
	madechanges = false;
	executionlinenum = -1;

	return true;
}



void EditorNewFile()
{
	GETBLOCK(64, firstblock);
	GETBLOCK(64, firstblock->next);
	firstblock->next->prev = firstblock;
	cursorblock = firstblock;
	cursorlinenum = 0;
	cursorx = 0;

	viewportystartblock = NULL;

	madechanges = true;
	executionlinenum = -1;

	key_dest = key_editor;
	editoractive = true;
}

void EditorOpenFile(char *name)
{
	int i;
	char line[8192];
	int len, flen, pos=0;
	vfsfile_t *F;
	fileblock_t *b;

	CloseEditor();

	strcpy(OpenEditorFile, name);

	if (!(F = FS_OpenVFS(OpenEditorFile, "rb", FS_GAME)))
	{
		sprintf(OpenEditorFile, "src/%s", name);
		if (!(F = FS_OpenVFS(OpenEditorFile, "rb", FS_GAME)))
		{
			Con_Printf("Couldn't open file \"%s\"\nA new file will be created\n", name);
			strcpy(OpenEditorFile, name);
			key_dest = key_console;
			EditorNewFile();
			return;
		}
	}
	i=1;

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
		if (editprogfuncs)
		{
			if (editprogfuncs->ToggleBreak(editprogfuncs, OpenEditorFile, i, 3))
			{
				firstblock->flags |= FB_BREAK;
			}
		}
		else
		{
			if (svprogfuncs)
			{
				if (svprogfuncs->ToggleBreak(svprogfuncs, OpenEditorFile, i, 3))
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
	executionlinenum = -1;

	key_dest = key_editor;
	editoractive = true;
}

void Editor_Key(int key, int unicode)
{
	int i;
	if (keybindings[key][0])
		if (!strcmp(keybindings[key][0], "toggleconsole"))
		{
			key_dest = key_console;
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

	if (useeval && key != K_F11 && key != K_F5)
	{
		switch(key)
		{
		case K_ESCAPE:
			if (editprogfuncs)
				*editprogfuncs->pr_trace = 0;
			useeval = false;
			break;
		case K_F3:
			useeval = false;
			break;
		case K_DEL:
			evalstring[0] = '\0';
			break;
		case K_BACKSPACE:
			i = strlen(evalstring);
			if (i < 1)
				break;
			evalstring[i-1] = '\0';
			break;
		default:
			if (unicode)
			{
				i = strlen(evalstring);
				evalstring[i] = unicode;
				evalstring[i+1] = '\0';
			}
			break;
		}
		return;
	}

/*	if (ctrl_down && (key == 'c' || key == K_INS))
		key = K_F9;
	if ((ctrl_down && key == 'v') || (shift_down && key == K_INS))
		key = K_F10;
*/
	switch (key)
	{
	case K_SHIFT:
		break;
	case K_ALT:
		break;
	case K_CTRL:
		break;
	case K_PGUP:
		GetCursorpos();
		{int a=(vid.height/8)/2;
		while(a) {a--;
		if (cursorblock->prev)
		{
			cursorblock = cursorblock->prev;
			cursorlinenum--;
		}
		}
		}
		SetCursorpos();
		break;
	case K_PGDN:
		GetCursorpos();
		{int a=(vid.height/8)/2;
			while(a)
			{
				a--;
				if (cursorblock->next)
				{
					cursorblock = cursorblock->next;
					cursorlinenum++;
				}
			}
		}
		SetCursorpos();
		break;

//	case K_BACK:
	case K_F1:
//		Editor_f();
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
				EditorOpenFile(file);
		}
		break;
	case K_F3:
		if (editprogfuncs)
			useeval = true;
		break;
	case K_F4:
		EditorSaveFile(OpenEditorFile);
		break;
	case K_F5:
		editormodal = false;
		if (editprogfuncs)
			*editprogfuncs->pr_trace = false;
		break;
	case K_F6:
		if (editprogfuncs)
			PR_StackTrace(editprogfuncs);
		break;
	case K_F7:
		EditorSaveFile(OpenEditorFile);
		if (editprogfuncs)
			Cbuf_AddText("compile\n", RESTRICT_LOCAL);
		break;
	case K_F8:
		executionlinenum = cursorlinenum;
		executionblock = cursorblock;
		break;
	case K_F9:
		{
			int f = 0;
			if (editprogfuncs)
			{
				if (editprogfuncs->ToggleBreak(editprogfuncs, OpenEditorFile+4, cursorlinenum, 2))
					f |= 1;
				else
					f |= 2;
			}
#ifndef CLIENTONLY
			else if (svprogfuncs)
			{
				if (svprogfuncs->ToggleBreak(svprogfuncs, OpenEditorFile+4, cursorlinenum, 2))
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
	case K_F10:
		EditorSaveFile(OpenEditorFile);
		Cbuf_AddText("applycompile\n", RESTRICT_LOCAL);
		break;
	case K_F11:
		editormodal = false;
		break;
//	case K_STOP:
	case K_ESCAPE:
		if (editprogfuncs)
			editprogfuncs->AbortStack(editprogfuncs);
		CloseEditor();
		break;

	case K_HOME:
		cursorx = 0;
		break;
	case K_END:
		cursorx = cursorblock->datalength;
		break;

	case K_LEFTARROW:
		cursorx--;
		if (cursorx < 0)
			cursorx = 0;
		break;

	case K_RIGHTARROW:
		cursorx++;
		if (cursorx > cursorblock->datalength)
			cursorx = cursorblock->datalength;
		break;

	case K_MWHEELUP:
	case K_UPARROW:

		GetCursorpos();
		if (cursorblock->prev)
		{
			cursorblock = cursorblock->prev;
			cursorlinenum--;
		}
		SetCursorpos();
		break;
	case K_MWHEELDOWN:
	case K_DOWNARROW:

		GetCursorpos();
		if (cursorblock->next)
		{
			cursorblock = cursorblock->next;
			cursorlinenum++;
		}

		SetCursorpos();
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

			break;
		}
	case K_DEL:	//bksp falls through.
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
		break;

	case K_INS:
		insertkeyhit = insertkeyhit?false:true;
		break;
	default:
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

void Draw_CursorLine(int ox, int y, fileblock_t *b)
{
#pragma message("Fixme: ")
/*
	int x=0;
	qbyte *d = b->data;
	int cx;
	int a = 0, i;

	int colour=COLOR_BLUE;

	int ts = edittabspacing.value;
	if (ts < 1)
		ts = 4;
	ts*=8;

	if (b->flags & (FB_BREAK))
		colour = COLOR_RED;	//red

	if (executionblock == b)
	{
		if (colour)	//break point too
			colour = COLOR_GREEN;	//green
		else
			colour = COLOR_YELLOW;	//yellow
	}

	if (cursorx <= strlen(d)+1 && (int)(Sys_DoubleTime()*4.0) & 1)
		cx = -1;
	else
		cx = cursorx;
	for (i = 0; i < b->datalength; i++)
	{
		if (*d == '\t')
		{
			if (a == cx)
				Draw_ColouredCharacter (x+ox, y, 11|CON_WHITEMASK);
			x+=ts;
			x-=x%ts;
			d++;
			a++;
			continue;
		}
		if (x+ox< vid.width)
		{
			if (a == cx)
				Draw_ColouredCharacter (x+ox, y, 11|CON_WHITEMASK);
			else
				Draw_ColouredCharacter (x+ox, y, (int)*d | (colour<<CON_FGSHIFT));
		}
		d++;
		x += 8;
		a++;
	}
	if (a == cx)
		Draw_ColouredCharacter (x+ox, y, 11|CON_WHITEMASK);
*/
}

void Draw_NonCursorLine(int x, int y, fileblock_t *b)
{
#pragma message("Fixme: ")
/*
	int nx = 0;
	qbyte *d = b->data;
	int i;

	int colour=COLOR_WHITE;

	int ts = edittabspacing.value;
	if (ts < 1)
		ts = 4;
	ts*=8;

	if (b->flags & (FB_BREAK))
		colour = COLOR_RED;	//red

	if (executionblock == b)
	{
		if (colour)	//break point too
			colour = COLOR_GREEN;	//green
		else
			colour = COLOR_YELLOW;	//yellow
	}

	for (i = 0; i < b->datalength; i++)
	{
		if (*d == '\t')
		{
			nx+=ts;
			nx-=nx%ts;
			d++;
			continue;
		}
		if (x+nx < vid.width)
			Draw_ColouredCharacter (x+nx, y, (int)*d | (colour<<CON_FGSHIFT));
		d++;
		nx += 8;
	}
*/
}

fileblock_t *firstline(void)
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
	int x;
	int y;
	fileblock_t *b;

	if (key_dest != key_console)
		key_dest = key_editor;

	if ((editoractive && cls.state == ca_disconnected) || editormodal)
		Draw_EditorBackground();

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

	x=0;
	for (y = 0; y < cursorx; y++)
	{
		if (cursorblock->data[y] == '\0')
			break;
		else if (cursorblock->data[y] == '\t')
		{
			x+=32;
			x&=~31;
		}
		else
			x+=8;
	}
	x=-x + vid.width/2;
	if (x > 0)
		x = 0;

	if (madechanges)
		Draw_FunString (vid.width - 8, 0, "!");
	if (!insertkeyhit)
		Draw_FunString (vid.width - 16, 0, "O");
	Draw_FunString(0, 0, va("%6i:%4i:%s", cursorlinenum, cursorx+1, OpenEditorFile));

	if (useeval)
	{
		if (!editprogfuncs)
			useeval = false;
		else
		{
			char *eq;
			Draw_FunString(0, 8, evalstring);

			eq = strchr(evalstring, '=');
			if (eq)
			{
				if (strchr(eq, ';'))
				{
					*strchr(eq, ';') = '\0';
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
		if (b == cursorblock)
			Draw_CursorLine(x, y, b);
		else
			Draw_NonCursorLine(x, y, b);
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

int QCLibEditor(progfuncs_t *prfncs, char *filename, int line, int nump, char **parms)
{
	if (editormodal || !developer.ival)
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

	if (!strncmp(OpenEditorFile, "src/", 4))
	{
		if (!editoractive || strcmp(OpenEditorFile+4, filename))
		{
			if (editoractive)
				EditorSaveFile(OpenEditorFile);

			EditorOpenFile(filename);
		}

	}
	else
	{
		if (!editoractive || strcmp(OpenEditorFile, filename))
		{
			if (editoractive)
				EditorSaveFile(OpenEditorFile);

			EditorOpenFile(filename);
		}
	}

	for (cursorlinenum = 1, cursorblock = firstblock; cursorlinenum < line && cursorblock->next; cursorlinenum++)
		cursorblock=cursorblock->next;

	executionblock = cursorblock;

	if (!parms)
	{
		editormodal = true;

		while(editormodal && editoractive && editprogfuncs)
		{
//			key_dest = key_editor;
			scr_disabled_for_loading=false;
			SCR_UpdateScreen();
			Sys_SendKeyEvents();
			S_ExtraUpdate();

			NET_Sleep(100, false);	//any os.
		}

		editormodal = false;
	}

	return line;
}

void Editor_f(void)
{
	if (Cmd_Argc() != 2)
	{
		Con_Printf("edit <filename>\n");
		return;
	}

	editprogfuncs = NULL;
	useeval = false;

	if (editoractive)
		EditorSaveFile(OpenEditorFile);
	EditorOpenFile(Cmd_Argv(1));
//	EditorNewFile();
}

void Editor_Init(void)
{
	Cmd_AddCommand("edit", Editor_f);

	Cvar_Register(&alloweditor, "Text editor");
	Cvar_Register(&editstripcr, "Text editor");
	Cvar_Register(&editaddcr, "Text editor");
	Cvar_Register(&edittabspacing, "Text editor");
}
#endif
