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

//this is the same order as q3, except that white and black are swapped...
consolecolours_t consolecolours[] = {
#define CON_WHITEMASK 7*256	//must be constant. things assume this
	{0, 0, 0},
	{1, 0, 0},
	{0, 1, 0},
	{1, 1, 0},
	{0.1, 0.1, 1},	//brighten dark blue a little
	{1, 0, 1},
	{0, 1, 1},
	{1, 1, 1}
};

int			con_ormask;
console_t	con_main;
console_t	*con;			// point to either con_main


#ifdef QTERM
#include <windows.h>
typedef struct qterm_s {
	console_t console;
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


cvar_t		con_notifytime = {"con_notifytime","3"};		//seconds
cvar_t		con_displaypossabilities = {"con_displaypossabilities", "1"};

#define	NUM_CON_TIMES 16
float		con_times[NUM_CON_TIMES];	// realtime time the line was generated
								// for transparent notify lines

//int			con_vislines;
int			con_notifylines;		// scan lines to clear for notify lines

qboolean	con_debuglog;

#define		MAXCMDLINE	256
extern	unsigned char	key_lines[32][MAXCMDLINE];
extern	int		edit_line;
extern	int		key_linepos;
		

qboolean	con_initialized;


void Con_Resize (console_t *con);





#ifdef QTERM
void Con_PrintCon (char *txt, console_t *con);
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
			if (con == &qt->console)
				continue;
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
					Con_PrintCon(buffer, &qt->console);
				}
			}
		}
		else
		{
			Con_PrintCon("Process ended\n", &qt->console);
			qt->running = false;
		}
	}
}

void QT_KeyPress(int key)
{
	qbyte k[2];
	qterm_t *qt;
	DWORD send = key;	//get around a gcc warning
	for (qt = qterms; qt; qt = qt->next)
	{
		if (&qt->console == con)
		{
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
					Con_PrintCon(k, &qt->console);
				}
			}
			return;
		}
	}
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
	qt->console.redirect = QT_KeyPress;

	Con_Resize(&qt->console);
	Con_PrintCon("Started Process\n", &qt->console);

	qt->next = qterms;
	qterms = activeqterm = qt;

	con = &qt->console;
}

void Con_QTerm_f(void)
{
	if(Cmd_FromServer())
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
		if (cls.state == ca_active || media_filmtype || UI_MenuState())
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
	for (i = 0; i < sizeof(con_main.text)/sizeof(unsigned short); i++)
	{
		con_main.text[i] = ' ';
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
void Con_Resize (console_t *con)
{
	int		i, j, width, oldwidth, oldtotallines, numlines, numchars;
	unsigned short	tbuf[CON_TEXTSIZE];	

	width = (vid.width >> 3) - 2;

	if (width == con->linewidth)
		return;

	if (width < 1)			// video hasn't been initialized yet
	{
		width = 38;
		con->linewidth = width;
		con->totallines = CON_TEXTSIZE / con->linewidth;
		for (i = 0; i < CON_TEXTSIZE; i++)
			con->text[i] = ' ';
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
		for (i = 0; i < sizeof(con->text)/sizeof(unsigned short); i++)
			con->text[i] = ' ';
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
#ifdef QTERM
	qterm_t *qt;
	for (qt = qterms; qt; qt=qt->next)
		Con_Resize(&qt->console);
#endif
	Con_Resize (&con_main);	
}


/*
================
Con_Init
================
*/
void Con_Init (void)
{
	con_debuglog = COM_CheckParm("-condebug");

#ifdef CRAZYDEBUGGING
	con_debuglog = true;
	TRACE(("dbg: Con_Init: con_debuglog forced\n"));
#endif

	con = &con_main;
	con->linewidth = -1;
	Con_CheckResize ();
	
	Con_Printf ("Console initialized.\n");

//
// register our commands
//
	Cvar_Register (&con_notifytime, "Console controls");
	Cvar_Register (&con_displaypossabilities, "Console controls");

	Cmd_AddCommand ("toggleconsole", Con_ToggleConsole_f);
	Cmd_AddCommand ("togglechat", Con_ToggleChat_f);
	Cmd_AddCommand ("messagemode", Con_MessageMode_f);
	Cmd_AddCommand ("messagemode2", Con_MessageMode2_f);
	Cmd_AddCommand ("clear", Con_Clear_f);
#ifdef QTERM
	Cmd_AddCommand ("qterm", Con_QTerm_f);
#endif
	con_initialized = true;
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
		con->text[i] = ' ';

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

#define INVIS_CHAR1 (char)12	//red
#define INVIS_CHAR2 (char)138	//green
#define INVIS_CHAR3 (char)160	//blue

void Con_PrintCon (char *txt, console_t *con)
{
	int		y;
	int		c, l;
	static int	cr;
	int		mask;

	if (txt[0] == 1 || txt[0] == 2)
	{
		mask = CON_STANDARDMASK|CON_WHITEMASK;		// go to colored text
		txt++;
	}
	else
		mask = CON_WHITEMASK;


	while ( (c = *txt) )
	{
		if (c == '^')
		{
			if (txt[1]>='0' && txt[1]<'8')
			{
				mask = (txt[1]-'0')*256 + (mask&~CON_COLOURMASK);	//change colour only.
				txt+=2;
				continue;
			}
			if (txt[1] == 'b')
			{
				mask = (mask & ~CON_BLINKTEXT) + (CON_BLINKTEXT - (mask & CON_BLINKTEXT));
				txt+=2;
				continue;
			}
			if (txt[1] == 's')
			{
				mask = (mask & ~CON_2NDCHARSETTEXT) + (CON_2NDCHARSETTEXT - (mask & CON_2NDCHARSETTEXT));
				txt+=2;
				continue;
			}
		}
		if (c=='\t')
			c = ' ';

		/*
		if (c == INVIS_CHAR1 || c == INVIS_CHAR2 || c == INVIS_CHAR3)
		{
			int col=0;
			if (c == INVIS_CHAR1)
				col=1;
			else if (c == INVIS_CHAR2)
				col=2;
			else if (c == INVIS_CHAR3)
				col=4;

			if (col)
			{
				if (txt[1] == INVIS_CHAR1)
				{
					col|=1;
					txt++;
				}
				else if (txt[1] == INVIS_CHAR2)
				{
					col|=2;
					txt++;
				}
				else if (txt[1] == INVIS_CHAR3)
				{
					col|=4;
					txt++;
				}

				mask = c*256;
				txt+=1;
				continue;
			}
		}
*/
	// count word length
		for (l=0 ; l< con->linewidth ; l++)
			if ( txt[l] <= ' ')
				break;

	// word wrap
		if (l != con->linewidth && (con->x + l > con->linewidth) )
			con->x = 0;

		txt++;

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

		switch (c)
		{
		case '\n':
			con->x = 0;
			break;

/*		case '\r':
			con->x = 0;
			cr = 1;
			break;*/

		default:	// display character and advance
			y = con->current % con->totallines;
			con->text[y*con->linewidth+con->x] = (qbyte)c | mask | con_ormask;			
			con->x++;
			if (con->x >= con->linewidth)
				con->x = 0;
			break;
		}
		
	}
}
void Con_Print (char *txt)
{
	Con_PrintCon(txt, &con_main);	//client console
}

void Con_CycleConsole(void)
{
	console_t *old = con;
#ifdef QTERM
	qterm_t *qt;
	for (qt = qterms; qt; qt=qt->next)
	{
		if (old == &qt->console)
		{
			if (qt->next)
			{
				con = &qt->next->console;
				return;
			}
		}
	}
	if (old == &con_main)
	{
		if (qterms)
		{
			con = &qterms->console;
			return;
		}
	}
#endif
	con = &con_main;
}

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
	_vsnprintf (msg,sizeof(msg)-1, fmt,argptr);
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
	if (con_debuglog)
		Sys_DebugLog(va("%s/qconsole.log",com_gamedir), "%s", msg);

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
	_vsnprintf (msg,sizeof(msg)-1, fmt,argptr);
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
	_vsnprintf (msg,sizeof(msg)-1, fmt,argptr);
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
	_vsnprintf (msg,sizeof(msg)-1, fmt,argptr);
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
		
	if (!developer.value)
		return;			// don't confuse non-developers with techie stuff...

	va_start (argptr,fmt);
	_vsnprintf (msg,sizeof(msg)-1, fmt,argptr);
	va_end (argptr);

	Con_Printf ("%s", msg);
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
	int		y;
	int		i;
	int p;
	int mask=CON_WHITEMASK;
	unsigned char	*text, *fname;
	extern int con_commandmatch;

	int oc;

	int si, x;

	if (key_dest != key_console && cls.state == ca_active)
		return;		// don't draw anything (allways draw if not active)

	if (con != &con_main)
		return;

	
	text = key_lines[edit_line];

	x = 1;
	if (text[1] == '/')
		x = 2;
	fname = Cmd_CompleteCommand(text+x, true, con_commandmatch);
	oc = text[key_linepos];
	if (!oc)
		text[key_linepos+1] = 0;
	if (fname)
	{
		si = strlen(text)-x;

		if ((int)(realtime*con_cursorspeed)&1)
		{
			text[key_linepos] = 11;
			strcat(text, "^2");
			if (*(fname+si))	//make sure we arn't skipping a null char
				strcat(text, fname+si+1);
		}
		else
		{
			strcat(text, "^2");
			strcat(text, fname+si);
		}
	}
	else if (((int)(realtime*con_cursorspeed)&1))
		text[key_linepos] = 11;
	else if (!text[key_linepos])
		text[key_linepos] = 10;

	for (i=0,p=0; ;p++)	//work out exactly how many charactures there really are. //FIXME: cache this
	{
		if (text[p] == '^')
		{
			if (text[p+1]>='0' && text[p+1]<='9')
			{				
				p++;
				continue;
			}
			else if (text[p+1] == '^')
				p++;
		}
		if (!text[p])
			break;
		i++;
	}

	if (i >= con->linewidth)	//work out the start point
		si = i - con->linewidth;
	else
		si = 0;

	y = con->vislines-22;

	for (i=0,p=0,x=8; x<=con->linewidth*8 ; p++)	//draw it
	{
		if (text[p] == '^')
		{
			if (text[p+1]>='0' && text[p+1]<'8')
			{
				mask = (text[p+1]-'0')*256;
				p++;
				continue;
			}
			else if (text[p+1] == '^')
				p++;
		}
		if (!text[p])
			break;
		if (si <= i)
		{
			Draw_ColouredCharacter ( x, con->vislines - 22, text[p]|mask);
			x+=8;
		}
		i++;
	}

	text[key_linepos] = oc;
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
	unsigned short	*text;
	int		i;
	float	time;
	char	*s;
	int		skip;

	console_t *con = &con_main;	//notify text should never use a chat console

#ifdef QTERM
	if (qterms)
		QT_Update();
#endif


	v = 0;
	for (i= con->current-NUM_CON_TIMES+1 ; i<=con->current ; i++)
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


	if (key_dest == key_message)
	{
		clearnotify = 0;
		scr_copytop = 1;
	
		if (chat_team)
		{
			Draw_String (8, v, "say_team:");
			skip = 11;
		}
		else
		{
			Draw_String (8, v, "say:");
			skip = 5;
		}

		s = chat_buffer;
		if (chat_bufferlen > (vid.width>>3)-(skip+1))
			s += chat_bufferlen - ((vid.width>>3)-(skip+1));
		x = 0;
		while(s[x])
		{
			Draw_Character ( (x+skip)<<3, v, s[x]);
			x++;
		}
		Draw_Character ( (x+skip)<<3, v, 10+((int)(realtime*con_cursorspeed)&1));
		v += 8;
	}

	if (v > con_notifylines)
		con_notifylines = v;
}

void Con_PrintToSys(void)	//send all the stuff that was con_printed to sys_print. This is so that system consoles in windows can scroll up and have all the text.
{
	int line, row, x;
	short *text;
	console_t *curcon = con;
	if (!con)
		return;

	row = curcon->current - con->totallines+1;
	for (line = 0; line < con->totallines-1; line++, row++)	//skip empty lines.
	{
		text = curcon->text + (row % con->totallines)*con->linewidth;
		for (x = 0; x < con->linewidth; x++)
			if (((qbyte)(text[x])&255) != ' ')
				goto breakout;
	}
breakout:
	for (; line < con->totallines-1; line++, row++)
	{
		text = curcon->text + (row % con->totallines)*con->linewidth;
		for (x = 0; x < con->linewidth; x++)
			Sys_Printf("%c", (qbyte)text[x]&255);
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
	unsigned short			*text;
	char *txt;
	int				row;
	unsigned char			dlbar[1024];
	char *progresstext;
	float progresspercent;

#ifdef RUNTIMELIGHTING
	extern model_t *lightmodel;
	extern int relitsurface;
#endif

	console_t *curcon = con;
	
	if (lines <= 0)
		return;

#ifdef QTERM
	if (qterms)
		QT_Update();
#endif

// draw the background
	if (!noback)
		Draw_ConsoleBackground (lines);

// draw the text
	con->vislines = lines;
	
// changed to line things up better
	rows = (lines-22)>>3;		// rows of text to draw

	y = lines - 30;

// draw from the bottom up
	if (curcon->display != curcon->current)
	{
	// draw arrows to show the buffer is backscrolled
		for (x=0 ; x<con->linewidth ; x+=4)
			Draw_Character ( (x+1)<<3, y, '^');
	
		y -= 8;
		rows--;
	}
	
	row = curcon->display;
	for (i=0 ; i<rows ; i++, y-=8, row--)
	{
		if (row < 0)
			break;
		if (curcon->current - row >= con->totallines)
			break;		// past scrollback wrap point
			
		text = curcon->text + (row % con->totallines)*con->linewidth;

		for (x=0 ; x<con->linewidth ; x++)
		{
			Draw_ColouredCharacter ( (x+1)<<3, y, text[x]);
		}
	}	

	progresstext = NULL;
	progresspercent = 0;

	// draw the download bar
	// figure out width
	if (cls.downloadmethod)
	{
		progresstext = cls.downloadname;
		progresspercent = cls.downloadpercent;

	}
#ifdef RUNTIMELIGHTING
	else if (lightmodel)
	{
		if (relitsurface < lightmodel->numsurfaces)
		{
			progresstext = "light";
			progresspercent = (relitsurface*100.0f) / lightmodel->numsurfaces;
		}
	}
#endif

	if (progresstext)
	{
		if ((txt = strrchr(progresstext, '/')) != NULL)
			txt++;
		else
			txt = progresstext;

		x = con->linewidth - ((con->linewidth * 7) / 40);
		y = x - strlen(txt) - 8;
		i = con->linewidth/3;
		if (strlen(txt) > i)
		{
			y = x - i - 11;
			Q_strncpyN(dlbar, txt, i);
			strcat(dlbar, "...");
		}
		else
			strcpy(dlbar, txt);
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

		sprintf(dlbar + strlen(dlbar), " %02d%%", (int)progresspercent);

		// draw it
		y = con->vislines-22 + 8;
		for (i = 0; i < strlen(dlbar); i++)
			Draw_Character ( (i+1)<<3, y, dlbar[i]);

		Draw_Character ((n+1+x)*8, y, '\x83');
	}

// draw the input prompt, user text, and cursor if desired
	Con_DrawInput ();
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
	realtime = 0;				// put the cursor back to invisible
}
