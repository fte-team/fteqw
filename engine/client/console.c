/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// console.c

#include "quakedef.h"
#include "shader.h"

console_t	con_main;
console_t	*con_curwindow;
console_t	*con_current;		// points to whatever is the visible console
console_t	*con_mouseover;		// points to whichever console's title is currently mouseovered, or null
console_t	*con_chat;			// points to a chat console

#define Font_ScreenWidth() (vid.pixelwidth)

static int Con_DrawProgress(int left, int right, int y);
static int Con_DrawConsoleLines(console_t *con, conline_t *l, int sx, int ex, int y, int top, int selactive, int selsx, int selex, int selsy, int seley, float lineagelimit);

#ifdef QTERM
#include <windows.h>
typedef struct qterm_s {
	console_t *console;
	qboolean running;
	HANDLE process;
	HANDLE pipein;
	HANDLE pipeout;

	HANDLE pipeinih;
	HANDLE pipeoutih;

	struct qterm_s *next;
} qterm_t;

qterm_t *qterms;
qterm_t *activeqterm;
#endif

//int 		con_linewidth;	// characters across screen
//int			con_totallines;		// total lines in console scrollback

float		con_cursorspeed = 4;


cvar_t		con_numnotifylines = CVAR("con_notifylines","4");		//max lines to show
cvar_t		con_notifytime = CVAR("con_notifytime","3");		//seconds
cvar_t		con_notify_x = CVAR("con_notify_x","0");
cvar_t		con_notify_y = CVAR("con_notify_y","0");
cvar_t		con_notify_w = CVAR("con_notify_w","1");
cvar_t		con_centernotify = CVAR("con_centernotify", "0");
cvar_t		con_displaypossibilities = CVAR("con_displaypossibilities", "1");
cvar_t		con_maxlines = CVAR("con_maxlines", "1024");
cvar_t		cl_chatmode = CVARD("cl_chatmode", "2", "0(nq) - everything is assumed to be a console command. prefix with 'say', or just use a messagemode bind\n1(q3) - everything is assumed to be chat, unless its prefixed with a /\n2(qw) - anything explicitly recognised as a command will be used as a command, anything unrecognised will be a chat message.\n/ prefix is supported in all cases.\nctrl held when pressing enter always makes any implicit chat into team chat instead.");
cvar_t		con_numnotifylines_chat = CVAR("con_numnotifylines_chat", "8");
cvar_t		con_notifytime_chat = CVAR("con_notifytime_chat", "8");
cvar_t		con_separatechat = CVAR("con_separatechat", "0");
cvar_t		con_timestamps = CVAR("con_timestamps", "0");
cvar_t		con_timeformat = CVAR("con_timeformat", "(%H:%M:%S) ");
cvar_t		con_textsize = CVARD("con_textsize", "8", "Resize the console text to be a different height, scaled separately from the hud. The value is the height in (virtual) pixels.");
extern cvar_t log_developer;

#define	NUM_CON_TIMES 24

qboolean	con_initialized;

/*makes sure the console object works*/
void Con_Finit (console_t *con)
{
	if (con->current == NULL)
	{
		con->oldest = con->current = Z_Malloc(sizeof(conline_t));
		con->linecount = 0;
	}
	if (con->display == NULL)
		con->display = con->current;

	con->selstartline = NULL;
	con->selendline = NULL;

	con->defaultcharbits = CON_WHITEMASK;
	con->parseflags = 0;
}

/*returns a bitmask:
1: currently active
2: has text that has not been seen yet
*/
int Con_IsActive (console_t *con)
{
	return (con == con_current) | (con->unseentext*2);
}
/*kills a console_t object. will never destroy the main console (which will only be cleared)*/
void Con_Destroy (console_t *con)
{
	shader_t *shader;
	console_t *prev;
	conline_t *t;

	if (con->close)
	{
		con->close(con, true);
		con->close = NULL;
	}

	/*purge the lines from the console*/
	while (con->current)
	{
		t = con->current;
		con->current = t->older;
		Z_Free(t);
	}
	con->display = con->current = con->oldest = NULL;

	if (con->footerline)
		Z_Free(con->footerline);
	con->footerline = NULL;
	if (con->completionline)
		Z_Free(con->completionline);
	con->completionline = NULL;

	if (con == &con_main)
	{
		/*main console is never destroyed, only cleared (unless shutting down)*/
		if (con_initialized)
			Con_Finit(con);
		return;
	}

	for (prev = &con_main; prev->next; prev = prev->next)
	{
		if (prev->next == con)
		{
			prev->next = con->next;
			break;
		}
	}

	shader = con->backshader;

	BZ_Free(con);

	if (con_current == con)
		con_current = &con_main;

	if (con_curwindow == con)
	{
		for (con_curwindow = &con_main; con_curwindow; con_curwindow = con_curwindow->next)
		{
			if (con_curwindow->flags & CONF_ISWINDOW)
				break;
		}
		if (!con_curwindow)
			Key_Dest_Remove(kdm_cwindows);
	}
	con_mouseover = NULL;

	if (shader)
		R_UnloadShader(shader);
}

/*just purges the background images for various consoles on restart/shutdown*/
void Con_FlushBackgrounds(void)
{
	console_t *con;
	//fixme: we really need to handle videomaps differently here, for vid_restarts.
	for (con = &con_main; con; con = con->next)
	{
		if (con->backshader)
			R_UnloadShader(con->backshader);
		con->backshader = NULL;
	}
}

/*obtains a console_t without creating*/
console_t *Con_FindConsole(const char *name)
{
	console_t *con;
	if (!strcmp(name, "current") && con_current)
		return con_current;
	for (con = &con_main; con; con = con->next)
	{
		if (!strcmp(con->name, name))
			return con;
	}
	return NULL;
}
/*creates a potentially duplicate console_t - please use Con_FindConsole first, as its confusing otherwise*/
console_t *Con_Create(const char *name, unsigned int flags)
{
	console_t *con;
	if (!strcmp(name, "current"))
		return NULL;
	con = Z_Malloc(sizeof(console_t));
	Q_strncpyz(con->name, name, sizeof(con->name));
	Q_strncpyz(con->title, name, sizeof(con->title));
	Q_strncpyz(con->prompt, "]", sizeof(con->prompt));

	con->flags = flags;
	Con_Finit(con);
	con->next = con_main.next;
	con_main.next = con;

	return con;
}
/*sets a console as the active one*/
void Con_SetActive (console_t *con)
{
	if (con->flags & CONF_ISWINDOW)
	{
		console_t *prev;
		Key_Dest_Add(kdm_cwindows);
		Key_Dest_Remove(kdm_console);

		if (con_curwindow == con)
			return;

		for (prev = &con_main; prev; prev = prev->next)
		{
			if (prev->next == con)
			{
				prev->next = con->next;
				while(prev->next)
				{
					prev = prev->next;
				}
				prev->next = con;
				con->next = NULL;
				break;
			}
		}
		con_curwindow = con;
	}
	else
		con_current = con;

	if (con->footerline)
	{
		con->selstartline = NULL;
		con->selendline = NULL;
		Z_Free(con->footerline);
		con->footerline = NULL;
	}
	con->buttonsdown = CB_NONE;
}
/*for enumerating consoles*/
qboolean Con_NameForNum(int num, char *buffer, int buffersize)
{
	console_t *con;
	for (con = &con_main; con; con = con->next, num--)
	{
		if (num <= 0)
		{
			Q_strncpyz(buffer, con->name, buffersize);
			return true;
		}
	}
	if (buffersize>0)
		*buffer = '\0';
	return false;
}

#ifdef QTERM
void QT_Kill(qterm_t *qt, qboolean killconsole)
{
	qterm_t **link;
	qt->console->close = NULL;
	qt->console->userdata = NULL;
	qt->console->redirect = NULL;
	if (killconsole)
		Con_Destroy(qt->console);

	//yes this loop will crash if you're not careful. it makes it easier to debug.
	for (link = &qterms; ; link = &(*link)->next)
	{
		if (*link == qt)
		{
			*link = qt->next;
			break;
		}
	}

	CloseHandle(qt->pipein);
	CloseHandle(qt->pipeout);

	CloseHandle(qt->pipeinih);
	CloseHandle(qt->pipeoutih);
	CloseHandle(qt->process);

	Z_Free(qt);
}
void QT_Update(void)
{
	char buffer[2048];
	DWORD ret;
	qterm_t *qt, *n;
	for (qt = qterms; qt; )
	{
		if (qt->running)
		{
			if (WaitForSingleObject(qt->process, 0) == WAIT_TIMEOUT)
			{
				if ((ret=GetFileSize(qt->pipeout, NULL)))
				{
					if (ret!=INVALID_FILE_SIZE)
					{
						ReadFile(qt->pipeout, buffer, sizeof(buffer)-32, &ret, NULL);
						buffer[ret] = '\0';
						Con_PrintCon(qt->console, buffer, PFS_NOMARKUP);
					}
				}
			}
			else
			{
				Con_PrintCon(qt->console, "Process ended\n", PFS_NOMARKUP);
				qt->running = false;
			}
		}

		n = qt->next;
		if (!qt->running)
		{
			if (!Con_IsActive(qt->console))
				QT_Kill(qt, true);
		}
		qt = n;
	}
}

qboolean QT_KeyPress(console_t *con, unsigned int unicode, int key)
{
	qbyte k[2];
	qterm_t *qt = con->userdata;
	DWORD send = key;	//get around a gcc warning


	k[0] = key;
	k[1] = '\0';

	if (qt->running)
	{
		if (*k == '\r')
		{
//					*k = '\r';
//					WriteFile(qt->pipein, k, 1, &key, NULL);
//					Con_PrintCon(k, &qt->console, PFS_NOMARKUP);
			*k = '\n';
		}
//		if (GetFileSize(qt->pipein, NULL)<512)
		{
			WriteFile(qt->pipein, k, 1, &send, NULL);
			Con_PrintCon(qt->console, k, PFS_NOMARKUP);
		}
	}
	return true;
}

qboolean	QT_Close(struct console_s *con, qboolean force)
{
	qterm_t *qt = con->userdata;
	QT_Kill(qt, false);

	return true;
}

void QT_Create(char *command)
{
	HANDLE StdIn[2];
	HANDLE StdOut[2];
	qterm_t *qt;
	SECURITY_ATTRIBUTES sa;
	STARTUPINFO SUInf;
	PROCESS_INFORMATION ProcInfo;

	int ret;

	qt = Z_Malloc(sizeof(*qt));

	memset(&sa,0,sizeof(sa));
	sa.nLength=sizeof(sa);
	sa.bInheritHandle=true;

	CreatePipe(&StdOut[0], &StdOut[1], &sa, 1024);
	CreatePipe(&StdIn[1], &StdIn[0], &sa, 1024);

	memset(&SUInf, 0, sizeof(SUInf));
	SUInf.cb = sizeof(SUInf);
	SUInf.dwFlags = STARTF_USESTDHANDLES;
/*
	qt->pipeout		= StdOut[0];
	qt->pipein		= StdIn[0];
*/
	qt->pipeoutih	= StdOut[1];
	qt->pipeinih	= StdIn[1];

	if (!DuplicateHandle(GetCurrentProcess(), StdIn[0],
						GetCurrentProcess(), &qt->pipein, 0,
						FALSE,                  // not inherited
						DUPLICATE_SAME_ACCESS))
		qt->pipein = StdIn[0];
	else
		CloseHandle(StdIn[0]);
	if (!DuplicateHandle(GetCurrentProcess(), StdOut[0],
						GetCurrentProcess(), &qt->pipeout, 0,
						FALSE,                  // not inherited
						DUPLICATE_SAME_ACCESS))
		qt->pipeout = StdOut[0];
	else
		CloseHandle(StdOut[0]);

	SUInf.hStdInput		= qt->pipeinih;
	SUInf.hStdOutput	= qt->pipeoutih;
	SUInf.hStdError		= qt->pipeoutih;	//we don't want to have to bother working out which one was written to first.

	if (!SetStdHandle(STD_OUTPUT_HANDLE, SUInf.hStdOutput))
		Con_Printf("Windows sucks\n");
	if (!SetStdHandle(STD_ERROR_HANDLE, SUInf.hStdError))
		Con_Printf("Windows sucks\n");
	if (!SetStdHandle(STD_INPUT_HANDLE, SUInf.hStdInput))
		Con_Printf("Windows sucks\n");

	printf("Started app\n");
	ret = CreateProcess(NULL, command, NULL, NULL, true, CREATE_NO_WINDOW, NULL, NULL, &SUInf, &ProcInfo);

	qt->process = ProcInfo.hProcess;
	CloseHandle(ProcInfo.hThread);

	qt->running = true;

	qt->console = Con_Create("QTerm", 0);
	qt->console->redirect = QT_KeyPress;
	qt->console->close = QT_Close;
	qt->console->userdata = qt;
	Con_PrintCon(qt->console, "Started Process\n", PFS_NOMARKUP);
	Con_SetActive(qt->console);

	qt->next = qterms;
	qterms = activeqterm = qt;
}

void Con_QTerm_f(void)
{
	if(Cmd_IsInsecure())
		Con_Printf("Server tried stuffcmding a restricted commandqterm %s\n", Cmd_Args());
	else
		QT_Create(Cmd_Args());
}
#endif




void Key_ClearTyping (void)
{
	key_lines[edit_line] = BZ_Realloc(key_lines[edit_line], 1);
	key_lines[edit_line][0] = 0;	// clear any typing
	key_linepos = 0;
}

void Con_History_Load(void)
{
	char line[8192];
	char *cr;
	vfsfile_t *file = FS_OpenVFS("conhistory.txt", "rb", FS_ROOT);

	for (edit_line=0 ; edit_line<=CON_EDIT_LINES_MASK ; edit_line++)
	{
		key_lines[edit_line] = BZ_Realloc(key_lines[edit_line], 1);
		key_lines[edit_line][0] = 0;
	}
	edit_line = 0;
	key_linepos = 0;

	if (file)
	{
		while (VFS_GETS(file, line, sizeof(line)-1))
		{
			//strip a trailing \r if its from windows.
			cr = line + strlen(line);
			if (cr > line && cr[-1] == '\r')
				cr[-1] = '\0';
			key_lines[edit_line] = BZ_Realloc(key_lines[edit_line], strlen(line)+1);
			strcpy(key_lines[edit_line], line);
			edit_line = (edit_line+1) & CON_EDIT_LINES_MASK;
		}
		VFS_CLOSE(file);
	}
	history_line = edit_line;
}
void Con_History_Save(void)
{
	vfsfile_t *file = FS_OpenVFS("conhistory.txt", "wb", FS_ROOT);
	int line;
	if (file)
	{
		line = edit_line - CON_EDIT_LINES_MASK;
		if (line < 0)
			line = 0;
		for(; line < edit_line; line++)
		{
			VFS_PUTS(file, key_lines[line]);
#ifdef _WIN32	//use an \r\n for readability with notepad.
			VFS_PUTS(file, "\r\n");
#else
			VFS_PUTS(file, "\n");
#endif
		}
		VFS_CLOSE(file);
	}
}

/*
================
Con_ToggleConsole_f
================
*/
void Con_ToggleConsole_Force(void)
{
	SCR_EndLoadingPlaque();
	Key_ClearTyping ();

	if (Key_Dest_Has(kdm_console))
		Key_Dest_Remove(kdm_console);
	else
		Key_Dest_Add(kdm_console);
}
void Con_ToggleConsole_f (void)
{
	extern cvar_t con_stayhidden;

	if (!con_curwindow)
	{
		for (con_curwindow = &con_main; con_curwindow; con_curwindow = con_curwindow->next)
			if (con_curwindow->flags & CONF_ISWINDOW)
				break;
	}

	if (con_curwindow && !Key_Dest_Has(kdm_cwindows|kdm_console))
	{
		Key_Dest_Add(kdm_cwindows);
		return;
	}

#ifdef CSQC_DAT
	if (!(key_dest_mask & kdm_editor) && CSQC_ConsoleCommand("toggleconsole"))
	{
		Key_Dest_Remove(kdm_console);
		return;
	}
#endif

	if (con_stayhidden.ival >= 3)
	{
		Key_Dest_Remove(kdm_cwindows);
		return;	//its hiding!
	}

	Con_ToggleConsole_Force();
}

void Con_ClearCon(console_t *con)
{
	conline_t *t;
	while (con->current)
	{
		t = con->current;
		con->current = t->older;
		Z_Free(t);
	}
	con->display = con->current = con->oldest = NULL;
	con->selstartline = NULL;
	con->selendline = NULL;

	/*reset the line pointers, create an active line*/
	Con_Finit(con);
}

/*
================
Con_Clear_f
================
*/
void Con_Clear_f (void)
{
	if (Cmd_IsInsecure())
		return;
	Con_ClearCon(&con_main);
}


void Cmd_ConEchoCenter_f(void)
{
	console_t *con;
	con = Con_FindConsole(Cmd_Argv(1));
	if (!con)
		con = Con_Create(Cmd_Argv(1), 0);
	if (con)
	{
		Cmd_ShiftArgs(1, false);
		Con_PrintCon(con, Cmd_Args(), con->parseflags|PFS_NONOTIFY|PFS_CENTERED );
		Con_PrintCon(con, "\n", con->parseflags|PFS_NONOTIFY|PFS_CENTERED);
	}
}
void Cmd_ConEcho_f(void)
{
	console_t *con;
	con = Con_FindConsole(Cmd_Argv(1));
	if (!con)
		con = Con_Create(Cmd_Argv(1), 0);
	if (con)
	{
		Cmd_ShiftArgs(1, false);
		Con_PrintCon(con, Cmd_Args(), con->parseflags);
		Con_PrintCon(con, "\n", con->parseflags);
	}
}

void Cmd_ConClear_f(void)
{
	console_t *con;
	con = Con_FindConsole(Cmd_Argv(1));
	if (con)
		Con_ClearCon(con);
}
void Cmd_ConClose_f(void)
{
	console_t *con;
	con = Con_FindConsole(Cmd_Argv(1));
	if (con)
		Con_Destroy(con);
}
void Cmd_ConActivate_f(void)
{
	console_t *con;
	con = Con_FindConsole(Cmd_Argv(1));
	if (con)
		Con_SetActive(con);
}

/*
================
Con_MessageMode_f
================
*/
void Con_MessageMode_f (void)
{
	chat_team = false;
	Key_Dest_Add(kdm_message);
	Key_Dest_Remove(kdm_console);
}

/*
================
Con_MessageMode2_f
================
*/
void Con_MessageMode2_f (void)
{
	chat_team = true;
	Key_Dest_Add(kdm_message);
	Key_Dest_Remove(kdm_console);
}

void Con_ForceActiveNow(void)
{
	Key_Dest_Add(kdm_console);
	scr_conlines = scr_con_current = vid.height;
}

/*
================
Con_Init
================
*/
void Log_Init (void);

void Con_Init (void)
{
	con_current = &con_main;
	Con_Finit(&con_main);

	con_main.linebuffered = Con_ExecuteLine;
	con_main.commandcompletion = true;
	Q_strncpyz(con_main.title, "MAIN", sizeof(con_main.title));
	Q_strncpyz(con_main.prompt, "]", sizeof(con_main.prompt));

	con_initialized = true;
//	Con_TPrintf ("Console initialized.\n");

//
// register our commands
//
	Cvar_Register (&con_centernotify, "Console controls");
	Cvar_Register (&con_notifytime, "Console controls");
	Cvar_Register (&con_notify_x, "Console controls");
	Cvar_Register (&con_notify_y, "Console controls");
	Cvar_Register (&con_notify_w, "Console controls");
	Cvar_Register (&con_numnotifylines, "Console controls");
	Cvar_Register (&con_displaypossibilities, "Console controls");
	Cvar_Register (&cl_chatmode, "Console controls");
	Cvar_Register (&con_maxlines, "Console controls");
	Cvar_Register (&con_numnotifylines_chat, "Console controls");
	Cvar_Register (&con_notifytime_chat, "Console controls");
	Cvar_Register (&con_separatechat, "Console controls");
	Cvar_Register (&con_timestamps, "Console controls");
	Cvar_Register (&con_timeformat, "Console controls");
	Cvar_Register (&con_textsize, "Console controls");

	Cmd_AddCommand ("toggleconsole", Con_ToggleConsole_f);
	Cmd_AddCommand ("messagemode", Con_MessageMode_f);
	Cmd_AddCommand ("messagemode2", Con_MessageMode2_f);
	Cmd_AddCommand ("clear", Con_Clear_f);
#ifdef QTERM
	Cmd_AddCommand ("qterm", Con_QTerm_f);
#endif

	Cmd_AddCommand ("conecho_center", Cmd_ConEchoCenter_f);
	Cmd_AddCommand ("conecho", Cmd_ConEcho_f);
	Cmd_AddCommand ("conclear", Cmd_ConClear_f);
	Cmd_AddCommand ("conclose", Cmd_ConClose_f);
	Cmd_AddCommand ("conactivate", Cmd_ConActivate_f);

	Log_Init();
}

void Con_Shutdown(void)
{
	int i;

	for (i = 0; i <= CON_EDIT_LINES_MASK; i++)
	{
		BZ_Free(key_lines[i]);
	}

	while(con_main.next)
	{
		Con_Destroy(con_main.next);
	}
	con_initialized = false;
	Con_Destroy(&con_main);
}

void TTS_SayConString(conchar_t *stringtosay);

/*
================
Con_Print

Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be logged to disk
If no console is visible, the notify window will pop up.
================
*/

//reallocates a line (with its buffer), and updates its links. if shrinking, be sure to reduce the length
conline_t *Con_ResizeLineBuffer(console_t *con, conline_t *old, unsigned int length)
{
	conline_t *l;

	old->maxlength = length & 0xffff;
	if (old->maxlength < old->length)
		return NULL;	//overflow.
	l = BZ_Realloc(old, sizeof(*l)+(old->maxlength)*sizeof(conchar_t));

	if (l->newer)
		l->newer->older = l;
	if (l->older)
		l->older->newer = l;

	if (con->selstartline == old)
		con->selstartline = l;
	if (con->selendline == old)
		con->selendline = l;
	if (con->display == old)
		con->display = l;
	if (con->oldest == old)
		con->oldest = l;
	if (con->current == old)
		con->current = l;
	if (con->footerline == old)
		con->footerline = l;
	if (con->userline == old)
		con->userline = l;
	if (con->highlightline == old)
		con->highlightline = old;
	return l;
}

qboolean Con_InsertConChars (console_t *con, conline_t *line, int offset, conchar_t *c, int len)
{
	conchar_t *o;

	if (line->length+len > line->maxlength)
	{
		line = Con_ResizeLineBuffer(con, line, line->length+len + 8);
		if (!line)
			return false;	//overflowed!
	}

	o = (conchar_t *)(line+1);
	if (line->length-offset)
		memmove(o+offset+len, o+offset, (line->length-offset)*sizeof(conchar_t));
	memcpy(o+offset, c, sizeof(*o) * len);
	line->length+=len;
	return true;
}

void Con_PrintCon (console_t *con, const char *txt, unsigned int parseflags)
{
	conchar_t expanded[4096];
	conchar_t *c;
	conline_t *reuse;
	int maxlines;

	if (con->maxlines)
		maxlines = con->maxlines;
	else
		maxlines = con_maxlines.ival;

	COM_ParseFunString(con->defaultcharbits, txt, expanded, sizeof(expanded), parseflags);

	c = expanded;
	if (*c)
		con->unseentext = true;
	while (*c)
	{
		switch (*c & (CON_CHARMASK|CON_HIDDEN))	//include hidden so we don't do \r or \n on hidden chars, allowing them to be embedded in links and stuff.
		{
		case '\r':
			con->cr = true;
			break;
		case '\n':
			con->cr = false;
			reuse = NULL;
			while (con->linecount >= maxlines)
			{
				if (con->oldest == con->current)
					break;

				if (con->selstartline == con->oldest)
					con->selstartline = NULL;
				if (con->selendline == con->oldest)
					con->selendline = NULL;

				if (con->display == con->oldest)
					con->display = con->oldest->newer;
				con->oldest = con->oldest->newer;
				if (reuse)
					Z_Free(con->oldest->older);
				else
					reuse = con->oldest->older;
				con->oldest->older = NULL;
				con->linecount--;
			}
			con->linecount++;
			con->current->time = realtime;
			con->current->flags = 0;
			if (parseflags & PFS_CENTERED)
				con->current->flags |= CONL_CENTERED;
			if (parseflags & PFS_NONOTIFY)
				con->current->flags |= CONL_NONOTIFY;

#if defined(HAVE_SPEECHTOTEXT)
			if (con->current)
				TTS_SayConString((conchar_t*)(con->current+1));
#endif

			if (!reuse)
			{
				reuse = Z_Malloc(sizeof(conline_t) + sizeof(conchar_t));
				reuse->maxlength = 1;
			}
			else
			{
				reuse->newer = NULL;
				reuse->older = NULL;
			}
			reuse->id = (++con->nextlineid) & 0xffff;
			reuse->older = con->current;
			con->current->newer = reuse;
			con->current = reuse;
			con->current->length = 0;
			if (con->display == con->current->older)
				con->display = con->current;
			break;
		default:
			if (con->cr)
			{
				con->current->length = 0;
				con->cr = false;
			}

			if (!con->current->length && con_timestamps.ival && !(parseflags & PFS_CENTERED))
			{
				char timeasc[64];
				conchar_t timecon[64], *timeconend;
				time_t rawtime;
				time (&rawtime);
				strftime(timeasc, sizeof(timeasc), con_timeformat.string, localtime (&rawtime));
				timeconend = COM_ParseFunString(con->defaultcharbits, timeasc, timecon, sizeof(timecon), false);
				Con_InsertConChars(con, con->current, con->current->length, timecon, timeconend-timecon);
			}

			//FIXME: don't do this a char at a time
			Con_InsertConChars(con, con->current, con->current->length, c, 1);
			break;
		}
		c++;
	}

	con->current->time = realtime;
}

void Con_CenterPrint(const char *txt)
{
	int flags = con_main.parseflags|PFS_NONOTIFY|PFS_CENTERED;
	Con_PrintCon(&con_main, "^Ue01d^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01f\n", flags);
	Con_PrintCon(&con_main, txt, flags);	//client console
	Con_PrintCon(&con_main, "\n^Ue01d^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01f\n", flags);
}

void Con_Print (const char *txt)
{
	Con_PrintCon(&con_main, txt, con_main.parseflags);	//client console
}
void Con_PrintFlags(const char *txt, unsigned int setflags, unsigned int clearflags)
{
	setflags |= con_main.parseflags;
	setflags &= ~clearflags;

// also echo to debugging console
	Sys_Printf ("%s", txt);	// also echo to debugging console

// log all messages to file
	Con_Log (txt);

	if (con_initialized)
		Con_PrintCon(&con_main, txt, setflags);
}

void Con_CycleConsole(void)
{
	while(1)
	{
		con_current = con_current->next;
		if (!con_current)
			con_current = &con_main;

		if (con_current->flags & (CONF_HIDDEN|CONF_ISWINDOW))
			continue;
		break;
	}
}

void Con_Log(char *s);

/*
================
Con_Printf

Handles cursor positioning, line wrapping, etc
================
*/

#ifndef CLIENTONLY
extern redirect_t	sv_redirected;
extern char	sv_redirected_buf[8000];
void SV_FlushRedirect (void);
#endif

#define	MAXPRINTMSG	4096
static void Con_PrintFromThread (void *ctx, void *data, size_t a, size_t b)
{
	Con_Printf("%s", (char*)data);
	BZ_Free(data);
}

// FIXME: make a buffer size safe vsprintf?
void VARGS Con_Printf (const char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	va_start (argptr,fmt);
	vsnprintf (msg,sizeof(msg), fmt,argptr);
	va_end (argptr);

	if (!Sys_IsMainThread())
	{
		COM_AddWork(WG_MAIN, Con_PrintFromThread, NULL, Z_StrDup(msg), 0, 0);
		return;
	}

#ifndef CLIENTONLY
	// add to redirected message
	if (sv_redirected)
	{
		if (strlen (msg) + strlen(sv_redirected_buf) > sizeof(sv_redirected_buf) - 1)
			SV_FlushRedirect ();
		strcat (sv_redirected_buf, msg);
		return;
	}
#endif

// also echo to debugging console
	Sys_Printf ("%s", msg);	// also echo to debugging console

// log all messages to file
	Con_Log (msg);

	if (!con_initialized)
		return;

// write it to the scrollable buffer
	Con_Print (msg);
}

void VARGS Con_SafePrintf (const char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	va_start (argptr,fmt);
	vsnprintf (msg,sizeof(msg)-1, fmt,argptr);
	va_end (argptr);

// write it to the scrollable buffer
	Con_Printf ("%s", msg);
}

void VARGS Con_TPrintf (translation_t text, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];
	const char *fmt = langtext(text, cls.language);

	va_start (argptr,text);
	vsnprintf (msg,sizeof(msg), fmt,argptr);
	va_end (argptr);

// write it to the scrollable buffer
	Con_Printf ("%s", msg);
}

void VARGS Con_SafeTPrintf (translation_t text, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];
	const char *fmt = langtext(text, cls.language);

	va_start (argptr,text);
	vsnprintf (msg,sizeof(msg), fmt,argptr);
	va_end (argptr);

// write it to the scrollable buffer
	Con_Printf ("%s", msg);
}

static void Con_DPrintFromThread (void *ctx, void *data, size_t a, size_t b)
{
	if (log_developer.ival)
		Con_Log(data);
	if (developer.ival >= (int)a)
	{
		Sys_Printf ("%s", (const char*)data);	// also echo to debugging console
		Con_PrintCon(&con_main, data, con_main.parseflags);
	}
	BZ_Free(data);
}
/*
================
Con_DPrintf

A Con_Printf that only shows up if the "developer" cvar is set
================
*/
void VARGS Con_DPrintf (const char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];

#ifdef CRAZYDEBUGGING
	va_start (argptr,fmt);
	vsnprintf (msg,sizeof(msg)-1, fmt,argptr);
	va_end (argptr);
	Sys_Printf("%s", msg);
	return;
#else
	if (!developer.ival && !log_developer.ival)
		return; // early exit
#endif

	va_start (argptr,fmt);
	vsnprintf (msg,sizeof(msg)-1, fmt,argptr);
	va_end (argptr);

	if (!Sys_IsMainThread())
	{
		COM_AddWork(WG_MAIN, Con_DPrintFromThread, NULL, Z_StrDup(msg), 1, 0);
		return;
	}

	if (log_developer.ival)
		Con_Log(msg);
	if (developer.ival)
	{
		Sys_Printf ("%s", msg);	// also echo to debugging console
		if (con_initialized)
			Con_PrintCon(&con_main, msg, con_main.parseflags);
	}
}
void VARGS Con_DLPrintf (int level, const char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];

#ifdef CRAZYDEBUGGING
	va_start (argptr,fmt);
	vsnprintf (msg,sizeof(msg)-1, fmt,argptr);
	va_end (argptr);
	Sys_Printf("%s", msg);
	return;
#else
	if (developer.ival<level && !log_developer.ival)
		return; // early exit
#endif

	va_start (argptr,fmt);
	vsnprintf (msg,sizeof(msg)-1, fmt,argptr);
	va_end (argptr);

	if (!Sys_IsMainThread())
	{
		COM_AddWork(WG_MAIN, Con_DPrintFromThread, NULL, Z_StrDup(msg), level, 0);
		return;
	}

	if (log_developer.ival)
		Con_Log(msg);
	if (developer.ival >= level)
	{
		Sys_Printf ("%s", msg);	// also echo to debugging console
		if (con_initialized)
			Con_PrintCon(&con_main, msg, con_main.parseflags);
	}
}


/*description text at the bottom of the console*/
void Con_Footerf(console_t *con, qboolean append, const char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];
	conchar_t	marked[MAXPRINTMSG], *markedend;
	int oldlen, newlen;
	conline_t *newf;
	if (!con)
		con = con_current;
	if (!con)
		return;

	va_start (argptr,fmt);
	vsnprintf (msg,sizeof(msg)-1, fmt,argptr);
	va_end (argptr);
	markedend = COM_ParseFunString((COLOR_YELLOW << CON_FGSHIFT)|(con->backshader?CON_NONCLEARBG:0), msg, marked, sizeof(marked), false);

	newlen = markedend - marked;
	if (append && con->footerline)
		oldlen = con->footerline->length;
	else
		oldlen = 0;

	if (!newlen && !oldlen)
		newf = NULL;
	else
	{
		newf = Z_Malloc(sizeof(*newf) + (oldlen + newlen) * sizeof(conchar_t));
		if (con->footerline)
			memcpy(newf, con->footerline, sizeof(*con->footerline)+oldlen*sizeof(conchar_t));
		markedend = (void*)(newf+1);
		markedend += oldlen;
		memcpy(markedend, marked, newlen*sizeof(conchar_t));
		newf->length = oldlen + newlen;
	}

	if (con->selstartline == con->footerline)
		con->selstartline = NULL;
	if (con->selendline == con->footerline)
		con->selendline = NULL;
	Z_Free(con->footerline);
	con->footerline = newf;
}

/*
==============================================================================

DRAWING

==============================================================================
*/


/*
================
Con_DrawInput

The input line scrolls horizontally if typing goes beyond the right edge
y is the bottom of the input
return value is the top of the region
================
*/
int Con_DrawInput (console_t *con, qboolean focused, int left, int right, int y, qboolean selactive, int selsx, int selex, int selsy, int seley)
{
	int		i;
	int lhs, rhs;
	int p;
	unsigned char	*text, *fname = NULL;
	extern int con_commandmatch;
	conchar_t maskedtext[2048];
	conchar_t *endmtext;
	conchar_t *cursor;
	conchar_t *cchar;
	conchar_t *textstart;
	size_t textsize;
	qboolean cursorframe;
	unsigned int codeflags, codepoint;

	int x;

	if (!con->linebuffered || con->linebuffered == Con_Navigate)
	{
		if (con->footerline)
		{
			y = Con_DrawConsoleLines(con, con->footerline, left, right, y, 0, selactive, selsx, selex, selsy, seley, 0);
		}
		return y;	//fixme: draw any unfinished lines of the current console instead.
	}

	y -= Font_CharHeight();

	if (!focused)
		return y;		// don't draw anything (always draw if not active)

	text = key_lines[edit_line];

	//copy it to an alternate buffer and fill in text colouration escape codes.
	//if it's recognised as a command, colour it yellow.
	//if it's not a command, and the cursor is at the end of the line, leave it as is,
	//	but add to the end to show what the compleation will be.

	textstart = COM_ParseFunString(CON_WHITEMASK, con->prompt, maskedtext, sizeof(maskedtext) - sizeof(maskedtext[0]), PFS_FORCEUTF8);
	textsize = (countof(maskedtext) - (textstart-maskedtext) - 1) * sizeof(maskedtext[0]);
	i = text[key_linepos];
	text[key_linepos] = 0;
	cursor = COM_ParseFunString(CON_WHITEMASK, text, textstart, textsize, PFS_KEEPMARKUP | PFS_FORCEUTF8);
	text[key_linepos] = i;
	endmtext = COM_ParseFunString(CON_WHITEMASK, text, textstart, textsize, PFS_KEEPMARKUP | PFS_FORCEUTF8);
//	endmtext = COM_ParseFunString(CON_WHITEMASK, text+key_linepos, cursor, ((char*)maskedtext)+sizeof(maskedtext) - (char*)(cursor+1), PFS_KEEPMARKUP | PFS_FORCEUTF8);

	if ((char*)endmtext == (char*)(maskedtext-2) + sizeof(maskedtext))
		endmtext[-1] = CON_WHITEMASK | '+' | CON_NONCLEARBG;
	endmtext[1] = 0;

	i = 0;
	x = left;

	if (con->commandcompletion)
	{
		if (cl_chatmode.ival && (text[0] == '/' || (cl_chatmode.ival == 2 && Cmd_IsCommand(text))))
		{	//color the first token yellow, it's a valid command
			for (p = 0; (textstart[p]&CON_CHARMASK)>' '; p++)
				textstart[p] = (textstart[p]&CON_CHARMASK) | (COLOR_YELLOW<<CON_FGSHIFT);
		}
//		else
//			Plug_SpellCheckMaskedText(maskedtext+1, i-1, x, y, 8, si, con_current->linewidth);

		if (cursor == endmtext)	//cursor is at end
		{
			int cmdstart;
			cmdstart = text[0] == '/'?1:0;
			fname = Cmd_CompleteCommand(text+cmdstart, true, true, con_commandmatch, NULL);
			if (fname && strlen(fname) < 256)	//we can compleate it to:
			{
				for (p = min(strlen(fname), key_linepos-cmdstart); fname[p]>' '; p++)
					textstart[p+cmdstart] = (unsigned int)fname[p] | (COLOR_GREEN<<CON_FGSHIFT);
				if (p < key_linepos-cmdstart)
					p = key_linepos-cmdstart;
				p = min(p+cmdstart, sizeof(maskedtext)/sizeof(maskedtext[0]) - 3);
				textstart[p] = 0;
				textstart[p+1] = 0;
			}
		}
	}
//	else
//		Plug_SpellCheckMaskedText(maskedtext+1, i-1, x, y, 8, si, con_current->linewidth);

	if (!vid.activeapp)
		cursorframe = 0;
	else
		cursorframe = ((int)(realtime*con_cursorspeed)&1);

	//FIXME: support tab somehow
	for (lhs = 0, cchar = maskedtext; cchar < cursor; )
	{
		cchar = Font_Decode(cchar, &codeflags, &codepoint);
		lhs += Font_CharWidth(codeflags, codepoint);
	}
	for (rhs = 0, cchar = cursor; *cchar; )
	{
		cchar = Font_Decode(cchar, &codeflags, &codepoint);
		rhs += Font_CharWidth(codeflags, codepoint);
	}

	//put the cursor in the middle
	x = (right-left)/2 + left;
	//move the line to the right if there's not enough text to touch the right hand side
	if (x < right-rhs - Font_CharWidth(CON_WHITEMASK, 0xe000|11))
		x = right - rhs - Font_CharWidth(CON_WHITEMASK, 0xe000|11);
	//if the left hand side is on the right of the left point (overrides right alignment)
	if (x > lhs + left)
		x = lhs + left;

	lhs = x - lhs;
	for (cchar = maskedtext; cchar < cursor; )
	{
		cchar = Font_Decode(cchar, &codeflags, &codepoint);
		lhs = Font_DrawChar(lhs, y, codeflags, codepoint);
	}
	rhs = x;
	cchar = Font_Decode(cursor, &codeflags, &codepoint);
	if (cursorframe)
	{
//		extern cvar_t com_parseutf8;
//		if (com_parseutf8.ival)
//			Font_DrawChar(rhs, y, (*cursor&~(CON_BGMASK|CON_FGMASK)) | (COLOR_BLUE<<CON_BGSHIFT) | CON_NONCLEARBG | CON_WHITEMASK);
//		else
			Font_DrawChar(rhs, y, CON_WHITEMASK, 0xe000|11);
	}
	else if (codepoint)
	{
		Font_DrawChar(rhs, y, codeflags, codepoint);
	}
	if (codepoint)
	{
		rhs += Font_CharWidth(codeflags, codepoint);
		while (*cchar)
		{
			cchar = Font_Decode(cchar, &codeflags, &codepoint);
			rhs = Font_DrawChar(rhs, y, codeflags, codepoint);
		}
	}

	/*if its getting completed to something, show some help about the command that is going to be used*/
	if (con->footerline)
	{
		y = Con_DrawConsoleLines(con, con->footerline, left, right, y, 0, selactive, selsx, selex, selsy, seley, 0);
	}

	/*just above that, we have the tab completion list*/
	if (con_commandmatch && con_displaypossibilities.value)
	{
		conchar_t *end, *s;
		char *cmd;//, *desc;
		int cmdstart;
		size_t newlen;
		cmdstart = text[0] == '/'?1:0;
		end = maskedtext;

		if (!con->completionline || con->completionline->length + 512 > con->completionline->maxlength)
		{
			newlen = (con->completionline?con->completionline->length:0) + 2048;

			Z_Free(con->completionline);
			con->completionline = Z_Malloc(sizeof(*con->completionline) + newlen*sizeof(conchar_t));
			con->completionline->maxlength = newlen;
		}
		con->completionline->length = 0;

		for (i = 1; ; i++)
		{
			cmd = Cmd_CompleteCommand (text+cmdstart, true, true, i, NULL);//&desc);
			if (!cmd)
			{
				if (i <= 2)
					con_commandmatch = 0;
				break;
			}

			if (i == 50)
			{
				s = (conchar_t*)(con->completionline+1);
				end = COM_ParseFunString((COLOR_WHITE<<CON_FGSHIFT), va("MORE"), s+con->completionline->length, (con->completionline->maxlength-con->completionline->length)*sizeof(maskedtext[0]), true);
				con->completionline->length = end - s;
				break;
			}

			s = (conchar_t*)(con->completionline+1);
//			if (desc)
//				end = COM_ParseFunString((COLOR_GREEN<<CON_FGSHIFT), va("^[^2/%s\\tip\\%s^]\t", cmd, desc), s+con->completionline->length, (con->completionline->maxlength-con->completionline->length)*sizeof(maskedtext[0]), true);
//			else
				end = COM_ParseFunString((COLOR_GREEN<<CON_FGSHIFT), va("^[^2/%s^]\t", cmd), s+con->completionline->length, (con->completionline->maxlength-con->completionline->length)*sizeof(maskedtext[0]), true);
			con->completionline->length = end - s;
		}

		if (con->completionline->length)
			y = Con_DrawConsoleLines(con, con->completionline, left, right, y, 0, selactive, selsx, selex, selsy, seley, 0);
	}

	return y;
}

/*
================
Con_DrawNotify

Draws the last few lines of output transparently over the game top
================
*/
void Con_DrawNotifyOne (console_t *con)
{
	conchar_t *starts[NUM_CON_TIMES], *ends[NUM_CON_TIMES];
	float alphas[NUM_CON_TIMES], a;
	conchar_t *c;
	conline_t *l;
	int lines=con->notif_l;
	int line;
	int nx, y;
	int nw;
	int x;
	unsigned int codeflags, codepoint;

	int maxlines;
	float t;

	Font_BeginString(font_console, con->notif_x * vid.width, con->notif_y * vid.height, &nx, &y);
	Font_Transform(con->notif_w * vid.width, 0, &nw, NULL);

	if (con->notif_l < 0)
		con->notif_l = 0;
	if (con->notif_l > NUM_CON_TIMES)
		con->notif_l = NUM_CON_TIMES;
	lines = maxlines = con->notif_l;

	if (!con->notif_x && !con->notif_y && con->notif_w == 1)
		y = Con_DrawProgress(0, nw, 0);

	l = con->current;
	if (!l->length)
		l = l->older;
	for (; l && lines > con->notif_l-maxlines; l = l->older)
	{
		if (l->flags & CONL_NONOTIFY)
			continue; //hidden from notify
		t = realtime - (l->time+con->notif_t);
		if (t > 0)
		{
			if (t > con->notif_fade)
			{
				l->flags |= CONL_NONOTIFY;
				break;
			}
			a = 1 - (t/con->notif_fade);
		}
		else a = 1;

		line = Font_LineBreaks((conchar_t*)(l+1), (conchar_t*)(l+1)+l->length, nw, lines, starts, ends);
		if (!line && lines > 0)
		{
			lines--;
			starts[lines] = NULL;
			ends[lines] = NULL;
			alphas[lines] = a;
		}
		while(line --> 0 && lines > 0)
		{
			lines--;
			starts[lines] = starts[line];
			ends[lines] = ends[line];
			alphas[lines] = a;
		}
		if (lines == 0)
			break;
	}

	//clamp it properly
	while (lines < con->notif_l-maxlines)
	{
		lines++;
	}
	if (con->flags & CONF_NOTIFY_BOTTOM)
		y -= (con->notif_l - lines) * Font_CharHeight();

	while (lines < con->notif_l)
	{
		x = 0;
		R2D_ImageColours(1, 1, 1, alphas[lines]);
		if (con->flags & CONF_NOTIFY_RIGHT)
		{
			for (c = starts[lines]; c < ends[lines]; )
			{
				c = Font_Decode(c, &codeflags, &codepoint);
				x += Font_CharWidth(codeflags, codepoint);
			}
			x = (nw - x);
		}
		else if (con_centernotify.value)
		{
			for (c = starts[lines]; c < ends[lines]; )
			{
				c = Font_Decode(c, &codeflags, &codepoint);
				x += Font_CharWidth(codeflags, codepoint);
			}
			x = (nw - x) / 2;
		}
		Font_LineDraw(nx+x, y, starts[lines], ends[lines]);

		y += Font_CharHeight();

		lines++;
	}

	Font_EndString(font_console);

	R2D_ImageColours(1,1,1,1);
}

void Con_ClearNotify(void)
{
	console_t *con;
	conline_t *l;
	for (con = &con_main; con; con = con->next)
	{
		for (l = con->current; l; l = l->older)
			l->flags |= CONL_NONOTIFY;
	}
}
void Con_DrawNotify (void)
{
	extern int startuppending;
	console_t *con;

	con_main.flags |= CONF_NOTIFY;
	/*keep the main console up to date*/
	con_main.notif_l = con_numnotifylines.ival;
	con_main.notif_w = con_notify_w.value;
	con_main.notif_x = con_notify_x.value;
	con_main.notif_y = con_notify_y.value;
	con_main.notif_t = con_notifytime.value;

	if (con_chat)
	{
		con_chat->notif_l = con_numnotifylines_chat.ival;
		con_chat->notif_w = 1;
		con_chat->notif_y = (vid.height - sb_lines - 8*4) / vid.width;
		con_chat->notif_t = con_notifytime_chat.value;
	}

	if (startuppending)
	{
		int x,y;
		Font_BeginString(font_console, 0, 0, &x, &y);
		Con_DrawProgress(0, vid.width, 0);
		Font_EndString(font_console);
	}
	else
	{
		for (con = &con_main; con; con = con->next)
		{
			if (con->flags & CONF_NOTIFY)
				Con_DrawNotifyOne(con);
		}
	}

	if (Key_Dest_Has(kdm_message))
	{
		int x, y;
		conchar_t *starts[8];
		conchar_t *ends[8];
		conchar_t markup[MAXCMDLINE+64];
		conchar_t *c, *end;
		char *foo = va(chat_team?"say_team: %s":"say: %s", chat_buffer?(char*)chat_buffer:"");
		int lines, i, pos;
		Font_BeginString(font_console, 0, 0, &x, &y);
		y = con_main.notif_l * Font_CharHeight();

		i = chat_team?10:5;
		pos = strlen(foo)+i;
		pos = min(pos, chat_bufferpos + i);

		//figure out where the cursor is, if its safe
		i = foo[pos];
		foo[pos] = 0;
		c = COM_ParseFunString(CON_WHITEMASK, foo, markup, sizeof(markup), PFS_KEEPMARKUP|PFS_FORCEUTF8);
		foo[pos] = i;

		//k, build the string properly.
		end = COM_ParseFunString(CON_WHITEMASK, foo, markup, sizeof(markup) - sizeof(markup[0])-1, PFS_KEEPMARKUP | PFS_FORCEUTF8);

		//and overwrite the cursor so that it blinks.
		*end = ' '|CON_WHITEMASK;
		if (((int)(realtime*con_cursorspeed)&1))
			*c = 0xe00b|CON_WHITEMASK;
		if (c == end)
			end++;

		lines = Font_LineBreaks(markup, end, Font_ScreenWidth(), 8, starts, ends);
		for (i = 0; i < lines; i++)
		{
			x = 0;
			Font_LineDraw(x, y, starts[i], ends[i]);
			y += Font_CharHeight();
		}
		Font_EndString(font_console);
	}
}

//send all the stuff that was con_printed to sys_print.
//This is so that system consoles in windows can scroll up and have all the text.
void Con_PrintToSys(void)
{
	console_t *curcon = &con_main;
	conline_t *l;
	int i;
	conchar_t *t;
	char buf[16];

	for (l = curcon->oldest; l; l = l->newer)
	{
		t = (conchar_t*)(l+1);
		//fixme: utf8?
		for (i = 0; i < l->length; i++)
		{
			if (!(t[i] & CON_HIDDEN))
			{
				if (com_parseutf8.ival>0)
				{
					int cl = utf8_encode(buf, t[i]&CON_CHARMASK, sizeof(buf)-1);
					if (cl)
					{
						buf[cl] = 0;
						Sys_Printf("%s", buf);
					}
				}
				else
					Sys_Printf("%c", t[i]&0xff);
			}
		}
		Sys_Printf("\n");
	}
}

//returns the bottom of the progress bar
static int Con_DrawProgress(int left, int right, int y)
{
	conchar_t			dlbar[1024], *chr;
	unsigned char	progresspercenttext[128];
	char *progresstext = NULL;
	char *txt;
	int x, tw;
	int i;
	int barwidth, barleft;
	float progresspercent = 0;
	unsigned int codeflags, codepoint;
	*progresspercenttext = 0;

	// draw the download bar
	// figure out width
	if (cls.download)
	{
		unsigned int count;
		qofs_t total;
		qboolean extra;
		progresstext = cls.download->localname;
		progresspercent = cls.download->percent;

		if (cls.download->sizeunknown && cls.download->size == 0)
			progresspercent = -1;

		CL_GetDownloadSizes(&count, &total, &extra);

		if (progresspercent < 0)
		{
			if ((int)(realtime/2)&1 || total == 0)
				sprintf(progresspercenttext, " (%ukbps)", CL_DownloadRate()/1000);
			else
			{
				sprintf(progresspercenttext, " (%u%skb)", (int)(total/1024), extra?"+":"");
			}

			//do some marquee thing, so the user gets the impression that SOMETHING is happening.
			progresspercent = realtime - (int)realtime;
			if ((int)realtime & 1)
				progresspercent  = 1 - progresspercent;
			progresspercent *= 100;
		}
		else
		{
			if ((int)(realtime/2)&1 || total == 0)
				sprintf(progresspercenttext, " %5.1f%% (%ukbps)", progresspercent, CL_DownloadRate()/1000);
			else
			{
				sprintf(progresspercenttext, " %5.1f%% (%u%skb)", progresspercent, (int)(total/1024), extra?"+":"");
			}
		}
	}
#ifdef RUNTIMELIGHTING
	else if (lightmodel)
	{
		if (relitsurface < lightmodel->numsurfaces)
		{
			progresstext = "light";
			progresspercent = (relitsurface*100.0f) / lightmodel->numsurfaces;
			sprintf(progresspercenttext, " %02d%%", (int)progresspercent);
		}
	}
#endif

	//at this point:
	//progresstext: what is being downloaded/done (can end up truncated)
	//progresspercent: its percentage (used only for the slider)
	//progresspercenttext: that percent as text, essentually the right hand part of the bar.

	if (progresstext)
	{
		//chop off any leading path
		if ((txt = strrchr(progresstext, '/')) != NULL)
			txt++;
		else
			txt = progresstext;

		x = 0;
		COM_ParseFunString(CON_WHITEMASK, txt, dlbar, sizeof(dlbar), false);
		for (i=0,chr = dlbar; *chr; )
		{
			chr = Font_Decode(chr, &codeflags, &codepoint);
			x += Font_CharWidth(codeflags, codepoint);
			i++;
		}

		//if the string is wider than a third of the screen
		if (x > (right - left)/3)
		{
			//truncate the file name and add ...
			x += 3*Font_CharWidth(CON_WHITEMASK, '.');
			while (x > (right - left)/3)
			{
				chr = Font_DecodeReverse(chr, dlbar, &codeflags, &codepoint);
				x -= Font_CharWidth(codeflags, codepoint);
			}

			dlbar[i++] = '.'|CON_WHITEMASK;
			dlbar[i++] = '.'|CON_WHITEMASK;
			dlbar[i++] = '.'|CON_WHITEMASK;
			dlbar[i] = 0;
		}

		//i is the char index of the dlbar so far, x is the char width of it.

		//add a couple chars
		dlbar[i] = ':'|CON_WHITEMASK;
		x += Font_CharWidth(CON_WHITEMASK, ':');
		i++;
		dlbar[i] = ' '|CON_WHITEMASK;
		x += Font_CharWidth(CON_WHITEMASK, ' ');
		i++;

		COM_ParseFunString(CON_WHITEMASK, progresspercenttext, dlbar+i, sizeof(dlbar)-i*sizeof(conchar_t), false);
		for (chr = &dlbar[i], tw = 0; *chr; )
		{
			chr = Font_Decode(chr, &codeflags, &codepoint);
			tw += Font_CharWidth(codeflags, codepoint);
		}

		barwidth = (right-left) - (x + tw);

		//draw the right hand side
		x = right - tw;
		for (chr = &dlbar[i]; *chr; )
		{
			chr = Font_Decode(chr, &codeflags, &codepoint);
			x = Font_DrawChar(x, y, codeflags, codepoint);
		}

		//draw the left hand side
		x = left;
		for (chr = dlbar; chr < &dlbar[i]; )
		{
			chr = Font_Decode(chr, &codeflags, &codepoint);
			x = Font_DrawChar(x, y, codeflags, codepoint);
		}

		//and in the middle we have lots of stuff

		barwidth -= (Font_CharWidth(CON_WHITEMASK, 0xe080) + Font_CharWidth(CON_WHITEMASK, 0xe082));
		x = Font_DrawChar(x, y, CON_WHITEMASK, 0xe080);
		barleft = x;
		for(;;)
		{
			if (x + Font_CharWidth(CON_WHITEMASK, 0xe081) > barleft+barwidth)
				break;
			x = Font_DrawChar(x, y, CON_WHITEMASK, 0xe081);
		}
		x = Font_DrawChar(x, y, CON_WHITEMASK, 0xe082);

		if (progresspercent >= 0)
			Font_DrawChar(barleft+(barwidth*progresspercent)/100 - Font_CharWidth(CON_WHITEMASK, 0xe083)/2, y, CON_WHITEMASK, 0xe083);

		y += Font_CharHeight();
	}
	return y;
}

//draws console selection choices at the top of the screen, if multiple consoles are available
//its ctrl+tab to switch between them
int Con_DrawAlternateConsoles(int lines)
{
	char *txt;
	int x, y = 0, lx;
	int consshown = 0;
	console_t *con = &con_main, *om = con_mouseover;
	conchar_t buffer[512], *end, *start;
	unsigned int codeflags, codepoint;

	for (con = &con_main; con; con = con->next)
	{
		if (!(con->flags & (CONF_HIDDEN|CONF_ISWINDOW)))
			consshown++;
	}

	if (lines == (int)scr_conlines && consshown > 1) 
	{
		int mx, my, h;
		Font_BeginString(font_console, mousecursor_x, mousecursor_y, &mx, &my);
		Font_BeginString(font_console, 0, y, &x, &y);
		h = Font_CharHeight();
		for (x = 0, con = &con_main; con; con = con->next)
		{
			if (con->flags & (CONF_HIDDEN|CONF_ISWINDOW))
				continue;
			txt = con->title;

			//yeah, om is an evil 1-frame delay. whatever
			end = COM_ParseFunString(CON_WHITEMASK, va("^&%c%i%s", ((con!=om)?'F':'B'), (con==con_current)+con->unseentext*4, txt), buffer, sizeof(buffer), false);

			lx = 0;
			for (lx = x, start = buffer; start < end; )
			{
				start = Font_Decode(start, &codeflags, &codepoint);
				lx = Font_CharEndCoord(font_console, lx, codeflags, codepoint);
			}
			if (lx > Font_ScreenWidth())
			{
				x = 0;
				y += h;
			}
			for (lx = x, start = buffer; start < end; )
			{
				start = Font_Decode(start, &codeflags, &codepoint);
				lx = Font_DrawChar(lx, y, codeflags, codepoint);
			}
			lx += 8;
			if (mx >= x && mx < lx && my >= y && my < y+h)
				con_mouseover = con;
			x = lx;
		}
		y+= h;
		Font_EndString(font_console);

		y = (y*(int)vid.height) / (float)vid.rotpixelheight;
	}
	return y;
}

//draws the conline_t list bottom-up within the width of the screen until the top of the screen is reached.
//if text is selected, the selstartline globals will be updated, so make sure the lines persist or check them.
static int Con_DrawConsoleLines(console_t *con, conline_t *l, int sx, int ex, int y, int top, int selactive, int selsx, int selex, int selsy, int seley, float lineagelimit)
{
	int linecount;
	conchar_t *starts[64], *ends[sizeof(starts)/sizeof(starts[0])];
	conchar_t *s, *e, *c;
	int x;
	int charh = Font_CharHeight();
	unsigned int codeflags, codepoint;
	float alphaval = 1;

	if (l != con->completionline)
	if (l != con->footerline)
	if (l != con->current)
	{
		y -= 8;
	// draw arrows to show the buffer is backscrolled
		for (x = sx ; x<ex; )
			x = (Font_DrawChar (x, y, CON_WHITEMASK, '^')-x)*4+x;
	}

	if (selactive != -1)
	{
		if (!selactive)
			selactive = 2;	//calculate, but don't draw (to track mouse-over)

		//deactivate the selection if the start and end is outside
		if (
			(selsx < sx && selex < sx) ||
			(selsx > ex && selex > ex) ||
			(selsy < top && seley < top) ||
			(selsy > y && seley > y)
			)
			selactive = false;	//don't track it at all

		if (selactive)
		{
			//clip it
			if (selsx < sx)
				selsx = sx;
			if (selex < sx)
				selex = sx;

			if (selsy > y)
				selsy = y;
			if (seley > y)
				seley = y;

			//scale the y coord to be in lines instead of pixels
			selsy -= y;
			seley -= y;
	//		selsy -= charh;
	//		seley -= charh;

			//invert the selections to make sense, text-wise
			/*if (selsy == seley)
			{
				//single line selected backwards
				if (selex < selsx)
				{
					x = selex;
					selex = selsx;
					selsx = x;
				}
			}
			*/
			if (seley < selsy)
			{	//selection goes upwards
				x = selsy;
				selsy = seley;
				seley = x;

				x = selex;
				selex = selsx;
				selsx = x;
			}
	//		selsy *= Font_CharHeight();
	//		seley *= Font_CharHeight();
			selsy += y;
			seley += y;
		}
	}

	if (l && l == con->current && l->length == 0 && con->userline != l)
		l = l->older;
	for (; l; l = l->older)
	{
		shader_t *pic = NULL;
		int picw=0, pich=0;
		s = (conchar_t*)(l+1);

		if (lineagelimit)
		{
			alphaval = realtime - (l->time+lineagelimit);
			if (alphaval > 0)
			{
				float fadetime = con->notif_fade?con->notif_fade:1;
				if (alphaval > fadetime)
					break;	//we're done here
				alphaval = 1 - (alphaval/fadetime);
			}
			else
				alphaval = 1;
		}

		if (l->length >= 2 && *s == CON_LINKSTART && (s[1]&CON_CHARMASK) == '\\')
		{	//leading tag with no text, look for an image in there
			conchar_t *e;
			char linkinfo[256];
			int linkinfolen = 0;
			for (e = s+1; e < s+l->length; e++)
			{
				if (*e == CON_LINKEND)
				{
					char *imgname;
					linkinfo[linkinfolen] = 0;
					imgname = Info_ValueForKey(linkinfo, "img");
					if (*imgname)
					{
						pic = R_RegisterPic(imgname, NULL);
						if (pic)
						{
							imgname = Info_ValueForKey(linkinfo, "w");
							if (*imgname)
								picw = (atoi(imgname) * charh) / 8.0;
							else if (pic->width)
								picw = (pic->width * vid.pixelwidth) / vid.width;
							else
								picw = 64;
							imgname = Info_ValueForKey(linkinfo, "h");
							if (*imgname)
								pich = (atoi(imgname) * charh) / 8.0;
							else if (pic->height)
								pich = (pic->height * vid.pixelheight) / vid.height;
							else
								pich = 64;

							if (picw >= ex-sx)
							{
								pich *= (float)(ex-sx) / picw;
								picw = ex-sx;
							}
						}
					}
					break;
				}
				linkinfolen += unicode_encode(linkinfo+linkinfolen, (*e & CON_CHARMASK), sizeof(linkinfo)-1-linkinfolen, true);
			}
		}

		if (con->flags & CONF_NOWRAP)
		{
			linecount = 1;
			starts[0] = s;
			ends[0] = s+l->length;
		}
		else
		{
			linecount = Font_LineBreaks(s, s+l->length, ex-sx-picw, sizeof(starts)/sizeof(starts[0]), starts, ends);
			//if Con_LineBreaks didn't find any lines at all, then it was an empty line, and we need to ensure that its still drawn
			if (linecount == 0 && !pic)
			{
				linecount = 1;
				starts[0] = ends[0] = s;
			}
		}

		if (pic)
		{
			float szx = (float)vid.width / vid.pixelwidth;
			float szy = (float)vid.height / vid.pixelheight;
			int texth = (linecount) * Font_CharHeight();
			if (R2D_Flush)
				R2D_Flush();
			R2D_ImageColours(1.0, 1.0, 1.0, 1.0);
			if (texth > pich)
			{
				texth = pich + (texth-pich)/2;
				R2D_Image(sx*szx, (y-texth)*szy, picw*szx, pich*szy, 0, 0, 1, 1, pic);
				pich = 0;	//don't pad the text...
			}
			else
			{
				R2D_Image(sx*szx, (y-pich)*szy, picw*szx, pich*szy, 0, 0, 1, 1, pic);
				pich -= texth;
				y-= pich/2;	//skip some space above and below the text block, to keep the text and image aligned.
			}
			if (R2D_Flush)
				R2D_Flush();

//			if (selsx < picw && selex < picw)

		}

		l->numlines = linecount;

		while(linecount-- > 0)
		{
			s = starts[linecount];
			e = ends[linecount];

			y -= Font_CharHeight();

			if (top && y < top)
				break;

			if (l->flags & (CONL_BREAKPOINT|CONL_EXECUTION))
			{
				if (l->flags & CONL_EXECUTION)
				{
					if (l->flags & CONL_BREAKPOINT)
						R2D_ImageColours(0.3,0.15,0.0, alphaval);
					else
						R2D_ImageColours(0.3,0.3,0.0, alphaval);
				}
				else //if (l->flags & CONL_BREAKPOINT)
					R2D_ImageColours(0.3,0.0,0.0, alphaval);
				R2D_FillBlock((sx*(float)vid.width)/(float)vid.rotpixelwidth, (y*vid.height)/(float)vid.rotpixelheight, ((ex - sx)*vid.width)/(float)vid.rotpixelwidth, (Font_CharHeight()*vid.height)/(float)vid.rotpixelheight);
				R2D_Flush();
			}

			if (selactive < 0)
			{	//display an existing selection
				int sstart = picw;
				int send = sstart;
				int center;
				if (selactive == -2 || l == con->selendline || l == con->selstartline)
				{
					for (c = s; c < e; )
					{
						c = Font_Decode(c, &codeflags, &codepoint);
						send = Font_CharEndCoord(font_console, send, codeflags, codepoint);
					}
					//show something on blank lines
					if (send == sstart)
						send = Font_CharEndCoord(font_console, send, CON_WHITEMASK, ' ');

					center = sx;
					if (l->flags&CONL_CENTERED)
						center += ((ex-sx) - send)/2;
				
					if (l == con->selendline)
					{
						selactive = -2;	//all following lines need to be selected, until we see the other end of the selection
						send = sstart;
						for (c = s; c < (conchar_t*)(con->selendline+1)+con->selendoffset; )
						{
							c = Font_Decode(c, &codeflags, &codepoint);
							send = Font_CharEndCoord(font_console, send, codeflags, codepoint);
						}
					}
					if (l == con->selstartline)
					{
						for (c = s; c < (conchar_t*)(con->selstartline+1)+con->selstartoffset; )
						{
							c = Font_Decode(c, &codeflags, &codepoint);
							sstart = Font_CharEndCoord(font_console, sstart, codeflags, codepoint);
						}
						if (c == (conchar_t*)(con->selstartline+1)+con->selstartoffset)
							selactive = 0;	//no need to track any other selections.
					}

					sstart += center;
					send += center;

					R2D_ImageColours(0.1,0.1,0.3, alphaval);
					if (send < sstart)
						R2D_FillBlock((send*(float)vid.width)/(float)vid.rotpixelwidth, (y*vid.height)/(float)vid.rotpixelheight, ((sstart - send)*vid.width)/(float)vid.rotpixelwidth, (Font_CharHeight()*vid.height)/(float)vid.rotpixelheight);
					else
						R2D_FillBlock((sstart*(float)vid.width)/(float)vid.rotpixelwidth, (y*vid.height)/(float)vid.rotpixelheight, ((send - sstart)*vid.width)/(float)vid.rotpixelwidth, (Font_CharHeight()*vid.height)/(float)vid.rotpixelheight);
					R2D_Flush();
				}
			}
			else if (selactive)
			{
				if (y+charh >= selsy)
				{
					if (y < seley)
					{
						int sstart;
						int send;
						int center;
						send = sstart = picw;
						for (c = s; c < e; )
						{
							c = Font_Decode(c, &codeflags, &codepoint);
							send = Font_CharEndCoord(font_console, send, codeflags, codepoint);
						}

						//show something on blank lines
						if (send == sstart)
							send = Font_CharEndCoord(font_console, send, CON_WHITEMASK, ' ');

						center = sx;
						if (l->flags&CONL_CENTERED)
							center += ((ex-sx) - send)/2;

						if (y+charh >= seley && y < selsy)
						{	//if they're both on the same line, make sure sx is to the left of ex, so our stuff makes sense
							if (selex < selsx)
							{
								x = selex;
								selex = selsx;
								selsx = x;
							}
						}

						if (y+charh >= seley)
						{
							send = sstart;
							for (c = s; c < e; )
							{
								c = Font_Decode(c, &codeflags, &codepoint);
								send = Font_CharEndCoord(font_console, send, codeflags, codepoint);

								if (send+center > selex)
									break;
							}

							con->selendline = l;
							if (s)
								con->selendoffset = c - (conchar_t*)(l+1);
							else
								con->selendoffset = 0;
						}
						if (y < selsy)
						{
							for (c = s; c < e; )
							{
								Font_Decode(c, &codeflags, &codepoint);
								x = Font_CharEndCoord(font_console, sstart, codeflags, codepoint);
								if (x+center > selsx)
									break;
								c = Font_Decode(c, &codeflags, &codepoint);
								sstart = x;
							}

							con->selstartline = l;
							if (s)
								con->selstartoffset = c - (conchar_t*)(l+1);
							else
								con->selstartoffset = 0;
						}

						sstart += center;
						send += center;

						if (selactive == 1)
						{
							R2D_ImageColours(0.1,0.1,0.3, alphaval);
							if (send < sstart)
								R2D_FillBlock((send*vid.width)/(float)vid.rotpixelwidth, (y*vid.height)/(float)vid.rotpixelheight, ((sstart - send)*vid.width)/(float)vid.rotpixelwidth, (Font_CharHeight()*vid.height)/(float)vid.rotpixelheight);
							else
								R2D_FillBlock((sstart*vid.width)/(float)vid.rotpixelwidth, (y*vid.height)/(float)vid.rotpixelheight, ((send - sstart)*vid.width)/(float)vid.rotpixelwidth, (Font_CharHeight()*vid.height)/(float)vid.rotpixelheight);
							R2D_Flush();
						}
					}
				}
			}
			R2D_ImageColours(1.0, 1.0, 1.0, alphaval);

			x = sx + picw;

			if (l->flags&CONL_CENTERED)
			{
				int send = 0;
				for (c = s; c < e; )
				{
					c = Font_Decode(c, &codeflags, &codepoint);
					send = Font_CharEndCoord(font_console, send, codeflags, codepoint);
				}

				x += ((ex-sx) - send)/2;
			}

			Font_LineDraw(x, y, s, e);


			if (con->userline == l && s <= (conchar_t*)(l+1)+con->useroffset && (conchar_t*)(l+1)+con->useroffset <= e)
			if ((int)(realtime*4)&1)
			{
				int sstart;
				sstart = picw;
				for (c = s; c < (conchar_t*)(l+1)+con->useroffset; )
				{
					c = Font_Decode(c, &codeflags, &codepoint);
					sstart = Font_CharEndCoord(font_console, sstart, codeflags, codepoint);
				}
				Font_DrawChar(sx+sstart, y, CON_WHITEMASK, 0xe00b);
			}

			if (y < top)
				break;
		}
		y -= pich/2;
		if (y < top)
			break;
	}
	return y;
}

void Draw_ExpandedString(float x, float y, conchar_t *str);

/*
================
Con_DrawConsole

Draws the console with the solid background
================
*/
void Con_DrawConsole (int lines, qboolean noback)
{
	extern qboolean scr_con_forcedraw;
	int x, y, sx, ex;
	conline_t *l;
	int selsx, selsy, selex, seley, selactive;
	qboolean haveprogress;
	console_t *w, *mouseconsole;
	float fadetime;

	con_mouseover = NULL;

	//draw any windowed consoles (under main console)
	for (w = &con_main; w; w = w->next)
	{
		srect_t srect;
		if ((w->flags & (CONF_HIDDEN|CONF_ISWINDOW)) != CONF_ISWINDOW)
			continue;

		if (Key_Dest_Has(kdm_cwindows))
			fadetime = 0;	//nothing fades when focused.
		else
			fadetime = 4;

		if (w->wnd_w > vid.width)
			w->wnd_w = vid.width;
		if (w->wnd_h > vid.height)
			w->wnd_h = vid.height;
		if (w->wnd_w < 64)
			w->wnd_w = 64;
		if (w->wnd_h < 16)
			w->wnd_h = 16;
		//windows that move off the top of the screen somehow are bad.
		if (w->wnd_y > vid.height - 8)
			w->wnd_y = vid.height - 8;
		if (w->wnd_y < 0)
			w->wnd_y = 0;
		if (w->wnd_x > vid.width-32)
			w->wnd_x = vid.width-32;
		if (w->wnd_x < -w->wnd_w+32)
			w->wnd_x = -w->wnd_w+32;

		if (w->wnd_h < 8)
			w->wnd_h = 8;

		if (mousecursor_x >= w->wnd_x && mousecursor_x < w->wnd_x+w->wnd_w && mousecursor_y >= w->wnd_y && mousecursor_y < w->wnd_y+w->wnd_h && mousecursor_y > lines)
			con_mouseover = w;

		w->mousecursor[0] = mousecursor_x - (w->wnd_x+8);
		w->mousecursor[1] = mousecursor_y - w->wnd_y;

		if (Key_Dest_Has(kdm_cwindows))
		{
			if (con_curwindow==w)
				R2D_ImageColours(0.0, 0.05, 0.1, 0.8);
			else
				R2D_ImageColours(0.0, 0.05, 0.1, 0.5);
			R2D_FillBlock(w->wnd_x, w->wnd_y, w->wnd_w, w->wnd_h);
			R2D_ImageColours(1, 1, 1, 1);

			if (w->backshader || *w->backimage)
			{
				shader_t *shader = w->backshader;
				if (!shader)
					shader = w->backshader = R_RegisterPic(w->backimage, NULL);// R_RegisterCustom(w->backimage, SUF_NONE, Shader_DefaultCinematic, w->backimage);
				if (shader)
				{
					int top = 8;
#ifdef HAVE_MEDIA_DECODER
					cin_t *cin = R_ShaderGetCinematic(shader);
					if (cin)
					{
						const char *url = Media_Send_GetProperty(cin, "url");
						if (url)
						{
							float x = 0;
//							float r = x+w->wnd_w-16;
							const char *buttons[] = {"bck", "fwd", "rld", "home", ((w->linebuffered == Con_Navigate)?(char*)key_lines[edit_line]:url)};
							const char *buttoncmds[] = {"cmd:back", "cmd:forward", "cmd:refresh", ENGINEWEBSITE, NULL};
							float tw;
							int i, fl;

							for (i = 0; i < countof(buttons); i++)
							{
								if (i == countof(buttons)-1)
									tw = ~0u;
								else if (i == countof(buttons)-2)
									tw = 40;
								else
									tw = 32;
								fl = con_curwindow==w;
								if (w->mousecursor[1] >= 8 && w->mousecursor[1] < 16 && w->mousecursor[0] >= x && w->mousecursor[0] < x+tw)
								{
									fl |= 2;
									if (w->buttonsdown == CB_ACTIONBAR)
									{
										w->buttonsdown = CB_NONE;
										if (buttoncmds[i])
											Media_Send_Command(cin, buttoncmds[i]);
										else if (w->linebuffered != Con_Navigate)
										{
											Key_ConsoleReplace(url);
											w->linebuffered = Con_Navigate;
										}
									}
								}
								if (tw > w->wnd_w-16 - x)
									tw = w->wnd_w-16 - x;
								Draw_FunStringWidth(w->wnd_x+8+x, w->wnd_y+top, buttons[i], tw, false, fl);
								x += tw;
							}
							top += 8;
						}

						Media_Send_Resize(cin, ((w->wnd_w-16.0)*(int)vid.rotpixelwidth) / (float)vid.width, ((w->wnd_h-8-top)*(int)vid.rotpixelheight) / (float)vid.height);
						Media_Send_MouseMove(cin, (w->mousecursor[0]) / (w->wnd_w-16.0), (w->mousecursor[1]-top) / (w->wnd_h-8.0-top));
						if (con_curwindow==w)
							Media_Send_Command(cin, "cmd:focus");
						else
							Media_Send_Command(cin, "cmd:unfocus");
					}
#endif
					R2D_Image(w->wnd_x+8, w->wnd_y+top, w->wnd_w-16, w->wnd_h-8-top, 0, 0, 1, 1, shader);
				}
			}

			Draw_FunStringWidth(w->wnd_x, w->wnd_y, w->title, w->wnd_w-8, 2, (con_curwindow==w)?true:false);
			Draw_FunStringWidth(w->wnd_x+w->wnd_w-8, w->wnd_y, "X", 8, 2, ((w->buttonsdown == CB_CLOSE && w->mousecursor[0] > w->wnd_w-16 && w->mousecursor[1] < 8) || (con_curwindow==w && w->mousecursor[0] >= w->wnd_w-16 && w->mousecursor[0] < w->wnd_w-8 && w->mousecursor[1] >= 0 && w->mousecursor[1] < 8))?true:false);
			w->unseentext = false;
		}
		else
			w->buttonsdown = 0;

		srect.x = (w->wnd_x+8) / vid.width;
		srect.y = (w->wnd_y+8) / vid.height;
		srect.width = (w->wnd_w-16) / vid.width;
		srect.height = (w->wnd_h-16) / vid.height;
		srect.dmin = -99999;
		srect.dmax = 99999;
		srect.y = (1-srect.y) - srect.height;
		if (srect.width && srect.height)
		{
			if (!fadetime)
			{
				R2D_ImageColours(0, 0.1, 0.2, 1.0);
				if ((w->buttonsdown & CB_SIZELEFT) || (con_curwindow==w && w->mousecursor[0] >= -8 && w->mousecursor[0] < 0 && w->mousecursor[1] >= 8 && w->mousecursor[1] < w->wnd_h))
					R2D_FillBlock(w->wnd_x, w->wnd_y+8, 8, w->wnd_h-8);
				if ((w->buttonsdown & CB_SIZERIGHT) || (con_curwindow==w && w->mousecursor[0] >= w->wnd_w-16 && w->mousecursor[0] < w->wnd_w-8 && w->mousecursor[1] >= 8 && w->mousecursor[1] < w->wnd_h))
					R2D_FillBlock(w->wnd_x+w->wnd_w-8, w->wnd_y+8, 8, w->wnd_h-8);
				if ((w->buttonsdown & CB_SIZEBOTTOM) || (con_curwindow==w && w->mousecursor[0] >= -8 && w->mousecursor[0] < w->wnd_w-8 && w->mousecursor[1] >= w->wnd_h-8 && w->mousecursor[1] < w->wnd_h))
					R2D_FillBlock(w->wnd_x, w->wnd_y+w->wnd_h-8, w->wnd_w, 8);
			}
			if (R2D_Flush)
				R2D_Flush();
			BE_Scissor(&srect);
			Con_DrawOneConsole(w, con_curwindow == w && Key_Dest_Has(kdm_console|kdm_cwindows) == kdm_cwindows, font_console, w->wnd_x+8, w->wnd_y, w->wnd_w-16, w->wnd_h-8, fadetime);
			if (R2D_Flush)
				R2D_Flush();
			BE_Scissor(NULL);
		}

		if (w->selstartline)
			mouseconsole = w;
		if (!con_curwindow)
			con_curwindow = w;
	}

	//draw main console...
	if (lines > 0)
	{
		int top;
#ifdef QTERM
		if (qterms)
			QT_Update();
#endif

// draw the background
		if (!noback)
			R2D_ConsoleBackground (0, lines, scr_con_forcedraw);

		con_current->unseentext = false;

		con_current->vislines = lines;

		top = Con_DrawAlternateConsoles(lines);

		if (!con_current->display)
			con_current->display = con_current->current;

		x = 8;
		y = lines;

		con_current->mousecursor[0] = mousecursor_x;
		con_current->mousecursor[1] = mousecursor_y;
		con_current->selstartline = NULL;
		con_current->selendline = NULL;
		selactive = Key_GetConsoleSelectionBox(con_current, &selsx, &selsy, &selex, &seley);

		Font_BeginString(font_console, x, y, &x, &y);
		Font_BeginString(font_console, selsx, selsy, &selsx, &selsy);
		Font_BeginString(font_console, selex, seley, &selex, &seley);
		ex = Font_ScreenWidth();
		sx = x;
		ex -= sx;

		y -= Font_CharHeight();
		haveprogress = Con_DrawProgress(x, ex - x, y) != y;
		y = Con_DrawInput (con_current, Key_Dest_Has(kdm_console), x, ex - x, y, selactive, selsx, selex, selsy, seley);

		l = con_current->display;

		y = Con_DrawConsoleLines(con_current, l, sx, ex, y, top, selactive, selsx, selex, selsy, seley, 0);

		if (!haveprogress && lines == vid.height)
		{
			char *version = version_string();
			int i;
			Font_BeginString(font_console, vid.width, lines, &x, &y);
			y -= Font_CharHeight();
			//assumption: version == ascii
			for (i = 0; version[i]; i++)
				x -= Font_CharWidth(CON_WHITEMASK|CON_HALFALPHA, version[i]);
			for (i = 0; version[i]; i++)
				x = Font_DrawChar(x, y, CON_WHITEMASK|CON_HALFALPHA, version[i]);
		}

		Font_EndString(font_console);
		mouseconsole = con_mouseover?con_mouseover:con_current;
	}
	else
		mouseconsole = con_mouseover?con_mouseover:NULL;

	if (mouseconsole && mouseconsole->selstartline)
	{
		char *tiptext = NULL;
		shader_t *shader = NULL;
		char *mouseover;
		if (!mouseconsole->mouseover || !mouseconsole->mouseover(mouseconsole, &tiptext, &shader))
		{
			mouseover = Con_CopyConsole(mouseconsole, false, true);
			if (mouseover)
			{
				char *end = strstr(mouseover, "^]");
				char *info = strchr(mouseover, '\\');
				if (!info)
					info = "";
				if (end)
					*end = 0;
	#ifdef PLUGINS
				if (!Plug_ConsoleLinkMouseOver(mousecursor_x, mousecursor_y, mouseover+2, info))
	#endif
				{
					char *key = Info_ValueForKey(info, "tipimg");
					if (*key)
						shader = R2D_SafeCachePic(key);
					else
					{
						key = Info_ValueForKey(info, "tiprawimg");
						if (*key)
						{
							shader = R2D_SafeCachePic("riprawimg");
							shader->defaulttextures->base = Image_FindTexture(key, NULL, IF_NOREPLACE|IF_PREMULTIPLYALPHA);
							if (!shader->defaulttextures->base)
							{
								size_t fsize;
								char *buf;
								shader->defaulttextures->base = Image_CreateTexture(key, NULL, IF_NOREPLACE|IF_PREMULTIPLYALPHA);
								if ((buf = COM_LoadFile (key, 5, &fsize)))
									Image_LoadTextureFromMemory(shader->defaulttextures->base, shader->defaulttextures->base->flags|IF_NOWORKER, key, key, buf, fsize);
							}
							shader->width = shader->defaulttextures->base->width;
							shader->height = shader->defaulttextures->base->height;
							if (shader->width > 320)
							{
								shader->height *= 320.0/shader->width;
								shader->width = 320;
							}
							if (shader->height > 240)
							{
								shader->width *= 240.0/shader->height;
								shader->height = 240;
							}
						}
					}
					tiptext = Info_ValueForKey(info, "tip");
				}
				Z_Free(mouseover);
			}
		}
		if ((tiptext && *tiptext) || shader)
		{
			//FIXME: draw a proper background.
			//FIXME: support line breaks.
			conchar_t buffer[2048], *starts[64], *ends[countof(starts)];
			int lines, i, px, py;
			float tw, th;
			float ih = 0, iw = 0;
			float x = mousecursor_x+8;
			float y = mousecursor_y+8;

			Font_BeginString(font_console, x, y, &px, &py);
			lines = Font_LineBreaks(buffer, COM_ParseFunString(CON_WHITEMASK, tiptext, buffer, sizeof(buffer), false), (256.0 * vid.pixelwidth) / vid.width, countof(starts), starts, ends);
			th = (Font_CharHeight()*lines * vid.height) / vid.pixelheight;

			if (shader)
			{
				int w, h;
				if (R_GetShaderSizes(shader, &w, &h, false) >= 0)
				{
					iw = w;
					ih = h;
				}
				else
					shader = NULL;
			}

			if (x + iw/2 + 8 + 256 > vid.width)
				x = vid.width - (iw/2 + 8 + 256);
			if (x < iw/2)
				x = iw/2;
			x += iw/2 + 8;

			if (y+max(th, ih) > vid.height)
				y = mousecursor_y - 8 - max(th, ih);
			if (y < 0)
				y = 0;

			Font_BeginString(font_console, x, y + (max(th, ih) - th)/2, &px, &py);
			for (i = 0, tw = 0; i < lines; i++)
			{
				int lw = Font_LineWidth(starts[i], ends[i]);
				if (lw > tw)
					tw = lw;
			}
			tw *= (float)vid.width / vid.pixelwidth;
			Font_EndString(font_console);
			R2D_ImageColours(0, 0, 0, .75);
			R2D_FillBlock(x, y + (max(th, ih) - th)/2, tw, th);
			R2D_ImageColours(1, 1, 1, 1);
			Font_BeginString(font_console, x, y + (max(th, ih) - th)/2, &px, &py);
			for (i = 0; i < lines; i++)
			{
				Font_LineDraw(px, py, starts[i], ends[i]);
				py += Font_CharHeight();
			}
			Font_EndString(font_console);

			if (shader)
			{
				if (th > ih)
					y += (th-ih)/2;
				R2D_Image(x-8-iw, y, iw, ih, 0, 0, 1, 1, shader);
			}
		}
	}
}

void Con_DrawOneConsole(console_t *con, qboolean focused, struct font_s *font, float fx, float fy, float fsx, float fsy, float lineagelimit)
{
	int selactive, selsx, selsy, selex, seley;
	int x, y, sx, sy;
	Font_BeginString(font, fx, fy, &x, &y);
	Font_BeginString(font, fx+fsx, fy+fsy, &sx, &sy);

	if (con == con_current && Key_Dest_Has(kdm_console))
	{
		selactive = false;	//don't change selections if this is the main console and we're looking at the console, because that main console has focus instead anyway.
		selsx = selsy = selex = seley = 0;
	}
	else
	{
		selactive = Key_GetConsoleSelectionBox(con, &selsx, &selsy, &selex, &seley);
		if ((con->flags & CONF_KEEPSELECTION) && con->selstartline && con->selendline)
		{
			selactive = -1;
			selsx = selsy = selex = seley = 0;
		}
		else
		{
			con->selstartline = NULL;
			con->selendline = NULL;
			Font_BeginString(font, selsx, selsy, &selsx, &selsy);
			Font_BeginString(font, selex, seley, &selex, &seley);
			selsx += x;
			selsy += y;
			selex += x;
			seley += y;
		}
	}

	R2D_ImageColours(1, 1, 1, 1);
	sy = Con_DrawInput (con, focused, x, sx, sy, selactive, selsx, selex, selsy, seley);

	sx -= con->displayoffset;
	selsx -= con->displayoffset;
	selex -= con->displayoffset;

	if (!con->display)
		con->display = con->current;
	Con_DrawConsoleLines(con, con->display, x, sx, sy, y, selactive, selsx, selex, selsy, seley, lineagelimit);

	Font_EndString(font);
}

//false=don't walk over it.
//true=fine and dandy
//2=ignore only if at the end.
static qbyte Con_IsTokenChar(unsigned int chr)
{
	if (chr >= 0x80)	//unicode chars are all continuation
		return true;
	if (chr == '(' || chr == ')' || chr == '{' || chr == '}')
		return false;
	if (chr == '.' || chr == '/' || chr == '\\')
		return 2;
	if (chr >= 'a' && chr <= 'z')
		return true;
	if (chr >= 'A' && chr <= 'Z')
		return true;
	if (chr >= '0' && chr <= '9')
		return true;
	if (chr == '[' || chr == ']' || chr == '_' || chr == ':')
		return true;
	return false;
}
char *Con_CopyConsole(console_t *con, qboolean nomarkup, qboolean onlyiflink)
{
	conchar_t *cur;
	conline_t *l;
	conchar_t *lend;
	char *result;
	int outlen, maxlen;
	int finalendoffset;
	unsigned int uc;

	if (!con->selstartline || !con->selendline)
		return NULL;

//	for (cur = (conchar_t*)(selstartline+1), finalendoffset = 0; cur < (conchar_t*)(selstartline+1) + selstartline->length; cur++, finalendoffset++)
//		result[finalendoffset] = *cur & 0xffff;

	l = con->selstartline;
	cur = (conchar_t*)(l+1) + con->selstartoffset;
	finalendoffset = con->selendoffset;

	if (con->selstartline == con->selendline)
	{
		if (con->selstartoffset+1 == finalendoffset)
		{
			//they only selected a single char?
			//fix that up to select the entire token
			while (cur > (conchar_t*)(l+1))
			{
				cur--;
				uc = (*cur & CON_CHARMASK);
				if (!Con_IsTokenChar(uc))
				{
					cur++;
					break;
				}
				if (*cur == CON_LINKSTART)
					break;
			}
			while (finalendoffset < l->length)
			{
				uc = (((conchar_t*)(l+1))[finalendoffset] & CON_CHARMASK);
				if (Con_IsTokenChar(uc)==1 && ((conchar_t*)(l+1))[finalendoffset] != CON_LINKEND)
					finalendoffset++;
				else
					break;
			}
			/*while (finalendoffset > l->length)
			{
				uc = (((conchar_t*)(l+1))[finalendoffset] & CON_CHARMASK);
				if (Con_IsTokenChar(uc) == 2)
					finalendoffset--;
				else
					break;
			}*/
		}
	}

	//scan backwards to find any link enclosure
	for(lend = cur-1; lend >= (conchar_t*)(l+1); lend--)
	{
		if (*lend == CON_LINKSTART)
		{
			//found one
			cur = lend;
			break;
		}
		if (*lend == CON_LINKEND)
		{
			//some other link ended here. don't use its start.
			break;
		}
	}
	//scan forwards to find the end of the selected link
	if (l->length && cur < (conchar_t*)(l+1)+l->length && *cur == CON_LINKSTART)
	{
		for(lend = (conchar_t*)(con->selendline+1) + finalendoffset; lend < (conchar_t*)(con->selendline+1) + con->selendline->length; lend++)
		{
			if (*lend == CON_LINKEND)
			{
				finalendoffset = lend+1 - (conchar_t*)(con->selendline+1);
				break;
			}
		}
	}
	else if (onlyiflink)
		return NULL;

	maxlen = 1024*1024;
	result = Z_Malloc(maxlen+1);

	outlen = 0;
	for (;;)
	{
		if (l == con->selendline)
			lend = (conchar_t*)(l+1) + finalendoffset;
		else
			lend = (conchar_t*)(l+1) + l->length;

		outlen = COM_DeFunString(cur, lend, result + outlen, maxlen - outlen, nomarkup, !!(con->parseflags & PFS_FORCEUTF8)) - result;

		if (l == con->selendline)
			break;

		l = l->newer;
		if (!l)
		{
			Con_Printf("Error: Bad console buffer\n");
			break;
		}

		if (outlen+3 > maxlen)
			break;
//#ifdef _WIN32
//		result[outlen++] = '\r';
//#endif
		result[outlen++] = '\n';
		cur = (conchar_t*)(l+1);
	}
	result[outlen++] = 0;

	return result;
}


/*
==================
Con_NotifyBox
==================
*/
void Con_NotifyBox (char *text)
{
	double		t1, t2;
	qboolean hadconsole;

// during startup for sound / cd warnings
	Con_Printf("\n\n^Ue01d^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01f\n");

	Con_Printf ("%s", text);

	Con_Printf ("Press a key.\n");
	Con_Printf("^Ue01d^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01f\n");

	key_count = -2;		// wait for a key down and up
	hadconsole = !!Key_Dest_Has(kdm_console);
	Key_Dest_Add(kdm_console);

	do
	{
		t1 = Sys_DoubleTime ();
		SCR_UpdateScreen ();
		Sys_SendKeyEvents ();
		t2 = Sys_DoubleTime ();
		realtime += t2-t1;		// make the cursor blink
	} while (key_count < 0);

	Con_Printf ("\n");

	if (!hadconsole)
		Key_Dest_Remove(kdm_console);
}
