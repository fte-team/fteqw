#include "../plugin.h"

int K_UPARROW;
int K_DOWNARROW;
int K_LEFTARROW;
int K_RIGHTARROW;
int K_ESCAPE;
int K_MOUSE1;
int K_MOUSE2;
int K_HOME;
int K_SHIFT;
int K_MWHEELDOWN;
int K_MWHEELUP;
int K_PAGEUP;
int K_PAGEDOWN;
int K_BACKSPACE;

qhandle_t con_chars;
qhandle_t pic_cursor;

float drawscalex;
float drawscaley;

unsigned char namebuffer[256];
int insertpos;
unsigned int currenttime;

void LoadPics(void)
{
	char buffer[256];

//main bar (add cvars later)
	con_chars = Draw_LoadImage("conchars", false);
	Cvar_GetString("cl_cursor", buffer, sizeof(buffer));
	if (*buffer)
		pic_cursor = Draw_LoadImage(buffer, false);
	else
		pic_cursor = NULL;
}

void DrawChar(unsigned int c, int x, int y)
{
static float size = 1.0f/16.0f;
	float s1 = size * (c&15);
	float t1 = size * (c>>4);
	Draw_Image((float)x*drawscalex, y*drawscaley, 16*drawscalex, 16*drawscaley, s1, t1, s1+size, t1+size, con_chars);
}

void InsertChar(int newchar)
{
	int oldlen;

	oldlen = strlen(namebuffer);
	if (oldlen + 1 == sizeof(namebuffer))
		return;
	namebuffer[oldlen+1] = 0;
	for (; oldlen > insertpos; oldlen--)
		namebuffer[oldlen] = namebuffer[oldlen-1];

	namebuffer[insertpos++] = newchar;
}

void KeyPress(int key, int mx, int my)
{
	int newchar;
	int oldlen;
	if (key == K_ESCAPE)
	{
		Menu_Control(0);
		Cvar_SetString("name", (char*)namebuffer);
	}
	else if (key == K_MOUSE1)
	{
		mx -= ((640 - (480-16))/2);
		my -= 16;
		mx /= (480-16)/16;
		my /= (480-16)/16;

		newchar = (int)mx + (int)my * 16;

		InsertChar(newchar);
	}
	else if (key == K_MOUSE2 || key == K_BACKSPACE)
	{
		if (insertpos > 0)
			insertpos--;
		for (oldlen = insertpos; namebuffer[oldlen]; oldlen++)
			namebuffer[oldlen] = namebuffer[oldlen+1];
	}
	else if (key == K_LEFTARROW)
	{
		insertpos--;
		if (insertpos < 0)
			insertpos = 0;
	}
	else if (key == K_RIGHTARROW)
	{
		insertpos++;
		if (insertpos > strlen(namebuffer))
			insertpos = strlen(namebuffer);
	}
	else if (key == K_SHIFT)
		return;
	else if (key > 0 && key < 255)
		InsertChar(key);
}

int Plug_MenuEvent(int *args)
{
	int i;
	float cbias;
	drawscalex = vid.width/640.0f;
	drawscaley = vid.height/480.0f;

	args[2]=(int)(args[2]/drawscalex);
	args[3]=(int)(args[3]/drawscaley);

	switch(args[0])
	{
	case 0:	//draw

		Draw_Colour4f(1,1,1,1);

		Draw_Image(((640 - (480-16))/2)*drawscalex, 16*drawscaley, (480-16)*drawscalex, (480-16)*drawscaley, 0, 0, 1, 1, con_chars);

		for (i = 0; namebuffer[i]; i++)
			DrawChar(namebuffer[i], i*16, 0);
		DrawChar(10 + (((currenttime/250)&1)==1), insertpos*16, 0);

		cbias = Cvar_GetFloat("cl_cursorbias");
		if (!pic_cursor || Draw_Image((float)(args[2]-cbias)*drawscalex, (float)(args[3]-cbias)*drawscaley, (float)32*drawscalex, (float)32*drawscaley, 0, 0, 1, 1, pic_cursor) <= 0)
			DrawChar('+', args[2]-4, args[3]-4);
		break;
	case 1:	//keydown
		KeyPress(args[1], args[2], args[3]);
		break;
	case 2:	//keyup
		break;
	case 3:	//menu closed (this is called even if we change it).
		break;
	case 4:	//mousemove
		break;
	}

	return 0;
}

int Plug_Tick(int *args)
{
	currenttime = args[0];
	return true;
}

int Plug_ExecuteCommand(int *args)
{
	char cmd[256];
	Cmd_Argv(0, cmd, sizeof(cmd));
	if (!strcmp("namemaker", cmd))
	{
		Menu_Control(1);
		Cvar_GetString("name", (char*)namebuffer, sizeof(namebuffer));
		insertpos = strlen(namebuffer);
		return 1;
	}
	return 0;
}

int Plug_Init(int *args)
{
	if (Plug_Export("Tick", Plug_Tick) &&
//		Plug_Export("SbarBase", UI_StatusBar) &&
//		Plug_Export("SbarOverlay", UI_ScoreBoard) &&
		Plug_Export("ExecuteCommand", Plug_ExecuteCommand) &&
		Plug_Export("MenuEvent", Plug_MenuEvent))
	{

		K_UPARROW		= Key_GetKeyCode("uparrow");
		K_DOWNARROW		= Key_GetKeyCode("downarrow");
		K_LEFTARROW		= Key_GetKeyCode("leftarrow");
		K_RIGHTARROW	= Key_GetKeyCode("rightarrow");
		K_ESCAPE		= Key_GetKeyCode("escape");
		K_HOME			= Key_GetKeyCode("home");
		K_MOUSE1		= Key_GetKeyCode("mouse1");
		K_MOUSE2		= Key_GetKeyCode("mouse2");
		K_MWHEELDOWN	= Key_GetKeyCode("mwheeldown");
		K_MWHEELUP		= Key_GetKeyCode("mwheelup");
		K_SHIFT			= Key_GetKeyCode("shift");
		K_PAGEUP		= Key_GetKeyCode("pgup");
		K_PAGEDOWN		= Key_GetKeyCode("pgdn");
		K_BACKSPACE		= Key_GetKeyCode("backspace");

		Cmd_AddCommand("namemaker");

		LoadPics();

		return 1;
	}
	return 0;
}
