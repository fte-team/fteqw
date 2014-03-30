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

console_t	con_main;
console_t	*con_current;		// points to whatever is the visible console
console_t	*con_mouseover;		// points to whichever console's title is currently mouseovered, or null
console_t	*con_chat;			// points to a chat console

#define Font_ScreenWidth() (vid.pixelwidth)

static int Con_DrawProgress(int left, int right, int y);
static int Con_DrawConsoleLines(console_t *con, conline_t *l, int sx, int ex, int y, int top, qboolean selactive, int selsx, int selex, int selsy, int seley);

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


cvar_t		con_numnotifylines = SCVAR("con_notifylines","4");		//max lines to show
cvar_t		con_notifytime = SCVAR("con_notifytime","3");		//seconds
cvar_t		con_centernotify = SCVAR("con_centernotify", "0");
cvar_t		con_displaypossibilities = SCVAR("con_displaypossibilities", "1");
cvar_t		con_maxlines = SCVAR("con_maxlines", "1024");
cvar_t		cl_chatmode = SCVAR("cl_chatmode", "2");
cvar_t		con_numnotifylines_chat = CVAR("con_numnotifylines_chat", "8");
cvar_t		con_notifytime_chat = CVAR("con_notifytime_chat", "8");
cvar_t		con_separatechat = CVAR("con_separatechat", "0");
cvar_t		con_timestamps = CVAR("con_timestamps", "0");
cvar_t		con_timeformat = CVAR("con_timeformat", "(%H:%M:%S) ");
cvar_t		con_textsize = CVARD("con_textsize", "8", "Resize the console text to be a different height, scaled separately from the hud. The value is the height in (virtual) pixels.");

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
	console_t *prev;
	conline_t *t;
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

	if (con == &con_main)
	{
		/*main console is never destroyed, only cleared (unless shutting down)*/
		if (con_initialized)
			Con_Finit(con);
		return;
	}

	con_mouseover = NULL;

	for (prev = &con_main; prev->next; prev = prev->next)
	{
		if (prev->next == con)
		{
			prev->next = con->next;
			break;
		}
	}

	BZ_Free(con);

	if (con_current == con)
		con_current = &con_main;
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

	con->flags = flags;
	Con_Finit(con);
	con->next = con_main.next;
	con_main.next = con;

	return con;
}
/*sets a console as the active one*/
void Con_SetActive (console_t *con)
{
	con_current = con;

	if (con->footerline)
	{
		con->selstartline = NULL;
		con->selendline = NULL;
		Z_Free(con->footerline);
		con->footerline = NULL;
	}
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

/*print text to a console*/
void Con_PrintCon (console_t *con, char *txt);




#ifdef QTERM
void QT_Update(void)
{
	char buffer[2048];
	DWORD ret;
	qterm_t *qt;
	qterm_t *prev = NULL;
	for (qt = qterms; qt; qt = (prev=qt)->next)
	{
		if (!qt->running)
		{
			if (Con_IsActive(qt->console))
				continue;

			Con_Destroy(qt->console);

			if (prev)
				prev->next = qt->next;
			else
				qterms = qt->next;

			CloseHandle(qt->pipein);
			CloseHandle(qt->pipeout);

			CloseHandle(qt->pipeinih);
			CloseHandle(qt->pipeoutih);
			CloseHandle(qt->process);

			Z_Free(qt);
			break;	//be lazy.
		}
		if (WaitForSingleObject(qt->process, 0) == WAIT_TIMEOUT)
		{
			if ((ret=GetFileSize(qt->pipeout, NULL)))
			{
				if (ret!=INVALID_FILE_SIZE)
				{
					ReadFile(qt->pipeout, buffer, sizeof(buffer)-32, &ret, NULL);
					buffer[ret] = '\0';
					Con_PrintCon(qt->console, buffer);
				}
			}
		}
		else
		{
			Con_PrintCon(qt->console, "Process ended\n");
			qt->running = false;
		}
	}
}

void QT_KeyPress(void *user, int key)
{
	qbyte k[2];
	qterm_t *qt = user;
	DWORD send = key;	//get around a gcc warning


	k[0] = key;
	k[1] = '\0';

	if (qt->running)
	{
		if (*k == '\r')
		{
//					*k = '\r';
//					WriteFile(qt->pipein, k, 1, &key, NULL);
//					Con_PrintCon(k, &qt->console);
			*k = '\n';
		}
		if (GetFileSize(qt->pipein, NULL)<512)
		{
			WriteFile(qt->pipein, k, 1, &send, NULL);
			Con_PrintCon(qt->console, k);
		}
	}
	return;
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
	Con_PrintCon(qt->console, "Started Process\n");
	Con_SetVisible(qt->console);

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
	key_lines[edit_line] = BZ_Realloc(key_lines[edit_line], 2);
	key_lines[edit_line][0] = ']';
	key_lines[edit_line][1] = 0;	// clear any typing
	key_linepos = 1;
}

void Con_History_Load(void)
{
	char line[8192];
	char *cr;
	vfsfile_t *file = FS_OpenVFS("conhistory.txt", "rb", FS_ROOT);

	for (edit_line=0 ; edit_line<=CON_EDIT_LINES_MASK ; edit_line++)
	{
		key_lines[edit_line] = BZ_Realloc(key_lines[edit_line], 2);
		key_lines[edit_line][0] = ']';
		key_lines[edit_line][1] = 0;
	}
	edit_line = 0;
	key_linepos = 1;

	if (file)
	{
		line[0] = ']';
		while (VFS_GETS(file, line+1, sizeof(line)-1))
		{
			//strip a trailing \r if its from windows.
			cr = line + strlen(line);
			if (cr > line && cr[-1] == '\r')
				cr[-1] = '\0';
			key_lines[edit_line] = BZ_Realloc(key_lines[edit_line], strlen(line)+1);
			strcpy(key_lines[edit_line], line);
			edit_line = (edit_line + 1) & CON_EDIT_LINES_MASK;
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
			VFS_PUTS(file, key_lines[line]+1);
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
#ifdef CSQC_DAT
	if (!(key_dest_mask & kdm_editor) && CSQC_ConsoleCommand("toggleconsole"))
	{
		Key_Dest_Remove(kdm_console);
		return;
	}
#endif

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
	Con_ClearCon(&con_main);
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
		Con_PrintCon(con, Cmd_Args());
		Con_PrintCon(con, "\n");
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

	con_initialized = true;
	Con_TPrintf ("Console initialized.\n");

//
// register our commands
//
	Cvar_Register (&con_notifytime, "Console controls");
	Cvar_Register (&con_centernotify, "Console controls");
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

	Cmd_AddCommand ("conecho", Cmd_ConEcho_f);
	Cmd_AddCommand ("conclear", Cmd_ConClear_f);
	Cmd_AddCommand ("conclose", Cmd_ConClose_f);
	Cmd_AddCommand ("conactivate", Cmd_ConActivate_f);

	Log_Init();
}

void Con_Shutdown(void)
{
	int i;

	Con_History_Save();

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

void Con_PrintConChars (console_t *con, conchar_t *c, int len)
{
	conline_t *oc;
	conchar_t *o;
	if (con->selstartline == con->current)
		con->selstartline = NULL;
	if (con->selendline == con->current)
		con->selendline = NULL;

	oc = con->current;
	if (oc->length+len > oc->maxlength)
	{
		oc->maxlength = (oc->length+len)+8;
		if (oc->maxlength < oc->length)
			oc->length = 0;	//don't crash from console line overflows.
		con->current = BZ_Realloc(con->current, sizeof(*con->current)+(oc->maxlength)*sizeof(conchar_t));
	}
	if (con->display == oc)
		con->display = con->current;
	if (con->oldest == oc)
		con->oldest = con->current;

	if (con->current->older)
		con->current->older->newer = con->current;
	o = (conchar_t *)(con->current+1)+con->current->length;
	memcpy(o, c, sizeof(*o) * len);
	con->current->length+=len;
}

void Con_PrintCon (console_t *con, char *txt)
{
	conchar_t expanded[4096];
	conchar_t *c;
	conline_t *oc;
	conline_t *reuse;

	COM_ParseFunString(con->defaultcharbits, txt, expanded, sizeof(expanded), con->parseflags);

	c = expanded;
	if (*c)
		con->unseentext = true;
	while (*c)
	{
		conchar_t *o;

		switch (*c & (CON_CHARMASK|CON_HIDDEN))	//include hidden so we don't do \r or \n on hidden chars, allowing them to be embedded in links and stuff.
		{
		case '\r':
			con->cr = true;
			break;
		case '\n':
			con->cr = false;
			reuse = NULL;
			while (con->linecount >= con_maxlines.ival)
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
			if (con->flags & CONF_NOTIMES)
				con->current->time = 0;
			else
				con->current->time = realtime;

#if defined(_WIN32) && !defined(NOMEDIA) && !defined(WINRT)
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

			if (!con->current->length && con_timestamps.ival)
			{
				char timeasc[64];
				conchar_t timecon[64], *timeconend;
				time_t rawtime;
				time (&rawtime);
				strftime(timeasc, sizeof(timeasc), con_timeformat.string, localtime (&rawtime));
				timeconend = COM_ParseFunString(con->defaultcharbits, timeasc, timecon, sizeof(timecon), false);
				Con_PrintConChars(con, timecon, timeconend-timecon);
			}

			if (con->selstartline == con->current)
				con->selstartline = NULL;
			if (con->selendline == con->current)
				con->selendline = NULL;

			oc = con->current;
			if (oc->length+1 > oc->maxlength)
			{
				oc->maxlength = (oc->length+1)+8;
				if (oc->maxlength < oc->length)
					oc->length = 0;	//don't crash from console line overflows.
				con->current = BZ_Realloc(con->current, sizeof(*con->current)+(oc->maxlength)*sizeof(conchar_t));
			}
			if (con->display == oc)
				con->display = con->current;
			if (con->oldest == oc)
				con->oldest = con->current;

			if (con->current->older)
				con->current->older->newer = con->current;
			o = (conchar_t *)(con->current+1)+con->current->length;
			*o = *c;
			con->current->length+=1;
			break;
		}
		c++;
	}
}

void Con_Print (char *txt)
{
	Con_PrintCon(&con_main, txt);	//client console
}

void Con_CycleConsole(void)
{
	while(1)
	{
		con_current = con_current->next;
		if (!con_current)
			con_current = &con_main;

		if (con_current->flags & CONF_HIDDEN)
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
extern char	outputbuf[8000];
void SV_FlushRedirect (void);
#endif

#define	MAXPRINTMSG	4096
// FIXME: make a buffer size safe vsprintf?
void VARGS Con_Printf (const char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	va_start (argptr,fmt);
	vsnprintf (msg,sizeof(msg), fmt,argptr);
	va_end (argptr);

#ifndef CLIENTONLY
	// add to redirected message
	if (sv_redirected)
	{
		if (strlen (msg) + strlen(outputbuf) > sizeof(outputbuf) - 1)
			SV_FlushRedirect ();
		strcat (outputbuf, msg);
		if (sv_redirected != -1)
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

void VARGS Con_SafePrintf (char *fmt, ...)
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
	extern cvar_t log_developer;
	if (!developer.value && !log_developer.value)
		return; // early exit
#endif

	va_start (argptr,fmt);
	vsnprintf (msg,sizeof(msg)-1, fmt,argptr);
	va_end (argptr);

	if (!developer.value)
		Con_Log(msg);
	else
	{
		Sys_Printf ("%s", msg);	// also echo to debugging console
		Con_PrintCon(&con_main, msg);
	}
}

/*description text at the bottom of the console*/
void Con_Footerf(qboolean append, char *fmt, ...)
{
	console_t *con = con_current;
	va_list		argptr;
	char		msg[MAXPRINTMSG];
	conchar_t	marked[MAXPRINTMSG], *markedend;
	int oldlen, newlen;
	conline_t *newf;

	va_start (argptr,fmt);
	vsnprintf (msg,sizeof(msg)-1, fmt,argptr);
	va_end (argptr);
	markedend = COM_ParseFunString(COLOR_YELLOW << CON_FGSHIFT, msg, marked, sizeof(marked), false);

	newlen = markedend - marked;
	if (append)
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
#ifdef _WIN32
	extern qboolean ActiveApp;
#endif
	int		i;
	int lhs, rhs;
	int p;
	unsigned char	*text, *fname = NULL;
	extern int con_commandmatch;
	conchar_t maskedtext[2048];
	conchar_t *endmtext;
	conchar_t *cursor;
	conchar_t *cchar;
	qboolean cursorframe;

	int x;

	if (!con->linebuffered)
		return y;	//fixme: draw any unfinished lines of the current console instead.

	y -= Font_CharHeight();

	if (!focused)
		return y;		// don't draw anything (always draw if not active)

	text = key_lines[edit_line];

	//copy it to an alternate buffer and fill in text colouration escape codes.
	//if it's recognised as a command, colour it yellow.
	//if it's not a command, and the cursor is at the end of the line, leave it as is,
	//	but add to the end to show what the compleation will be.

	i = text[key_linepos];
	text[key_linepos] = 0;
	cursor = COM_ParseFunString(CON_WHITEMASK, text, maskedtext, sizeof(maskedtext) - sizeof(maskedtext[0]), PFS_KEEPMARKUP | PFS_FORCEUTF8);
	text[key_linepos] = i;
	endmtext = COM_ParseFunString(CON_WHITEMASK, text, maskedtext, sizeof(maskedtext) - sizeof(maskedtext[0]), PFS_KEEPMARKUP | PFS_FORCEUTF8);
//	endmtext = COM_ParseFunString(CON_WHITEMASK, text+key_linepos, cursor, ((char*)maskedtext)+sizeof(maskedtext) - (char*)(cursor+1), PFS_KEEPMARKUP | PFS_FORCEUTF8);

	if ((char*)endmtext == (char*)(maskedtext-2) + sizeof(maskedtext))
		endmtext[-1] = CON_WHITEMASK | '+' | CON_NONCLEARBG;
	endmtext[1] = 0;

	i = 0;
	x = left;

	if (con->commandcompletion)
	{
		if (cl_chatmode.ival && (text[1] == '/' || (cl_chatmode.ival == 2 && Cmd_IsCommand(text+1))))
		{	//color the first token yellow, it's a valid command
			for (p = 1; (maskedtext[p]&CON_CHARMASK)>' '; p++)
				maskedtext[p] = (maskedtext[p]&CON_CHARMASK) | (COLOR_YELLOW<<CON_FGSHIFT);
		}
//		else
//			Plug_SpellCheckMaskedText(maskedtext+1, i-1, x, y, 8, si, con_current->linewidth);

		if (cursor == endmtext)	//cursor is at end
		{
			int cmdstart;
			cmdstart = text[1] == '/'?2:1;
			fname = Cmd_CompleteCommand(text+cmdstart, true, true, con_commandmatch, NULL);
			if (fname && strlen(fname) < 256)	//we can compleate it to:
			{
				for (p = min(strlen(fname), key_linepos-cmdstart); fname[p]>' '; p++)
					maskedtext[p+cmdstart] = (unsigned int)fname[p] | (COLOR_GREEN<<CON_FGSHIFT);
				if (p < key_linepos-cmdstart)
					p = key_linepos-cmdstart;
				maskedtext[p+cmdstart] = 0;
				maskedtext[p+cmdstart+1] = 0;
			}
		}
	}
//	else
//		Plug_SpellCheckMaskedText(maskedtext+1, i-1, x, y, 8, si, con_current->linewidth);

#ifdef _WIN32
	if (!ActiveApp)
		cursorframe = 0;
	else
#endif
		cursorframe = ((int)(realtime*con_cursorspeed)&1);

	for (lhs = 0, i = cursor - maskedtext-1; i >= 0; i--)
	{
		lhs += Font_CharWidth(maskedtext[i]);
	}
	for (rhs = 0, i = cursor - maskedtext; maskedtext[i]; i++)
	{
		rhs += Font_CharWidth(maskedtext[i]);
	}

	//put the cursor in the middle
	x = (right-left)/2 + left;
	//move the line to the right if there's not enough text to touch the right hand side
	if (x < right-rhs - Font_CharWidth(0xe000|11|CON_WHITEMASK))
		x = right - rhs - Font_CharWidth(0xe000|11|CON_WHITEMASK);
	//if the left hand side is on the right of the left point (overrides right alignment)
	if (x > lhs + left)
		x = lhs + left;

	lhs = x - lhs;
	for (cchar = maskedtext; cchar < cursor; cchar++)
	{
		lhs = Font_DrawChar(lhs, y, *cchar);
	}
	rhs = x;
	if (cursorframe)
	{
//		extern cvar_t com_parseutf8;
//		if (com_parseutf8.ival)
//			Font_DrawChar(rhs, y, (*cursor&~(CON_BGMASK|CON_FGMASK)) | (COLOR_BLUE<<CON_BGSHIFT) | CON_NONCLEARBG | CON_WHITEMASK);
//		else
			Font_DrawChar(rhs, y, 0xe000|11|CON_WHITEMASK);
	}
	else if (*cursor)
		Font_DrawChar(rhs, y, *cursor);
	rhs += Font_CharWidth(*cursor);
	for (cchar = cursor+1; *cchar; cchar++)
	{
		rhs = Font_DrawChar(rhs, y, *cchar);
	}

	/*if its getting completed to something, show some help about the command that is going to be used*/
	if (con->footerline)
	{
		y = Con_DrawConsoleLines(con, con->footerline, left, right, y, 0, selactive, selsx, selex, selsy, seley);
	}

	/*just above that, we have the tab completion list*/
	if (con_commandmatch && con_displaypossibilities.value)
	{
		conchar_t *end, *s;
		char *cmd;//, *desc;
		int cmdstart;
		size_t newlen;
		cmdstart = text[1] == '/'?2:1;
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

			s = (conchar_t*)(con->completionline+1);
//			if (desc)
//				end = COM_ParseFunString((COLOR_GREEN<<CON_FGSHIFT), va("^[^2/%s\\tip\\%s^]\t", cmd, desc), s+con->completionline->length, (con->completionline->maxlength-con->completionline->length)*sizeof(maskedtext[0]), true);
//			else
				end = COM_ParseFunString((COLOR_GREEN<<CON_FGSHIFT), va("^[^2/%s^]\t", cmd), s+con->completionline->length, (con->completionline->maxlength-con->completionline->length)*sizeof(maskedtext[0]), true);
			con->completionline->length = end - s;
		}

		if (con->completionline->length)
			y = Con_DrawConsoleLines(con, con->completionline, left, right, y, 0, selactive, selsx, selex, selsy, seley);
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
	conchar_t *c;
	conline_t *l;
	int lines=con->notif_l;
	int line;
	int x = con->notif_x, y = con->notif_y;
	int w = con->notif_w;

	int maxlines;
	float t;

	Font_BeginString(font_console, x, y, &x, &y);
	Font_Transform(con->notif_w, 0, &w, NULL);

	if (con->notif_l < 0)
		con->notif_l = 0;
	if (con->notif_l > NUM_CON_TIMES)
		con->notif_l = NUM_CON_TIMES;
	lines = maxlines = con->notif_l;

	if (x == 0 && y == 0 && con->notif_w == vid.width)
		y = Con_DrawProgress(0, w, 0);

	l = con->current;
	if (!l->length)
		l = l->older;
	for (; l && lines > con->notif_l-maxlines; l = l->older)
	{
		t = l->time;
		if (!t)
			continue; //hidden from notify
		t = realtime - t;
		if (t > con->notif_t)
			break;

		line = Font_LineBreaks((conchar_t*)(l+1), (conchar_t*)(l+1)+l->length, w, lines, starts, ends);
		if (!line && lines > 0)
		{
			lines--;
			starts[lines] = NULL;
			ends[lines] = NULL;
		}
		while(line --> 0 && lines > 0)
		{
			lines--;
			starts[lines] = starts[line];
			ends[lines] = ends[line];
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
		if (con_centernotify.value)
		{
			for (c = starts[lines]; c < ends[lines]; c++)
			{
				x += Font_CharWidth(*c);
			}
			x = (w - x) / 2;
		}
		Font_LineDraw(x, y, starts[lines], ends[lines]);

		y += Font_CharHeight();

		lines++;
	}

	Font_EndString(font_console);
}

void Con_DrawNotify (void)
{
	console_t *con;

	con_main.flags |= CONF_NOTIFY;
	/*keep the main console up to date*/
	con_main.notif_l = con_numnotifylines.ival;
	con_main.notif_w = vid.width;
	con_main.notif_t = con_notifytime.value;

	if (con_chat)
	{
		con_chat->notif_l = con_numnotifylines_chat.ival;
		con_chat->notif_w = vid.width - 64;
		con_chat->notif_y = vid.height - sb_lines - 8*4;
		con_chat->notif_t = con_notifytime_chat.value;
	}

	for (con = &con_main; con; con = con->next)
	{
		if (con->flags & CONF_NOTIFY)
			Con_DrawNotifyOne(con);
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
#ifdef RUNTIMELIGHTING
	extern model_t *lightmodel;
	extern int relitsurface;
#endif

	conchar_t			dlbar[1024];
	unsigned char	progresspercenttext[128];
	char *progresstext = NULL;
	char *txt;
	int x, tw;
	int i, j;
	int barwidth, barleft;
	float progresspercent = 0;
	*progresspercenttext = 0;

	// draw the download bar
	// figure out width
	if (cls.downloadmethod)
	{
		unsigned int count, total;
		qboolean extra;
		progresstext = cls.downloadlocalname;
		progresspercent = cls.downloadpercent;

		CL_GetDownloadSizes(&count, &total, &extra);

		if ((int)(realtime/2)&1 || total == 0)
			sprintf(progresspercenttext, " %5.1f%% (%ukbps)", progresspercent, CL_DownloadRate()/1000);
		else
		{
			sprintf(progresspercenttext, " %5.1f%% (%u%skb)", progresspercent, total/1024, extra?"+":"");
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
		for (i = 0; dlbar[i]; )
		{
			x += Font_CharWidth(dlbar[i]);
			i++;
		}

		//if the string is wider than a third of the screen
		if (x > (right - left)/3)
		{
			//truncate the file name and add ...
			x += 3*Font_CharWidth('.'|CON_WHITEMASK);
			while (x > (right - left)/3 && i > 0)
			{
				i--;
				x -= Font_CharWidth(dlbar[i]);
			}

			dlbar[i++] = '.'|CON_WHITEMASK;
			dlbar[i++] = '.'|CON_WHITEMASK;
			dlbar[i++] = '.'|CON_WHITEMASK;
			dlbar[i] = 0;
		}

		//i is the char index of the dlbar so far, x is the char width of it.

		//add a couple chars
		dlbar[i] = ':'|CON_WHITEMASK;
		x += Font_CharWidth(dlbar[i]);
		i++;
		dlbar[i] = ' '|CON_WHITEMASK;
		x += Font_CharWidth(dlbar[i]);
		i++;

		COM_ParseFunString(CON_WHITEMASK, progresspercenttext, dlbar+i, sizeof(dlbar)-i*sizeof(conchar_t), false);
		for (j = i, tw = 0; dlbar[j]; )
		{
			tw += Font_CharWidth(dlbar[j]);
			j++;
		}

		barwidth = (right-left) - (x + tw);

		//draw the right hand side
		x = right - tw;
		for (j = i; dlbar[j]; j++)
			x = Font_DrawChar(x, y, dlbar[j]);

		//draw the left hand side
		x = left;
		for (j = 0; j < i; j++)
			x = Font_DrawChar(x, y, dlbar[j]);

		//and in the middle we have lots of stuff

		barwidth -= (Font_CharWidth(0xe080|CON_WHITEMASK) + Font_CharWidth(0xe082|CON_WHITEMASK));
		x = Font_DrawChar(x, y, 0xe080|CON_WHITEMASK);
		barleft = x;
		for(;;)
		{
			if (x + Font_CharWidth(0xe081|CON_WHITEMASK) > barleft+barwidth)
				break;
			x = Font_DrawChar(x, y, 0xe081|CON_WHITEMASK);
		}
		x = Font_DrawChar(x, y, 0xe082|CON_WHITEMASK);

		Font_DrawChar(barleft+(barwidth*progresspercent)/100 - Font_CharWidth(0xe083|CON_WHITEMASK)/2, y, 0xe083|CON_WHITEMASK);

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

	con_mouseover = NULL;

	for (con = &con_main; con; con = con->next)
	{
		if (!(con->flags & CONF_HIDDEN))
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
			if (con->flags & CONF_HIDDEN)
				continue;
			txt = con->title;

			//yeah, om is an evil 1-frame delay. whatever
			end = COM_ParseFunString(CON_WHITEMASK, va("^&%c%i%s", ((con!=om)?'F':'B'), (con==con_current)+con->unseentext*4, txt), buffer, sizeof(buffer), false);

			lx = 0;
			for (lx = x, start = buffer; start < end; start++)
			{
				lx = Font_CharEndCoord(font_console, lx, *start);
			}
			if (lx > Font_ScreenWidth())
			{
				x = 0;
				y += h;
			}
			for (lx = x, start = buffer; start < end; start++)
			{
				Font_DrawChar(lx, y, *start);
				lx = Font_CharEndCoord(font_console, lx, *start);
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
#include "shader.h"
//draws the conline_t list bottom-up within the width of the screen until the top of the screen is reached.
//if text is selected, the selstartline globals will be updated, so make sure the lines persist or check them.
static int Con_DrawConsoleLines(console_t *con, conline_t *l, int sx, int ex, int y, int top, qboolean selactive, int selsx, int selex, int selsy, int seley)
{
	int linecount;
	int linelength;
	conchar_t *starts[64], *ends[sizeof(starts)/sizeof(starts[0])];
	conchar_t *s;
	int i;
	int x;

	if (l != con->completionline)
	if (l != con->footerline)
	if (l != con->current)
	{
		y -= 8;
	// draw arrows to show the buffer is backscrolled
		for (x = sx ; x<ex; )
			x = (Font_DrawChar (x, y, '^'|CON_WHITEMASK)-x)*4+x;
	}

	if (!selactive)
		selactive = 2;

	//deactivate the selection if the start and end is outside
	if (
		(selsx < sx && selex < sx) ||
		(selsx > ex && selex > ex) ||
		(selsy < top && seley < top) ||
		(selsy > y && seley > y)
		)
		selactive = false;

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
		selsy /= Font_CharHeight();
		seley /= Font_CharHeight();
		selsy--;
		seley--;

		//invert the selections to make sense, text-wise
		if (selsy == seley)
		{
			//single line selected backwards
			if (selex < selsx)
			{
				x = selex;
				selex = selsx;
				selsx = x;
			}
		}
		if (seley < selsy)
		{	//selection goes upwards
			x = selsy;
			selsy = seley;
			seley = x;

			x = selex;
			selex = selsx;
			selsx = x;
		}
		selsy *= Font_CharHeight();
		seley *= Font_CharHeight();
		selsy += y;
		seley += y;
	}

	if (l && l == con->current && l->length == 0)
		l = l->older;
	for (; l; l = l->older)
	{
		shader_t *pic = NULL;
		int picw=0, pich=0;
		s = (conchar_t*)(l+1);

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
						pic = R_RegisterPic(imgname);
						if (pic)
						{
							imgname = Info_ValueForKey(linkinfo, "w");
							if (*imgname)
								picw = atoi(imgname);
							else if (pic->width)
								picw = (pic->width * vid.pixelwidth) / vid.width;
							else
								picw = 64;
							imgname = Info_ValueForKey(linkinfo, "h");
							if (*imgname)
								pich = atoi(imgname);
							else if (pic->height)
								pich = (pic->height * vid.pixelheight) / vid.height;
							else
								pich = 64;
						}
					}
					break;
				}
				linkinfolen += unicode_encode(linkinfo+linkinfolen, (*e & CON_CHARMASK), sizeof(linkinfo)-1-linkinfolen, true);
			}
		}

		linecount = Font_LineBreaks(s, s+l->length, ex-sx-picw, sizeof(starts)/sizeof(starts[0]), starts, ends);
		//if Con_LineBreaks didn't find any lines at all, then it was an empty line, and we need to ensure that its still drawn
		if (linecount == 0)
		{
			linecount = 1;
			starts[0] = ends[0] = NULL;
		}

		if (pic)
		{
			float szx = (float)vid.width / vid.pixelwidth;
			float szy = (float)vid.height / vid.pixelheight;
			int texth = (linecount) * Font_CharHeight();
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
		}

		l->lines = linecount;

		while(linecount-- > 0)
		{
			s = starts[linecount];
			linelength = ends[linecount] - s;

			y -= Font_CharHeight();

			if (top && y < top)
				break;

			if (selactive)
			{
				if (y >= selsy)
				{
					if (y <= seley)
					{
						int sstart;
						int send;
						sstart = sx+picw;
						send = sstart;
						for (i = 0; i < linelength; i++)
							send = Font_CharEndCoord(font_console, send, s[i]);

						//show something on blank lines
						if (send == sstart)
							send = Font_CharEndCoord(font_console, send, ' ');

						if (y >= seley)
						{
							send = sstart;
							for (i = 0; i < linelength; )
							{
								send = Font_CharEndCoord(font_console, send, s[i++]);

								if (send > selex)
									break;
							}

							con->selendline = l;
							if (s)
								con->selendoffset = (s+i) - (conchar_t*)(l+1);
							else
								con->selendoffset = 0;
						}
						if (y <= selsy)
						{
							for (i = 0; i < linelength; i++)
							{
								x = Font_CharEndCoord(font_console, sstart, s[i]);
								if (x > selsx)
									break;
								sstart = x;
							}

							con->selstartline = l;
							if (s)
								con->selstartoffset = (s+i) - (conchar_t*)(l+1);
							else
								con->selstartoffset = 0;
						}
						if (selactive == 1)
						{
							R2D_ImagePaletteColour(0, 1.0);
							R2D_FillBlock((sstart*vid.width)/(float)vid.rotpixelwidth, (y*vid.height)/(float)vid.rotpixelheight, ((send - sstart)*vid.width)/(float)vid.rotpixelwidth, (Font_CharHeight()*vid.height)/(float)vid.rotpixelheight);
						}
					}
				}
			}
			R2D_ImageColours(1.0, 1.0, 1.0, 1.0);

			x = sx + picw;
			Font_LineDraw(x, y, s, s+linelength);

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
	int top;
	qboolean haveprogress;

	if (lines <= 0)
		return;

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

	y = Con_DrawConsoleLines(con_current, l, sx, ex, y, top, selactive, selsx, selex, selsy, seley);

	if (!haveprogress && lines == vid.height)
	{
		char *version = version_string();
		int i;
		Font_BeginString(font_console, vid.width, lines, &x, &y);
		y -= Font_CharHeight();
		for (i = 0; version[i]; i++)
			x -= Font_CharWidth(version[i] | CON_WHITEMASK|CON_HALFALPHA);
		for (i = 0; version[i]; i++)
			x = Font_DrawChar(x, y, version[i] | CON_WHITEMASK|CON_HALFALPHA);
	}

	Font_EndString(font_console);

	if (con_current->selstartline)
	{
		char *mouseover = Con_CopyConsole(false, true);
		if (mouseover)
		{
			char *end = strstr(mouseover, "^]");
			char *info = strchr(mouseover, '\\');
			char *key;
			if (!info)
				info = "";
			if (end)
				*end = 0;
#ifdef PLUGINS
			if (!Plug_ConsoleLinkMouseOver(mousecursor_x, mousecursor_y, mouseover+2, info))
#endif
			{
				float x = mousecursor_x+8;
				float y = mousecursor_y+8;
				float ih = 0;
				key = Info_ValueForKey(info, "tipimg");
				if (*key)
				{
					shader_t *s = R2D_SafeCachePic(key);
					if (s)
					{
						R2D_Image(x, y, s->width, s->height, 0, 0, 1, 1, s);
						ih = s->height;
						x += s->width + 8;
					}
				}
				key = Info_ValueForKey(info, "tip");
				if (*key)
				{
					//FIXME: draw a proper background.
					//FIXME: support line breaks.
					conchar_t buffer[2048], *starts[8], *ends[8];
					int lines, i, px, py;
					Font_BeginString(font_console, x, y, &px, &py);
					lines = Font_LineBreaks(buffer, COM_ParseFunString(CON_WHITEMASK|CON_NONCLEARBG, key, buffer, sizeof(buffer), false), 256, 8, starts, ends);
					ih = max(Font_CharHeight()*lines, ih)/2;
					y += ih - (Font_CharHeight()*lines)/2;
					Font_BeginString(font_console, x, y, &px, &py);
					for (i = 0; i < lines; i++)
					{
						Font_LineDraw(px, py, starts[i], ends[i]);
						py += Font_CharHeight();
					}
					Font_EndString(font_console);
				}
			}
		}
		Z_Free(mouseover);
	}
}

void Con_DrawOneConsole(console_t *con, struct font_s *font, float fx, float fy, float fsx, float fsy)
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
		con->selstartline = NULL;
		con->selendline = NULL;
		selactive = Key_GetConsoleSelectionBox(con, &selsx, &selsy, &selex, &seley);

		Font_BeginString(font, selsx, selsy, &selsx, &selsy);
		Font_BeginString(font, selex, seley, &selex, &seley);
		selsx += x;
		selsy += y;
		selex += x;
		seley += y;
	}

	sy = Con_DrawInput (con, con->flags & CONF_KEYFOCUSED, x, sx, sy, selactive, selsx, selex, selsy, seley);

	if (!con->display)
		con->display = con->current;
	Con_DrawConsoleLines(con, con->display, x, sx, sy, y, selactive, selsx, selex, selsy, seley);

	Font_EndString(font);
}


char *Con_CopyConsole(qboolean nomarkup, qboolean onlyiflink)
{
	console_t *con = con_current;
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
				if (uc == ' ' || uc == '\t')
				{
					cur++;
					break;
				}
				if (*cur == CON_LINKSTART)
					break;
			}
			while (finalendoffset < con->selendline->length)
			{
				uc = (((conchar_t*)(l+1))[finalendoffset] & CON_CHARMASK);
				if (uc != ' ' && uc != '\t' && ((conchar_t*)(l+1))[finalendoffset] != CON_LINKEND)
					finalendoffset++;
				else
					break;
			}
		}
	}
	
	//scan backwards to find any link enclosure
	for(lend = cur; lend >= (conchar_t*)(l+1); lend--)
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
	if (*cur == CON_LINKSTART)
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

		outlen = COM_DeFunString(cur, lend, result + outlen, maxlen - outlen, nomarkup) - result;

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
#ifdef _WIN32
		result[outlen++] = '\r';
#endif
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
	Con_Printf("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n");

	Con_Printf ("%s", text);

	Con_Printf ("Press a key.\n");
	Con_Printf("\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n");

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
