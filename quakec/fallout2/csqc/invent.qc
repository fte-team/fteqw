void bprint(float plev, string st, string s2){}
float PRINT_MEDIUM = 1;
#include "/../inventory.qc"

float show_inventory;

vector mousepos;

float showcontextmenu;
vector contextpos;

float slotnum;	//the current slot under the mousecursor

float sliderpos;

float downslotnum;	//the slot number when the mousecursor when down

vector toppos = '0 32 0';

float k_mouse1;
float mouseisdown;

void(vector pos, float slotno) SlotImage =
{
	local float it;
	local string itname;

	if (slotno == downslotnum && slotnum)
		slotno = slotnum;
	else if (slotno == slotnum && downslotnum)
		slotno = downslotnum;

	it = getstati(31+slotno);

	itname = GetItemImage(ToIID(it));
	drawpic(pos, strcat("gui/", itname), '96 96 0', '1 1 1', 1);
};

void() Invent_Draw =
{
	local float i;
	local float it;
	local string itname;
	local float op;
	
	local float height;
	height = cvar("vid_conheight");
	//how much space have we got for the slider?


	local float slotofs;
	slotofs = sliderpos*96*8 + 96*3;

	for (i = 3; i < 16; i++)
	{
		SlotImage(((i*96-slotofs) * '0 1 0'), i+1);

//		itname = GetItemName(ToIID(it));
//		drawstring(toppos + ((i*8-slotofs) * '0 1 0'), strcat(itname, " (", ftos(ToStatus(it)), ")"), '8 8 0', '1 1 1', 1);
	}

	drawstring('112 248 0', "   HAND 1   ", '8 8 8', '1 1 1', 1);
	SlotImage('112 256 0', 1);
	drawstring('112 248 0'+'96 0 0', "   HAND 2   ", '8 8 8', '1 1 1', 1);
	SlotImage('112 256 0'+'96 0 0', 2);
	drawstring('112 248 0'+'192 0 0', "   ARMOUR   ", '8 8 8', '1 1 1', 1);
	SlotImage('112 256 0'+'192 0 0', 3);



	drawpic('96 0 0', "gui/scrollup.jpg", '16 16 0', '1 1 1', 1);
	for (i = 0; i < height; i+=16)
		drawpic('96 16 0'+i*'0 1 0', "gui/scrollbar.jpg", '16 16 0', '1 1 1', 1);
	drawpic('96 0 0'+(height-16)*'0 1 0', "gui/scrolldown.jpg", '16 16 0', '1 1 1', 1);

	drawpic('96 16 0'+sliderpos*(height-48)*'0 1 0', "gui/scrollbox.jpg", '16 16 0', '1 1 1', 1);


	if (showcontextmenu)
	{
		op = floor((mousepos_y - contextpos_y)/8);
		drawfill(contextpos - '8 8 0', '88 24 0'+'16 16 0', '0 0 0', 0.7);
		drawstring(contextpos + (0 * '0 8 0'), "Use/Reload", '8 8 0', '1 1 0' + (op!=0)*'0 0 1', 1);
		drawstring(contextpos + (1 * '0 8 0'), "Put In Hand", '8 8 0', '1 1 0' + (op!=1)*'0 0 1', 1);
		drawstring(contextpos + (2 * '0 8 0'), "Drop", '8 8 0', '1 1 0' + (op!=2)*'0 0 1', 1);
	}
	else
	{
		if (mousepos_x >= 96)
		{
			slotnum = 0;
			if (mousepos_x >= 112 && mousepos_x <= 112+3*96)
				if (mousepos_y >= 256 && mousepos_y <= 256+96)
					slotnum = floor((mousepos_x - 112)/96) + 1;
		}
		else
		{
			slotnum = floor((mousepos_y + slotofs)/96) + 1;
		}
//			slotnum = floor((mousepos_y - toppos_y)/8) + 1;
	}

	drawstring('128 0 0', ftos(slotnum), '8 8 0', '1 1 1', 1);

	it = getstati(31+slotnum);
	itname = GetItemName(ToIID(it));
	drawstring('128 32 0', itname, '8 8 0', '1 1 1', 1);
	itname = GetItemDesc(ToIID(it));
	drawstring('128 48 0', itname, '8 8 0', '1 1 1', 1);


//	drawfill(mousepos, '8 8 0', '0 0 0', 0.7);
//	drawstring(mousepos, "^", '8 8 0', '1 1 1', 1);
	drawpic(mousepos, "gui/cursor.jpg", '32 32 0', '1 1 1', 1);
};

void() CalcScrollPos =
{
	sliderpos = (mousepos_y-24)/(cvar("vid_conheight")-48);
	if (sliderpos < 0)
		sliderpos = 0;
	if (sliderpos > 1)
		sliderpos = 1;
};

float(float eventtype, float param1, float param2) CSQC_InputEvent =
{
	local float op;
	if (eventtype == 0)	//key down
	{
		if (param1 == 'i')
		{
			show_inventory = !show_inventory;
		}
		else if (!show_inventory)
			return false;
		else if (param1 == k_mouse1)
		{
			if (mousepos_x >= 96 && mousepos_x <= 96+16)
			{
				mouseisdown = true;
				CalcScrollPos();
			}
			else
				downslotnum = slotnum;
		}
		else
			return false;
		return true;
	}
	if (eventtype == 1 && show_inventory)	//key up
	{
		if (param1 == k_mouse1)
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
				if (mousepos_x >= 96 && mousepos_x <= 96+16)
				{	//within the scrollbar
					mouseisdown = false;
				}

				else if (downslotnum == slotnum)	//mouse didn't move away
				{
					if (slotnum >= 1 && slotnum <= MAXSLOTS)
					{
						if (getstati(32+slotnum-1) != 0)	//if there's actually an item there
						{
							showcontextmenu = true;	//show the context menu
							contextpos = mousepos;
						}
					}
					else
					{
						show_inventory = false;	//they clicked outside, fools!
						showcontextmenu = false;
					}
				}
				else
				{
					//they dragged
					showcontextmenu = false;
					localcmd(strcat("cmd invswap ", ftos(downslotnum), " ", ftos(slotnum), "\nimpulse 1\n"));
				}
			}
			mouseisdown = false;
			downslotnum = 0;
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

		if (mouseisdown)
			CalcScrollPos();

		return true;
	}
	return false;
};

void() FigureOutButtons =
{
	k_mouse1 = stringtokeynum("K_MOUSE1");
};
