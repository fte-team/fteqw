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
F10 will step over.
F11 will step into.
*/

#include "quakedef.h"
#ifdef TEXTEDITOR
#include "pr_common.h"

#include "shader.h"

//#if defined(ANDROID) || defined(SERVERONLY)
#define debugger_default "0"
//#else
//#define debugger_default "1"
//#endif

static cvar_t editstripcr = CVARD("edit_stripcr", "1", "remove \\r from eols (on load)");
static cvar_t editaddcr = CVARD("edit_addcr", "", "make sure that each line ends with a \\r (on save). Empty will be assumed to be 1 on windows and 0 otherwise.");
//static cvar_t edittabspacing = CVARD("edit_tabsize", "4", "How wide tab alignment is");
cvar_t pr_debugger = CVARAFD("pr_debugger", debugger_default, "debugger", CVAR_SAVE, "When enabled, QC errors and debug events will enable step-by-step tracing.");
extern cvar_t pr_sourcedir;

static pubprogfuncs_t *editprogfuncs;

qboolean editoractive;			//(export)
console_t *editormodal;			//doesn't return. (export)
int editorstep;					//execution resumption type
static qboolean stepasm;		//debugging with (generated) asm.
static int executionlinenum;	//debugging execution line.

#if defined(CSQC_DAT) && !defined(SERVERONLY)
extern world_t csqc_world;
#endif
#if defined(MENU_DAT) && !defined(SERVERONLY)
extern world_t menu_world;
#endif



void Editor_Draw(void)
{
	R2D_EditorBackground();
	Key_Dest_Add(kdm_cwindows);
}
qboolean Editor_Key(int key, int unicode)
{
	if (editormodal)
	{
		if (key >= K_F1 && key <= K_F12)
			return editormodal->redirect(editormodal, unicode, key);
	}
	return false;
}

void Editor_Demodalize(void)
{
	if (editormodal && editormodal->highlightline)
	{
		editormodal->highlightline->flags &= ~CONL_EXECUTION;
		editormodal->highlightline = NULL;
	}
	editormodal = NULL;
}


int Con_Editor_GetLine(console_t *con, conline_t *line)
{
	int linenum = 1;
	conline_t *l;
	for (l = con->oldest; l; l = l->newer, linenum++)
	{
		if (l == line)
			return linenum;
	}
	return 0;
}
conline_t *Con_Editor_FindLine(console_t *con, int line)
{
	conline_t *l;
	for (l = con->oldest; l; l = l->newer)
	{
		if (--line == 0)
			return l;
	}
	return NULL;
}

int Con_Editor_Evaluate(console_t *con, char *evalstring)
{
	char *eq, *term;

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
	Con_Footerf(con, false, "%s", evalstring);
	if (eq)
	{
		*eq = '=';
		Con_Footerf(con, true, " = %s", editprogfuncs->EvaluateDebugString(editprogfuncs, evalstring));
	}
	else
		Con_Footerf(con, true, " == %s", editprogfuncs->EvaluateDebugString(editprogfuncs, evalstring));
	con->linebuffered = NULL;
	return true;
}

//creates a new line following an existing line by splitting the previous
conline_t *Con_EditorSplit(console_t *con, conline_t *orig, int offset)
{
	conline_t *l;
	l = BZ_Malloc(sizeof(*l)+(orig->length-offset)*sizeof(conchar_t));
	*l = *orig;
	l->length = l->maxlength = orig->length-con->useroffset;
	memcpy(l+1, (conchar_t*)(orig+1)+offset, l->length*sizeof(conchar_t));
	orig->length = offset;	//truncate the old line
	l->older = orig;
	l->flags &= ~(CONL_EXECUTION|CONL_BREAKPOINT);
	orig->newer = l;
	if (con->current == orig)
		con->current = l;
	else
		l->newer->older = l;
	if (con->display == orig)
		con->display = l;
	con->linecount++;

	con->selendline = con->selstartline = NULL;
	return l;
}
conline_t *Con_EditorMerge(console_t *con, conline_t *first, conline_t *second)
{
	conline_t *l;
	l = Con_ResizeLineBuffer(con, first, first->length+second->length);

	//unlink the second line
	l->newer = second->newer;
	if (l->newer)
		l->newer->older = l;

	//heal references to the second to point to the first
	if (con->selstartline == second)
	{
		con->selstartline = l;
		con->selstartoffset += l->length;
	}
	if (con->selendline == second)
	{
		con->selendline = l;
		con->selendoffset += l->length;
	}
	if (con->display == second)
		con->display = l;
	if (con->oldest == second)
		con->oldest = l;
	if (con->current == second)
		con->current = l;
	if (con->userline == second)
	{
		con->userline = l;
		con->useroffset += l->length;
	}
	if (con->highlightline == second)
	{
		con->highlightline = l;
		con->highlightline->flags |= CONL_EXECUTION;
	}
	
	//copy over the chars
	memcpy((conchar_t*)(l+1)+l->length, (conchar_t*)(second+1), second->length*sizeof(conchar_t));
	l->length += second->length;

	//and that line is now dead.
	con->linecount--;
	BZ_Free(second);

	return l;
}
static void Con_Editor_DeleteSelection(console_t *con)
{
	conline_t *n;
	con->flags &= ~CONF_KEEPSELECTION;
	if (con->selstartline == con->selendline)
	{
		memmove((conchar_t*)(con->selstartline+1)+con->selstartoffset, (conchar_t*)(con->selendline+1)+con->selendoffset, sizeof(conchar_t)*(con->selendline->length - con->selendoffset));
		con->selendline->length = con->selstartoffset + (con->selendline->length - con->selendoffset);
	}
	else
	{
		con->selstartline->length = con->selstartoffset;
		for(n = con->selstartline;;)
		{
			n = n->newer;
			if (!n)
				break;	//shouldn't happen
			if (n == con->selendline)
			{
				//this is the last line, we need to keep the end of the string but not the start.
				memmove(n+1, (conchar_t*)(n+1)+con->selendoffset, sizeof(conchar_t)*(n->length - con->selendoffset));
				n->length = n->length - con->selendoffset;
				n = Con_EditorMerge(con, con->selstartline, n);
				break;
			}
			//truncate and merge
			n->length = 0;
			n = Con_EditorMerge(con, con->selstartline, n);
		}
	}
	con->userline = con->selstartline;
	con->useroffset = con->selstartoffset;
}
static void Con_Editor_Paste(console_t *con)
{
	char *clipText = Sys_GetClipboard();
	if (clipText)
	{
		conchar_t buffer[8192], *end;
		char *s, *nl;
		if (*clipText && (con->flags & CONF_KEEPSELECTION))
			Con_Editor_DeleteSelection(con);
		for(s = clipText; ; )
		{
			nl = strchr(s, '\n');
			if (nl)
				*nl = 0;
			end = COM_ParseFunString(CON_WHITEMASK, s, buffer, sizeof(buffer), PFS_FORCEUTF8);
			if (Con_InsertConChars(con, con->userline, con->useroffset, buffer, end-buffer))
				con->useroffset += end-buffer;

			if (nl)
			{
				con->userline = Con_EditorSplit(con, con->userline, con->useroffset);
				con->useroffset = 0;
				s = nl+1;
			}
			else
				break;
		}
		Sys_CloseClipboard(clipText);
	}
}
static void Con_Editor_Save(console_t *con)
{
	vfsfile_t *file;
	conline_t *line;

	FS_CreatePath(con->name, FS_GAMEONLY);
	file = FS_OpenVFS(con->name, "wb", FS_GAMEONLY);
	if (file)
	{
		for (line = con->oldest; line; line = line->newer)
		{
			conchar_t *cl = (conchar_t*)(line+1);
			conchar_t *el = cl + line->length;
			char buffer[65536];
			char *bend = COM_DeFunString(cl, el, buffer, sizeof(buffer)-2, true, !!(con->parseflags & PFS_FORCEUTF8));
			if (editaddcr.ival 
#ifdef _WIN32
				|| !*editaddcr.string
#endif
				)
				*bend++ = '\r';
			*bend++ = '\n';
			VFS_WRITE(file, buffer, bend-buffer);
		}
		VFS_CLOSE(file);
	

		Q_snprintfz(con->title, sizeof(con->title), "SAVED: %s", con->name);

		if (!Q_strncasecmp(con->name, "scripts/", 8))
			Shader_NeedReload(true);
	}
}
qboolean	Con_Editor_MouseOver(struct console_s *con, char **out_tiptext, shader_t **out_shader)
{
	char *mouseover = Con_CopyConsole(con, true, false);

	if (mouseover)
	{
		if (editprogfuncs && editprogfuncs->EvaluateDebugString)
			*out_tiptext = editprogfuncs->EvaluateDebugString(editprogfuncs, mouseover);
		else
		{
#ifndef SERVERONLY
#ifdef CSQC_DAT
			if (csqc_world.progs && csqc_world.progs->EvaluateDebugString && !*out_tiptext)
				*out_tiptext = csqc_world.progs->EvaluateDebugString(csqc_world.progs, mouseover);
#endif
#ifdef MENU_DAT
			if (menu_world.progs && menu_world.progs->EvaluateDebugString && !*out_tiptext)
				*out_tiptext = menu_world.progs->EvaluateDebugString(menu_world.progs, mouseover);
#endif
#endif
#ifndef CLIENTONLY
			if (sv.world.progs && sv.world.progs->EvaluateDebugString && !*out_tiptext)
				*out_tiptext = sv.world.progs->EvaluateDebugString(sv.world.progs, mouseover);
#endif
		}
		Z_Free(mouseover);
	}

	return true;
}
void Con_EditorMoveCursor(console_t *con, conline_t *newline, int newoffset, qboolean shiftheld, qboolean moveprior)
{
	if (!shiftheld)
		con->flags &= ~CONF_KEEPSELECTION;
	else
	{
		if (!(con->flags & CONF_KEEPSELECTION) || (con->selendline == con->selstartline && con->selendoffset == con->selstartoffset))
		{
			con->flags |= CONF_KEEPSELECTION;
			if (moveprior)
			{
				con->selstartline = newline;
				con->selstartoffset = newoffset;
				con->selendline = con->userline;
				con->selendoffset = con->useroffset;
			}
			else
			{
				con->selstartline = con->userline;
				con->selstartoffset = con->useroffset;
				con->selendline = newline;
				con->selendoffset = newoffset;
			}
		}
		else
		{
			if (con->selendline == con->userline && con->selendoffset == con->useroffset)
			{
				if (con->selstartline != con->selendline && con->selstartline == newline && moveprior)
				{	//inverted
					con->selendline = con->selstartline;
					con->selendoffset = con->selstartoffset;

					con->selstartline = newline;
					con->selstartoffset = newoffset;
				}
				else
				{
					con->selendline = newline;
					con->selendoffset = newoffset;
				}
			}
			else if (con->selstartline == con->userline && con->selstartoffset == con->useroffset)
			{
				if (con->selstartline == con->selendline && con->selstartline != newline && !moveprior)
				{	//inverted
					con->selstartline = con->selendline;
					con->selstartoffset = con->selendoffset;

					con->selendline = newline;
					con->selendoffset = newoffset;
				}
				else
				{
					con->selstartline = newline;
					con->selstartoffset = newoffset;
				}
			}
		}
	}

	if (con->userline == con->display && !moveprior)
		con->display = newline;

	con->userline = newline;
	con->useroffset = newoffset;
}
qboolean Con_Editor_Key(console_t *con, unsigned int unicode, int key)
{
	extern qboolean	keydown[K_MAX];
	qboolean altdown = keydown[K_LALT] || keydown[K_RALT];
	qboolean ctrldown = keydown[K_LCTRL] || keydown[K_RCTRL];
	qboolean shiftdown = keydown[K_LSHIFT] || keydown[K_RSHIFT];
	if (key == K_MOUSE1)
	{
		con->flags &= ~CONF_KEEPSELECTION;
		con->buttonsdown = CB_SELECT;
		return true;
	}
	if (key == K_MOUSE2)
	{
		con->flags &= ~CONF_KEEPSELECTION;
		con->buttonsdown = CB_SCROLL;
		return true;
	}
	if (!con->userline)
		return false;
	if (con->linebuffered)
		return false;
	switch(key)
	{
	case K_BACKSPACE:
		if (con->flags & CONF_KEEPSELECTION)
			Con_Editor_DeleteSelection(con);
		else if (con->useroffset == 0)
		{
			if (con->userline->older)
				Con_EditorMerge(con, con->userline->older, con->userline);
		}
		else
		{
			con->useroffset--;
			memmove((conchar_t*)(con->userline+1)+con->useroffset, (conchar_t*)(con->userline+1)+con->useroffset+1, (con->userline->length - con->useroffset)*sizeof(conchar_t));
			con->userline->length -= 1;
		}
		return true;
	case K_DEL:
		if (con->flags & CONF_KEEPSELECTION)
			Con_Editor_DeleteSelection(con);
		else if (con->useroffset == con->userline->length)
		{
			if (con->userline->newer)
				Con_EditorMerge(con, con->userline, con->userline->newer);
		}
		else
		{
			memmove((conchar_t*)(con->userline+1)+con->useroffset, (conchar_t*)(con->userline+1)+con->useroffset+1, (con->userline->length - con->useroffset)*sizeof(conchar_t));
			con->userline->length -= 1;
		}
		break;
	case K_ENTER:	/*split the line into two, selecting the new line*/
		if (con->flags & CONF_KEEPSELECTION)
			Con_Editor_DeleteSelection(con);
		con->userline = Con_EditorSplit(con, con->userline, con->useroffset);
		con->useroffset = 0;
		break;
	case K_HOME:
		if (ctrldown)
			con->display = con->oldest;
		else
			Con_EditorMoveCursor(con, con->userline, 0, shiftdown, true);
		return true;
	case K_END:
		if (ctrldown)
			con->display = con->current;
		else
			Con_EditorMoveCursor(con, con->userline, con->userline->length, shiftdown, false);
		return true;
	case K_UPARROW:
		if (con->userline->older)
		{
			if (con->useroffset > con->userline->older->length)
				Con_EditorMoveCursor(con, con->userline->older, con->userline->older->length, shiftdown, true);
			else
				Con_EditorMoveCursor(con, con->userline->older, con->useroffset, shiftdown, true);
		}
		return true;
	case K_DOWNARROW:
		if (con->userline->newer)
		{
			if (con->useroffset > con->userline->newer->length)
				Con_EditorMoveCursor(con, con->userline->newer, con->userline->newer->length, shiftdown, false);
			else
				Con_EditorMoveCursor(con, con->userline->newer, con->useroffset, shiftdown, false);
		}
		return true;
	case K_LEFTARROW:
		if (con->useroffset == 0)
		{
			if (con->userline->older)
				Con_EditorMoveCursor(con, con->userline->older, con->userline->older->length, shiftdown, true);
		}
		else
			Con_EditorMoveCursor(con, con->userline, con->useroffset-1, shiftdown, true);
		return true;
	case K_RIGHTARROW:
		if (con->useroffset == con->userline->length)
		{
			if (con->userline->newer)
				Con_EditorMoveCursor(con, con->userline->newer, 0, shiftdown, false);
		}
		else
			Con_EditorMoveCursor(con, con->userline, con->useroffset+1, shiftdown, false);
		return true;
	case K_INS:
		if (shiftdown)
		{
			if (con->flags & CONF_KEEPSELECTION)
				Con_Editor_DeleteSelection(con);
			Con_Editor_Paste(con);
			break;
		}
		if (ctrldown && (con->flags & CONF_KEEPSELECTION))
		{
			char *buffer = Con_CopyConsole(con, true, false);	//don't keep markup if we're copying to the clipboard
			if (buffer)
			{
				Sys_SaveClipboard(buffer);
				Z_Free(buffer);
			}
			break;
		}
		return false;
	case K_LSHIFT:
	case K_RSHIFT:
	case K_LCTRL:
	case K_RCTRL:
	case K_LALT:
	case K_RALT:
		return true;	//these non-printable chars generally should not be allowed to trigger bindings.

	case K_F1:
		Con_Printf(
			"Editor help:\n"
			"F1: Show help\n"
			"F2: Open file named on cursor line\n"
			"F3: Toggle expression evaluator\n"
			"CTRL+S: Save file\n"
			"F5: Stop tracing (continue running)\n"
			"F6: Print stack trace\n"
			"F8: Change current point of execution\n"
			"F9: Set breakpoint\n"
			"ALT+F10: save+recompile\n"
			"F10: Step Over (skipping children)\n"
			"SHIFT+F11: Step Out\n"
			"F11: Step Into\n"
//			"F12: Go to definition\n"
			);
		Cbuf_AddText("toggleconsole\n", RESTRICT_LOCAL);
		break;
	case K_F2:
		/*{
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
		}*/
		return true;
	case K_F3:
		if (editprogfuncs)
		{
			con->linebuffered = Con_Editor_Evaluate;
		}
		return true;
	case K_F5:	//stop debugging
		if (editormodal)
		{
			Editor_Demodalize();
			editorstep = DEBUG_TRACE_OFF;
		}
		return true;
	case K_F6:
		if (editprogfuncs)
		{
			PR_StackTrace(editprogfuncs, 2);
			Key_Dest_Add(kdm_console);
			return true;
		}
		return false;
	case K_F8:	//move execution point to here - I hope you move to the same function!
		if (editprogfuncs && con->userline)
		{
			int l = Con_Editor_GetLine(con, con->userline);
			if (l)
			{
				conline_t *n = Con_Editor_FindLine(con, l);
				if (n)
				{
					if (con->highlightline)
					{
						con->highlightline->flags &= ~CONL_EXECUTION;
						con->highlightline = NULL;
					}

					executionlinenum = l;
					con->highlightline = n;
					n->flags |= CONL_EXECUTION;
				}
			}
		}
		return true;
	case K_F9: /*set breakpoint*/
		{
			conline_t *cl;
			char *fname = con->name;
			int mode;
			int line;
			if (!strncmp(fname, pr_sourcedir.string, strlen(pr_sourcedir.string)) && fname[strlen(pr_sourcedir.string)] == '/')
				fname += strlen(pr_sourcedir.string)+1;
			else if (!strncmp(fname, "src/", 4))
				fname += 4;
			else if (!strncmp(fname, "source/", 7))
				fname += 7;
			else if (!strncmp(fname, "qcsrc/", 7))
				fname += 7;


			cl = con->userline;
			line = Con_Editor_GetLine(con, cl);

			if (cl->flags & CONL_BREAKPOINT)
				mode = 0;
			else
				mode = 1;

#ifndef SERVERONLY
#ifdef CSQC_DAT
			if (csqc_world.progs && csqc_world.progs->ToggleBreak)
				csqc_world.progs->ToggleBreak(csqc_world.progs, fname, line, mode);
#endif
#ifdef MENU_DAT
			if (menu_world.progs && menu_world.progs->ToggleBreak)
				menu_world.progs->ToggleBreak(menu_world.progs, fname, line, mode);
#endif
#endif
#ifndef CLIENTONLY
			if (sv.world.progs && sv.world.progs->ToggleBreak)
				sv.world.progs->ToggleBreak(sv.world.progs, fname, line, mode);
#endif

			if (mode)
				cl->flags |= CONL_BREAKPOINT;
			else
				cl->flags &= ~CONL_BREAKPOINT;
		}
		return true;
	case K_F10:
		if (altdown)
		{
			Con_Editor_Save(con);
			if (!editprogfuncs)
				Cbuf_AddText("compile; toggleconsole\n", RESTRICT_LOCAL);
			return true;
		}
		//if (editormodal)	//careful of autorepeat
		{
			Editor_Demodalize();
			editorstep = DEBUG_TRACE_OVER;
			return true;
		}
		return false;
	case K_F11: //single step
		//if (editormodal)	//careful of auto-repeat
		{
			Editor_Demodalize();
			editorstep = shiftdown?DEBUG_TRACE_OUT:DEBUG_TRACE_INTO;
			return true;
		}
		return false;


	default:
		if (ctrldown && key =='s')
		{
			Con_Editor_Save(con);
			return true;
		}
		if (ctrldown && key =='v')
		{
			if (con->flags & CONF_KEEPSELECTION)
				Con_Editor_DeleteSelection(con);
			Con_Editor_Paste(con);
			break;
		}
		if (ctrldown && key =='c' && (con->flags & CONF_KEEPSELECTION))
		{
			char *buffer = Con_CopyConsole(con, true, false);	//don't keep markup if we're copying to the clipboard
			if (buffer)
			{
				Sys_SaveClipboard(buffer);
				Z_Free(buffer);
			}
			break;
		}
		if (unicode)
		{
			conchar_t c[2];
			int l = 0;
			if (unicode > 0xffff)
				c[l++] = CON_LONGCHAR | (unicode>>16);
			c[l++] = CON_WHITEMASK | (unicode&0xffff);
			if (con->flags & CONF_KEEPSELECTION)
				Con_Editor_DeleteSelection(con);
			if (Con_InsertConChars(con, con->userline, con->useroffset, c, l))
				con->useroffset += l;
			break;
		}
		return false;
	}
	Q_snprintfz(con->title, sizeof(con->title), "MODIFIED: %s", con->name);
	return true;
}
void Con_Editor_CloseCallback(void *ctx, int op)
{
	console_t *con = ctx;

	if (con != con_curwindow)	//ensure that it still exists (lame only-active-window check)
		return;
	
	if (op == 0)
		Con_Editor_Save(con);
	if (op != -1)	//-1 == cancel
		Con_Destroy(con);
}
qboolean Con_Editor_Close(console_t *con, qboolean force)
{
	if (!force)
	{
		if (!strncmp(con->title, "MODIFIED: ", 10))
		{
			M_Menu_Prompt(Con_Editor_CloseCallback, con, va("Save changes?\n%s\n", con->name), "Yes", "No", "Cancel");
			return false;
		}
	}
	if (con == editormodal)
	{
		Editor_Demodalize();
		editorstep = DEBUG_TRACE_OFF;
	}
	return true;
}
void Con_Editor_GoToLine(console_t *con, int line)
{
	con->userline = con->oldest;
	while (line --> 1)
	{
		if (!con->userline->newer)
			break;
		con->userline = con->userline->newer;
	}
	con->useroffset = 0;
	con->display = con->userline;

	//FIXME: we REALLY need to support top-down style consoles.
	line = con->wnd_h / 8;
	line /= 2;
	while (con->display->newer && line --> 0)
	{
		con->display = con->display->newer;
	}
}
console_t *Con_TextEditor(const char *fname, const char *line, qboolean newfile)
{
	static int editorcascade;
	console_t *con;
	con = Con_FindConsole(fname);
	if (con)
	{
		Con_SetActive(con);
		if (line && con->redirect == Con_Editor_Key)
			Con_Editor_GoToLine(con, atoi(line));

		if (con->close != Con_Editor_Close)
			con = NULL;
	}
	else
	{
		con = Con_Create(fname, 0);
		if (con)
		{
			vfsfile_t *file;
			Q_snprintfz(con->title, sizeof(con->title), "EDIT: %s", con->name);

			/*make it a console window thing*/
			con->flags |= CONF_ISWINDOW;
			con->wnd_x = (editorcascade & 1)?vid.width/2:0;
#ifdef ANDROID
			con->wnd_y = 0;
#else
			con->wnd_y = (editorcascade & 2)?vid.height/2:0;
#endif
			con->wnd_w = vid.width/2;
			con->wnd_h = vid.height/2;
			editorcascade++;

			con->flags |= CONF_NOWRAP;	//disable line wrapping. yay editors.
			con->flags |= CONF_KEEPSELECTION;	//only change the selection if we ask for it.
			con->parseflags = PFS_FORCEUTF8;
			con->userdata = NULL;
			con->linebuffered = NULL;
			con->redirect = Con_Editor_Key;
			con->mouseover = Con_Editor_MouseOver;
			con->close = Con_Editor_Close;
			con->maxlines = 0x7fffffff;	//line limit is effectively unbounded.
			
			if (!newfile)
			{
				file = FS_OpenVFS(fname, "rb", FS_GAME);
				if (file)
				{
					char buffer[65536];
					while (VFS_GETS(file, buffer, sizeof(buffer)))
					{
						Con_PrintCon(con, buffer, PFS_FORCEUTF8|PFS_KEEPMARKUP|PFS_NONOTIFY);
						Con_PrintCon(con, "\n", PFS_FORCEUTF8|PFS_KEEPMARKUP|PFS_NONOTIFY);
					}
					VFS_CLOSE(file);
				}
			}

			con->display = con->oldest;
			con->selstartline = con->selendline = con->oldest;	//put the cursor at the start of the file
			con->selstartoffset = con->selendoffset = 0;

			if (line)
				Con_Editor_GoToLine(con, atoi(line));

			Con_Footerf(con, false, "    ^2%i lines", con->linecount);

			Con_SetActive(con);
		}
	}
	return con;
}

void Con_TextEditor_f(void)
{
	char *fname = Cmd_Argv(1);
	char *line = strrchr(fname, ':');
	if (line)
		*line++ = 0;
	if (!*fname)
	{
		Con_Printf("%s [filename[:line]]: edit a file\n", Cmd_Argv(0));
		return;
	}
	Con_TextEditor(fname, line, false);
}

int QCLibEditor(pubprogfuncs_t *prfncs, const char *filename, int *line, int *statement, char *reason, pbool fatal)
{
	char newname[MAX_QPATH];
	console_t *edit;
	if (!pr_debugger.ival)
	{
		Con_Printf("Set %s to trace\n", pr_debugger.name);
		if (fatal)
			return DEBUG_TRACE_ABORT;
		return DEBUG_TRACE_OFF;	//get lost
	}

	//we can cope with no line info by displaying asm
	if (editormodal || !statement
		|| !line || *line == -1	//FIXME
		)
	{
		if (fatal)
			return DEBUG_TRACE_ABORT;
		return DEBUG_TRACE_OFF;	//whoops
	}

	if (qrenderer == QR_NONE)
	{	//just dump the line of code that's being execed onto the console.
		int i;
		char buffer[8192];
		char *r;
		vfsfile_t *f;

		if (!line)
		{	//please don't crash
			if (fatal)
				return DEBUG_TRACE_ABORT;
			return DEBUG_TRACE_OFF;	//whoops
		}

		f = FS_OpenVFS(filename, "rb", FS_GAME);
		if (!f)
			Con_Printf("%s - %i\n", filename, *line);
		else
		{
			for (i = 0; i < *line; i++)
			{
				VFS_GETS(f, buffer, sizeof(buffer));
			}
			if ((r = strchr(buffer, '\r')))
			{ r[0] = '\n';r[1]='\0';}
			Con_Printf("%s", buffer);
			VFS_CLOSE(f);
		}
		return DEBUG_TRACE_OUT;	//only display the line itself.
	}

	stepasm = !line;
	editprogfuncs = prfncs;

	if (!COM_FCheckExists(filename))
	{
		//people generally run their qcc from $mod/src/ or so, so paths are usually relative to that instead of the mod directory.
		//this means we now need to try and guess what the user used.
		if (filename != newname && *pr_sourcedir.string)
		{
			Q_snprintfz(newname, sizeof(newname), "%s/%s", pr_sourcedir.string, filename);
			if (COM_FCheckExists(newname))
				filename = newname;
		}
		if (filename != newname)
		{
			Q_snprintfz(newname, sizeof(newname), "src/%s", filename);
			if (COM_FCheckExists(newname))
				filename = newname;
		}
		if (filename != newname)
		{
			Q_snprintfz(newname, sizeof(newname), "source/%s", filename);
			if (COM_FCheckExists(newname))
				filename = newname;
		}
		if (filename != newname)
		{
			Q_snprintfz(newname, sizeof(newname), "qcsrc/%s", filename);
			if (COM_FCheckExists(newname))
				filename = newname;
		}
		if (filename != newname)
		{	//some people are fucked in the head
			Q_snprintfz(newname, sizeof(newname), "progs/%s", filename);
			if (COM_FCheckExists(newname))
				filename = newname;
		}
		if (filename != newname)
		{
			if (fatal)
			{
				Con_Printf(CON_ERROR "Unable to find %s\n", filename);
				return DEBUG_TRACE_ABORT;
			}
			Con_Printf(CON_WARNING "Unable to find %s\n", filename);
			return DEBUG_TRACE_OFF;	//whoops
		}
	}

	if (stepasm)
	{
		return DEBUG_TRACE_OFF;
	}
	else
	{
		edit = Con_TextEditor(filename, NULL, false);
		if (!edit)
			return DEBUG_TRACE_OFF;
		Con_Editor_GoToLine(edit, *line);
	}

	executionlinenum = *line;

	{
		double oldrealtime = realtime;

		Editor_Demodalize();
		editormodal = edit;
		editorstep = DEBUG_TRACE_OFF;

		if (edit->userline)
		{
			edit->highlightline = edit->userline;
			edit->highlightline->flags |= CONL_EXECUTION;
		}

		while(editormodal && editprogfuncs)
		{
			realtime = Sys_DoubleTime();
			scr_disabled_for_loading=false;
			SCR_UpdateScreen();
			Sys_SendKeyEvents();
			IN_Commands ();
			S_ExtraUpdate();

			NET_Sleep(20/1000.0, false);	//any os.
		}
		realtime = oldrealtime;

		Editor_Demodalize();
	}

	if (stepasm)
	{
		if (line)
			*line = 0;
		*statement = executionlinenum;
	}
	else if (line)
		*line = executionlinenum;
	return editorstep;
}
void Editor_ProgsKilled(pubprogfuncs_t *dead)
{
	if (editprogfuncs == dead)
	{
		editprogfuncs = NULL;
		Editor_Demodalize();
		editorstep = DEBUG_TRACE_OFF;
	}
}




void Editor_Init(void)
{
	Cmd_AddCommand("edit", Con_TextEditor_f);

	Cvar_Register(&editstripcr, "Text editor");
	Cvar_Register(&editaddcr, "Text editor");
//	Cvar_Register(&edittabspacing, "Text editor");
	Cvar_Register(&pr_debugger, "Text editor");
}

#endif
