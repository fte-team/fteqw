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
console_t	*con_current;			// point to either con_main

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
cvar_t		cl_chatmode = SCVAR("cl_chatmode", "2");

#define	NUM_CON_TIMES 24
float		con_times[NUM_CON_TIMES];	// realtime time the line was generated
								// for transparent notify lines

//int			con_vislines;
int			con_notifylines;		// scan lines to clear for notify lines

#define		MAXCMDLINE	256
extern	unsigned char	key_lines[32][MAXCMDLINE];
extern	int		edit_line;
extern	int		key_linepos;
		

qboolean	con_initialized;

void Con_ResizeCon (console_t *con);

qboolean Con_IsActive (console_t *con)
{
	return (con == con_current) | (con->unseentext*2);
}
void Con_Destroy (console_t *con)
{
	console_t *prev;

	if (con == &con_main)
		return;

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
console_t *Con_FindConsole(char *name)
{
	console_t *con;
	for (con = &con_main; con; con = con->next)
	{
		if (!strcmp(con->name, name))
			return con;
	}
	return NULL;
}
console_t *Con_Create(char *name)
{
	console_t *con;
	con = Z_Malloc(sizeof(console_t));
	Q_strncpyz(con->name, name, sizeof(con->name));

	Con_ResizeCon(con);
	con->next = con_main.next;
	con_main.next = con;

	return con;
}
void Con_SetActive (console_t *con)
{
	con_current = con;
}
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

	qt->console = Con_Create("QTerm");
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
	key_lines[edit_line][1] = 0;	// clear any typing
	key_linepos = 1;
}

/*
================
Con_ToggleConsole_f
================
*/
void Con_ToggleConsole_f (void)
{
	SCR_EndLoadingPlaque();
	Key_ClearTyping ();

	if (key_dest == key_console)
	{
		if (m_state)
			key_dest = key_menu;
		else
			key_dest = key_game;
	}
	else
		key_dest = key_console;
	
	Con_ClearNotify ();
}

/*
================
Con_ToggleChat_f
================
*/
void Con_ToggleChat_f (void)
{
	Key_ClearTyping ();

	if (key_dest == key_console)
	{
		if (cls.state == ca_active)
			key_dest = key_game;
	}
	else
		key_dest = key_console;
	
	Con_ClearNotify ();
}

/*
================
Con_Clear_f
================
*/
void Con_Clear_f (void)
{
	int i;
	//wide chars, not standard ascii
	for (i = 0; i < sizeof(con_main.text)/sizeof(conchar_t); i++)
	{
		con_main.text[i] = CON_DEFAULTCHAR;
//	Q_memset (con_main.text, ' ', sizeof(con_main.text));
	}
}

						
/*
================
Con_ClearNotify
================
*/
void Con_ClearNotify (void)
{
	int		i;
	
	for (i=0 ; i<NUM_CON_TIMES ; i++)
		con_times[i] = 0;
}

						
/*
================
Con_MessageMode_f
================
*/
void Con_MessageMode_f (void)
{
	chat_team = false;
	key_dest = key_message;
}

/*
================
Con_MessageMode2_f
================
*/
void Con_MessageMode2_f (void)
{
	chat_team = true;
	key_dest = key_message;
}

/*
================
Con_Resize

================
*/
void Con_ResizeCon (console_t *con)
{
	int		i, j, width, oldwidth, oldtotallines, numlines, numchars;
	conchar_t tbuf[CON_TEXTSIZE];	

	if (scr_chatmode == 2)
		width = (vid.width >> 4) - 2;
	else
		width = (vid.width >> 3) - 2;

	if (width == con->linewidth)
		return;

	if (width < 1)			// video hasn't been initialized yet
	{
		width = 38;
		con->linewidth = width;
		con->totallines = CON_TEXTSIZE / con->linewidth;
		for (i = 0; i < CON_TEXTSIZE; i++)
			con->text[i] = CON_DEFAULTCHAR;
//		Q_memset (con->text, ' ', sizeof(con->text));		
	}
	else
	{
		oldwidth = con->linewidth;
		con->linewidth = width;
		oldtotallines = con->totallines;
		con->totallines = CON_TEXTSIZE / con->linewidth;
		numlines = oldtotallines;

		if (con->totallines < numlines)
			numlines = con->totallines;

		numchars = oldwidth;
	
		if (con->linewidth < numchars)
			numchars = con->linewidth;

		Q_memcpy (tbuf, con->text, sizeof(con->text));
		for (i = 0; i < sizeof(con->text)/sizeof(conchar_t); i++)
			con->text[i] = CON_DEFAULTCHAR;
//		Q_memset (con->text, ' ', sizeof(con->text));

		for (i=0 ; i<numlines ; i++)
		{
			for (j=0 ; j<numchars ; j++)
			{
				con->text[(con->totallines - 1 - i) * con->linewidth + j] =
						tbuf[((con->current - i + oldtotallines) %
							  oldtotallines) * oldwidth + j];
			}
		}

		Con_ClearNotify ();
	}

	con->current = con->totallines - 1;
	con->display = con->current;
}
					
/*
================
Con_CheckResize

If the line width has changed, reformat the buffer.
================
*/
void Con_CheckResize (void)
{
	console_t *c;
	for (c = &con_main; c; c = c->next)
		Con_ResizeCon (c);
}

void Con_ForceActiveNow(void)
{
	key_dest = key_console;
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
	con_main.linebuffered = Con_ExecuteLine;
	con_main.commandcompletion = true;
	con_main.linewidth = -1;
	Con_CheckResize ();
	
	Con_Printf ("Console initialized.\n");

//
// register our commands
//
	Cvar_Register (&con_notifytime, "Console controls");
	Cvar_Register (&con_centernotify, "Console controls");
	Cvar_Register (&con_numnotifylines, "Console controls");
	Cvar_Register (&con_displaypossibilities, "Console controls");
	Cvar_Register (&cl_chatmode, "Console controls");

	Cmd_AddCommand ("toggleconsole", Con_ToggleConsole_f);
	Cmd_AddCommand ("togglechat", Con_ToggleChat_f);
	Cmd_AddCommand ("messagemode", Con_MessageMode_f);
	Cmd_AddCommand ("messagemode2", Con_MessageMode2_f);
	Cmd_AddCommand ("clear", Con_Clear_f);
#ifdef QTERM
	Cmd_AddCommand ("qterm", Con_QTerm_f);
#endif
	con_initialized = true;

	Log_Init();
}


/*
===============
Con_Linefeed
===============
*/
void Con_Linefeed (console_t *con)
{
	int i, min, max;
	con->x = 0;
	if (con->display == con->current)
		con->display++;
	con->current++;

	min = (con->current%con->totallines)*con->linewidth;
	max = min + con->linewidth;
	for (i = min; i < max; i++)
		con->text[i] = CON_DEFAULTCHAR;

//	Q_memset (&con->text[(con->current%con_totallines)*con_linewidth]
//	, ' ', con_linewidth*sizeof(unsigned short));
}

/*
================
Con_Print

Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be logged to disk
If no console is visible, the notify window will pop up.
================
*/

void Con_PrintCon (console_t *con, char *txt)
{
	conchar_t expanded[4096];
	conchar_t *c;
	int		y;
	int		l;
	static int	cr;

	COM_ParseFunString(CON_WHITEMASK, txt, expanded, sizeof(expanded));

	c = expanded;
	while (*c)
	{
		if (*c&CON_CHARMASK=='\t')
			*c = (*c&~CON_CHARMASK)|' ';

	// count word length
		for (l=0 ; l< con->linewidth ; l++)
			if ( c[l]&CON_CHARMASK <= ' ')
				break;

	// word wrap
		if (l != con->linewidth && (con->x + l > con->linewidth) )
			con->x = 0;

		if (cr)
		{
			con->current--;
			cr = false;
		}

		
		if (!con->x)
		{
			Con_Linefeed (con);
		// mark time for transparent overlay
			con_times[con->current % NUM_CON_TIMES] = realtime;
		}

		switch (*c & (CON_CHARMASK&~CON_HIGHCHARSMASK))
		{
		case '\n':
			con->x = 0;
			break;

		case '\r':
			con->x = 0;
			cr = 1;
			break;

		default:	// display character and advance
			y = con->current % con->totallines;
			con->text[y*con->linewidth+con->x] = *c;			
			con->x++;
			if (con->x >= con->linewidth)
				con->x = 0;
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
	con_current = con_current->next;
	if (!con_current)
		con_current = &con_main;
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
/*
	if (con != &con_main)
		return;

// update the screen immediately if the console is displayed
	if (cls.state != ca_active && !filmactive)
#ifndef CLIENTONLY
	if (progfuncs != svprogfuncs)	//cover our back - don't do rendering stuff that will change this
#endif
	{
	// protect against infinite loop if something in SCR_UpdateScreen calls
	// Con_Printd
		if (!inupdate)
		{
			inupdate = true;
			SCR_UpdateScreen ();
			inupdate = false;
		}
	}
*/
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
	char *fmt = languagetext[text][cls.language];
	
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
	char *fmt = languagetext[text][cls.language];
	
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
void VARGS Con_DPrintf (char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];
	extern cvar_t log_developer;
		
	if (!developer.value && !log_developer.value)
		return; // early exit

	va_start (argptr,fmt);
	vsnprintf (msg,sizeof(msg)-1, fmt,argptr);
	va_end (argptr);

	if (!developer.value)
		Con_Log(msg);
	else
		Con_Printf("%s", msg);
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
================
*/
void Con_DrawInput (void)
{
#ifdef _WIN32
	extern qboolean ActiveApp;
#endif
	int		y;
	int		i;
	int p;
	unsigned char	*text, *fname;
	extern int con_commandmatch;
	conchar_t maskedtext[MAXCMDLINE];

	conchar_t mask=CON_WHITEMASK;
	int maskstack[4];
	int maskstackdepth = 0;

	int si, x;

	if (key_dest != key_console && con_current->vislines != vid.height)
		return;		// don't draw anything (always draw if not active)

	if (!con_current->linebuffered)
		return;	//fixme: draw any unfinished lines of the current console instead.

	text = key_lines[edit_line];

	//copy it to an alternate buffer and fill in text colouration escape codes.
	//if it's recognised as a command, colour it yellow.
	//if it's not a command, and the cursor is at the end of the line, leave it as is,
	//	but add to the end to show what the compleation will be.

	for (i = 0; text[i]; i++)
	{
		if (text[i] == '^')	//is this an escape code?
		{
			if (text[i+1]>='0' && text[i+1]<='9')
				mask = q3codemasks[text[i+1]-'0'] | (mask&~CON_Q3MASK); //change colour only.
			else if (text[i+1] == '&') // extended code
			{
				if (isextendedcode(text[i+2]) && isextendedcode(text[i+3]))
				{
					if (text[i+2] == '-') // default for FG
						mask = (COLOR_WHITE << CON_FGSHIFT) | (mask&~CON_FGMASK);
					else if (text[i+2] >= 'A')
						mask = ((text[i+2] - ('A' - 10)) << CON_FGSHIFT) | (mask&~CON_FGMASK);
					else
						mask = ((text[i+2] - '0') << CON_FGSHIFT) | (mask&~CON_FGMASK);
					if (text[i+3] == '-') // default (clear) for BG
						mask &= ~CON_BGMASK & ~CON_NONCLEARBG;
					else if (text[i+3] >= 'A')
						mask = ((text[i+3] - ('A' - 10)) << CON_BGSHIFT) | (mask&~CON_BGMASK) | CON_NONCLEARBG;
					else
						mask = ((text[i+3] - '0') << CON_BGSHIFT) | (mask&~CON_BGMASK) | CON_NONCLEARBG;
				}
			}
			else if (text[i+1] == 'b')
				mask ^= CON_BLINKTEXT;
			else if (text[i+1] == 'a')	//alternate
				mask ^= CON_2NDCHARSETTEXT;
			else if (text[i+1] == 'h') // half-alpha
				mask ^= CON_HALFALPHA;
			else if (text[i+1] == 's')	//store on stack (it's great for names)
			{
				if (maskstackdepth < sizeof(maskstack)/sizeof(maskstack[0]))
				{
					maskstack[maskstackdepth] = mask;
					maskstackdepth++;
				}
			}
			else if (text[i+1] == 'r')	//restore from stack (it's great for names)
			{
				if (maskstackdepth)
				{
					maskstackdepth--;
					mask = maskstack[maskstackdepth];
				}
			}
			else if (text[i+1] == '^')//next does nothing either
			{
				maskedtext[i] = (unsigned char)text[i] | mask;
				i++;
			}
		}

		maskedtext[i] = (unsigned char)text[i] | mask;
	}	//that's the default compleation applied

	maskedtext[i] = '\0';
	maskedtext[i+1] = '\0';	//just in case i==key_linepos

	x = 8;
	y = con_current->vislines-22;

	if (i >= con_current->linewidth)	//work out the start point
		si = i - con_current->linewidth;
	else
		si = 0;


	if (con_current->commandcompletion)
	{
		if (text[1] == '/' || Cmd_IsCommand(text+1))
		{	//color the first token yellow, it's a valid command
			for (p = 1; (maskedtext[p]&255)>' '; p++)
				maskedtext[p] = (maskedtext[p]&255) | (COLOR_YELLOW<<CON_FGSHIFT);
		}
		else
			Plug_SpellCheckMaskedText(maskedtext+1, i-1, x, y, 8, si, con_current->linewidth);

		if (key_linepos == i)	//cursor is at end
		{
			int cmdstart;
			cmdstart = text[1] == '/'?2:1;
			fname = Cmd_CompleteCommand(text+cmdstart, true, true, con_commandmatch);
			if (fname)	//we can compleate it to:
			{
				for (p = i-cmdstart; fname[p]>' '; p++)
					maskedtext[p+cmdstart] = (unsigned char)fname[p] | (COLOR_GREEN<<CON_FGSHIFT);
				maskedtext[p+cmdstart] = '\0';
			}
		}
	}
	else
		Plug_SpellCheckMaskedText(maskedtext+1, i-1, x, y, 8, si, con_current->linewidth);

#ifdef _WIN32
	if (ActiveApp)
#endif
	if (((int)(realtime*con_cursorspeed)&1))
	{
		maskedtext[key_linepos] = 11|CON_WHITEMASK;	//make it blink
	}

	for (i=0,p=0; x<=con_current->linewidth*8 ; p++)	//draw it
	{
		if (!maskedtext[p])
			break;
		if (si <= i)
		{
			Draw_ColouredCharacter ( x, con_current->vislines - 22, maskedtext[p]);
			x+=8;
		}
		i++;
	}
}

/*
================
Con_DrawNotify

Draws the last few lines of output transparently over the game top
================
*/
void Con_DrawNotify (void)
{
	int		x, v;
	conchar_t *text;
	int		i;
	float	time;
	char	*s;
	int		skip;
	int maxlines;
	int inset;

	int mask=CON_WHITEMASK;
	int maskstack[4];
	int maskstackdepth = 0;

	console_t *con = &con_main;	//notify text should never use a chat console

#ifdef QTERM
	if (qterms)
		QT_Update();
#endif

	maxlines = con_numnotifylines.value;
	if (maxlines < 0)
		maxlines = 0;
	if (maxlines > NUM_CON_TIMES)
		maxlines = NUM_CON_TIMES;

	v = 0;
	if (con_centernotify.value)
	{
		for (i= con->current-maxlines+1 ; i<=con->current ; i++)
		{
			if (i < 0)
				continue;
			time = con_times[i % NUM_CON_TIMES];
			if (time == 0)
				continue;
			time = realtime - time;
			if (time > con_notifytime.value)
				continue;
			text = con->text + (i % con->totallines)*con->linewidth;
			
			clearnotify = 0;
			scr_copytop = 1;

			for (x = con->linewidth-1 ; x >= 0 ; x--)
			{
				if ((text[x]&0xff) != ' ')
					break;
			}
			inset = con->linewidth*4-x*4;

			for (x = 0 ; x < con->linewidth ; x++)
				Draw_ColouredCharacter ( inset + ((x+1)<<3), v, text[x]);

			v += 8;
		}

	}
	else
	{
		for (i= con->current-maxlines+1 ; i<=con->current ; i++)
		{
			if (i < 0)
				continue;
			time = con_times[i % NUM_CON_TIMES];
			if (time == 0)
				continue;
			time = realtime - time;
			if (time > con_notifytime.value)
				continue;
			text = con->text + (i % con->totallines)*con->linewidth;
			
			clearnotify = 0;
			scr_copytop = 1;

			for (x = 0 ; x < con->linewidth ; x++)
				Draw_ColouredCharacter ( (x+1)<<3, v, text[x]);

			v += 8;
		}
	}


	if (key_dest == key_message)
	{
		clearnotify = 0;
		scr_copytop = 1;
	
		if (chat_team)
		{
			Draw_FunString (8, v, "say_team:");	//make sure we get coloration right
			skip = 11;
		}
		else
		{
			Draw_FunString (8, v, "say:");
			skip = 5;
		}

		s = chat_buffer;
		if (chat_bufferlen > (vid.width>>3)-(skip+1))
			s += chat_bufferlen - ((vid.width>>3)-(skip+1));
		x = chat_buffer - s;

		while(x < 0)
		{
			if (s[x] == '^')
			{
				if (s[x+1]>='0' && s[x+1]<='9')
					mask = q3codemasks[s[x+1]-'0'] | (mask&~CON_Q3MASK); //change colour only.
				else if (s[x+1] == '&') // extended code
				{
					if (isextendedcode(s[x+2]) && isextendedcode(s[x+3]))
					{
						if (s[x+2] == '-') // default for FG
							mask = (COLOR_WHITE << CON_FGSHIFT) | (mask&~CON_FGMASK);
						else if (s[x+2] >= 'A')
							mask = ((s[x+2] - ('A' - 10)) << CON_FGSHIFT) | (mask&~CON_FGMASK);
						else
							mask = ((s[x+2] - '0') << CON_FGSHIFT) | (mask&~CON_FGMASK);
						if (s[x+3] == '-') // default (clear) for BG
							mask &= ~CON_BGMASK & ~CON_NONCLEARBG;
						else if (s[x+3] >= 'A')
							mask = ((s[x+3] - ('A' - 10)) << CON_BGSHIFT) | (mask&~CON_BGMASK) | CON_NONCLEARBG;
						else
							mask = ((s[x+3] - '0') << CON_BGSHIFT) | (mask&~CON_BGMASK) | CON_NONCLEARBG;
					}
				}
				else if (s[x+1] == 'b')
					mask ^= CON_BLINKTEXT;
				else if (s[x+1] == 'a')	//alternate
					mask ^= CON_2NDCHARSETTEXT;
				else if (s[x+1] == 'h') //half-alpha
					mask ^= CON_HALFALPHA;
				else if (s[x+1] == 's')	//store on stack (it's great for names)
				{
					if (maskstackdepth < sizeof(maskstack)/sizeof(maskstack[0]))
					{
						maskstack[maskstackdepth] = mask;
						maskstackdepth++;
					}
				}
				else if (s[x+1] == 'r')	//restore from stack (it's great for names)
				{
					if (maskstackdepth)
					{
						maskstackdepth--;
						mask = maskstack[maskstackdepth];
					}
				}
			}
			x++;
		}

		while(s[x])
		{
			if (s[x] == '^')
			{
				if (s[x+1]>='0' && s[x+1]<='9')
					mask = q3codemasks[s[x+1]-'0'] | (mask&~CON_Q3MASK);	//change colour only.
				else if (s[x+1] == '&') // extended code
				{
					if (isextendedcode(s[x+2]) && isextendedcode(s[x+3]))
					{
						if (s[x+2] == '-') // default for FG
							mask = (COLOR_WHITE << CON_FGSHIFT) | (mask&~CON_FGMASK);
						else if (s[x+2] >= 'A')
							mask = ((s[x+2] - ('A' - 10)) << CON_FGSHIFT) | (mask&~CON_FGMASK);
						else
							mask = ((s[x+2] - '0') << CON_FGSHIFT) | (mask&~CON_FGMASK);
						if (s[x+3] == '-') // default (clear) for BG
							mask &= ~CON_BGMASK & ~CON_NONCLEARBG;
						else if (s[x+3] >= 'A')
							mask = ((s[x+3] - ('A' - 10)) << CON_BGSHIFT) | (mask&~CON_BGMASK) | CON_NONCLEARBG;
						else
							mask = ((s[x+3] - '0') << CON_BGSHIFT) | (mask&~CON_BGMASK) | CON_NONCLEARBG;
					}
				}
				else if (s[x+1] == 'b')
					mask ^= CON_BLINKTEXT;
				else if (s[x+1] == 'a')	//alternate
					mask ^= CON_2NDCHARSETTEXT;
				else if (s[x+1] == 'h') //halfalpha
					mask ^= CON_HALFALPHA;
				else if (s[x+1] == 's')	//store on stack (it's great for names)
				{
					if (maskstackdepth < sizeof(maskstack)/sizeof(maskstack[0]))
					{
						maskstack[maskstackdepth] = mask;
						maskstackdepth++;
					}
				}
				else if (s[x+1] == 'r')	//restore from stack (it's great for names)
				{
					if (maskstackdepth)
					{
						maskstackdepth--;
						mask = maskstack[maskstackdepth];
					}
				}
			}

			Draw_ColouredCharacter ( (x+skip)<<3, v, s[x]|mask);
			x++;
		}
		Draw_ColouredCharacter ( (x+skip)<<3, v, (10+((int)(realtime*con_cursorspeed)&1))|CON_WHITEMASK);
		v += 8;
	}

	if (v > con_notifylines)
		con_notifylines = v;
}

//send all the stuff that was con_printed to sys_print. 
//This is so that system consoles in windows can scroll up and have all the text.
void Con_PrintToSys(void)	
{
	int line, row, x, spc, content;
	conchar_t *text;
	console_t *curcon = &con_main;

	content = 0;
	row = curcon->current - curcon->totallines+1;
	for (line = 0; line < curcon->totallines-1; line++, row++)
	{
		text = curcon->text + (row % curcon->totallines)*curcon->linewidth;
		spc = 0;
		for (x = 0; x < curcon->linewidth; x++)
		{
			if (((qbyte)text[x]&255) == ' ')
				spc++; // track spaces but don't print yet
			else
			{
				content = 1; // start printing blank lines
				for (; spc > 0; spc--)
					Sys_Printf(" "); // print leading spaces
				Sys_Printf("%c", (qbyte)text[x]&255);
			}
		}
		if (content)
			Sys_Printf("\n");
	}
}

/*
================
Con_DrawConsole

Draws the console with the solid background
================
*/
void Con_DrawConsole (int lines, qboolean noback)
{
	int				i, j, x, y;
	float n;
	int				rows;
	conchar_t *text;
	char *txt;
	int				row;
	unsigned char			dlbar[1024];
	unsigned char	progresspercenttext[128];
	char *progresstext;
	int progresspercent;
	extern qboolean scr_con_forcedraw;

#ifdef RUNTIMELIGHTING
	extern model_t *lightmodel;
	extern int relitsurface;
#endif

	console_t *curcon = con_current;
	
	if (lines <= 0)
		return;

#ifdef QTERM
	if (qterms)
		QT_Update();
#endif

// draw the background
	if (!noback)
		Draw_ConsoleBackground (0, lines, scr_con_forcedraw);

	con_current->unseentext = false;

// draw the text
	con_current->vislines = lines;
	
// changed to line things up better
	rows = (lines-22)>>3;		// rows of text to draw

	y = lines - 30;

	if (lines == scr_conlines && con_main.next)
	{
		console_t *con = con_current;
		rows--;
		for (x = 0, con = &con_main; con; con = con->next)
		{
			if (con == &con_main)
				txt = "MAIN";
			else
				txt = con->name;

			if (x != 0 && x+(strlen(txt)+1)*8 > curcon->linewidth*8)
			{
				x = 0;
				rows--;
			}
			Draw_FunString(x, 0, va("^&F%i%s", (con==con_current)+con->unseentext*4, txt));
			x+=(strlen(txt)+1)*8;
		}
		rows--;
	}

	Key_ConsoleDrawSelectionBox();

// draw from the bottom up
	if (curcon->display != curcon->current)
	{
	// draw arrows to show the buffer is backscrolled
		for (x=0 ; x<curcon->linewidth ; x+=4)
			Draw_Character ( (x+1)<<3, y, '^');
	
		y -= 8;
		rows--;
	}

	row = curcon->display;
	for (i=0 ; i<rows ; i++, y-=8, row--)
	{
		if (row < 0)
			break;
		if (curcon->current - row >= curcon->totallines)
			break;		// past scrollback wrap point
			
		text = curcon->text + (row % curcon->totallines)*curcon->linewidth;

		for (x=0 ; x<curcon->linewidth ; x++)
		{
			if (text[x])
				Draw_ColouredCharacter ( (x+1)<<3, y, text[x]);
		}
	}	

	progresstext = NULL;
	progresspercent = 0;
	*progresspercenttext = 0;
	
	// draw the download bar
	// figure out width
	if (cls.downloadmethod)
	{
		unsigned int count, total;
		qboolean extra;
		progresstext = cls.downloadlocalname;
		progresspercent = cls.downloadpercent;

		if ((int)(realtime/2)&1)
			sprintf(progresspercenttext, " %02d%% (%ukbps)", progresspercent, CL_DownloadRate()/1000);
		else
		{
			CL_GetDownloadSizes(&count, &total, &extra);
			if (total == 0)
			{
				//just show progress
				sprintf(progresspercenttext, " %02d%%", progresspercent);
			}
			else
			{
				sprintf(progresspercenttext, " %02d%% (%u%skb)", progresspercent, total/1024, extra?"+":"");
			}
		}
	}
#ifdef RUNTIMELIGHTING
	else if (lightmodel)
	{
		if (relitsurface < lightmodel->numsurfaces)
		{
			progresstext = "light";
			progresspercent = (int)((relitsurface*100.0f) / lightmodel->numsurfaces);
			sprintf(progresspercenttext, " %02d%%", progresspercent);
		}
	}
#endif

	if (progresstext)
	{
		if ((txt = strrchr(progresstext, '/')) != NULL)
			txt++;
		else
			txt = progresstext;

		x = curcon->linewidth;// - ((curcon->linewidth * 7) / 40);
		y = x - strlen(txt) - 4;
		i = curcon->linewidth/3;
		if (strlen(txt) > i)
		{
			y = x - i - 7;
			Q_strncpyN(dlbar, txt, i);
			strcat(dlbar, "...");
		}
		else
			strcpy(dlbar, txt);
		y -= strlen(progresspercenttext);
		strcat(dlbar, ": ");
		i = strlen(dlbar);
		dlbar[i++] = '\x80';
		// where's the dot go?
		if (progresspercent == 0)
			n = 0;
		else
			n = y * progresspercent / 100;

		x = i;
		for (j = 0; j < y; j++)
		{
//			if (j == n)
//				dlbar[i++] = '\x83';
//			else
				dlbar[i++] = '\x81';
		}
		dlbar[i++] = '\x82';
		dlbar[i] = 0;

		strcpy(dlbar + strlen(dlbar), progresspercenttext);

		// draw it
		y = curcon->vislines-22 + 8;
		for (i = 0; i < strlen(dlbar); i++)
			Draw_ColouredCharacter ( (i+1)<<3, y, (unsigned char)dlbar[i] | CON_WHITEMASK);

		Draw_ColouredCharacter ((n+1+x)*8, y, (unsigned char)'\x83' | CON_WHITEMASK);
	}
	
// draw the input prompt, user text, and cursor if desired
	Con_DrawInput ();
	DrawCursor();
}


/*
==================
Con_NotifyBox
==================
*/
void Con_NotifyBox (char *text)
{
	double		t1, t2;

// during startup for sound / cd warnings
	Con_Printf("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n");

	Con_Printf (text);

	Con_Printf ("Press a key.\n");
	Con_Printf("\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n");

	key_count = -2;		// wait for a key down and up
	key_dest = key_console;

	do
	{
		t1 = Sys_DoubleTime ();
		SCR_UpdateScreen ();
		Sys_SendKeyEvents ();
		t2 = Sys_DoubleTime ();
		realtime += t2-t1;		// make the cursor blink
	} while (key_count < 0);

	Con_Printf ("\n");
	key_dest = key_game;
}
