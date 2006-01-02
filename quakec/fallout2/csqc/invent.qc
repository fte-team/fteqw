void bprint(float plev, string st, string s2){}
float PRINT_MEDIUM = 1;
#include "/../inventory.qc"

float show_inventory;

vector mousepos;

float showcontextmenu;
vector contextpos;
float slotnum;

vector toppos = '0 32 0';

float k_mouse1;

void() Invent_Draw =
{
	local float i;
	local float it;
	local string itname;
	local float op;

	for (i = 0; i < 16; i++)
	{
		it = getstati(32+i);
		itname = GetItemName(ToIID(it));
		drawstring(toppos + (i * '0 8 0'), strcat(itname, " (", ftos(ToStatus(it)), ")"), '8 8 0', '1 1 1', 1);
	}

	if (showcontextmenu)
	{
		op = floor((mousepos_y - contextpos_y)/8);
		drawfill(contextpos - '8 8 0', '88 24 0'+'16 16 0', '0 0 0', 0.7);
		drawstring(contextpos + (0 * '0 8 0'), "Use/Reload", '8 8 0', '1 1 0' + (op!=0)*'0 0 1', 1);
		drawstring(contextpos + (1 * '0 8 0'), "Put In Hand", '8 8 0', '1 1 0' + (op!=1)*'0 0 1', 1);
		drawstring(contextpos + (2 * '0 8 0'), "Drop", '8 8 0', '1 1 0' + (op!=2)*'0 0 1', 1);
	}
	else
		slotnum = floor((mousepos_y - toppos_y)/8) + 1;


	drawfill(mousepos, '8 8 0', '0 0 0', 0.7);
	drawstring(mousepos, "^", '8 8 0', '1 1 1', 1);
//	drawpic(mousepos, "gui/cursor.jpg", '32 32 0', '1 1 1', 1);
};


float(float eventtype, float param1, float param2) CSQC_InputEvent =
{
	local float op;
	if (eventtype == 0)
	{
		if (param1 == 'i')
			show_inventory = !show_inventory;
		else if (!show_inventory)
			return false;
		else if (param1 == k_mouse1)
		{
			if (showcontextmenu)
			{
				op = floor((mousepos_y - contextpos_y)/8);

				if (op == 0)
					localcmd(strcat("cmd invuse ", ftos(slotnum), "\n"));
				else if (op == 1)
					localcmd(strcat("cmd invswap 1 ", ftos(slotnum), "\nimpulse 1\n"));
				else if (op == 2)
					localcmd(strcat("cmd invdrop ", ftos(slotnum), "\n"));


				showcontextmenu = false;
			}
			else
			{
				if (slotnum >= 1 && slotnum <= MAXSLOTS)
				{
					if (getstati(32+slotnum-1) != 0)
					{
						showcontextmenu = true;
						contextpos = mousepos;
					}
				}
				else
					show_inventory = false;
			}
		}
		else
			return false;
		return true;
	}
	if (eventtype == 2 && show_inventory)	//mouse
	{
		mousepos_x += param1;
		mousepos_y += param2;

		if (mousepos_x < 0)
			mousepos_x = 0;
		if (mousepos_y < 0)
			mousepos_y = 0;

		return true;
	}
	return false;
};

void() FigureOutButtons =
{
	k_mouse1 = stringtokeynum("K_MOUSE1");
};
