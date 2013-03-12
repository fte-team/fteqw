#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <richedit.h>
#include <stdio.h>
#include <sys/stat.h>

#include "qcc.h"
#include "gui.h"


/*
==============
LoadFile
==============
*/
unsigned char *PDECL QCC_ReadFile (const char *fname, void *buffer, int len)
{
	long    length;
	FILE *f;
	f = fopen(fname, "rb");
	if (!f)
		return NULL;
	length = fread(buffer, 1, len, f);
	fclose(f);

	if (length != len)
		return NULL;

	return buffer;
}
int PDECL QCC_FileSize (const char *fname)
{
	long    length;
	FILE *f;
	f = fopen(fname, "rb");
	if (!f)
		return -1;
	fseek(f, 0, SEEK_END);
	length = ftell(f);
	fclose(f);

	return length;
}

pbool PDECL QCC_WriteFile (const char *name, void *data, int len)
{
	long    length;
	FILE *f;
	f = fopen(name, "wb");
	if (!f)
		return false;
	length = fwrite(data, 1, len, f);
	fclose(f);

	if (length != len)
		return false;

	return true;
}

#undef printf
#undef Sys_Error

void Sys_Error(const char *text, ...)
{
	va_list argptr;
	static char msg[2048];	

	va_start (argptr,text);
	QC_vsnprintf (msg,sizeof(msg)-1, text,argptr);
	va_end (argptr);

	QCC_Error(ERR_INTERNAL, "%s", msg);
}


FILE *logfile;
int logprintf(const char *format, ...)
{
	va_list		argptr;
	static char		string[1024];

	va_start (argptr, format);
#ifdef _WIN32
	_vsnprintf (string,sizeof(string)-1, format,argptr);
#else
	vsnprintf (string,sizeof(string), format,argptr);
#endif
	va_end (argptr);

	printf("%s", string);
	if (logfile)
		fputs(string, logfile);

	return 0;
}











#define Edit_Redo(hwndCtl)                      ((BOOL)(DWORD)SNDMSG((hwndCtl), EM_REDO, 0L, 0L))


#define MAIN_WINDOW_CLASS_NAME "FTEMAINWINDOW"
#define MDI_WINDOW_CLASS_NAME "FTEMDIWINDOW"
#define EDIT_WINDOW_CLASS_NAME "FTEEDITWINDOW"
#define OPTIONS_WINDOW_CLASS_NAME "FTEOPTIONSWINDOW"

#define EM_GETSCROLLPOS  (WM_USER + 221)
#define EM_SETSCROLLPOS  (WM_USER + 222)



int GUIprintf(const char *msg, ...);
void GUIPrint(HWND wnd, char *msg);

char finddef[256];

void RunCompiler(char *args);

HINSTANCE ghInstance;
HMODULE richedit;

pbool resetprogssrc;	//progs.src was changed, reload project info.


HWND mainwindow;
HWND mdibox;
HWND outputwindow;
HWND outputbox;
HWND projecttree;
HWND gotodefbox;
HWND gotodefaccept;

FILE *logfile;

struct{
	char *text;
	HWND hwnd;
	int washit;
} buttons[] = {
	{"Compile"},
	{"Edit"},
	{"Options"},
	{"Quit"}
};

#define ID_COMPILE	0
#define ID_EDIT	1
#define ID_OPTIONS 2
#define ID_QUIT	3

#define NUMBUTTONS sizeof(buttons)/sizeof(buttons[0])



void GUI_DialogPrint(char *title, char *text)
{
	MessageBox(mainwindow, text, title, 0);
}

HWND CreateAnEditControl(HWND parent)
{
	HWND newc;

	if (!richedit)
		richedit = LoadLibrary("RICHED32.DLL");

	newc=CreateWindowEx(WS_EX_CLIENTEDGE,
		richedit?RICHEDIT_CLASS:"EDIT",
		"",
		WS_CHILD /*| ES_READONLY*/ | WS_VISIBLE | 
		WS_HSCROLL | WS_VSCROLL | ES_LEFT | ES_WANTRETURN |
		ES_MULTILINE | ES_AUTOVSCROLL,
		0, 0, 0, 0,
		parent,
		NULL,
		ghInstance,
		NULL);

	if (!newc)
		newc=CreateWindowEx(WS_EX_CLIENTEDGE,
			richedit?RICHEDIT_CLASS10A:"EDIT",	//fall back to the earlier version
			"",
			WS_CHILD /*| ES_READONLY*/ | WS_VISIBLE | 
			WS_HSCROLL | WS_VSCROLL | ES_LEFT | ES_WANTRETURN |
			ES_MULTILINE | ES_AUTOVSCROLL,
			0, 0, 0, 0,
			parent,
			NULL,
			ghInstance,
			NULL);

	if (!newc)
	{	//you've not got RICHEDIT installed properly, I guess
		FreeLibrary(richedit);
		richedit = NULL;
		newc=CreateWindowEx(WS_EX_CLIENTEDGE,
			"EDIT",
			"",
			WS_CHILD /*| ES_READONLY*/ | WS_VISIBLE | 
			WS_HSCROLL | WS_VSCROLL | ES_LEFT | ES_WANTRETURN |
			ES_MULTILINE | ES_AUTOVSCROLL,
			0, 0, 0, 0,
			parent,
			NULL,
			ghInstance,
			NULL);
	}

	//go to lucidia console, 10pt
	{
		CHARFORMAT cf;
		memset(&cf, 0, sizeof(cf));
		cf.cbSize = sizeof(cf);
		cf.dwMask = CFM_BOLD | CFM_FACE;// | CFM_SIZE;
		strcpy(cf.szFaceName, "Lucida Console");
		cf.yHeight = 5;

		SendMessage(newc, EM_SETCHARFORMAT, SCF_ALL, (WPARAM)&cf);
	}

	if (richedit)
	{
		SendMessage(newc, EM_EXLIMITTEXT, 0, 1<<20);
	}

	ShowWindow(newc, SW_SHOW);

	return newc;
}




enum {
	IDM_OPENDOCU=32,
	IDM_OPENNEW,
	IDM_GOTODEF,
	IDM_SAVE,
	IDM_FIND,
	IDM_QUIT,
	IDM_UNDO,
	IDM_REDO,
	IDM_ABOUT,
	IDM_HIGHTLIGHT,
	IDM_CASCADE,
	IDM_TILE_HORIZ,
	IDM_TILE_VERT,

	IDI_O_LEVEL0,
	IDI_O_LEVEL1,
	IDI_O_LEVEL2,
	IDI_O_LEVEL3,
	IDI_O_DEFAULT,
	IDI_O_DEBUG,
	IDI_O_CHANGE_PROGS_SRC,
	IDI_O_ADDITIONALPARAMETERS,
	IDI_O_OPTIMISATION,
	IDI_O_COMPILER_FLAG,
	IDI_O_USE,
	IDI_O_APPLY,
	IDI_O_TARGET,
	IDI_O_SYNTAX_HIGHLIGHTING,

	IDM_FIRSTCHILD
};


typedef struct editor_s {
	char filename[MAX_PATH];	//abs
	HWND window;
	HWND editpane;
	HWND tooltip;
	char tooltiptext[1024];
	pbool modified;
	time_t filemodifiedtime;
	struct editor_s *next;
} editor_t;

editor_t *editors;

void EditorReload(editor_t *editor);
int EditorSave(editor_t *edit);
void EditFile(char *name, int line);
pbool EditorModified(editor_t *e);
int Rehighlight(editor_t *edit);

void QueryOpenFile(void)
{
	char filename[MAX_PATH];
	char oldpath[MAX_PATH+10];
	OPENFILENAME ofn;
	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hInstance = ghInstance;
	ofn.lpstrFile = filename;
	ofn.nMaxFile = sizeof(filename)-1;
	memset(filename, 0, sizeof(filename));
	GetCurrentDirectory(sizeof(oldpath)-1, oldpath);
	if (GetOpenFileName(&ofn))
		EditFile(filename, -1);
	SetCurrentDirectory(oldpath);
}

//IDM_ stuff that needs no active menu
void GenericMenu(WPARAM wParam)
{
	switch(LOWORD(wParam))
	{
	case IDM_OPENNEW:
		QueryOpenFile();
		break;

	case IDM_ABOUT:
		MessageBox(NULL, "FTE QuakeC Compiler\nWritten by Forethough Entertainment.\n\nIt has a few cool features, like a semi-useful IDE.\n\nSupports:\nPrecompiler (with macros)\nArrays\n+= / -= / *= / /= operations.\nSwitch statements\nfor loops\nLots of optimisations.", "About", 0);
		break;

	case IDM_CASCADE:
		SendMessage(mdibox, WM_MDICASCADE, 0, 0);
		break;
	case IDM_TILE_HORIZ:
		SendMessage(mdibox, WM_MDITILE, MDITILE_HORIZONTAL, 0);
		break;
	case IDM_TILE_VERT:
		SendMessage(mdibox, WM_MDITILE, MDITILE_VERTICAL, 0);
		break;
	}
}

void EditorMenu(editor_t *editor, WPARAM wParam)
{
	switch(LOWORD(wParam))
	{
	case IDM_OPENDOCU:
		{
			char buffer[1024];
			int total;
			total = SendMessage(editor->editpane, EM_GETSELTEXT, (WPARAM)sizeof(buffer)-1, (LPARAM)buffer);
			buffer[total]='\0';
			if (!total)
			{
				MessageBox(NULL, "There is no name currently selected.", "Whoops", 0);
				break;
			}
			else
				EditFile(buffer, -1);
		}
		break;
	case IDM_SAVE:
		EditorSave(editor);
		break;
	case IDM_GOTODEF:
		{
			char buffer[1024];
			int total;
			total = SendMessage(editor->editpane, EM_GETSELTEXT, (WPARAM)sizeof(buffer)-1, (LPARAM)buffer);
			buffer[total]='\0';
			if (!total)
			{
				MessageBox(NULL, "There is no name currently selected.", "Whoops", 0);
				break;
			}
			else
				GoToDefinition(buffer);
		}
		break;
	case IDM_HIGHTLIGHT:
		Rehighlight(editor);
		break;

	case IDM_UNDO:
		Edit_Undo(editor->editpane);
		break;
	case IDM_REDO:
		Edit_Redo(editor->editpane);
		break;

	default:
		GenericMenu(wParam);
		break;
	}
}

char *WordUnderCursor(editor_t *editor, char *buffer, int buffersize)
{
	unsigned char linebuf[1024];
	DWORD charidx;
	DWORD lineidx;
	POINT pos;
	RECT rect;
	GetCursorPos(&pos);
	GetWindowRect(editor->editpane, &rect);
	pos.x -= rect.left;
	pos.y -= rect.top;
	charidx = SendMessage(editor->editpane, EM_CHARFROMPOS, 0, (LPARAM)&pos);
	lineidx = SendMessage(editor->editpane, EM_LINEFROMCHAR, charidx, 0);
	charidx -= SendMessage(editor->editpane, EM_LINEINDEX, lineidx, 0);

	Edit_GetLine(editor->editpane, lineidx, linebuf, sizeof(linebuf));

	//skip back to the start of the word
	while(charidx > 0 && (
		(linebuf[charidx-1] >= 'a' && linebuf[charidx-1] <= 'z') ||
		(linebuf[charidx-1] >= 'A' && linebuf[charidx-1] <= 'Z') ||
		(linebuf[charidx-1] >= '0' && linebuf[charidx-1] <= '9') ||
		linebuf[charidx-1] == '_' ||
		linebuf[charidx-1] >= 128
		))
	{
		charidx--;
	}

	//copy the result out
	lineidx = 0;
	buffersize--;
	while (buffersize && 
		(linebuf[charidx] >= 'a' && linebuf[charidx] <= 'z') ||
		(linebuf[charidx] >= 'A' && linebuf[charidx] <= 'Z') ||
		(linebuf[charidx] >= '0' && linebuf[charidx] <= '9') ||
		linebuf[charidx] == '_' ||
		linebuf[charidx] >= 128
		)
	{
		buffer[lineidx++] = linebuf[charidx++];
		buffersize--;
	}

	buffer[lineidx++] = 0;
	return buffer;
}
char *GetTooltipText(editor_t *editor)
{
	char wordbuf[256];
	char *defname;
	defname = WordUnderCursor(editor, wordbuf, sizeof(wordbuf));
	if (!*defname)
		return NULL;
	else if (globalstable.numbuckets)
	{
		QCC_def_t *def;
		def = QCC_PR_GetDef(NULL, defname, NULL, false, 0, false);
		if (def)
		{
			static char buffer[1024];
			//note function argument names do not persist beyond the function def. we might be able to read the function's localdefs for them, but that's unreliable/broken with builtins where they're most needed.
			if (def->comment)
				_snprintf(buffer, sizeof(buffer)-1, "%s	%s\r\n%s", TypeName(def->type), def->name, def->comment);
			else
				_snprintf(buffer, sizeof(buffer)-1, "%s %s", TypeName(def->type), def->name);
			return buffer;
		}
		return NULL;
	}
	else
		return NULL;//"Type info not available. Compile first.";
}
static LONG CALLBACK EditorWndProc(HWND hWnd,UINT message,
				     WPARAM wParam,LPARAM lParam)
{
	RECT rect;
	HDC hdc;
	PAINTSTRUCT ps;

	editor_t *editor;
	for (editor = editors; editor; editor = editor->next)
	{
		if (editor->window == hWnd)
			break;
		if (editor->window == NULL)
			break;	//we're actually creating it now.
	}
	if (!editor)
		goto gdefault;

	switch (message)
	{
	case WM_CLOSE:
	case WM_QUIT:
		if (editor->modified)
		{
			switch (MessageBox(hWnd, "Would you like to save?", editor->filename, MB_YESNOCANCEL))
			{
			case IDCANCEL:
				return false;
			case IDYES:
				if (!EditorSave(editor))
					return false;
			case IDNO:
			default:
				break;
			}
		}
		goto gdefault;
	case WM_DESTROY:
		{
			editor_t *e;
			if (editor == editors)
			{
				editors = editor->next;
				free(editor);
				return 0;
			}
			for (e = editors; e; e = e->next)
			{
				if (e->next == editor)
				{
					e->next = editor->next;
					free(editor);
					return 0;
				}
			}
			MessageBox(0, "Couldn't destroy file reference", "WARNING", 0);
		}
		goto gdefault;
	case WM_CREATE:
		editor->editpane = CreateAnEditControl(hWnd);
		if (richedit)
		{
			SendMessage(editor->editpane, EM_EXLIMITTEXT, 0, 1<<31);
			SendMessage(editor->editpane, EM_SETUNDOLIMIT, 256, 256);
		}

		editor->tooltip = CreateWindowEx(0, TOOLTIPS_CLASS, NULL, WS_POPUP|TTS_ALWAYSTIP|TTS_NOPREFIX, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hWnd, NULL, ghInstance, NULL);
		if (editor->tooltip)
		{                          
			TOOLINFO toolInfo = { 0 };
			toolInfo.cbSize = sizeof(toolInfo);
			toolInfo.hwnd = hWnd;
			toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS | TTF_TRACK | TTF_ABSOLUTE;
			toolInfo.uId = (UINT_PTR)editor->editpane;
			toolInfo.lpszText = "";
			SendMessage(editor->tooltip, TTM_ADDTOOL, 0, (LPARAM)&toolInfo);
			SendMessage(editor->tooltip, TTM_SETMAXTIPWIDTH, 0, 500);
		}
		goto gdefault;
	case WM_SETFOCUS:
		SetFocus(editor->editpane);
		goto gdefault;
	case WM_SIZE:
		GetClientRect(hWnd, &rect);
		SetWindowPos(editor->editpane, NULL, 0, 0, rect.right-rect.left, rect.bottom-rect.top, 0);
		goto gdefault;
	case WM_PAINT:
		hdc=BeginPaint(hWnd,(LPPAINTSTRUCT)&ps);

		EndPaint(hWnd,(LPPAINTSTRUCT)&ps);
		return TRUE;
		break;
	case WM_SETCURSOR:
		{
			POINT pos;
			char *newtext;
			TOOLINFO toolInfo = { 0 };
			toolInfo.cbSize = sizeof(toolInfo);
			toolInfo.hwnd = hWnd;
			toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS | TTF_TRACK;
			toolInfo.uId = (UINT_PTR)editor->editpane;
			newtext = GetTooltipText(editor);
			toolInfo.lpszText = editor->tooltiptext;
			if (!newtext)
				newtext = "";
			if (strcmp(editor->tooltiptext, newtext))
			{
				strncpy(editor->tooltiptext, newtext, sizeof(editor->tooltiptext)-1);
				SendMessage(editor->tooltip, TTM_UPDATETIPTEXT, (WPARAM)0, (LPARAM)&toolInfo);
				if (*editor->tooltiptext)
					SendMessage(editor->tooltip, TTM_TRACKACTIVATE, (WPARAM)TRUE, (LPARAM)&toolInfo);
				else
					SendMessage(editor->tooltip, TTM_TRACKACTIVATE, (WPARAM)FALSE, (LPARAM)&toolInfo);
			}

			GetCursorPos(&pos);
			if (pos.x >= 60)
				pos.x -= 60;
			else
				pos.x = 0;
			pos.y += 30;
			SendMessage(editor->tooltip, TTM_TRACKPOSITION, (WPARAM)0, MAKELONG(pos.x, pos.y));
		}
		goto gdefault;
	case WM_COMMAND:
		if (HIWORD(wParam) == EN_CHANGE && (HWND)lParam == editor->editpane)
		{
			if (!editor->modified)
			{
				char title[2048];
				CHARRANGE chrg;

				editor->modified = true;
				if (EditorModified(editor))
					if (MessageBox(NULL, "warning: file was modified externally. reload?", "Modified!", MB_YESNO) == IDYES)
						EditorReload(editor);


				SendMessage(editor->editpane, EM_EXGETSEL, 0, (LPARAM) &chrg);
				if (editor->modified)
					sprintf(title, "*%s:%i - FTEQCC Editor", editor->filename, 1+Edit_LineFromChar(editor->editpane, chrg.cpMin));
				else
					sprintf(title, "%s:%i - FTEQCC Editor", editor->filename, 1+Edit_LineFromChar(editor->editpane, chrg.cpMin));
				SetWindowText(editor->window, title);
			}
		}
		else
		{
			if (mdibox)
				goto gdefault;
			EditorMenu(editor, wParam);
		}
		break;
	case WM_NOTIFY:
		{
		    NMHDR *nmhdr;
			SELCHANGE *sel;
			char title[2048];
			nmhdr = (NMHDR *)lParam;
			switch(nmhdr->code)
			{
			case EN_SELCHANGE:
				sel = (SELCHANGE *)nmhdr;
				if (editor->modified)
					sprintf(title, "*%s:%i - FTEQCC Editor", editor->filename, 1+Edit_LineFromChar(editor->editpane, sel->chrg.cpMin));
				else
					sprintf(title, "%s:%i - FTEQCC Editor", editor->filename, 1+Edit_LineFromChar(editor->editpane, sel->chrg.cpMin));
				SetWindowText(editor->window, title);
				break;
			}
		}
	default:
	gdefault:
		if (mdibox)
			return DefMDIChildProc(hWnd,message,wParam,lParam);
		else
			return DefWindowProc(hWnd,message,wParam,lParam);
	}
	return 0;
}

#if 1
static DWORD lastcolour;
int GUIEmitText(HWND wnd, int start, char *text, int len)
{
	int c, cr;
	DWORD colour;
	CHARFORMAT cf;

	if (!len)
		return start;

	c = text[len];
	text[len] = '\0';
	Edit_SetSel(wnd,start,start);
	Edit_ReplaceSel(wnd,text);

	if (!strcmp(text, "void"))
		colour = RGB(0, 0, 255);
	else if (!strcmp(text, "float"))
		colour = RGB(0, 0, 255);
	else if (!strcmp(text, "vector"))
		colour = RGB(0, 0, 255);
	else if (!strcmp(text, "entity"))
		colour = RGB(0, 0, 255);
	else if (!strcmp(text, "local"))
		colour = RGB(0, 0, 255);
	else if (!strcmp(text, "string"))
		colour = RGB(0, 0, 255);
	else if (!strcmp(text, "struct"))
		colour = RGB(0, 0, 255);
	else if (!strcmp(text, "class"))
		colour = RGB(0, 0, 255);
	else if (!strcmp(text, "union"))
		colour = RGB(0, 0, 255);
	else if (!strcmp(text, "const"))
		colour = RGB(0, 0, 255);
	else if (!strcmp(text, "var"))
		colour = RGB(0, 0, 255);
	else if (!strcmp(text, "nosave"))
		colour = RGB(0, 0, 255);

	else if (!strcmp(text, "goto"))
		colour = RGB(255, 0, 0);
	else if (!strcmp(text, "thinktime"))
		colour = RGB(0, 0, 255);
	else if (!strcmp(text, "if"))
		colour = RGB(0, 0, 255);
	else if (!strcmp(text, "else"))
		colour = RGB(0, 0, 255);
	else if (!strcmp(text, "switch"))
		colour = RGB(0, 0, 255);
	else if (!strcmp(text, "case"))
		colour = RGB(0, 0, 255);
	else if (!strcmp(text, "default"))
		colour = RGB(0, 0, 255);
	else if (!strcmp(text, "break"))
		colour = RGB(0, 0, 255);
	else if (!strcmp(text, "continue"))
		colour = RGB(0, 0, 255);
	else if (!strcmp(text, "do"))
		colour = RGB(0, 0, 255);
	else if (!strcmp(text, "while"))
		colour = RGB(0, 0, 255);
	else if (!strcmp(text, "for"))
		colour = RGB(0, 0, 255);
	else if (!strcmp(text, "return"))
		colour = RGB(0, 0, 255);

	else if (!strcmp(text, "self"))
		colour = RGB(0, 0, 127);
	else if (!strcmp(text, "this"))
		colour = RGB(0, 0, 127);
	else if (!strcmp(text, "other"))
		colour = RGB(0, 0, 127);
	else if (!strcmp(text, "world"))
		colour = RGB(0, 0, 127);
	else if (!strcmp(text, "time"))
		colour = RGB(0, 0, 127);


	else if (!strcmp(text, "#define"))
		colour = RGB(0, 128, 255);
	else if (!strcmp(text, "#ifdef"))
		colour = RGB(0, 128, 255);
	else if (!strcmp(text, "#ifndef"))
		colour = RGB(0, 128, 255);
	else if (!strcmp(text, "#else"))
		colour = RGB(0, 128, 255);
	else if (!strcmp(text, "#endif"))
		colour = RGB(0, 128, 255);
	else if (!strcmp(text, "#undef"))
		colour = RGB(0, 128, 255);
	else if (!strcmp(text, "#pragma"))
		colour = RGB(0, 128, 255);
	else if (!strcmp(text, "#includelist"))
		colour = RGB(0, 128, 255);
	else if (!strcmp(text, "#endlist"))
		colour = RGB(0, 128, 255);


	else if (*text == '\"')
		colour = RGB(128, 0, 0);

	else if (!strncmp(text, "//", 2))
		colour = RGB(0, 127, 0);
	else if (!strncmp(text, "/*", 2))
		colour = RGB(0, 127, 0);
	else
		colour = RGB(0, 0, 0);

	text[len] = c;

	cr = 0;
	for (c = 0; c < len; c++)
		if (text[c] == '\r')
			cr++;
	if (cr)
		len-=cr;

	if (colour == lastcolour)
		return start+len;

	lastcolour = colour;

	Edit_SetSel(wnd,start,start+len);
	memset(&cf, 0, sizeof(cf));
	cf.cbSize = sizeof(cf);
	cf.dwMask = CFM_COLOR;
	cf.crTextColor = colour;
	SendMessage(wnd, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
	Edit_SetSel(wnd,start+len,start+len);

	return start + len;
}
void GUIFormattingPrint(HWND wnd, char *msg)
{
	int len=Edit_GetTextLength(wnd);
	char *start;
	CHARRANGE chrg;
	lastcolour = RGB(0,0,0);
	SendMessage(wnd, WM_SETREDRAW, false, 0);
	chrg.cpMin = chrg.cpMax = 0;
	SendMessage(wnd, EM_EXSETSEL, 0, (LPARAM) &chrg);

	for(start = msg;;)
	{
		if (!*msg)
			break;
		else if (*msg == '/' && msg[1] == '/')
		{
			len = GUIEmitText(wnd, len, start, msg - start);
			start = msg;

			msg+=2;
			while(*msg && *msg != '\n' && *msg != '\r')
				msg++;
		}
		else if (*msg == '/' && msg[1] == '*')
		{
			len = GUIEmitText(wnd, len, start, msg - start);
			start = msg;
			msg+=2;
			while(*msg)
			{
				if (msg[0] == '*' && msg[1] == '/')
				{
					msg+=2;
					break;
				}
				msg++;
			}
		}
		else if (*msg == '#' || *msg == '_' || (*msg >= 'A' && *msg <= 'Z') || (*msg >= 'a' && *msg <= 'z'))
		{
			len = GUIEmitText(wnd, len, start, msg - start);
			start = msg;
			msg++;
			while (*msg == '_' || (*msg >= 'A' && *msg <= 'Z') || (*msg >= 'a' && *msg <= 'z' || *msg >= '0' && *msg <= '9'))
				msg++;
		}
		else if (*msg == '\"')
		{
			len = GUIEmitText(wnd, len, start, msg - start);
			start = msg;
			msg++;
			while(*msg)
			{
				if (*msg == '\\')
					msg++;
				else if (*msg == '\"')
				{
					msg++;
					break;
				}

				msg++;
			}
		}
/*		else if (*msg <= ' ')
		{
			while (*msg <= ' ' && *msg)
				msg++;
		}*/
		else
		{
			msg++;
			continue;
		}

		len = GUIEmitText(wnd, len, start, msg - start);
		start = msg;
	}
	len = GUIEmitText(wnd, len, start, msg - start);
	start = msg;
	SendMessage(wnd, WM_SETREDRAW, true, 0);
}

int Rehighlight(editor_t *edit)
{
	int len;
	char *file;

	CHARRANGE chrg;
	POINT scrollpos;

	SendMessage(edit->editpane, EM_SETEVENTMASK, 0, 0);

	SendMessage(edit->editpane, EM_GETSCROLLPOS, 0, (LPARAM)&scrollpos);
	SendMessage(edit->editpane, EM_EXGETSEL, 0, (LPARAM) &chrg);

	len = Edit_GetTextLength(edit->editpane);
	file = malloc(len+1);
	if (!file)
	{
		MessageBox(NULL, "Save failed - not enough mem", "Error", 0);
		return false;
	}
	Edit_GetText(edit->editpane, file, len);
	file[len] = '\0';

	SetWindowText(edit->editpane,"");

//	GUIPrint(edit->editpane, file);
	GUIFormattingPrint(edit->editpane, file);
	free(file);

//	Edit_SetSel(edit->editpane, Edit_LineIndex(neweditor->editpane, 0), Edit_LineIndex(neweditor->editpane, 0));

	InvalidateRect(edit->editpane, NULL, true);
	InvalidateRect(edit->window, NULL, true);

	SendMessage(edit->editpane, EM_SETEVENTMASK, 0, ENM_SELCHANGE|ENM_CHANGE);

	SendMessage(edit->editpane, EM_SETSCROLLPOS, 0, (LPARAM)&scrollpos);
	SendMessage(edit->editpane, EM_EXSETSEL, 0, (LPARAM) &chrg);

	UpdateWindow(edit->editpane);
	RedrawWindow(edit->window, NULL, NULL, 0);

	return true;
}
#else
static void chunkcolour(HWND pane, int start, int end, DWORD colour)
{
	CHARFORMAT cf;
	if (colour == RGB(0,0,0))
		return;	//don't need to
	Edit_SetSel(pane,start,end);
	memset(&cf, 0, sizeof(cf));
	cf.cbSize = sizeof(cf);
	cf.dwMask = CFM_COLOR;
	cf.crTextColor = colour;
	SendMessage(pane, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
}
void GUIFormattingPrint(HWND wnd, char *msg)
{
}
int Rehighlight(editor_t *edit)
{
	char *file;
	int c, last, len;
	DWORD color;
	CHARRANGE chrg;
	POINT scrollpos;

	//Dsiable redraws
	SendMessage(edit->editpane, WM_SETREDRAW, false, 0);

	//Don't notify us for a bit..
	SendMessage(edit->editpane, EM_SETEVENTMASK, 0, 0);

	//get state so we can restore scroll positions and things.
	SendMessage(edit->editpane, EM_GETSCROLLPOS, 0, (LPARAM)&scrollpos);
	SendMessage(edit->editpane, EM_EXGETSEL, 0, (LPARAM) &chrg);


	len = Edit_GetTextLength(edit->editpane);
	file = malloc(len+1);
	if (!file)
	{
		MessageBox(NULL, "Highlight failed - not enough mem", "Error", 0);
		return false;
	}
	Edit_GetText(edit->editpane, file, len);
	file[len] = '\0';
	SetWindowText(edit->editpane,file);	//this is so that we guarentee that the \rs or whatever that windows insists on inserting don't get in the way

	color = RGB(0,0,0);
	for (last = 0, c = 0; c < len; c++)
	{
		if (file[c] == '/' && file[c+1] == '/')	//do special syntax
		{
			chunkcolour(edit->editpane, last, c, color);
			last = c;

			while(file[c] != '\n')
				c++;
			color = RGB(0, 127, 0);
		}
		else
		{
			chunkcolour(edit->editpane, last, c, color);
			last = c;

			while(file[c] >= 'a' && file[c] <= 'z' || file[c] >= 'A' && file[c] <= 'Z' || file[c] == '_')
				c++;

			color = RGB(rand(), rand(), rand());
		}
	}

	free(file);

	//reenable drawing
	SendMessage(edit->editpane, WM_SETREDRAW, true, 0);
}
#endif

void EditorReload(editor_t *editor)
{
	struct stat sbuf;
	int flen;
	char *file;

	flen = QCC_FileSize(editor->filename);
	if (flen >= 0)
	{
		file = malloc(flen+1);
		QCC_ReadFile(editor->filename, file, flen);
		file[flen] = 0;
	}
	else
		file = NULL;

	SendMessage(editor->editpane, EM_SETEVENTMASK, 0, 0);

	/*clear it out*/
	Edit_SetSel(editor->editpane,0,Edit_GetTextLength(editor->editpane));
	Edit_ReplaceSel(editor->editpane,"");

	if (file)
	{
		if (!fl_autohighlight)
		{
			GUIPrint(editor->editpane, file);
		}
		else
		{
			GUIFormattingPrint(editor->editpane, file);
		}
		free(file);
	}

	editor->modified = false;
	stat(editor->filename, &sbuf);
	editor->filemodifiedtime = sbuf.st_mtime;

	SendMessage(editor->editpane, EM_SETEVENTMASK, 0, ENM_SELCHANGE|ENM_CHANGE);
}

void EditFile(char *name, int line)
{
	char title[1024];
	editor_t *neweditor;
	WNDCLASS wndclass;
	HMENU menu, menufile, menuhelp, menunavig;

	for (neweditor = editors; neweditor; neweditor = neweditor->next)
	{
		if (neweditor->window && !strcmp(neweditor->filename, name))
		{
			if (line >= 0)
			{
				Edit_SetSel(neweditor->editpane, Edit_LineIndex(neweditor->editpane, line), Edit_LineIndex(neweditor->editpane, line+1));
				Edit_ScrollCaret(neweditor->editpane);
			}
			if (mdibox)
				SendMessage(mdibox, WM_MDIACTIVATE, (WPARAM)neweditor->window, 0);
			SetFocus(neweditor->window);
			SetFocus(neweditor->editpane);
			return;
		}
	}

	if (QCC_FileSize(name) == -1)
	{
		MessageBox(NULL, "File not found.", "Error", 0);
		return;
	}

	neweditor = malloc(sizeof(editor_t));
	if (!neweditor)
	{
		MessageBox(NULL, "Low memory", "Error", 0);
		return;
	}

	neweditor->next = editors;
	editors = neweditor;

	strncpy(neweditor->filename, name, sizeof(neweditor->filename)-1);

	if (!mdibox)
	{
		menu = CreateMenu();
		menufile = CreateMenu();
		menuhelp = CreateMenu();
		menunavig = CreateMenu();
		AppendMenu(menu, MF_POPUP, (UINT_PTR)menufile,	"&File");
		AppendMenu(menu, MF_POPUP, (UINT_PTR)menunavig,	"&Navigation");
		AppendMenu(menu, MF_POPUP, (UINT_PTR)menuhelp,	"&Help");
		AppendMenu(menufile, 0, IDM_OPENNEW,	"Open &new file ");
		AppendMenu(menufile, 0, IDM_SAVE,		"&Save          ");
	//	AppendMenu(menufile, 0, IDM_FIND,		"&Find");
		AppendMenu(menufile, 0, IDM_UNDO,		"&Undo          Ctrl+Z");
		AppendMenu(menufile, 0, IDM_REDO,		"&Redo          Ctrl+Y");
		AppendMenu(menunavig, 0, IDM_GOTODEF, "Go to definition");
		AppendMenu(menunavig, 0, IDM_OPENDOCU, "Open selected file");
		AppendMenu(menuhelp, 0, IDM_ABOUT, "About");
		AppendMenu(menu, 0, IDM_HIGHTLIGHT, "H&ighlight");
	}


	
	wndclass.style      = 0;
    wndclass.lpfnWndProc   = (WNDPROC)EditorWndProc;
    wndclass.cbClsExtra    = 0;
    wndclass.cbWndExtra    = 0;
    wndclass.hInstance     = ghInstance;
    wndclass.hIcon         = 0;
    wndclass.hCursor       = LoadCursor (NULL,IDC_ARROW);
	wndclass.hbrBackground = (void *)COLOR_WINDOW;
    wndclass.lpszMenuName  = 0;
    wndclass.lpszClassName = EDIT_WINDOW_CLASS_NAME;
	RegisterClass(&wndclass);

	neweditor->window = NULL;
	if (mdibox)
	{
		MDICREATESTRUCT mcs;

		sprintf(title, "%s", name);

		mcs.szClass = EDIT_WINDOW_CLASS_NAME;
		mcs.szTitle = name;
		mcs.hOwner = ghInstance;
		mcs.x = mcs.cx = CW_USEDEFAULT;
		mcs.y = mcs.cy = CW_USEDEFAULT;
		mcs.style = WS_OVERLAPPEDWINDOW;
		mcs.lParam = 0;

		neweditor->window = (HWND) SendMessage (mdibox, WM_MDICREATE, 0, 
			(LONG_PTR) (LPMDICREATESTRUCT) &mcs); 
	}
	else
	{
		sprintf(title, "%s - FTEEditor", name);

		neweditor->window=CreateWindow(EDIT_WINDOW_CLASS_NAME, title, WS_OVERLAPPEDWINDOW,
			0, 0, 640, 480, NULL, NULL, ghInstance, NULL);
	}

	if (!mdibox)
		SetMenu(neweditor->window, menu);

	if (!neweditor->window)
	{
		MessageBox(NULL, "Failed to create editor window", "Error", 0);
		return;
	}

	EditorReload(neweditor);

	if (line >= 0)
		Edit_SetSel(neweditor->editpane, Edit_LineIndex(neweditor->editpane, line), Edit_LineIndex(neweditor->editpane, line+1));
	else
		Edit_SetSel(neweditor->editpane, Edit_LineIndex(neweditor->editpane, 0), Edit_LineIndex(neweditor->editpane, 0));

	Edit_ScrollCaret(neweditor->editpane);

	ShowWindow(neweditor->window, SW_SHOW);
	SetFocus(mainwindow);
	SetFocus(neweditor->window);
	SetFocus(neweditor->editpane);
}

int EditorSave(editor_t *edit)
{
	DWORD selstart;
	char title[2048];
	struct stat sbuf;
	int len;
	char *file;
	len = Edit_GetTextLength(edit->editpane);
	file = malloc(len+1);
	if (!file)
	{
		MessageBox(NULL, "Save failed - not enough mem", "Error", 0);
		return false;
	}
	Edit_GetText(edit->editpane, file, len);
	if (!QCC_WriteFile(edit->filename, file, len))
	{
		MessageBox(NULL, "Save failed\nCheck path and ReadOnly flags", "Failure", 0);
		return false;
	}
	free(file);

	/*now whatever is on disk should have the current time*/
	edit->modified = false;
	stat(edit->filename, &sbuf);
	edit->filemodifiedtime = sbuf.st_mtime;

	//remove the * in a silly way.
	SendMessage(edit->editpane, EM_GETSEL, (WPARAM)&selstart, (LPARAM)0);
	sprintf(title, "%s:%i - FTEQCC Editor", edit->filename, 1+Edit_LineFromChar(edit->editpane, selstart));
	SetWindowText(edit->window, title);

	return true;
}
void EditorsRun(void)
{
}


char *GUIReadFile(const char *fname, void *buffer, int blen)
{
	editor_t *e;
	for (e = editors; e; e = e->next)
	{
		if (e->window && !strcmp(e->filename, fname))
		{
			int elen = Edit_GetTextLength(e->editpane);
			Edit_GetText(e->editpane, buffer, blen);
			return buffer;
		}
	}

	return QCC_ReadFile(fname, buffer, blen);
}

int GUIFileSize(const char *fname)
{
	editor_t *e;
	for (e = editors; e; e = e->next)
	{
		if (e->window && !strcmp(e->filename, fname))
		{
			int len = Edit_GetTextLength(e->editpane);
			return len;
		}
	}
	return QCC_FileSize(fname);
}

/*checks if the file has been modified externally*/
pbool EditorModified(editor_t *e)
{
	struct stat sbuf;
	stat(e->filename, &sbuf);
	if (e->filemodifiedtime != sbuf.st_mtime)
		return true;

	return false;
}













HWND optionsmenu;
HWND hexen2item;
HWND nokeywords_coexistitem;
HWND autoprototype_item;
HWND autohighlight_item;
HWND extraparmsitem;
static LONG CALLBACK OptionsWndProc(HWND hWnd,UINT message,
				     WPARAM wParam,LPARAM lParam)
{
	int i;
	switch (message)
	{
	case WM_DESTROY:
		optionsmenu = NULL;
		break;

	case WM_COMMAND:
		switch(wParam)
		{
		case IDI_O_USE:
		case IDI_O_APPLY:
			for (i = 0; optimisations[i].enabled; i++)
			{
				if (optimisations[i].flags & FLAG_HIDDENINGUI)
					continue;

				if (Button_GetCheck(optimisations[i].guiinfo))
					optimisations[i].flags |= FLAG_SETINGUI;
				else
					optimisations[i].flags &= ~FLAG_SETINGUI;
			}
			fl_hexen2 = Button_GetCheck(hexen2item);
			for (i = 0; compiler_flag[i].enabled; i++)
			{
				if (compiler_flag[i].flags & FLAG_HIDDENINGUI)
					continue;
				if (Button_GetCheck(compiler_flag[i].guiinfo))
					compiler_flag[i].flags |= FLAG_SETINGUI;
				else
					compiler_flag[i].flags &= ~FLAG_SETINGUI;
			}
			fl_autohighlight = Button_GetCheck(autohighlight_item);
			Edit_GetText(extraparmsitem, parameters, sizeof(parameters)-1);

			if (wParam == IDI_O_USE)
				buttons[ID_COMPILE].washit = true;
			break;
		case IDI_O_CHANGE_PROGS_SRC:
			{
				char *s, *s2;
				char filename[MAX_PATH];
				char oldpath[MAX_PATH+10];
				OPENFILENAME ofn;
				memset(&ofn, 0, sizeof(ofn));
				ofn.lStructSize = sizeof(ofn);
				ofn.hInstance = ghInstance;
				ofn.lpstrFile = filename;
				ofn.lpstrTitle = "Please find progs.src";
				ofn.nMaxFile = sizeof(filename)-1;
				ofn.lpstrFilter = "QuakeC source\0*.src\0All files\0*.*\0";
				memset(filename, 0, sizeof(filename));
				GetCurrentDirectory(sizeof(oldpath)-1, oldpath);
				ofn.lpstrInitialDir = oldpath;
				if (GetOpenFileName(&ofn))
				{
					strcpy(progssrcdir, filename);
					for(s = progssrcdir; s; s = s2)
					{
						s2 = strchr(s+1, '\\');
						if (!s2)
							break;
						s = s2;
					}
					if (s)
					{
						*s = '\0';
						strcpy(progssrcname, s+1);
					}
					else
						strcpy(progssrcname, filename);

					SetCurrentDirectory(progssrcdir);
					*progssrcdir = '\0';
				}
				resetprogssrc = true;
			}
			break;
		case IDI_O_LEVEL0:
		case IDI_O_LEVEL1:
		case IDI_O_LEVEL2:
		case IDI_O_LEVEL3:
			for (i = 0; optimisations[i].enabled; i++)
			{
				if (optimisations[i].flags & FLAG_HIDDENINGUI)
					continue;

				if (optimisations[i].optimisationlevel<=(int)wParam-IDI_O_LEVEL0)
					Button_SetCheck(optimisations[i].guiinfo, 1);
				else
					Button_SetCheck(optimisations[i].guiinfo, 0);
			}
			break;
		case IDI_O_DEBUG:
			for (i = 0; optimisations[i].enabled; i++)
			{
				if (optimisations[i].flags & FLAG_HIDDENINGUI)
					continue;

				if (optimisations[i].flags&FLAG_KILLSDEBUGGERS)
					Button_SetCheck(optimisations[i].guiinfo, 0);
			}
			break;
		case IDI_O_DEFAULT:
			for (i = 0; optimisations[i].enabled; i++)
			{
				if (optimisations[i].flags & FLAG_HIDDENINGUI)
					continue;

				if (optimisations[i].flags & FLAG_ASDEFAULT)
					Button_SetCheck(optimisations[i].guiinfo, 1);
				else
					Button_SetCheck(optimisations[i].guiinfo, 0);
			}
			break;
		}
		break;
	case WM_HELP:
		{
			HELPINFO *hi;
			hi = (HELPINFO *)lParam;
			switch(hi->iCtrlId) 
			{
			case IDI_O_DEFAULT:
				MessageBox(hWnd, "Sets the default optimisations", "Help", MB_OK|MB_ICONINFORMATION);
				break;
			case IDI_O_DEBUG:
				MessageBox(hWnd, "Clears all optimisations which can make your progs harder to debug", "Help", MB_OK|MB_ICONINFORMATION);
				break;
			case IDI_O_LEVEL0:
			case IDI_O_LEVEL1:
			case IDI_O_LEVEL2:
			case IDI_O_LEVEL3:
				MessageBox(hWnd, "Sets a specific optimisation level", "Help", MB_OK|MB_ICONINFORMATION);
				break;
			case IDI_O_CHANGE_PROGS_SRC:
				MessageBox(hWnd, "Use this button to change your root source file.\nNote that fteqcc compiles sourcefiles from editors first, rather than saving. This means that changes are saved ONLY when you save them, but means that switching project mid-compile can result in problems.", "Help", MB_OK|MB_ICONINFORMATION);
				break;
			case IDI_O_ADDITIONALPARAMETERS:
				MessageBox(hWnd, "Type in additional commandline parameters here. Use -Dname to define a named precompiler constant before compiling.", "Help", MB_OK|MB_ICONINFORMATION);
				break;
			case IDI_O_APPLY:
				MessageBox(hWnd, "Apply changes shown, but do not recompile yet.", "Help", MB_OK|MB_ICONINFORMATION);
				break;
			case IDI_O_USE:
				MessageBox(hWnd, "Apply changes shown here and recompile.", "Help", MB_OK|MB_ICONINFORMATION);
				break;
			case IDI_O_OPTIMISATION:
				for (i = 0; optimisations[i].enabled; i++)
				{
					if (optimisations[i].guiinfo == hi->hItemHandle)
					{
						MessageBox(hWnd, optimisations[i].description, "Help", MB_OK|MB_ICONINFORMATION);
						break;
					}
				}
				break;
			case IDI_O_COMPILER_FLAG:
				for (i = 0; compiler_flag[i].enabled; i++)
				{
					if (compiler_flag[i].guiinfo == hi->hItemHandle)
					{
						MessageBox(hWnd, compiler_flag[i].description, "Help", MB_OK|MB_ICONINFORMATION);
						break;
					}
				}
				break;
			case IDI_O_TARGET:
				MessageBox(hWnd, "Click here to compile a hexen2 compatible progs. Note that this uses the -Thexen2. There are other targets available.", "Help", MB_OK|MB_ICONINFORMATION);
				break;
			case IDI_O_SYNTAX_HIGHLIGHTING:
				MessageBox(hWnd, "Should syntax be highlighted automatically when a file is opened?", "Help", MB_OK|MB_ICONINFORMATION);
				break;
			}
		}
		break;
	default:
		return DefWindowProc(hWnd,message,wParam,lParam);
	}
	return 0;
}
void OptionsDialog(void)
{
	HWND subsection;
	RECT r;
	WNDCLASS wndclass;
	HWND wnd;
	int i;
	int flagcolums=1;

	int x;
	int y;
	int my;
	int height;
	int num;
	int cflagsshown;

	if (optionsmenu)
	{
		BringWindowToTop(optionsmenu);
		return;
	}


	memset(&wndclass, 0, sizeof(wndclass));
	wndclass.style      = 0;
    wndclass.lpfnWndProc   = (WNDPROC)OptionsWndProc;
    wndclass.cbClsExtra    = 0;
    wndclass.cbWndExtra    = 0;
    wndclass.hInstance     = ghInstance;
    wndclass.hIcon         = 0;
    wndclass.hCursor       = LoadCursor (NULL,IDC_ARROW);
	wndclass.hbrBackground = (void *)COLOR_WINDOW;
    wndclass.lpszMenuName  = 0;
    wndclass.lpszClassName = OPTIONS_WINDOW_CLASS_NAME;
	RegisterClass(&wndclass);

	height = 0;
	for (i = 0; optimisations[i].enabled; i++)
	{
		if (optimisations[i].flags & FLAG_HIDDENINGUI)
			continue;

		height++;
	}

	cflagsshown = 2;
	for (i = 0; compiler_flag[i].enabled; i++)
	{
		if (compiler_flag[i].flags & FLAG_HIDDENINGUI)
			continue;

		cflagsshown++;
	}

	height = (height-1)/2;

	while (cflagsshown > ((480-(88+40))/16)*(flagcolums))
		flagcolums++;

	if (height < (cflagsshown+flagcolums-1)/flagcolums)
		height = (cflagsshown+flagcolums-1)/flagcolums;

	r.right = 408 + flagcolums*168;
	if (r.right < 640)
		r.right = 640;

	height *= 16;

	height += 88+40;

	r.left = GetSystemMetrics(SM_CXSCREEN)/2-320;
	r.top = GetSystemMetrics(SM_CYSCREEN)/2-240;
	r.bottom = r.top + height;
	r.right  += r.left;



	AdjustWindowRectEx (&r, WS_CAPTION|WS_SYSMENU, FALSE, 0);

	optionsmenu=CreateWindowEx(WS_EX_CONTEXTHELP, OPTIONS_WINDOW_CLASS_NAME, "Options - FTE QuakeC compiler", WS_CAPTION|WS_SYSMENU,
		r.left, r.top, r.right-r.left, r.bottom-r.top, NULL, NULL, ghInstance, NULL);

	subsection = CreateWindow("BUTTON", "Optimisations", WS_CHILD|WS_VISIBLE|BS_GROUPBOX,
		0, 0, 400, height-48, optionsmenu, NULL, ghInstance, NULL);

	num = 0;
	for (i = 0; optimisations[i].enabled; i++)
	{
		if (optimisations[i].flags & FLAG_HIDDENINGUI)
		{
			optimisations[i].guiinfo = NULL;
			continue;
		}

		optimisations[i].guiinfo = wnd = CreateWindow("BUTTON",optimisations[i].fullname,
			   WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
			   8+200*(num&1),16+16*(num/2),200-16,16,
			   subsection,
			   (HMENU)IDI_O_OPTIMISATION,
			   ghInstance,
			   NULL);

		if (optimisations[i].flags&FLAG_SETINGUI)
			Button_SetCheck(wnd, 1);
		else
			Button_SetCheck(wnd, 0);

		num++;
	}

	CreateWindow("BUTTON","O0",
		   WS_CHILD | WS_VISIBLE,
		   8,height-88,64,32,
		   optionsmenu,
		   (HMENU)IDI_O_LEVEL0,
		   ghInstance,
		   NULL);
	CreateWindow("BUTTON","O1",
		   WS_CHILD | WS_VISIBLE,
		   8+64,height-88,64,32,
		   optionsmenu,
		   (HMENU)IDI_O_LEVEL1,
		   ghInstance,
		   NULL);
	CreateWindow("BUTTON","O2",
		   WS_CHILD | WS_VISIBLE,
		   8+64*2,height-88,64,32,
		   optionsmenu,
		   (HMENU)IDI_O_LEVEL2,
		   ghInstance,
		   NULL);
	CreateWindow("BUTTON","O3",
		   WS_CHILD | WS_VISIBLE,
		   8+64*3,height-88,64,32,
		   optionsmenu,
		   (HMENU)IDI_O_LEVEL3,
		   ghInstance,
		   NULL);
	CreateWindow("BUTTON","Debug",
		   WS_CHILD | WS_VISIBLE,
		   8+64*4,height-88,64,32,
		   optionsmenu,
		   (HMENU)IDI_O_DEBUG,
		   ghInstance,
		   NULL);
	CreateWindow("BUTTON","Default",
		   WS_CHILD | WS_VISIBLE,
		   8+64*5,height-88,64,32,
		   optionsmenu,
		   (HMENU)IDI_O_DEFAULT,
		   ghInstance,
		   NULL);
	CreateWindow("BUTTON","Apply",
		   WS_CHILD | WS_VISIBLE,
		   8,height-40,64,32,
		   optionsmenu,
		   (HMENU)IDI_O_APPLY,
		   ghInstance,
		   NULL);
	CreateWindow("BUTTON","Use",
		   WS_CHILD | WS_VISIBLE,
		   8+64,height-40,64,32,
		   optionsmenu,
		   (HMENU)IDI_O_USE,
		   ghInstance,
		   NULL);
	CreateWindow("BUTTON","progs.src",
		   WS_CHILD | WS_VISIBLE,
		   8+64*2,height-40,64,32,
		   optionsmenu,
		   (HMENU)IDI_O_CHANGE_PROGS_SRC,
		   ghInstance,
		   NULL);



		y=4;
	hexen2item = wnd = CreateWindow("BUTTON","HexenC",
		   WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
		   408,y,200-16,16,
		   optionsmenu,
		   (HMENU)IDI_O_TARGET,
		   ghInstance,
		   NULL);
	y+=16;
	if (fl_hexen2)
		Button_SetCheck(wnd, 1);
	else
		Button_SetCheck(wnd, 0);

	autohighlight_item = wnd = CreateWindow("BUTTON","Syntax Highlighting",
		   WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
		   408,y,200-16,16,
		   optionsmenu,
		   (HMENU)IDI_O_SYNTAX_HIGHLIGHTING,
		   ghInstance,
		   NULL);
	y+=16;
	if (fl_autohighlight)
		Button_SetCheck(wnd, 1);
	else
		Button_SetCheck(wnd, 0);

	x = 408;
	my = y;
	for (i = 0; compiler_flag[i].enabled; i++)
	{
		if (compiler_flag[i].flags & FLAG_HIDDENINGUI)
		{
			compiler_flag[i].guiinfo = NULL;
			continue;
		}

		if (y > height-(88+40))
		{
			y = 4;
			x += 168;
		}

		compiler_flag[i].guiinfo = wnd = CreateWindow("BUTTON",compiler_flag[i].fullname,
			   WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
			   x,y,168,16,
			   optionsmenu,
			   (HMENU)IDI_O_COMPILER_FLAG,
			   ghInstance,
			   NULL);
		y+=16;

		if (my < y)
			my = y;

		if (compiler_flag[i].flags & FLAG_SETINGUI)
			Button_SetCheck(wnd, 1);
		else
			Button_SetCheck(wnd, 0);
	}

	CreateWindow("STATIC","Extra Parameters:",
		   WS_CHILD | WS_VISIBLE,
		   408,my,200-16,16,
		   optionsmenu,
		   (HMENU)0,
		   ghInstance,
		   NULL);
	my+=16;
	extraparmsitem = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT",parameters,
		   WS_CHILD | WS_VISIBLE|ES_LEFT | ES_WANTRETURN |
		ES_MULTILINE | ES_AUTOVSCROLL,
		   408,my,r.right-r.left - 408 - 8,height-my-4,
		   optionsmenu,
		   (HMENU)IDI_O_ADDITIONALPARAMETERS,
		   ghInstance,
		   NULL);

	ShowWindow(optionsmenu, SW_SHOWDEFAULT);
}











#undef printf



static LONG CALLBACK MainWndProc(HWND hWnd,UINT message,
				     WPARAM wParam,LPARAM lParam)
{
	int width;
	int i;
	RECT rect;
	HDC hdc;
	PAINTSTRUCT ps;
	switch (message)
	{
	case WM_CREATE:
		{
			CLIENTCREATESTRUCT ccs;

			HMENU rootmenu, windowmenu, m;
			rootmenu = CreateMenu();
			
				AppendMenu(rootmenu, MF_POPUP, (UINT_PTR)(m = CreateMenu()),	"&File");
					AppendMenu(m, 0, IDM_OPENNEW,	"Open &new file ");
					AppendMenu(m, 0, IDM_SAVE,		"&Save          ");
				//	AppendMenu(m, 0, IDM_FIND,		"&Find");
					AppendMenu(m, 0, IDM_UNDO,		"&Undo          Ctrl+Z");
					AppendMenu(m, 0, IDM_REDO,		"&Redo          Ctrl+Y");
				AppendMenu(rootmenu, MF_POPUP, (UINT_PTR)(m = CreateMenu()),	"&Navigation");
					AppendMenu(m, 0, IDM_GOTODEF, "Go to definition");
					AppendMenu(m, 0, IDM_OPENDOCU, "Open selected file");
				AppendMenu(rootmenu, MF_POPUP, (UINT_PTR)(m = windowmenu = CreateMenu()),	"&Window");
					AppendMenu(m, 0, IDM_CASCADE, "&Cascade");
					AppendMenu(m, 0, IDM_TILE_HORIZ, "Tile &Horizontally");
					AppendMenu(m, 0, IDM_TILE_VERT, "Tile &Vertically");
				AppendMenu(rootmenu, MF_POPUP, (UINT_PTR)(m = CreateMenu()),	"&Help");
					AppendMenu(m, 0, IDM_ABOUT, "About");

			SetMenu(hWnd, rootmenu);

			// Retrieve the handle to the window menu and assign the
			// first child window identifier.

			memset(&ccs, 0, sizeof(ccs));
			ccs.hWindowMenu = windowmenu;
			ccs.idFirstChild = IDM_FIRSTCHILD;

			// Create the MDI client window.

			mdibox = CreateWindow( "MDICLIENT", (LPCTSTR) NULL,
					WS_CHILD | WS_CLIPCHILDREN | WS_VSCROLL | WS_HSCROLL,
					0, 0, 320, 200, hWnd, (HMENU) 0xCAC, ghInstance, (LPSTR) &ccs);
			ShowWindow(mdibox, SW_SHOW);

			projecttree = CreateWindow(WC_TREEVIEW, (LPCTSTR) NULL,
					WS_CHILD | WS_CLIPCHILDREN | WS_VSCROLL | WS_HSCROLL
					|	TVS_HASBUTTONS |TVS_LINESATROOT|TVS_HASLINES,
			0, 0, 320, 200, hWnd, (HMENU) 0xCAC, ghInstance, (LPSTR) &ccs);
			ShowWindow(projecttree, SW_SHOW);

			if (projecttree)
			{
				gotodefbox = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", (LPCTSTR) NULL,
						WS_CHILD | WS_CLIPCHILDREN,
						0, 0, 320, 200, hWnd, (HMENU) 0xCAC, ghInstance, (LPSTR) NULL);
				ShowWindow(gotodefbox, SW_SHOW);

				gotodefaccept = CreateWindowEx(WS_EX_CLIENTEDGE, "BUTTON", "Def",
						WS_CHILD | WS_CLIPCHILDREN | BS_DEFPUSHBUTTON,
						0, 0, 320, 200, hWnd, (HMENU) 0x4404, ghInstance, (LPSTR) NULL);
				ShowWindow(gotodefaccept, SW_SHOW);
			}
		}
		break;
	case WM_CTLCOLORBTN:
		return GetSysColorBrush(COLOR_HIGHLIGHT);//COLOR_BACKGROUND;
	case WM_DESTROY:
		mainwindow = NULL;
		break;

	case WM_SIZE:
		GetClientRect(mainwindow, &rect);
		if (projecttree)
		{
			SetWindowPos(projecttree, NULL, 0, 0, 192, rect.bottom-rect.top - 34 - 24, 0);

			SetWindowPos(gotodefbox, NULL, 0, rect.bottom-rect.top - 33 - 24, 160, 24, 0);
			SetWindowPos(gotodefaccept, NULL, 160, rect.bottom-rect.top - 33 - 24, 32, 24, 0);
			SetWindowPos(mdibox?mdibox:outputbox, NULL, 192, 0, rect.right-rect.left-192, rect.bottom-rect.top - 32, 0);
		}
		else
			SetWindowPos(mdibox?mdibox:outputbox, NULL, 0, 0, rect.right-rect.left, rect.bottom-rect.top - 32, 0);
		width = (rect.right-rect.left);
		width/=NUMBUTTONS;
		for (i = 0; i < NUMBUTTONS; i++)
		{
			SetWindowPos(buttons[i].hwnd, NULL, width*i, rect.bottom-rect.top - 32, width, 32, 0);
		}
		break;
	case WM_PAINT:
		hdc=BeginPaint(hWnd,(LPPAINTSTRUCT)&ps);

		EndPaint(hWnd,(LPPAINTSTRUCT)&ps);
		return TRUE;
		break;
	case WM_COMMAND:
		if (wParam == 0x4404)
		{
			GetWindowText(gotodefbox, finddef, sizeof(finddef)-1);
			return true;
		}
		if (LOWORD(wParam)>0 && LOWORD(wParam) <= NUMBUTTONS)
		{
			if (LOWORD(wParam))
				buttons[LOWORD(wParam)-1].washit = 1;
			break;
		}
		if (LOWORD(wParam) < IDM_FIRSTCHILD)
		{
			HWND ew;
			editor_t *editor;
	
			ew = (HWND)SendMessage(mdibox, WM_MDIGETACTIVE, 0, 0);

			for (editor = editors; editor; editor = editor->next)
			{
				if (editor->window == ew)
					break;
			}
			if (editor)
				EditorMenu(editor, wParam);
			else
				GenericMenu(wParam);
			break;
		}
		break;
	case WM_NOTIFY:
		if (lParam)
		{
			NMHDR *nm;
			HANDLE item;
			TVITEM i;
			char filename[256];
			char itemtext[256];
			int oldlen;
			int newlen;
			nm = (NMHDR*)lParam;
			if (nm->hwndFrom == projecttree)
			{
				switch(nm->code)
				{
				case NM_DBLCLK:
					item = TreeView_GetSelection(projecttree);
					i.hItem = item;
					i.mask = TVIF_TEXT;
					i.pszText = itemtext;
					i.cchTextMax = sizeof(itemtext)-1;
					if (!TreeView_GetItem(projecttree, &i))
						return 0;
					strcpy(filename, i.pszText);
					while(item)
					{
						item = TreeView_GetParent(projecttree, item);
						i.hItem = item;
						if (!TreeView_GetItem(projecttree, &i))
							break;
						if (!TreeView_GetParent(projecttree, item))
							break;

						oldlen = strlen(filename);
						newlen = strlen(i.pszText);
						memmove(filename+newlen+1, filename, oldlen+1);
						filename[newlen] = '/';
						strncpy(filename, i.pszText, newlen);
					}
					EditFile(filename, -1);
					break;
				}
			}
		}
	default:
		if (mdibox)
			return DefFrameProc(hWnd,mdibox,message,wParam,lParam);
		else
			return DefWindowProc(hWnd,message,wParam,lParam);
	}
	return 0;
}

static LONG CALLBACK OutputWindowProc(HWND hWnd,UINT message,
				     WPARAM wParam,LPARAM lParam)
{
	RECT rect;
	switch (message)
	{
	case WM_DESTROY:
		outputwindow = NULL;
		outputbox = NULL;
		break;
	case WM_CREATE:
		outputbox = CreateAnEditControl(hWnd);
	case WM_SIZE:
		GetClientRect(hWnd, &rect);
		SetWindowPos(outputbox, NULL, 0, 0, rect.right-rect.left, rect.bottom-rect.top, 0);
	default:
		return DefMDIChildProc(hWnd,message,wParam,lParam);
	}
	return 0;
}

void GUIPrint(HWND wnd, char *msg)
{
	MSG        wmsg;
	int len;
	static int writing;

	if (writing)
		return;
	if (!mainwindow)
	{
		printf("%s", msg);
		return;
	}
	writing=true;
	len=Edit_GetTextLength(wnd);
/*	if ((unsigned)len>(32767-strlen(msg)))
		Edit_SetSel(wnd,0,len);
	else*/
		Edit_SetSel(wnd,len,len);
	Edit_ReplaceSel(wnd,msg);

	while (PeekMessage (&wmsg, NULL, 0, 0, PM_NOREMOVE))
	{
		if (!GetMessage (&wmsg, NULL, 0, 0))
			break;
		TranslateMessage (&wmsg);
		DispatchMessage (&wmsg);
	}
	writing=false;
}
int GUIEmitOutputText(HWND wnd, int start, char *text, int len, DWORD colour)
{
	int c, cr;
	CHARFORMAT cf;

	if (!len)
		return start;

	c = text[len];
	text[len] = '\0';
	Edit_SetSel(wnd,start,start);
	Edit_ReplaceSel(wnd,text);

	text[len] = c;

	cr = 0;
	for (c = 0; c < len; c++)
		if (text[c] == '\r')
			cr++;
	if (cr)
		len-=cr;

	Edit_SetSel(wnd,start,start+len);
	memset(&cf, 0, sizeof(cf));
	cf.cbSize = sizeof(cf);
	cf.dwMask = CFM_COLOR;
	cf.crTextColor = colour;
	SendMessage(wnd, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
	Edit_SetSel(wnd,start+len,start+len);
	Edit_ScrollCaret(wnd);

	return start + len;
}
int outlen;
int outstatus;
int GUIprintf(const char *msg, ...)
{
	va_list		argptr;
	char		buf[1024];
	char rn[3] = "\n";
	char *st, *s;
	int args;
	MSG        wmsg;

	DWORD col;

	va_start (argptr,msg);
	args = QC_vsnprintf (buf,sizeof(buf)-1, msg,argptr);
	va_end (argptr);

	printf("%s", buf);
	if (logfile)
		fprintf(logfile, "%s", buf);

	if (!*buf)
	{
		/*clear text*/
		SetWindowText(outputbox,"");
		outlen = 0;

		/*make sure its active so we can actually scroll. stupid windows*/
		SetFocus(outputwindow);
		SetFocus(outputbox);

		/*colour background to default*/
		TreeView_SetBkColor(projecttree, -1);
		outstatus = 0;
		return 0;
	}

	if (strstr(buf, ": error"))
	{
		if (outstatus < 2)
		{
			TreeView_SetBkColor(projecttree, RGB(255, 0, 0));
			outstatus = 2;
		}
		col = RGB(255, 0, 0);
	}
	else if (strstr(buf, ": warning"))
	{
		if (outstatus < 1)
		{
			TreeView_SetBkColor(projecttree, RGB(255, 255, 0));
			outstatus = 1;
		}
		col = RGB(128, 128, 0);
	}
	else
		col = RGB(0, 0, 0);

	s = st = buf;
	while(*s)
	{
		if (*s == '\n')
		{
			*s = '\0';
			if (*st)
				outlen = GUIEmitOutputText(outputbox, outlen, st, strlen(st), col);
			outlen = GUIEmitOutputText(outputbox, outlen, rn, 1, col);
			st = s+1;
		}

		s++;
	}
	if (*st)
		outlen = GUIEmitOutputText(outputbox, outlen, st, strlen(st), col);

	while (PeekMessage (&wmsg, NULL, 0, 0, PM_NOREMOVE))
	{
		if (!GetMessage (&wmsg, NULL, 0, 0))
			break;
		TranslateMessage (&wmsg);
		DispatchMessage (&wmsg);
	}
/*
	s = st = buf;
	while(*s)
	{
		if (*s == '\n')
		{
			*s = '\0';
			if (*st)
				GUIPrint(outputbox, st);
			GUIPrint(outputbox, "\r\n");
			st = s+1;
		}

		s++;
	}
	if (*st)
		GUIPrint(outputbox, st);
*/
	return args;
}

#undef Sys_Error

void Sys_Error(const char *text, ...);
void RunCompiler(char *args)
{
	char *argv[128];
	int argc;
	progexterns_t ext;
	progfuncs_t funcs;

	editor_t *editor;
	for (editor = editors; editor; editor = editor->next)
	{
		if (editor->modified)
		{
			if (EditorModified(editor))
			{
				char msg[1024];
				sprintf(msg, "%s is modified in both memory and on disk. Overwrite external modification? (saying no will reload from disk)", editor->filename);
				switch(MessageBox(NULL, msg, "Modification conflict", MB_YESNOCANCEL))
				{
				case IDYES:
					EditorSave(editor);
					break;
				case IDNO:
					EditorReload(editor);
					break;
				case IDCANCEL:
					break; /*compiling will use whatever is in memory*/
				}
			}
			else
			{
				/*not modified on disk, but modified in memory? try and save it, cos we might as well*/
				EditorSave(editor);
			}
		}
		else
		{
			/*modified on disk but not in memory? just reload it off disk*/
			if (EditorModified(editor))
				EditorReload(editor);
		}
	}

	memset(&funcs, 0, sizeof(funcs));
	funcs.funcs.parms = &ext;
	memset(&ext, 0, sizeof(ext));
	ext.ReadFile = GUIReadFile;
	ext.FileSize = GUIFileSize;
	ext.WriteFile = QCC_WriteFile;
	ext.Printf = GUIprintf;
	ext.Sys_Error = Sys_Error;
	GUIprintf("");
	
	if (logfile)
		fclose(logfile);
	if (fl_log)
		logfile = fopen("fteqcc.log", "wb");
	else
		logfile = NULL;

	argc = GUI_BuildParms(args, argv);

	CompileParams(&funcs, true, argc, argv);

	if (logfile)
		fclose(logfile);
}


void CreateOutputWindow(void)
{
	WNDCLASS wndclass;
	MDICREATESTRUCT mcs;

	if (!mdibox)	//should already be created
		return;

	if (!outputwindow)
	{
		wndclass.style      = 0;
		wndclass.lpfnWndProc   = (WNDPROC)OutputWindowProc;
		wndclass.cbClsExtra    = 0;
		wndclass.cbWndExtra    = 0;
		wndclass.hInstance     = ghInstance;
		wndclass.hIcon         = 0;
		wndclass.hCursor       = LoadCursor (NULL,IDC_ARROW);
		wndclass.hbrBackground = (void *)COLOR_WINDOW;
		wndclass.lpszMenuName  = 0;
		wndclass.lpszClassName = MAIN_WINDOW_CLASS_NAME;
		RegisterClass(&wndclass);



		mcs.szClass = MAIN_WINDOW_CLASS_NAME;
		mcs.szTitle = "Compiler output";
		mcs.hOwner = ghInstance;
		mcs.x = mcs.cx = CW_USEDEFAULT;
		mcs.y = mcs.cy = CW_USEDEFAULT;
		mcs.style = WS_OVERLAPPEDWINDOW;
		mcs.lParam = 0;

		outputwindow = (HWND) SendMessage (mdibox, WM_MDICREATE, 0, 
		(LONG_PTR) (LPMDICREATESTRUCT) &mcs); 

		ShowWindow(outputwindow, SW_SHOW);
	}

	//bring it to the front.
	SendMessage(mdibox, WM_MDIACTIVATE, (WPARAM)outputwindow, 0);
}

//progssrcname should already have been set.
void SetProgsSrc(void)
{
	FILE *f;

	HANDLE rootitem, pi;
	TVINSERTSTRUCT item;
	TV_ITEM parent;
	char parentstring[256];
	memset(&item, 0, sizeof(item));
	memset(&parent, 0, sizeof(parent));

	if (projecttree)
	{
		int size;
		char *buffer;
		char *slash;

		f = fopen (progssrcname, "rb");
		if (!f)
			return;
		fseek(f, 0, SEEK_END);
		size = ftell(f);
		fseek(f, 0, SEEK_SET);
		buffer = malloc(size+1);
		if (!buffer)
		{
			fclose(f);
			return;
		}
		buffer[size] = '\0';
		fread(buffer, 1, size, f);
		fclose(f);

		pr_file_p = QCC_COM_Parse(buffer);
		if (*qcc_token == '#')
		{
			free(buffer);	//aaaahhh! newstyle!
			return;
		}

		pr_file_p = QCC_COM_Parse(pr_file_p);	//we dont care about the produced progs.dat


		item.hParent = TVI_ROOT;
		item.hInsertAfter = TVI_SORT;
		item.item.pszText = progssrcname;
		item.item.mask = TVIF_TEXT;
		rootitem = (HANDLE)SendMessage(projecttree,TVM_INSERTITEM,0,(LPARAM)&item);
		while(pr_file_p)
		{
			pi = item.hParent = rootitem;
			item.item.pszText = qcc_token;
			while(slash = strchr(item.item.pszText, '/'))
			{
				*slash = '\0';
				item.hParent = TreeView_GetChild(projecttree, item.hParent);
				do
				{
					parent.hItem = item.hParent;
					parent.mask = TVIF_TEXT;
					parent.pszText = parentstring;
					parent.cchTextMax = sizeof(parentstring)-1;
					if (TreeView_GetItem(projecttree, &parent))
					{
						if (!stricmp(parent.pszText, item.item.pszText))
							break;
					}
				} while(item.hParent=TreeView_GetNextSibling(projecttree, item.hParent));
				if (!item.hParent)
				{	//add a directory.
					item.hParent = pi;
					pi = (HANDLE)SendMessage(projecttree,TVM_INSERTITEM,0,(LPARAM)&item);
					item.hParent = pi;
				}
				else pi = item.hParent;

				item.item.pszText = slash+1;
			}
			SendMessage(projecttree,TVM_INSERTITEM,0,(LPARAM)&item);
			pr_file_p = QCC_COM_Parse(pr_file_p);
		}

		free(buffer);
	}
}

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	unsigned int i;
	WNDCLASS wndclass;
	ghInstance= hInstance;

	GUI_SetDefaultOpts();

	if(strstr(lpCmdLine, "-stdout"))
	{
		GUI_ParseCommandLine(lpCmdLine);
		RunCompiler(lpCmdLine);
		return 0;
	}

	if (!*lpCmdLine)
	{
		int len;
		FILE *f;
		char *s;

		f = fopen("fteqcc.cfg", "rb");
		if (f)
		{
			fseek(f, 0, SEEK_END);
			len = ftell(f);
			fseek(f, 0, SEEK_SET);
			lpCmdLine = malloc(len+1);
			fread(lpCmdLine, 1, len, f);
			lpCmdLine[len] = '\0';
			fclose(f);

			while(s = strchr(lpCmdLine, '\r'))
				*s = ' ';
			while(s = strchr(lpCmdLine, '\n'))
				*s = ' ';
		}
	}

	GUI_ParseCommandLine(lpCmdLine);

	GUI_RevealOptions();

	if (/*!fl_acc &&*/ !*progssrcname)
	{
		strcpy(progssrcname, "preprogs.src");
		if (QCC_FileSize(progssrcname)==-1)
			strcpy(progssrcname, "progs.src");
		if (QCC_FileSize(progssrcname)==-1)
		{
			char *s, *s2;
			char filename[MAX_PATH];
			char oldpath[MAX_PATH+10];
			OPENFILENAME ofn;
			memset(&ofn, 0, sizeof(ofn));
			ofn.lStructSize = sizeof(ofn);
			ofn.hInstance = ghInstance;
			ofn.lpstrFile = filename;
			ofn.lpstrTitle = "Please find progs.src";
			ofn.nMaxFile = sizeof(filename)-1;
			ofn.lpstrFilter = "QuakeC source\0*.src\0All files\0*.*\0";
			memset(filename, 0, sizeof(filename));
			GetCurrentDirectory(sizeof(oldpath)-1, oldpath);
			ofn.lpstrInitialDir = oldpath;
			if (GetOpenFileName(&ofn))
			{
				strcpy(progssrcdir, filename);
				for(s = progssrcdir; s; s = s2)
				{
					s2 = strchr(s+1, '\\');
					if (!s2)
						break;
					s = s2;
				}
				if (s)
				{
					*s = '\0';
					strcpy(progssrcname, s+1);
				}
				else
					strcpy(progssrcname, filename);
			}
			else
			{
				MessageBox(NULL, "You didn't select a file", "Error", 0);
				return 0;
			}
			SetCurrentDirectory(progssrcdir);
			*progssrcdir = '\0';
		}
	}

	resetprogssrc = true;

    wndclass.style      = 0;
    wndclass.lpfnWndProc   = (WNDPROC)MainWndProc;
    wndclass.cbClsExtra    = 0;
    wndclass.cbWndExtra    = 0;
    wndclass.hInstance     = ghInstance;
    wndclass.hIcon         = 0;
    wndclass.hCursor       = LoadCursor (NULL,IDC_ARROW);
	wndclass.hbrBackground = (void *)COLOR_WINDOW;
    wndclass.lpszMenuName  = 0;
    wndclass.lpszClassName = MDI_WINDOW_CLASS_NAME;
	RegisterClass(&wndclass);

	mainwindow=CreateWindow(MDI_WINDOW_CLASS_NAME, "FTE QuakeC compiler", WS_OVERLAPPEDWINDOW,
		0, 0, 640, 480, NULL, NULL, ghInstance, NULL);

	if (mdibox)
	{
		SetWindowText(mainwindow, "FTE QuakeC Development Suite");
	}

	if (!mainwindow)
	{
		MessageBox(NULL, "Failed to create main window", "Error", 0);
		return 0;
	}

	InitCommonControls();
/*
	outputbox=CreateWindowEx(WS_EX_CLIENTEDGE,
		"EDIT",
		"",
		WS_CHILD | ES_READONLY | WS_VISIBLE | 
		WS_VSCROLL | ES_LEFT | ES_WANTRETURN |
		ES_MULTILINE | ES_AUTOVSCROLL,
		0, 0, 0, 0,
		mainwindow,
		NULL,
		ghInstance,
		NULL);
*/

	if (!mdibox)
		outputbox = CreateAnEditControl(mainwindow);

	for (i = 0; i < NUMBUTTONS; i++)
	{
		buttons[i].hwnd = CreateWindowEx(WS_EX_CLIENTEDGE,
			"BUTTON",
			buttons[i].text,
			WS_CHILD | WS_VISIBLE,
			0, 0, 5, 5,
			mainwindow,
			(HMENU)(i+1),
			ghInstance,
			NULL); 
	}

	ShowWindow(mainwindow, SW_SHOWDEFAULT);

	if (fl_compileonstart)
	{
		CreateOutputWindow();
		RunCompiler(lpCmdLine);
	}
	else
	{
		if (mdibox)
		{
			buttons[ID_EDIT].washit = true;
		}
		else
		{
			GUIprintf("Welcome to FTE QCC\n");
			GUIprintf("Source file: ");
			GUIprintf(progssrcname);
			GUIprintf("\n");

			RunCompiler("-?");
		}
	}

	while(mainwindow || editors)
	{
		MSG        msg;

		if (resetprogssrc)
		{	//this here, with the compiler below, means that we don't run recursivly.
			resetprogssrc = false;
			SetProgsSrc();
		}

		EditorsRun();

		while (PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE))
		{
			if (!GetMessage (&msg, NULL, 0, 0))
				break;
			if (!mdibox || !TranslateMDISysAccel(mdibox, &msg))
			{ 
				TranslateMessage (&msg);
				DispatchMessage (&msg);
			}
		}

		if (mainwindow)
		{
			i = Edit_GetSel(outputbox);
			if ((i>>16) != (i&0xffff) && i != -1)	//some text is selected.
			{
				int bytes;
				char line[1024];
				char *colon1, *colon2 = NULL;

				int l1;
				int l2;

				l1 = Edit_LineFromChar(outputbox, i&0xffff);
				l2 = Edit_LineFromChar(outputbox, (i>>16)&0xffff);
				if (l1 == l2)
				{
					bytes = Edit_GetLine(outputbox, Edit_LineFromChar(outputbox, i&0xffff), line, sizeof(line)-1);
					line[bytes] = 0;

					for (colon1 = line+strlen(line)-1; *colon1 <= ' ' && colon1>=line; colon1--)
						*colon1 = '\0';
					if (!strncmp(line, "warning: ", 9))
						memmove(line, line+9, sizeof(line));
					colon1=line;
					do
					{
						colon1 = strchr(colon1+1, ':');
					} while (colon1 && colon1[1] == '\\');

					if (colon1)
					{
						colon2 = strchr(colon1+1, ':');
						while (colon2 && colon2[1] == '\\')
						{
							colon2 = strchr(colon2+1, ':');
						}
						if (colon2)
						{
							*colon1 = '\0';
							*colon2 = '\0';
							EditFile(line, atoi(colon1+1)-1);
						}
						else if (!strncmp(line, "Source file: ", 13))
							EditFile(line+13, -1);
						else if (!strncmp(line, "Including: ", 11))
							EditFile(line+11, -1);
					}
					else if (!strncmp(line, "compiling ", 10))
						EditFile(line+10, -1);
					else if (!strncmp(line, "prototyping ", 12))
						EditFile(line+12, -1);
					Edit_SetSel(outputbox, i&0xffff, i&0xffff);	//deselect it.
				}
			}

			if (buttons[ID_COMPILE].washit)
			{
				CreateOutputWindow();
				RunCompiler(parameters);

				buttons[ID_COMPILE].washit = false;
			}
			if (buttons[ID_EDIT].washit)
			{
				buttons[ID_EDIT].washit = false;
				EditFile(progssrcname, -1);
			}
			if (buttons[ID_OPTIONS].washit)
			{
				buttons[ID_OPTIONS].washit = false;
				OptionsDialog();
			}
			if (buttons[ID_QUIT].washit)
			{
				buttons[ID_QUIT].washit = false;
				DestroyWindow(mainwindow);
			}
		}

		if (*finddef)
		{
			GoToDefinition(finddef);
			*finddef = '\0';
		}

		Sleep(10);
	}

	return 0;
}
