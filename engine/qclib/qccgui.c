#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <richedit.h>
#include <stdio.h>

#include "qcc.h"
#include "gui.h"

#define Edit_Redo(hwndCtl)                      ((BOOL)(DWORD)SNDMSG((hwndCtl), EM_REDO, 0L, 0L))


#define MAIN_WINDOW_CLASS_NAME "FTEMAINWINDOW"
#define EDIT_WINDOW_CLASS_NAME "FTEEDITWINDOW"
#define OPTIONS_WINDOW_CLASS_NAME "FTEOPTIONSWINDOW"

#define EM_GETSCROLLPOS  (WM_USER + 221)
#define EM_SETSCROLLPOS  (WM_USER + 222)

static pbool fl_hexen2;
static pbool fl_nokeywords_coexist;
static pbool fl_autohighlight;
static pbool fl_compileonstart;
static pbool fl_autoprototype;
static pbool fl_acc;

char parameters[16384];

char progssrcname[256];
char progssrcdir[256];


int GUIprintf(const char *msg, ...);
void GUIPrint(HWND wnd, char *msg);
char *QCC_ReadFile (char *fname, void *buffer, int len);
int QCC_FileSize (char *fname);
pbool QCC_WriteFile (char *name, void *data, int len);

void RunCompiler(char *args);

HINSTANCE ghInstance;
HMODULE richedit;



HWND mainwindow;
HWND outputbox;

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

	IDI_O_LEVEL0,
	IDI_O_LEVEL1,
	IDI_O_LEVEL2,
	IDI_O_LEVEL3,
	IDI_O_DEFAULT,
	IDI_O_DEBUG,
	IDI_CHANGE_PROGS_SRC
};


typedef struct editor_s {
	char filename[MAX_PATH];	//abs
	HWND window;
	HWND editpane;
	struct editor_s *next;
} editor_t;

editor_t *editors;

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
	}
	if (!editor)
		return DefWindowProc(hWnd,message,wParam,lParam);

	switch (message)
	{
	case WM_CLOSE:
	case WM_QUIT:
		if (EditorModified(editor))
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
		DestroyWindow(editor->window);
		break;
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
		break;
	case WM_SIZE:
		GetClientRect(editor->window, &rect);
		SetWindowPos(editor->editpane, NULL, 0, 0, rect.right-rect.left, rect.bottom-rect.top, 0);
		break;
	case WM_PAINT:
		hdc=BeginPaint(hWnd,(LPPAINTSTRUCT)&ps);

		EndPaint(hWnd,(LPPAINTSTRUCT)&ps);
		return TRUE;
		break;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDM_OPENNEW:
			QueryOpenFile();
			break;
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
		case IDM_ABOUT:
			MessageBox(NULL, "FTE QuakeC Compiler\nWritten by Forethough Entertainment.\nBasically that means it was written by Spike.\n\nIt has a few cool features, like a useful IDE.\n\nSupports:\nPrecompiler (with macros)\nArrays\n+= / -= / *= / /= operations.\nSwitch statements\nfor loops\nLots of optimisations.", "About", 0);
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
		}
		return DefWindowProc(hWnd,message,wParam,lParam);
	case WM_NOTIFY:
		{
		    NMHDR *nmhdr;
			SELCHANGE *sel;
			char message[2048];
			nmhdr = (NMHDR *)lParam;
			switch(nmhdr->code)
			{
			case EN_SELCHANGE:
				sel = (SELCHANGE *)nmhdr;
				sprintf(message, "%s:%i - FTEQCC Editor", editor->filename, 1+Edit_LineFromChar(editor->editpane, sel->chrg.cpMin));
				SetWindowText(editor->window, message);
				break;
			}
		}
		return DefWindowProc(hWnd,message,wParam,lParam);
	default:
		return DefWindowProc(hWnd,message,wParam,lParam);
	}
	return 0;
}

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

	SendMessage(edit->editpane, EM_SETEVENTMASK, 0, ENM_SELCHANGE);

	SendMessage(edit->editpane, EM_SETSCROLLPOS, 0, (LPARAM)&scrollpos);
	SendMessage(edit->editpane, EM_EXSETSEL, 0, (LPARAM) &chrg);

	InvalidateRect(edit->editpane, NULL, true);
	UpdateWindow(edit->editpane);

	return true;
}

void EditFile(char *name, int line)
{
	char title[1024];
	int flen;
	char *file;
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
			SetFocus(neweditor->window);
			SetFocus(neweditor->editpane);
			return;
		}
	}

	flen = QCC_FileSize(name);
	if (flen == -1)
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

	menu = CreateMenu();
	menufile = CreateMenu();
	menuhelp = CreateMenu();
	menunavig = CreateMenu();
	AppendMenu(menu, MF_POPUP, (UINT)menufile,	"&File");
	AppendMenu(menu, MF_POPUP, (UINT)menunavig,	"&Navigation");
	AppendMenu(menu, MF_POPUP, (UINT)menuhelp,	"&Help");
	AppendMenu(menufile, 0, IDM_OPENNEW,	"Open &new file ");
	AppendMenu(menufile, 0, IDM_SAVE,		"&Save          ");
//	AppendMenu(menufile, 0, IDM_FIND,		"&Find");
	AppendMenu(menufile, 0, IDM_UNDO,		"&Undo          Ctrl+Z");
	AppendMenu(menufile, 0, IDM_REDO,		"&Redo          Ctrl+Y");
	AppendMenu(menunavig, 0, IDM_GOTODEF, "Go to definition");
	AppendMenu(menunavig, 0, IDM_OPENDOCU, "Open selected file");
	AppendMenu(menuhelp, 0, IDM_ABOUT, "About");
	AppendMenu(menu, 0, IDM_HIGHTLIGHT, "H&ighlight");



	
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

	sprintf(title, "%s - FTEEditor", name);
	neweditor->window=CreateWindow(EDIT_WINDOW_CLASS_NAME, title, WS_OVERLAPPEDWINDOW,
		0, 0, 640, 480, NULL, NULL, ghInstance, NULL);

	SetMenu(neweditor->window, menu);

	if (!neweditor->window)
	{
		MessageBox(NULL, "Failed to create editor window", "Error", 0);
		return;
	}

	richedit = LoadLibrary("RICHED32.DLL");
	neweditor->editpane=CreateWindowEx(WS_EX_CLIENTEDGE,
		richedit?RICHEDIT_CLASS:"EDIT",
		"",
		WS_CHILD /*| ES_READONLY*/ | WS_VISIBLE | 
		WS_HSCROLL | WS_VSCROLL | ES_LEFT | ES_WANTRETURN |
		ES_MULTILINE | ES_AUTOVSCROLL,
		0, 0, 0, 0,
		neweditor->window,
		NULL,
		ghInstance,
		NULL);

	if (richedit)
	{
		SendMessage(neweditor->editpane, EM_EXLIMITTEXT, 0, 1<<31);

		SendMessage(neweditor->editpane, EM_SETUNDOLIMIT, 256, 256);
	}

	flen = QCC_FileSize(name);
	file = malloc(flen+1);
	QCC_ReadFile(name, file, flen);
	file[flen] = 0;

	SendMessage(neweditor->editpane, EM_SETEVENTMASK, 0, 0);

	if (!fl_autohighlight)
	{
		GUIPrint(neweditor->editpane, file);
	}
	else
	{
		GUIFormattingPrint(neweditor->editpane, file);
	}

	SendMessage(neweditor->editpane, EM_SETEVENTMASK, 0, ENM_SELCHANGE);

	if (line >= 0)
		Edit_SetSel(neweditor->editpane, Edit_LineIndex(neweditor->editpane, line), Edit_LineIndex(neweditor->editpane, line+1));
	else
		Edit_SetSel(neweditor->editpane, Edit_LineIndex(neweditor->editpane, 0), Edit_LineIndex(neweditor->editpane, 0));

	Edit_ScrollCaret(neweditor->editpane);

	ShowWindow(neweditor->window, SW_SHOWDEFAULT);
	SetFocus(neweditor->window);
	SetFocus(neweditor->editpane);
}

int EditorSave(editor_t *edit)
{
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

	return true;
}
void EditorsRun(void)
{
}


char *GUIReadFile(char *fname, void *buffer, int blen)
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

int GUIFileSize(char *fname)
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

pbool EditorModified(editor_t *e)
{
	char *buffer;
	int elen, flen;
	elen = Edit_GetTextLength(e->editpane);
	flen = QCC_FileSize(e->filename);

	if (elen != flen)
		return true;

	buffer = malloc(elen+flen);
	Edit_GetText(e->editpane, buffer, elen);
	QCC_ReadFile(e->filename, buffer+elen, flen);
	if (memcmp(buffer, buffer+elen, elen))
	{
		free(buffer);
		return true;
	}
	free(buffer);

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
		case IDRETRY:
		case IDOK:
			for (i = 0; optimisations[i].enabled; i++)
			{
				if (Button_GetCheck(optimisations[i].guiinfo))
					optimisations[i].flags |= 8;
				else
					optimisations[i].flags &= ~8;
			}
			fl_hexen2 = Button_GetCheck(hexen2item);
			fl_nokeywords_coexist = Button_GetCheck(nokeywords_coexistitem);
			fl_autohighlight = Button_GetCheck(autohighlight_item);
			fl_autoprototype = Button_GetCheck(autoprototype_item);
			Edit_GetText(extraparmsitem, parameters, sizeof(parameters)-1);

			if (wParam == IDRETRY)
				buttons[ID_COMPILE].washit = true;
			break;
		case IDI_CHANGE_PROGS_SRC:
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
			}
			break;
		case IDI_O_LEVEL0:
		case IDI_O_LEVEL1:
		case IDI_O_LEVEL2:
		case IDI_O_LEVEL3:
			for (i = 0; optimisations[i].enabled; i++)
			{
				if (optimisations[i].optimisationlevel<=(int)wParam-IDI_O_LEVEL0)
					Button_SetCheck(optimisations[i].guiinfo, 1);
				else
					Button_SetCheck(optimisations[i].guiinfo, 0);
			}
			break;
		case IDI_O_DEBUG:
			for (i = 0; optimisations[i].enabled; i++)
			{
				if (optimisations[i].flags&1)
					Button_SetCheck(optimisations[i].guiinfo, 0);
			}
			break;
		case IDI_O_DEFAULT:
			for (i = 0; optimisations[i].enabled; i++)
			{
				if (optimisations[i].flags & 2)
					Button_SetCheck(optimisations[i].guiinfo, 1);
				else
					Button_SetCheck(optimisations[i].guiinfo, 0);
			}
			break;
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

	int y;
	int height;

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
		height++;

	height = ((height-1)/2)*16;

	height += 88+40;

	r.left = GetSystemMetrics(SM_CXSCREEN)/2-320;
	r.top = GetSystemMetrics(SM_CYSCREEN)/2-240;
	r.right  = r.left + 640;
	r.bottom = r.top + height;

	AdjustWindowRectEx (&r, WS_CAPTION|WS_SYSMENU, FALSE, 0);

	optionsmenu=CreateWindow(OPTIONS_WINDOW_CLASS_NAME, "Options - FTE QuakeC compiler", WS_CAPTION|WS_SYSMENU,
		r.left, r.top, r.right-r.left, r.bottom-r.top, NULL, NULL, ghInstance, NULL);

	subsection = CreateWindow("BUTTON", "Optimisations", WS_CHILD|WS_VISIBLE|BS_GROUPBOX,
		0, 0, 400, height-48, optionsmenu, NULL, ghInstance, NULL);

	for (i = 0; optimisations[i].enabled; i++)
	{
		optimisations[i].guiinfo = wnd = CreateWindow("BUTTON",optimisations[i].fullname,
			   WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
			   8+200*(i&1),16+16*(i/2),200-16,16,
			   subsection,
			   (HMENU)i,
			   ghInstance,
			   NULL);

		if (optimisations[i].flags&8)
			Button_SetCheck(wnd, 1);
		else
			Button_SetCheck(wnd, 0);
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
		   (HMENU)IDOK,
		   ghInstance,
		   NULL);
	CreateWindow("BUTTON","Use",
		   WS_CHILD | WS_VISIBLE,
		   8+64,height-40,64,32,
		   optionsmenu,
		   (HMENU)IDRETRY,
		   ghInstance,
		   NULL);
	CreateWindow("BUTTON","progs.src",
		   WS_CHILD | WS_VISIBLE,
		   8+64*2,height-40,64,32,
		   optionsmenu,
		   (HMENU)IDI_CHANGE_PROGS_SRC,
		   ghInstance,
		   NULL);



		y=4;
	hexen2item = wnd = CreateWindow("BUTTON","HexenC",
		   WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
		   408,y,200-16,16,
		   optionsmenu,
		   (HMENU)i,
		   ghInstance,
		   NULL);
	y+=16;
	if (fl_hexen2)
		Button_SetCheck(wnd, 1);
	else
		Button_SetCheck(wnd, 0);

	nokeywords_coexistitem = wnd = CreateWindow("BUTTON","Disable Keywords",
		   WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
		   408,y,200-16,16,
		   optionsmenu,
		   (HMENU)i,
		   ghInstance,
		   NULL);
	y+=16;
	if (fl_nokeywords_coexist)
		Button_SetCheck(wnd, 1);
	else
		Button_SetCheck(wnd, 0);

	autohighlight_item = wnd = CreateWindow("BUTTON","Syntax Highlighting",
		   WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
		   408,y,200-16,16,
		   optionsmenu,
		   (HMENU)i,
		   ghInstance,
		   NULL);
	y+=16;
	if (fl_autohighlight)
		Button_SetCheck(wnd, 1);
	else
		Button_SetCheck(wnd, 0);

	autoprototype_item = wnd = CreateWindow("BUTTON","Automate prototypes",
		   WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
		   408,y,200-16,16,
		   optionsmenu,
		   (HMENU)i,
		   ghInstance,
		   NULL);
	y+=16;
	if (fl_autoprototype)
		Button_SetCheck(wnd, 1);
	else
		Button_SetCheck(wnd, 0);

	CreateWindow("STATIC","Extra Parameters:",
		   WS_CHILD | WS_VISIBLE,
		   408,y,200-16,16,
		   optionsmenu,
		   (HMENU)IDOK,
		   ghInstance,
		   NULL);
	y+=16;
	extraparmsitem = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT",parameters,
		   WS_CHILD | WS_VISIBLE|ES_LEFT | ES_WANTRETURN |
		ES_MULTILINE | ES_AUTOVSCROLL,
		   408,y,240-16+16-12,height-y-4,
		   optionsmenu,
		   (HMENU)IDI_O_DEFAULT,
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
	case WM_DESTROY:
		mainwindow = NULL;
		break;

	case WM_SIZE:
		GetClientRect(mainwindow, &rect);
		SetWindowPos(outputbox, NULL, 0, 0, rect.right-rect.left, rect.bottom-rect.top - 32, 0);
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
		if (LOWORD(wParam))
			buttons[LOWORD(wParam)-1].washit = 1;
		break;
	default:
		return DefWindowProc(hWnd,message,wParam,lParam);
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

	return start + len;
}
int outlen;
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

	if (!*buf)
	{
		SetWindowText(outputbox,"");
		outlen = 0;
		return 0;
	}

	if (strstr(buf, "warning: "))
		col = RGB(128, 128, 0);
	else if (strstr(buf, "error: "))
		col = RGB(255, 0, 0);
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

int BuildParms(char *args, char **argv)
{
	static char param[2048];
	int paramlen = 0;
	int argc;
	char *next;
	int i;


	argc = 1;
	argv[0] = "fteqcc";

	while(*args)
	{
		while (*args <= ' '&& *args)
			args++;

		for (next = args; *next>' '; next++)
			;
		strncpy(param+paramlen, args, next-args);
		param[paramlen+next-args] = '\0';
		argv[argc++] = param+paramlen;
		paramlen += strlen(param+paramlen)+1;

		args=next;
	}

	if (fl_hexen2)
	{
		strcpy(param+paramlen, "-Th2");
		argv[argc++] = param+paramlen;
		paramlen += strlen(param+paramlen)+1;
	}
	if (fl_nokeywords_coexist)
	{
		strcpy(param+paramlen, "-Fno-kce");
		argv[argc++] = param+paramlen;
		paramlen += strlen(param+paramlen)+1;
	}
	if (fl_autoprototype)
	{
		strcpy(param+paramlen, "-Fautoproto");
		argv[argc++] = param+paramlen;
		paramlen += strlen(param+paramlen)+1;
	}

	if (fl_acc)
	{
		strcpy(param+paramlen, "-Facc");
		argv[argc++] = param+paramlen;
		paramlen += strlen(param+paramlen)+1;
	}


	for (i = 0; optimisations[i].enabled; i++)	//enabled is a pointer
	{
		if (optimisations[i].flags & 8)
			sprintf(param+paramlen, "-O%s", optimisations[i].abbrev);
		else
			sprintf(param+paramlen, "-Ono-%s", optimisations[i].abbrev);
		argv[argc++] = param+paramlen;
		paramlen += strlen(param+paramlen)+1;
	}


/*	while(*args)
	{
		while (*args <= ' '&& *args)
			args++;

		for (next = args; *next>' '; next++)
			;
		strncpy(param+paramlen, args, next-args);
		param[paramlen+next-args] = '\0';
		argv[argc++] = param+paramlen;
		paramlen += strlen(param+paramlen)+1;
		args=next;
	}*/

	if (*progssrcname)
	{
		argv[argc++] = "-srcfile";
		argv[argc++] = progssrcname;
	}
	if (*progssrcdir)
	{
		argv[argc++] = "-src";
		argv[argc++] = progssrcdir;
	}

	return argc;
}

void Sys_Error(const char *text, ...);
void RunCompiler(char *args)
{
	char *argv[64];
	int argc;
	progexterns_t ext;
	progfuncs_t funcs;

	memset(&funcs, 0, sizeof(funcs));
	funcs.parms = &ext;
	memset(&ext, 0, sizeof(ext));
	funcs.parms->ReadFile = GUIReadFile;
	funcs.parms->FileSize = GUIFileSize;
	funcs.parms->WriteFile = QCC_WriteFile;
	funcs.parms->printf = GUIprintf;
	funcs.parms->Sys_Error = Sys_Error;
	GUIprintf("");
	

	argc = BuildParms(args, argv);

	CompileParams(&funcs, true, argc, argv);
}


//this function takes the windows specified commandline and strips out all the options menu items.
void GuiParseCommandLine(char *args)
{
	int paramlen=0;
	int l, p;
	char *next;
	while(*args)
	{
		while (*args <= ' ' && *args)
			args++;

		for (next = args; *next>' '; next++)
			;

		strncpy(parameters+paramlen, args, next-args);
		parameters[paramlen+next-args] = '\0';
		l = strlen(parameters+paramlen)+1;

		if (!strnicmp(parameters+paramlen, "-O", 2) || !strnicmp(parameters+paramlen, "/O", 2))
		{	//strip out all -O
			if (parameters[paramlen+2])
			{
				if (parameters[paramlen+2] >= '0' && parameters[paramlen+2] <= '3')
				{
					p = parameters[paramlen+2]-'0';
					for (l = 0; optimisations[l].enabled; l++)
					{
						if (optimisations[l].optimisationlevel<=p)
							optimisations[l].flags |= 8;
						else
							optimisations[l].flags &= ~8;
					}
				}
				else if (!strncmp(parameters+paramlen+2, "no-", 3))
				{
					if (parameters[paramlen+5])
					{
						for (p = 0; optimisations[p].enabled; p++)
							if ((*optimisations[p].abbrev && !strcmp(parameters+paramlen+5, optimisations[p].abbrev)) || !strcmp(parameters+paramlen+5, optimisations[p].fullname))
							{
								optimisations[p].flags &= ~8;
								break;
							}

						if (!optimisations[p].enabled)
						{
							parameters[paramlen+next-args] = ' ';
							paramlen += l;
						}
					}
				}
				else
				{
					for (p = 0; optimisations[p].enabled; p++)
						if ((*optimisations[p].abbrev && !strcmp(parameters+paramlen+2, optimisations[p].abbrev)) || !strcmp(parameters+paramlen+2, optimisations[p].fullname))
						{
							optimisations[p].flags |= 8;
							break;
						}

					if (!optimisations[p].enabled)
					{
						parameters[paramlen+next-args] = ' ';
						paramlen += l;
					}
				}
			}
		}
		else if (!strnicmp(parameters+paramlen, "-Fno-kce", 8) || !strnicmp(parameters+paramlen, "/Fno-kce", 8))	//keywords stuph
		{
			fl_nokeywords_coexist = true;
		}
		else if (!strnicmp(parameters+paramlen, "-Fkce", 5) || !strnicmp(parameters+paramlen, "/Fkce", 5))
		{
			fl_nokeywords_coexist = false;
		}
		else if (!strnicmp(parameters+paramlen, "-sh", 3) || !strnicmp(parameters+paramlen, "/sh", 3))
		{
			fl_autohighlight = true;
		}
		else if (!strnicmp(parameters+paramlen, "-autoproto", 10) || !strnicmp(parameters+paramlen, "/autoproto", 10))
		{
			fl_autoprototype = true;
		}
		else if (!strnicmp(parameters+paramlen, "-Facc", 5) || !strnicmp(parameters+paramlen, "/Facc", 5))
		{
			fl_acc = true;
		}
		else if (!strnicmp(parameters+paramlen, "-ac", 3) || !strnicmp(parameters+paramlen, "/ac", 3))
		{
			fl_compileonstart = true;
		}
		else if (!strnicmp(parameters+paramlen, "-T", 2) || !strnicmp(parameters+paramlen, "/T", 2))	//the target
		{
			if (!strnicmp(parameters+paramlen+2, "h2", 2))
			{
				fl_hexen2 = true;
			}
			else
			{
				fl_hexen2 = false;
				parameters[paramlen+next-args] = ' ';
				paramlen += l;
			}
		}
		else
		{
			parameters[paramlen+next-args] = ' ';
			paramlen += l;
		}

		args=next;
	}
	if (paramlen)
		parameters[paramlen-1] = '\0';
	else
		*parameters = '\0';
}

void SetDefaultOpts(void)
{
	int i;
	for (i = 0; optimisations[i].enabled; i++)	//enabled is a pointer
	{
		if (optimisations[i].flags & 2)
			optimisations[i].flags |= 8;
		else
			optimisations[i].flags &= ~8;
	}
}

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	unsigned int i;
	WNDCLASS wndclass;
	ghInstance= hInstance;

	SetDefaultOpts();

	if(strstr(lpCmdLine, "-stdout"))
	{
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

	GuiParseCommandLine(lpCmdLine);

	if (!fl_acc && !*progssrcname)
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

    wndclass.style      = 0;
    wndclass.lpfnWndProc   = (WNDPROC)MainWndProc;
    wndclass.cbClsExtra    = 0;
    wndclass.cbWndExtra    = 0;
    wndclass.hInstance     = ghInstance;
    wndclass.hIcon         = 0;
    wndclass.hCursor       = LoadCursor (NULL,IDC_ARROW);
	wndclass.hbrBackground = (void *)COLOR_WINDOW;
    wndclass.lpszMenuName  = 0;
    wndclass.lpszClassName = MAIN_WINDOW_CLASS_NAME;
	RegisterClass(&wndclass);

	mainwindow=CreateWindow(MAIN_WINDOW_CLASS_NAME, "FTE QuakeC compiler", WS_OVERLAPPEDWINDOW,
		0, 0, 640, 480, NULL, NULL, ghInstance, NULL);

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
	richedit = LoadLibrary("RICHED32.DLL");
	outputbox=CreateWindowEx(WS_EX_CLIENTEDGE,
		richedit?RICHEDIT_CLASS:"EDIT",
		"",
		WS_CHILD /*| ES_READONLY*/ | WS_VISIBLE | 
		WS_HSCROLL | WS_VSCROLL | ES_LEFT | ES_WANTRETURN |
		ES_MULTILINE | ES_AUTOVSCROLL,
		0, 0, 0, 0,
		mainwindow,
		NULL,
		ghInstance,
		NULL);

	if (!outputbox)
		outputbox=CreateWindowEx(WS_EX_CLIENTEDGE,
			richedit?RICHEDIT_CLASS10A:"EDIT",	//fall back to the earlier version
			"",
			WS_CHILD /*| ES_READONLY*/ | WS_VISIBLE | 
			WS_HSCROLL | WS_VSCROLL | ES_LEFT | ES_WANTRETURN |
			ES_MULTILINE | ES_AUTOVSCROLL,
			0, 0, 0, 0,
			mainwindow,
			NULL,
			ghInstance,
			NULL);

	if (!outputbox)
	{	//you've not got RICHEDIT installed properly, I guess
		FreeLibrary(richedit);
		richedit = NULL;
		outputbox=CreateWindowEx(WS_EX_CLIENTEDGE,
			"EDIT",
			"",
			WS_CHILD /*| ES_READONLY*/ | WS_VISIBLE | 
			WS_HSCROLL | WS_VSCROLL | ES_LEFT | ES_WANTRETURN |
			ES_MULTILINE | ES_AUTOVSCROLL,
			0, 0, 0, 0,
			mainwindow,
			NULL,
			ghInstance,
			NULL);
	}
	if (richedit)
	{
		SendMessage(outputbox, EM_EXLIMITTEXT, 0, 1<<20);
	}


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
		RunCompiler(lpCmdLine);
	}
	else
	{
		GUIprintf("Welcome to FTE QCC\n");
		GUIprintf("Source file: ");
		GUIprintf(progssrcname);
		GUIprintf("\n");

		RunCompiler("-?");
	}

	while(mainwindow || editors)
	{
		MSG        msg;

		EditorsRun();

		while (PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE))
		{
			if (!GetMessage (&msg, NULL, 0, 0))
				break;
			TranslateMessage (&msg);
			DispatchMessage (&msg);
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

		Sleep(10);
	}

	return 0;
}
