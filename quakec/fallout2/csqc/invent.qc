void bprint(float plev, string st, string s2){}
float PRINT_MEDIUM = 1;
#include "../inventory.qc"

float show_inventory;
float show_pipboy;
float show_trader;
float show_perks;
float show_items;
float show_gainperk;
float scrwidth;
float scrheight;

vector mousepos;

float showcontextmenu;
float showmixmenu;

vector contextpos;

float slotnum;	//the current slot under the mousecursor

float sliderpos;

float downslotnum;	//the slot number when the mousecursor when down

float selected_class;
float perkslot;
float which_perk;

vector toppos = '0 32 0';

float k_mouse1;
float k_mwheelup;
float k_mwheeldown;
float mouseisdown;

#define IMGSIZEF 64
#define IMGSIZEV ('1 1 0'*(scrwidth/10))
#define IMGSIZEK 32
#define IMGSIZEH ('1 1 0'*IMGSIZEK)
#define IMGSIZEX 48
#define IMGSIZEZ ('1 1 0'*IMGSIZEX)
#define IMGSIZEQ ('1 1 0'*2)

#define CURSORSIZE 8




void(vector pos, float slotno) TraderImage =
{
	local float it, mod_s, mod_e;
	local string itname;


	if (slotno == downslotnum && slotnum)
		slotno = slotnum;
	else if (slotno == slotnum && downslotnum)
		slotno = downslotnum;

	if (slotno <= 20)
		it = getstati(70+slotno);


	itname = GetItemImage(ToIID(it));
	drawpic(pos, strcat("gui/", itname), IMGSIZEV, '1 1 1', 1);

	it = ToStatus(it);
	if (it <= 1)
		return;
	itname = ftos(it);
	it = strlen(itname);
	drawstring(pos + IMGSIZEV - '0 8 0' - '8 0 0'*it, itname, '8 8 8', '1 1 1', 1);
};


void(vector pos, float slotno) SlotImage =
{
	local float it, mod_s, mod_e;
	local string itname;


	if (slotno == downslotnum && slotnum)
		slotno = slotnum;
	else if (slotno == slotnum && downslotnum)
		slotno = downslotnum;

	if (slotno <= 16)
		it = getstati(31+slotno);
	else
	{
		if (slotno == 17)
			it = getstati(91);
		if (slotno == 18)
			it = getstati(92);
		if (slotno == 19)
			it = getstati(93);
		if (slotno == 20)
			it = getstati(94);
		if (slotno == 21)
			it = getstati(95);
		if (slotno == 22)
			it = getstati(96);
		if (slotno == 23)
			it = getstati(97);
		if (slotno == 24)
			it = getstati(98);
		if (slotno == 25)
			it = getstati(104);
		if (slotno == 26)
			it = getstati(105);
	}

	itname = GetItemImage(ToIID(it));
	drawpic(pos, strcat("gui/", itname), IMGSIZEV, '1 1 1', 1);

	it = ToStatus(it);
	if (it <= 1)
		return;
	itname = ftos(it);
	it = strlen(itname);
	drawstring(pos + IMGSIZEV - '0 8 0' - '8 0 0'*it, itname, '8 8 8', '1 1 1', 1);
};

void(vector pos, float slotno, vector imgsize) SlotImageBig =
{
	local float it, mod_s, mod_e;
	local string itname;


	if (slotno == downslotnum && slotnum)
		slotno = slotnum;
	else if (slotno == slotnum && downslotnum)
		slotno = downslotnum;

	if (slotno <= 16)
		it = getstati(31+slotno);
	else
	{
		if (slotno == 17)
			it = getstati(91);
		if (slotno == 18)
			it = getstati(92);
		if (slotno == 19)
			it = getstati(93);
		if (slotno == 20)
			it = getstati(94);
		if (slotno == 21)
			it = getstati(95);
		if (slotno == 22)
			it = getstati(96);
		if (slotno == 23)
			it = getstati(97);
		if (slotno == 24)
			it = getstati(98);
		if (slotno == 25)
			it = getstati(104);
		if (slotno == 26)
			it = getstati(105);
	}

	itname = GetItemImage(ToIID(it));
	drawpic(pos, strcat("gui/weapons/", itname), imgsize, '1 1 1', 1);

	it = ToStatus(it);
	if (it <= 1)
		return;
	itname = ftos(it);
	it = strlen(itname);
	drawstring(pos + IMGSIZEV - '0 8 0' - '8 0 0'*it, itname, '8 8 8', '1 1 1', 1);
};


void(vector pos, float slotno) SlotImageSmall =
{
	local float it, mod_s, mod_e;
	local string itname;


	if (slotno == downslotnum && slotnum)
		slotno = slotnum;
	else if (slotno == slotnum && downslotnum)
		slotno = downslotnum;

	if (slotno <= 16)
		it = getstati(31+slotno);
	else
	{
		if (slotno == 17)
			it = getstati(91);
		if (slotno == 18)
			it = getstati(92);
		if (slotno == 19)
			it = getstati(93);
		if (slotno == 20)
			it = getstati(94);
		if (slotno == 21)
			it = getstati(95);
		if (slotno == 22)
			it = getstati(96);
		if (slotno == 23)
			it = getstati(97);
		if (slotno == 24)
			it = getstati(98);
		if (slotno == 25)
			it = getstati(104);
		if (slotno == 26)
			it = getstati(105);
	}

	itname = GetItemImage(ToIID(it));
	drawpic(pos, strcat("gui/", itname), IMGSIZEH, '1 1 1', 1);


	it = ToStatus(it);
	if (it <= 1)
		return;
	itname = ftos(it);
	it = strlen(itname);
	drawstring(pos + IMGSIZEZ - '0 16 0' - '16 0 0'*it, itname, '8 8 8', '1 1 1', 1);
};

void(vector pos, float slotno) QuickImage =
{
	local float it;
	local string itname;

	if (slotno == downslotnum && slotnum)
		slotno = slotnum;
	else if (slotno == slotnum && downslotnum)
		slotno = downslotnum;

	it = getstati(31+slotno);

	itname = GetItemImage(ToIID(it));
	drawpic(pos, strcat("gui/", itname), IMGSIZEH, '1 1 1', 1);

	it = ToStatus(it);
	if (it <= 1)
		return;
	itname = ftos(it);
	it = strlen(itname);
	drawstring(pos + IMGSIZEH - '0 8 0' - '8 0 0'*it, itname, '8 8 8', '1 1 1', 1);
};

void() ScreenImage =
{
	float width,height;
	vector w;

	width = cvar("vid_conwidth");
	height = cvar("vid_conheight");
	w = '1 1 0'*width;

	if (getstati(57) == 0)
		drawpic('0 0 0', "gui/screen.jpg", w, '1 1 1', 1);
	else if (getstati(57) == 2)
		drawpic('0 0 0', "gui/moveout.jpg", w, '1 1 1', 1);
	else if (getstati(57) == 3)
		drawpic('0 0 0', "gui/complete.jpg", w, '1 1 1', 1);
	else if (getstati(57) == 4)
		drawpic('0 0 0', "gui/none.jpg", w, '1 1 1', 1);
	else if (getstati(57) == 5)
		drawpic('0 0 0', "gui/xflash.jpg", w, '1 1 1', 1);
	else if (getstati(57) == 6)
		drawpic('0 0 0', "gui/xcarnage.jpg", w, '1 1 1', 1);


	w = '0 32 0'+('1 0 0'*(scrwidth*0.10))+('0 1 0'*(scrheight*0.20));

	if (getstati(57) == 3)
	{
		if (getstati(105) == 1)
			drawstring(w, "the players were slain...", '8 8 8', '1 1 1', 1);
		if (getstati(105) == 2)
			drawstring(w, "the hostages were slain...", '8 8 8', '1 1 1', 1);
		if (getstati(105) == 3)
			drawstring(w, "the hostages were saved!", '8 8 8', '1 1 1', 1);
		if (getstati(105) == 4)
			drawstring(w, "some hostages were saved.", '8 8 8', '1 1 1', 1);
		if (getstati(105) == 5)
			drawstring(w, "no hostages were saved...", '8 8 8', '1 1 1', 1);
		if (getstati(105) == 6)
			drawstring(w, "the bombs were defused!", '8 8 8', '1 1 1', 1);
		if (getstati(105) == 7)
			drawstring(w, "the bombs were not defused...", '8 8 8', '1 1 1', 1);
		if (getstati(105) == 8)
			drawstring(w, "the supplies were not found...", '8 8 8', '1 1 1', 1);
		if (getstati(105) == 9)
			drawstring(w, "the supplies were found!", '8 8 8', '1 1 1', 1);
		if (getstati(105) == 10)
			drawstring(w, "all commanders were slain!", '8 8 8', '1 1 1', 1);
		if (getstati(105) == 11)
			drawstring(w, "the players did not escape...", '8 8 8', '1 1 1', 1);
		if (getstati(105) == 12)
			drawstring(w, "the players were discovered...", '8 8 8', '1 1 1', 1);
		if (getstati(105) == 13)
			drawstring(w, "the fuel tanks were destroyed!", '8 8 8', '1 1 1', 1);
		if (getstati(105) == 14)
			drawstring(w, "the fuel tanks were not destroyed...", '8 8 8', '1 1 1', 1);
	}

};

string(float x) GetMonsterName =
{
	if (x == 1 || x == 2)
		return "wild dogs";
	else if (x == 3 || x == 4)
		return "ghouls";
	else if (x == 5 || x == 6 || x == 7)
		return "raiders";
	else if (x == 8 || x == 9)
		return "mutants";
	else if (x == 10 || x == 11)
		return "scrags";
	else if (x == 12 || x == 13)
		return "enclave";
	else if (x == 14 || x == 15)
		return "deathclaws";
	else
		return "multiple unknowns";
};

void() BriefImage =
{
	float width,height, t, o, e1, e2, e3, loca;
	vector w;
	string m, x, m4, loc;

	t = getstati(64);
	loca = getstati(65);
	o = getstati(66);
	e1 = getstati(67);
	e2 = getstati(68);
	e3 = getstati(69);

	width = cvar("vid_conwidth");
	width = width * 0.5;
	height = cvar("vid_conheight");
	w = '1 1 0'*width;

	if (loca == 1)
		m4 = "gui/brief/map1.jpg";
	else if (loca == 2)
		m4 = "gui/brief/map2.jpg";
	else if (loca == 3)
		m4 = "gui/brief/map3.jpg";
	else if (loca == 4)
		m4 = "gui/brief/map4.jpg";
	else
		m4 = "gui/brief/map5.jpg";

	if (loca == 1)
		loc = "Eastgate";
	else if (loca == 2)
		loc = "Moonbrook";
	else if (loca == 3)
		loc = "Malan";
	else if (loca == 4)
		loc = "Military Base";
	else
		loc = "D Zone";

	if (o == 0)
		drawpic('0 0 0', "gui/brief/brief1.jpg", w, '1 1 1', 1);
	if (o == 1)
		drawpic('0 0 0', "gui/brief/brief1.jpg", w, '1 1 1', 1);
	if (o == 2)
		drawpic('0 0 0', "gui/brief/brief2.jpg", w, '1 1 1', 1);
	if (o == 3)
		drawpic('0 0 0', "gui/brief/brief3.jpg", w, '1 1 1', 1);
	if (o == 4)
		drawpic('0 0 0', "gui/brief/brief4.jpg", w, '1 1 1', 1);
	if (o == 5)
		drawpic('0 0 0', "gui/brief/brief5.jpg", w, '1 1 1', 1);
	if (o == 6)
		drawpic('0 0 0', "gui/brief/brief6.jpg", w, '1 1 1', 1);
	if (o == 7)
		drawpic('0 0 0', "gui/brief/brief7.jpg", w, '1 1 1', 1);
	if (o == 8)
		drawpic('0 0 0', "gui/brief/brief8.jpg", w, '1 1 1', 1);

	drawpic('0 0 0'+('1 0 0'*(width)), "gui/brief/icon.jpg", w, '1 1 1', 1);
	drawpic('0 0 0'+('0 1 0'*(width)), "gui/brief/intel.jpg", w, '1 1 1', 1);
	drawpic('0 0 0'+('1 1 0'*(width)), m4, w, '1 1 1', 1);

	drawstring('0 0 0'+('1 0 0'*(width*0.40))+('0 1 0'*(height*0.69)), loc, '8 8 8', '1 1 1', 1);
	m = GetMonsterName(e1);
	drawstring('0 20 0'+('1 0 0'*(width*0.10))+('0 1 0'*(height*0.85)), m, '8 8 8', '1 1 1', 1);
	m = GetMonsterName(e2);
	drawstring('0 35 0'+('1 0 0'*(width*0.10))+('0 1 0'*(height*0.85)), m, '8 8 8', '1 1 1', 1);
	m = GetMonsterName(e3);
	drawstring('0 50 0'+('1 0 0'*(width*0.10))+('0 1 0'*(height*0.85)), m, '8 8 8', '1 1 1', 1);

};

void(vector pos, float slotno) BlankImage =
{
	drawpic(pos, strcat("gui/", "none.jpg"), IMGSIZEV, '1 1 1', 1);
};

void(vector pos, float hpcolor, string image) HealthImage =
{
	float height, width, o, tyme, min, sec;
	local string itname, ts, tm, t;

	if (hpcolor == 0)
		drawpic(pos, strcat("gui/hnum/", image), IMGSIZEH, '1 1 1', 1);
	else if (hpcolor == 1)
		drawpic(pos, strcat("gui/ynum/", image), IMGSIZEH, '1 1 1', 1);
	else
		drawpic(pos, strcat("gui/rnum/", image), IMGSIZEH, '1 1 1', 1);

	width = cvar("vid_conwidth");
	height = cvar("vid_conheight");

	o = getstati(66);
	if (o == 1)
		drawstring('0 16 0'+('1 0 0'*(width*0.10))+('0 1 0'*(height*0.90)), "mission: civilian rescue", '8 8 8', '1 1 1', 1);
	if (o == 2)
		drawstring('0 16 0'+('1 0 0'*(width*0.10))+('0 1 0'*(height*0.90)), "mission: secure supply line", '8 8 8', '1 1 1', 1);
	if (o == 3)
		drawstring('0 16 0'+('1 0 0'*(width*0.10))+('0 1 0'*(height*0.90)), "mission: shadow op", '8 8 8', '1 1 1', 1);
	if (o == 4)
		drawstring('0 16 0'+('1 0 0'*(width*0.10))+('0 1 0'*(height*0.90)), "mission: deadtown", '8 8 8', '1 1 1', 1);
	if (o == 5)
		drawstring('0 16 0'+('1 0 0'*(width*0.10))+('0 1 0'*(height*0.90)), "mission: destroy fuel tanks", '8 8 8', '1 1 1', 1);
	if (o == 6)
		drawstring('0 16 0'+('1 0 0'*(width*0.10))+('0 1 0'*(height*0.90)), "mission: defuse explosives", '8 8 8', '1 1 1', 1);
};

void(vector pos, float img) PosImage =
{
	if (img == 1)
		drawpic(pos, strcat("gui/", "sneak.jpg"), IMGSIZEH, '1 1 1', 1);
	if (img == 2)
		drawpic(pos, strcat("gui/", "prone.jpg"), IMGSIZEH, '1 1 1', 1);

};

void(vector pos, float img) ChemImage =
{
	if (img == 0)
		drawpic(pos, strcat("gui/chem/", "zone.jpg"), IMGSIZEH, '1 1 1', 1);
	if (img == 1)
		drawpic(pos, strcat("gui/chem/", "rage1.jpg"), IMGSIZEH, '1 1 1', 1);
	if (img == 2)
		drawpic(pos, strcat("gui/chem/", "rage2.jpg"), IMGSIZEH, '1 1 1', 1);
	if (img == 3)
		drawpic(pos, strcat("gui/chem/", "rage3.jpg"), IMGSIZEH, '1 1 1', 1);
	if (img == 4)
		drawpic(pos, strcat("gui/chem/", "heal.jpg"), IMGSIZEH, '1 1 1', 1);
	if (img == 5)
		drawpic(pos, strcat("gui/chem/", "paranoia.jpg"), IMGSIZEH, '1 1 1', 1);

};

void(vector pos, string image) MoneyImage =
{
	local string itname;


	drawpic(pos, strcat("gui/", image), IMGSIZEH, '1 1 1', 1);
};


void(vector pos, string image) Crosshair =
{
	local string itname;


	drawpic(pos, image, IMGSIZEQ, '1 1 1', 1);
};


void() Trader_Draw =
{
	local float i, price, money;
	local float it;
	local string itname, money2, s2, s3, s4, s5, s6, s7, s8, s9, perki, perk2i, p1, p2, pn, pd, price2;
	local float op;
	float width,height,lvl,bar;
	vector w;

	width = cvar("vid_conwidth");
	height = cvar("vid_conheight");
	//w = '1 1 0'*height;




	drawpic('0 0 0', "gui/pipboy/buyscreen.jpg", '468 468 0', '1 1 1', 1);
	TraderImage('1 1 0'*IMGSIZEF, 1);
	TraderImage('2 1 0'*IMGSIZEF, 2);
	TraderImage('3 1 0'*IMGSIZEF, 3);
	TraderImage('4 1 0'*IMGSIZEF, 4);
	TraderImage('5 1 0'*IMGSIZEF, 5);
	TraderImage('1 2 0'*IMGSIZEF, 6);
	TraderImage('2 2 0'*IMGSIZEF, 7);
	TraderImage('3 2 0'*IMGSIZEF, 8);
	TraderImage('4 2 0'*IMGSIZEF, 9);
	TraderImage('5 2 0'*IMGSIZEF, 10);
	TraderImage('1 3 0'*IMGSIZEF, 11);
	TraderImage('2 3 0'*IMGSIZEF, 12);
	TraderImage('3 3 0'*IMGSIZEF, 13);
	TraderImage('4 3 0'*IMGSIZEF, 14);
	TraderImage('5 3 0'*IMGSIZEF, 15);
	TraderImage('1 4 0'*IMGSIZEF, 16);
	TraderImage('2 4 0'*IMGSIZEF, 17);
	TraderImage('3 4 0'*IMGSIZEF, 18);
	TraderImage('4 4 0'*IMGSIZEF, 19);
	TraderImage('5 4 0'*IMGSIZEF, 20);


	if (mousepos_x >= 1*IMGSIZEF && mousepos_x <= 6*IMGSIZEF)
		if (mousepos_y >= 1*IMGSIZEF && mousepos_y <= 2*IMGSIZEF)
			slotnum = floor((mousepos_x)/IMGSIZEF) + 0;

	if (mousepos_x >= 1*IMGSIZEF && mousepos_x <= 6*IMGSIZEF)
		if (mousepos_y >= 2*IMGSIZEF && mousepos_y <= 3*IMGSIZEF)
			slotnum = floor((mousepos_x)/IMGSIZEF) + 5;

	if (mousepos_x >= 1*IMGSIZEF && mousepos_x <= 6*IMGSIZEF)
		if (mousepos_y >= 3*IMGSIZEF && mousepos_y <= 4*IMGSIZEF)
			slotnum = floor((mousepos_x)/IMGSIZEF) + 10;

	if (mousepos_x >= 1*IMGSIZEF && mousepos_x <= 6*IMGSIZEF)
		if (mousepos_y >= 4*IMGSIZEF && mousepos_y <= 5*IMGSIZEF)
			slotnum = floor((mousepos_x)/IMGSIZEF) + 15;


	if (showcontextmenu && show_trader)
	{
		drawfill(contextpos - '8 8 0', '48 48 0', '0 0 0', 1.0);
		op = floor((mousepos_y - contextpos_y)/8);
		drawstring(contextpos + '0 8 0', "buy?", '8 8 0', '1 1 0' + (op!=1)*'0 0 1', 
1);
	}

	if (slotnum <= 20)
		it = getstati(70+slotnum);
	itname = GetItemName(ToIID(it));
		drawstring('32 16 0', itname, '8 8 0', '1 1 1', 1);

	price = GetBaseValue(ToIID(it));
	price2 = ftos(price);
	money = getstati(50);
	money2 = ftos(money);

	drawstring('2 6 0'*IMGSIZEF - '16 48 0', " TOTAL ", '16 16 0', '1 1 1', 1);
	drawstring('2 6 0'*IMGSIZEF - '16 32 0', " PRICE ", '16 16 0', '1 1 1', 1);
	drawstring('2 6 0'*IMGSIZEF + '16 0 0', price2, '16 16 0', '1 1 1', 1);
	drawstring('4 6 0'*IMGSIZEF - '16 48 0', " TOTAL ", '16 16 0', '1 1 1', 1);
	drawstring('4 6 0'*IMGSIZEF - '16 32 0', " MONEY ", '16 16 0', '1 1 1', 1);
	drawstring('4 6 0'*IMGSIZEF + '16 0 0', money2, '16 16 0', '1 1 1', 1);
	drawpic(mousepos, "gui/cursor.jpg", '1 1 0'*CURSORSIZE, '1 1 1', 1);
};

void() Perks_Draw =
{
	local string perki, perk2i, p1, p2;

	drawpic('0 0 0', "gui/pipboy/perks.png", '640 480 0'*(scrwidth/640), '1 1 1', 1);




	if (mousepos_y >= 2.0*IMGSIZEF*(scrwidth/640) && mousepos_y <= 4*IMGSIZEF*(scrwidth/640))
	{
		if (mousepos_x >= 3*IMGSIZEF*(scrwidth/640) && mousepos_x <= 5.5*IMGSIZEF*(scrwidth/640))
			selected_class = 8;
		else if (mousepos_x >= 6*IMGSIZEF*(scrwidth/640) && mousepos_x <= 7.5*IMGSIZEF*(scrwidth/640))
			selected_class = 9;
		else
			selected_class = 0;		
	}
	else if (mousepos_y >= 390*(scrwidth/640) && mousepos_y <= 425*(scrwidth/640))
	{


		if (mousepos_x >= 300*(scrwidth/640) && mousepos_x <= 335*(scrwidth/640))
			selected_class = 5;
		else if (mousepos_x >= 350*(scrwidth/640) && mousepos_x <= 385*(scrwidth/640))
			selected_class = 6;

		else if (mousepos_x >= 400*(scrwidth/640) && mousepos_x <= 435*(scrwidth/640))
			selected_class = 7;
		else
			selected_class = 0;
	}
	else
		selected_class = 0;

	perki = GetPerkImage(getstati(60));
	perk2i = GetPerkImage(getstati(59));


	drawpic('3.5 2 0'*IMGSIZEF*(scrwidth/640),  perki, '128 128 0'*(scrwidth/640), '1 1 1', 1);
	drawpic('6 2 0'*IMGSIZEF*(scrwidth/640), perk2i, '128 128 0'*(scrwidth/640), '1 1 1', 1);

	p1 = PerkNameCentered(1, getstati(60));
	p2 = PerkNameCentered(2, getstati(59));

	drawstring('4 4.5 0'*IMGSIZEF*(scrwidth/640) - '45 0 0'*(scrwidth/640), p1, '8 8 8'*(scrwidth/640), '1 1 1', 1);
	drawstring('6 4.5 0'*IMGSIZEF*(scrwidth/640) - '7 0 0'*(scrwidth/640), p2, '8 8 8'*(scrwidth/640), '1 1 1', 1);



	drawpic(mousepos, "gui/cursor.jpg", '1 1 0'*CURSORSIZE, '1 1 1', 1);
};

void() Items_Draw =
{
	local float it;
	local string itname;

	drawpic('0 0 0', "gui/pipboy/items.png", '640 480 0'*(scrwidth/640), '1 1 1', 1);

	if (mousepos_y >= 390*(scrwidth/640) && mousepos_y <= 425*(scrwidth/640))
	{

		drawpic('0 0 0', "gui/pipboy/items.png", '640 480 0'*(scrwidth/640), '1 1 1', 1);

		if (mousepos_x >= 300*(scrwidth/640) && mousepos_x <= 325*(scrwidth/640))
			selected_class = 5;
		else if (mousepos_x >= 350*(scrwidth/640) && mousepos_x <= 385*(scrwidth/640))
			selected_class = 6;

		else if (mousepos_x >= 400*(scrwidth/640) && mousepos_x <= 435*(scrwidth/640))
			selected_class = 7;
		else
			selected_class = 0;
	}

	SlotImageBig('3 1.25 0'*IMGSIZEF*(scrwidth/640), 1, '180 64 0'*(scrwidth/640));

	SlotImageBig('5.5 2.75 0'*IMGSIZEF*(scrwidth/640), 2, '180 64 0'*(scrwidth/640));

	SlotImageBig('3 3.4 0'*IMGSIZEF*(scrwidth/640), 3, '128 128 0'*(scrwidth/640));


	it = getstati(32);
	itname = GetItemName(ToIID(it));
	drawstring('3 1 0'*IMGSIZEF*(scrwidth/640) + '0 0 0'*(scrwidth/640), itname, '8 8 0'*(scrwidth/640), '1 1 1', 1);
	itname = (ftos(GetWeaponDamage(ToIID(it))));
	drawstring('6 1.5 0'*IMGSIZEF*(scrwidth/640) + '0 8 0'*(scrwidth/640), strcat("damage: ", itname, "\n"), '8 8 0'*(scrwidth/640), '1 1 1', 1);
	itname = (ftos(GetWeaponRange(ToIID(it))));
	drawstring('6 1.5 0'*IMGSIZEF*(scrwidth/640) + '0 16 0'*(scrwidth/640), strcat("range:  ", itname, "\n"), '8 8 0'*(scrwidth/640), '1 1 1', 1);
	itname = (ftos(GetItemWeight(ToIID(it))));
	drawstring('6 1.5 0'*IMGSIZEF*(scrwidth/640) + '0 24 0'*(scrwidth/640), strcat("weight: ", itname, "\n"), '8 8 0'*(scrwidth/640), '1 1 1', 1);

	it = getstati(33);
	itname = GetItemName(ToIID(it));
	drawstring('5.5 2.5 0'*IMGSIZEF*(scrwidth/640) + '0 0 0'*(scrwidth/640), itname, '8 8 0'*(scrwidth/640), '1 1 1', 1);
	itname = (ftos(GetWeaponDamage(ToIID(it))));
	drawstring('4.5 2.8 0'*IMGSIZEF*(scrwidth/640) + '0 8 0'*(scrwidth/640), strcat("damage: ", itname, "\n"), '8 8 0'*(scrwidth/640), '1 1 1', 1);
	itname = (ftos(GetWeaponRange(ToIID(it))));
	drawstring('4.5 2.8 0'*IMGSIZEF*(scrwidth/640) + '0 16 0'*(scrwidth/640), strcat("range:  ", itname, "\n"), '8 8 0'*(scrwidth/640), '1 1 1', 1);
	itname = (ftos(GetItemWeight(ToIID(it))));
	drawstring('4.5 2.8 0'*IMGSIZEF*(scrwidth/640) + '0 24 0'*(scrwidth/640), strcat("weight: ", itname, "\n"), '8 8 0'*(scrwidth/640), '1 1 1', 1);

	it = getstati(34);
	itname = GetItemName(ToIID(it));
	drawstring('3 5 0'*IMGSIZEF*(scrwidth/640) + '0 0 0'*(scrwidth/640), itname, '8 8 0'*(scrwidth/640), '1 1 1', 1);
	itname = (ftos(GetArmorThreshold(ToIID(it))));
	drawstring('5 4.3 0'*IMGSIZEF*(scrwidth/640) + '0 8 0'*(scrwidth/640), strcat("threshold: ", itname, "\n"), '8 8 0'*(scrwidth/640), '1 1 1', 1);
	itname = (ftos(GetArmorAbsorb(ToIID(it))));
	drawstring('5 4.3 0'*IMGSIZEF*(scrwidth/640) + '0 16 0'*(scrwidth/640), strcat("absorb:    ", itname, "\n"), '8 8 0'*(scrwidth/640), '1 1 1', 1);
	itname = (ftos(GetItemWeight(ToIID(it))));
	drawstring('5 4.3 0'*IMGSIZEF*(scrwidth/640) + '0 24 0'*(scrwidth/640), strcat("weight:    ", itname, "\n"), '8 8 0'*(scrwidth/640), '1 1 1', 1);

	drawpic(mousepos, "gui/cursor.jpg", '1 1 0'*CURSORSIZE, '1 1 1', 1);
};



void() GainPerk_Draw =
{
	local float it;
	local string itname, pn, pd;

	drawpic('0 0 0', "gui/pipboy/gainperk.png", '640 480 0'*(scrwidth/640), '1 1 1', 1);

	if (mousepos_y >= 2*IMGSIZEF*(scrwidth/640) && mousepos_y <= 3*IMGSIZEF*(scrwidth/640))
	{
		if (mousepos_x >= 2*IMGSIZEF*(scrwidth/640) && mousepos_x <= 3*IMGSIZEF*(scrwidth/640))
			which_perk = 1;
		else if (mousepos_x >= 3*IMGSIZEF*(scrwidth/640) && mousepos_x <= 4*IMGSIZEF*(scrwidth/640))
			which_perk = 2;
		else if (mousepos_x >= 4*IMGSIZEF*(scrwidth/640) && mousepos_x <= 5*IMGSIZEF*(scrwidth/640))
			which_perk = 3;
		else if (mousepos_x >= 5*IMGSIZEF*(scrwidth/640) && mousepos_x <= 6*IMGSIZEF*(scrwidth/640))
			which_perk = 4;
		else if (mousepos_x >= 6*IMGSIZEF*(scrwidth/640) && mousepos_x <= 7*IMGSIZEF*(scrwidth/640))
			which_perk = 5;
		else
			which_perk = 0;
	}
	if (mousepos_y >= 3*IMGSIZEF*(scrwidth/640) && mousepos_y <= 4*IMGSIZEF*(scrwidth/640))
	{
		if (mousepos_x >= 2*IMGSIZEF*(scrwidth/640) && mousepos_x <= 3*IMGSIZEF*(scrwidth/640))
			which_perk = 6;
		else if (mousepos_x >= 3*IMGSIZEF*(scrwidth/640) && mousepos_x <= 4*IMGSIZEF*(scrwidth/640))
			which_perk = 7;
		else if (mousepos_x >= 4*IMGSIZEF*(scrwidth/640) && mousepos_x <= 5*IMGSIZEF*(scrwidth/640))
			which_perk = 8;
		else if (mousepos_x >= 5*IMGSIZEF*(scrwidth/640) && mousepos_x <= 6*IMGSIZEF*(scrwidth/640))
			which_perk = 9;
		else if (mousepos_x >= 6*IMGSIZEF*(scrwidth/640) && mousepos_x <= 7*IMGSIZEF*(scrwidth/640))
			which_perk = 10;
		else
			which_perk = 0;
	}
	if (mousepos_y >= 4*IMGSIZEF*(scrwidth/640) && mousepos_y <= 5*IMGSIZEF*(scrwidth/640))
	{
		if (mousepos_x >= 2*IMGSIZEF*(scrwidth/640) && mousepos_x <= 3*IMGSIZEF*(scrwidth/640))
			which_perk = 11;
		else
			which_perk = 0;
	}

	drawpic('2 2 0'*IMGSIZEF*(scrwidth/640), "gui/perks/Support Fire.png", '64 64 0'*(scrwidth/640), '1 1 1', 1);
	drawpic('3 2 0'*IMGSIZEF*(scrwidth/640), "gui/perks/Duck And Cover", '64 64 0'*(scrwidth/640), '1 1 1', 1);
	drawpic('4 2 0'*IMGSIZEF*(scrwidth/640), "gui/perks/Puppet Master.png", '64 64 0'*(scrwidth/640), '1 1 1', 1);
	drawpic('5 2 0'*IMGSIZEF*(scrwidth/640), "gui/perks/Stay Low.png", '64 64 0'*(scrwidth/640), '1 1 1', 1);
	drawpic('6 2 0'*IMGSIZEF*(scrwidth/640), "gui/perks/Surgical Precision.png", '64 64 0'*(scrwidth/640), '1 1 1', 1);

	drawpic('2 3 0'*IMGSIZEF*(scrwidth/640), "gui/perks/Mutation.png", '64 64 0'*(scrwidth/640), '1 1 1', 1);
	drawpic('3 3 0'*IMGSIZEF*(scrwidth/640), "gui/perks/Narrow Escape.png", '64 64 0'*(scrwidth/640), '1 1 1', 1);
	drawpic('4 3 0'*IMGSIZEF*(scrwidth/640), "gui/perks/Schizophrenia.png", '64 64 0'*(scrwidth/640), '1 1 1', 1);
	drawpic('5 3 0'*IMGSIZEF*(scrwidth/640), "gui/perks/Sleep Tight.png", '64 64 0'*(scrwidth/640), '1 1 1', 1);
	drawpic('6 3 0'*IMGSIZEF*(scrwidth/640), "gui/perks/Economic Moves.png", '64 64 0'*(scrwidth/640), '1 1 1', 1);

	drawpic('2 4 0'*IMGSIZEF*(scrwidth/640), "gui/perks/Nuclear Man.png", '64 64 0'*(scrwidth/640), '1 1 1', 1);

	pn = PerkName(1, which_perk);
	pd = GetPerkDesc(which_perk);
	drawstring('32 16 0'*(scrwidth/640), pn, '8 8 0'*(scrwidth/640), '1 1 1', 1);
	drawstring('32 32 0'*(scrwidth/640), pd, '8 8 0'*(scrwidth/640), '1 1 1', 1);


	drawpic(mousepos, "gui/cursor.jpg", '1 1 0'*CURSORSIZE, '1 1 1', 1);
};


void() Pipboy_Draw =
{
	float width,height,lvl,bar;
	string skill;
	vector w;

	width = cvar("vid_conwidth");
	height = cvar("vid_conheight");



	if (mousepos_y >= 200*(scrwidth/640) && mousepos_y <= 325*(scrwidth/640))
	{
		if (mousepos_x >= 150*(scrwidth/640) && mousepos_x <= 250*(scrwidth/640))
		{
			drawpic('0 0 0', "gui/pipboy/skills2.jpg", '640 480 0'*(scrwidth/640), '1 1 1', 1);
			selected_class = 1;
		}
		else if (mousepos_x >= 250*(scrwidth/640) && mousepos_x <= 350*(scrwidth/640))
		{
			drawpic('0 0 0', "gui/pipboy/skills3.jpg", '640 480 0'*(scrwidth/640), '1 1 1', 1);
			selected_class = 2;
		}
		else if (mousepos_x >= 350*(scrwidth/640) && mousepos_x <= 450*(scrwidth/640))
		{
			drawpic('0 0 0', "gui/pipboy/skills4.jpg", '640 480 0'*(scrwidth/640), '1 1 1', 1);
			selected_class = 3;
		}
		else if (mousepos_x >= 450*(scrwidth/640) && mousepos_x <= 550*(scrwidth/640))
		{
			drawpic('0 0 0', "gui/pipboy/skills5.jpg", '640 480 0'*(scrwidth/640), '1 1 1', 1);
			selected_class = 4;
		}
		else
		{
			drawpic('0 0 0', "gui/pipboy/skills1.jpg", '640 480 0'*(scrwidth/640), '1 1 1', 1);
			selected_class = 0;
		}
	}
	else if (mousepos_y >= 390*(scrwidth/640) && mousepos_y <= 425*(scrwidth/640))
	{

		drawpic('0 0 0', "gui/pipboy/skills1.jpg", '640 480 0'*(scrwidth/640), '1 1 1', 1);

		if (mousepos_x >= 300*(scrwidth/640) && mousepos_x <= 325*(scrwidth/640))
			selected_class = 5;
		else if (mousepos_x >= 350*(scrwidth/640) && mousepos_x <= 375*(scrwidth/640))
			selected_class = 6;

		else if (mousepos_x >= 400*(scrwidth/640) && mousepos_x <= 425*(scrwidth/640))
			selected_class = 7;
		else
			selected_class = 0;
	}
	else
	{
		drawpic('0 0 0', "gui/pipboy/skills1.jpg", '640 480 0'*(scrwidth/640), '1 1 1', 1);
		selected_class = 0;
	}


	if (selected_class == 1)
	{
		drawstring('200 50 0'*(scrwidth/640), "   combat", '8 8 0'*(scrwidth/640), '1 1 1', 1);
		drawstring('200 66 0'*(scrwidth/640), "place points into this skill for improved", '8 8 0'*(scrwidth/640), '1 1 1', 1);
		drawstring('200 74 0'*(scrwidth/640), "handling of weapons, as well as increased", '8 8 0'*(scrwidth/640), '1 1 1', 1);
		drawstring('200 82 0'*(scrwidth/640), "carry capacity and other aspects of being", '8 8 0'*(scrwidth/640), '1 1 1', 1);
		drawstring('200 90 0'*(scrwidth/640), "trained in combat.", '8 8 0'*(scrwidth/640), '1 1 1', 1);
	}
	if (selected_class == 2)
	{
		drawstring('200 50 0'*(scrwidth/640), "   doctor", '8 8 0'*(scrwidth/640), '1 1 1', 1);
		drawstring('200 66 0'*(scrwidth/640), "place points into this skill for improved", '8 8 0'*(scrwidth/640), '1 1 1', 1);
		drawstring('200 74 0'*(scrwidth/640), "chem use, which substantially affects the", '8 8 0'*(scrwidth/640), '1 1 1', 1);
		drawstring('200 82 0'*(scrwidth/640), "amount of healing done, as well as all of", '8 8 0'*(scrwidth/640), '1 1 1', 1);
		drawstring('200 90 0'*(scrwidth/640), "the other aspects of first aid.", '8 8 0'*(scrwidth/640), '1 1 1', 1);
	}
	if (selected_class == 3)
	{
		drawstring('200 50 0'*(scrwidth/640), "   sneak", '8 8 0'*(scrwidth/640), '1 1 1', 1);
		drawstring('200 66 0'*(scrwidth/640), "place points into this skill to maximize", '8 8 0'*(scrwidth/640), '1 1 1', 1);
		drawstring('200 74 0'*(scrwidth/640), "your ability to remain unseen by enemies.", '8 8 0'*(scrwidth/640), '1 1 1', 1);
		drawstring('200 82 0'*(scrwidth/640), "grants higher sneak ability and how fast", '8 8 0'*(scrwidth/640), '1 1 1', 1);
		drawstring('200 90 0'*(scrwidth/640), "you can pick locks, among other things.", '8 8 0'*(scrwidth/640), '1 1 1', 1);

	}
	if (selected_class == 4)
	{
		drawstring('200 50 0'*(scrwidth/640), "   science", '8 8 0'*(scrwidth/640), '1 1 1', 1);
		drawstring('200 66 0'*(scrwidth/640), "place points into this skill to increase", '8 8 0'*(scrwidth/640), '1 1 1', 1);
		drawstring('200 74 0'*(scrwidth/640), "your chances of making useful items from", '8 8 0'*(scrwidth/640), '1 1 1', 1);
		drawstring('200 82 0'*(scrwidth/640), "junk found in the wasteland. use the mix", '8 8 0'*(scrwidth/640), '1 1 1', 1);
		drawstring('200 90 0'*(scrwidth/640), "box found in your inventory screen.", '8 8 0'*(scrwidth/640), '1 1 1', 1);

	}


	bar = 0;
	lvl = getstati(100);
	skill = ftos(lvl+1);
	while (bar <= lvl)
	{
		drawpic('210 190 0'*(scrwidth/640) - ('0 4 0'*bar*(scrwidth/640)),  "gui/graph/red.jpg", '16 2 0'*2*(scrwidth/640), '1 1 1', 1);		
		bar = bar + 1;
	}
		drawstring('220 190 0'*(scrwidth/640) - ('0 4 0'*(lvl+4)*(scrwidth/640)), skill, '8 8 0'*(scrwidth/640), '1 1 1', 1);


	bar = 0;
	lvl = getstati(101);
	skill = ftos(lvl+1);
	while (bar <= lvl)
	{
		drawpic('305 190 0'*(scrwidth/640)  - ('0 4 0'*bar*(scrwidth/640)),  "gui/graph/green.jpg", '16 2 0'*2*(scrwidth/640), '1 1 1', 1);		
		bar = bar + 1;
	}
		drawstring('315 190 0'*(scrwidth/640) - ('0 4 0'*(lvl+4)*(scrwidth/640)), skill, '8 8 0'*(scrwidth/640), '1 1 1', 1);

	bar = 0;
	lvl = getstati(102);
	skill = ftos(lvl+1);
	while (bar <= lvl)
	{
		drawpic('395 190 0'*(scrwidth/640) - ('0 4 0'*bar*(scrwidth/640)),  "gui/graph/blue.jpg", '16 2 0'*2*(scrwidth/640), '1 1 1', 1);		
		bar = bar + 1;
	}
		drawstring('405 190 0'*(scrwidth/640) - ('0 4 0'*(lvl+4)*(scrwidth/640)), skill, '8 8 0'*(scrwidth/640), '1 1 1', 1);

	bar = 0;
	lvl = getstati(103);
	skill = ftos(lvl+1);
	while (bar <= lvl)
	{
		drawpic('485 190 0'*(scrwidth/640) - ('0 4 0'*bar*(scrwidth/640)),  "gui/graph/yellow.jpg", '16 2 0'*2*(scrwidth/640), '1 1 1', 1);		
		bar = bar + 1;
	}
		drawstring('495 190 0'*(scrwidth/640) - ('0 4 0'*(lvl+4)*(scrwidth/640)), skill, '8 8 0'*(scrwidth/640), '1 1 1', 1);

	drawpic(mousepos, "gui/cursor.jpg", '1 1 0'*CURSORSIZE, '1 1 1', 1);
};


void() Invent_Draw =
{
	local float i, check;
	local float it;
	local string itname, s1, s2, s3, s4, s5, s6, s7, s8, s9, perki, perk2i, p1, p2, pn, pd;
	local float op;
	
	local float height,width;
	vector w;
	height = cvar("vid_conheight");
	width = cvar("vid_conwidth");
	//how much space have we got for the slider?

	drawpic('0 0 0', "gui/pipboy/inventory.jpg", '500 470 0'*(scrwidth/640), '1 1 1', 1);

	local float slotofs;
	slotofs = sliderpos*(height - (IMGSIZEF*6)) + (IMGSIZEF*6);


	SlotImage('2 1 0'*IMGSIZEF*(scrwidth/640), 5);
	SlotImage('2 2 0'*IMGSIZEF*(scrwidth/640), 6);
	SlotImage('2 3 0'*IMGSIZEF*(scrwidth/640), 7);
	SlotImage('2 4 0'*IMGSIZEF*(scrwidth/640), 8);
	SlotImage('2 5 0'*IMGSIZEF*(scrwidth/640), 9);
	SlotImage('2 6 0'*IMGSIZEF*(scrwidth/640), 10);
	SlotImage('3 1 0'*IMGSIZEF*(scrwidth/640), 11);
	SlotImage('3 2 0'*IMGSIZEF*(scrwidth/640), 12);
	SlotImage('3 3 0'*IMGSIZEF*(scrwidth/640), 13);
	SlotImage('3 4 0'*IMGSIZEF*(scrwidth/640), 14);
	SlotImage('3 5 0'*IMGSIZEF*(scrwidth/640), 15);
	SlotImage('3 6 0'*IMGSIZEF*(scrwidth/640), 16);

	check = getstati(32);
	if (check > 0)
		SlotImage('6 1 0'*IMGSIZEF*(scrwidth/640) - '16 2 0'*(scrwidth/640), 1);


	check = getstati(33);
	if (check > 0)
		SlotImage('6 2 0'*IMGSIZEF*(scrwidth/640) - '16 -5 0'*(scrwidth/640), 2);


	check = getstati(34);
	if (check > 0)
		SlotImage('6 3 0'*IMGSIZEF*(scrwidth/640) - '16 -13 0'*(scrwidth/640), 3);

	check = getstati(35);
	if (check > 0)
		SlotImageSmall('1 12.5 0'*IMGSIZEK*(scrwidth/640), 4);

	SlotImageSmall('1 3 0'*IMGSIZEK*(scrwidth/640), 17);
	SlotImageSmall('1 4 0'*IMGSIZEK*(scrwidth/640), 18);
	SlotImageSmall('1 5 0'*IMGSIZEK*(scrwidth/640), 19);
	SlotImageSmall('1 6 0'*IMGSIZEK*(scrwidth/640), 20);
	SlotImageSmall('1 7 0'*IMGSIZEK*(scrwidth/640), 21);
	SlotImageSmall('1 8 0'*IMGSIZEK*(scrwidth/640), 22);




	check = getstati(97);
	if (check > 0)
		SlotImageSmall('11 12 0'*IMGSIZEK*(scrwidth/640), 23);



	check = getstati(98);
	if (check > 0)
		SlotImageSmall('13 12 0'*IMGSIZEK*(scrwidth/640), 24);


	if (showcontextmenu && show_inventory)
	{
			drawfill(contextpos - '8 8 0', '88 24 0'+'32 16 0'*(scrwidth/640), '0 0 0', 1);

		op = floor((mousepos_y - contextpos_y)/8);
		drawstring(contextpos + (0 * '0 8 0'), "use/reload", '8 8 0', '1 1 0' + (op!=0)*'0 0 1', 1);
		drawstring(contextpos + (1 * '0 8 0'), "drop", '8 8 0', '1 1 0' + (op!=1)*'0 0 1', 1);
		if (getstati(108) == 1)
			drawstring(contextpos + (2 * '0 8 0'), "sell", '8 8 0', '1 1 0' + (op!=2)*'0 0 1', 1);

	}
	else if (showmixmenu && show_inventory)
	{
			drawfill(contextpos - '8 8 0', '88 24 0'+'96 96 0', '0 0 0', 0.9);

		op = floor((mousepos_y - contextpos_y)/8);
		drawstring(contextpos + (0 * '0 8 0'), "mix items", '8 8 0', '1 1 0' + (op!=0)*'0 0 1', 1);
	}
	else
	{
		if  ((mousepos_y >= 12.5*IMGSIZEH && mousepos_y <= 13*IMGSIZEH) && mousepos_x <= IMGSIZEF)
		{
				slotnum = 4;
		}
		else if (mousepos_x >= IMGSIZEF*(scrwidth/640) )
		{
			slotnum = 0;
			if (mousepos_x >= 2*IMGSIZEF*(scrwidth/640) && mousepos_x <= 3*IMGSIZEF*(scrwidth/640))
				if (mousepos_y >= 1*IMGSIZEF*(scrwidth/640) && mousepos_y <= 7*IMGSIZEF*(scrwidth/640))
					slotnum = floor((mousepos_y)/(IMGSIZEF*(scrwidth/640))) + 4;

			if (mousepos_x >= 3*IMGSIZEF*(scrwidth/640) && mousepos_x <= 4*IMGSIZEF*(scrwidth/640))
				if (mousepos_y >= 1*IMGSIZEF*(scrwidth/640) && mousepos_y <= 7*IMGSIZEF*(scrwidth/640))
					slotnum = floor((mousepos_y)/(IMGSIZEF*(scrwidth/640))) + 10;

			if (mousepos_x >= 5.5*IMGSIZEF*(scrwidth/640) && mousepos_x <= 6.7*IMGSIZEF*(scrwidth/640))
				if (mousepos_y >= 1*IMGSIZEF*(scrwidth/640) && mousepos_y <= 2*IMGSIZEF*(scrwidth/640))
					slotnum = 1;

			if (mousepos_x >= 5.5*IMGSIZEF*(scrwidth/640) && mousepos_x <= 6.7*IMGSIZEF*(scrwidth/640))
				if (mousepos_y >= 2*IMGSIZEF*(scrwidth/640) && mousepos_y <= 3*IMGSIZEF*(scrwidth/640))
					slotnum = 2;

			if (mousepos_x >= 5.5*IMGSIZEF*(scrwidth/640) && mousepos_x <= 6.7*IMGSIZEF*(scrwidth/640))
				if (mousepos_y >= 3*IMGSIZEF*(scrwidth/640) && mousepos_y <= 4*IMGSIZEF*(scrwidth/640))
					slotnum = 3;




			if (mousepos_x >= 10.75*IMGSIZEK*(scrwidth/640) && mousepos_x <= 12.25*IMGSIZEK*(scrwidth/640))
				if (mousepos_y >= 11.75*IMGSIZEK*(scrwidth/640) && mousepos_y <= 13.25*IMGSIZEK*(scrwidth/640))
					slotnum = 23;
			if (mousepos_x >= 12.75*IMGSIZEK*(scrwidth/640) && mousepos_x <= 14.25*IMGSIZEK*(scrwidth/640))
				if (mousepos_y >= 11.75*IMGSIZEK*(scrwidth/640) && mousepos_y <= 13.25*IMGSIZEK*(scrwidth/640))
					slotnum = 24;
		}
		else
		{
			slotnum = floor((mousepos_y)/(IMGSIZEK*(scrwidth/640))) + 14;
			if (slotnum <= 16)
				slotnum = 0;
			if (slotnum >= 25)
				slotnum = 0;
		}
//			slotnum = floor((mousepos_y - toppos_y)/8) + 1;
	}




if (slotnum <= 24)
{

	if (slotnum <= 16)
		it = getstati(31+slotnum);
	else
	{
		if (slotnum == 17)
			it = getstati(91);
		if (slotnum == 18)
			it = getstati(92);
		if (slotnum == 19)
			it = getstati(93);
		if (slotnum == 20)
			it = getstati(94);
		if (slotnum == 21)
			it = getstati(95);
		if (slotnum == 22)
			it = getstati(96);
		if (slotnum == 23)
			it = getstati(97);
		if (slotnum == 24)
			it = getstati(98);
	}

	itname = GetItemName(ToIID(it));
	drawstring('32 16 0'*(scrwidth/640), itname, '16 16 0'*(scrwidth/640), '1 1 1', 1);
}


	if (mousepos_x > scrwidth*0.95)
		mousepos_x = scrwidth*0.95;

	if (mousepos_y > scrheight*0.95)
		mousepos_y = scrheight*0.95;


	if (downslotnum == 0)
		drawpic(mousepos, "gui/cursor.jpg", '1 1 0'*CURSORSIZE, '1 1 1', 1);
	else
	{

		if (downslotnum <= 16)
			it = getstati(31+downslotnum);
		else
		{
			if (downslotnum == 17)
				it = getstati(91);
			if (downslotnum == 18)
				it = getstati(92);
			if (downslotnum == 19)
				it = getstati(93);
			if (downslotnum == 20)
				it = getstati(94);
			if (downslotnum == 21)
				it = getstati(95);
			if (downslotnum == 22)
				it = getstati(96);
			if (downslotnum == 23)
				it = getstati(97);
			if (downslotnum == 24)
				it = getstati(98);
		}
		itname = GetItemImage(ToIID(it));
		drawpic(mousepos - '16 16 0', strcat("gui/", itname), '48 48 0', '1 1 1', 1);
	}
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
			show_pipboy = false;
			show_trader = false;
			show_perks = false;
			show_items = false;
			show_gainperk = false;
		}
		if (param1 == 'p')
		{
			show_pipboy = !show_pipboy;
			show_inventory = false;
			show_trader = false;
			show_perks = false;
			show_items = false;
			show_gainperk = false;
		}
		if (param1 == 'b' && (getstati(108) == 1))
		{
			show_trader = !show_trader;
			show_pipboy = false;
			show_inventory = false;
			show_perks = false;
			show_items = false;
			show_gainperk = false;
		}
		else if (!show_inventory && !show_trader && !show_pipboy && !show_perks && !show_items && !show_gainperk) 
			return false;
		else if (param1 == k_mouse1)
		{
			if (mousepos_x >= IMGSIZEF && mousepos_x <= IMGSIZEF+16)
			{
				mouseisdown = true;
				CalcScrollPos();
			}
			else
				downslotnum = slotnum;
		}
		else if (param1 == k_mwheelup)
		{
			sliderpos = sliderpos - 0.05;
			if (sliderpos < 0)
				sliderpos = 0;
		}
		else if (param1 == k_mwheeldown)
		{
			sliderpos = sliderpos + 0.05;
			if (sliderpos > 1)
				sliderpos = 1;
		}
		else
			return false;
		return true;
	}
	if (eventtype == 1 && (show_inventory || show_trader || show_pipboy || show_items || show_perks || show_gainperk))	//key up
	{
		if (param1 == k_mouse1)
		{		
			if (showcontextmenu && show_inventory)
			{
				op = floor((mousepos_y - contextpos_y)/8);

				if (op == 0)
					localcmd(strcat("cmd invuse ", ftos(slotnum), "\n"));
				else if ((op == 1))
					localcmd(strcat("cmd invdrop ", ftos(slotnum), "\n"));
				else if (op == 2)
					localcmd(strcat("cmd invsell ", ftos(slotnum), "\n"));

				showcontextmenu = false;
			}
			else if (showmixmenu && show_inventory)
			{
				op = floor((mousepos_y - contextpos_y)/8);

				if (op == 0)
					localcmd(strcat("cmd invmix ", ftos(slotnum), "\n"));


				showmixmenu = false;
			}
			else if (showcontextmenu && show_trader)
			{
				op = floor((mousepos_y - contextpos_y)/8);

				if (op == 1)
					localcmd(strcat("cmd invbuy ", ftos(slotnum), "\n"));
	
				showcontextmenu = false;
			}
			else
			{
				if (mouseisdown)
				{	//within the scrollbar
					mouseisdown = false;
				}

				else if (downslotnum == slotnum)	//mouse didn't move away
				{

					if (show_gainperk)	
					{
						if (which_perk == getstati(60))
							return;
						if (which_perk == getstati(59))
							return;
						if (which_perk >= 1 && which_perk <= 11)
							localcmd(strcat("cmd gainperk ", ftos(which_perk), " ", ftos(perkslot), "\n"));
							show_inventory = false;
							show_pipboy = false;
							show_trader = false;
							show_perks = true;
							show_items = false;
							show_gainperk = false;
					}
					else if (show_pipboy)	
					{
						if (selected_class == 1)
							localcmd("cmd gainlevel combat\n");
						else if (selected_class == 2)
							localcmd("cmd gainlevel doctor\n");
						else if (selected_class == 3)
							localcmd("cmd gainlevel sneak\n");
						else if (selected_class == 4)
							localcmd("cmd gainlevel science\n");
						else if (selected_class == 5)
						{
							show_inventory = false;
							show_pipboy = true;
							show_trader = false;
							show_perks = false;
							show_items = false;
							show_gainperk = false;
						}
						else if (selected_class == 6)
						{
							show_inventory = false;
							show_pipboy = false;
							show_trader = false;
							show_perks = false;
							show_items = true;
							show_gainperk = false;
						}
						else if (selected_class == 7)
						{
							show_inventory = false;
							show_pipboy = false;
							show_trader = false;
							show_perks = true;
							show_items = false;
							show_gainperk = false;
						}

					}
					else if (show_items)	
					{
						if (selected_class == 5)
						{
							show_inventory = false;
							show_pipboy = true;
							show_trader = false;
							show_perks = false;
							show_items = false;
							show_gainperk = false;
						}
						else if (selected_class == 6)
						{
							show_inventory = false;
							show_pipboy = false;
							show_trader = false;
							show_perks = false;
							show_items = true;
							show_gainperk = false;
						}
						else if (selected_class == 7)
						{
							show_inventory = false;
							show_pipboy = false;
							show_trader = false;
							show_perks = true;
							show_items = false;
							show_gainperk = false;
						}
					}
					else if (show_perks)	
					{
						if (selected_class == 5)
						{
							show_inventory = false;
							show_pipboy = true;
							show_trader = false;
							show_perks = false;
							show_items = false;
							show_gainperk = false;
						}
						else if (selected_class == 6)
						{
							show_inventory = false;
							show_pipboy = false;
							show_trader = false;
							show_perks = false;
							show_items = true;
							show_gainperk = false;
						}
						else if (selected_class == 7)
						{
							show_inventory = false;
							show_pipboy = false;
							show_trader = false;
							show_perks = true;
							show_items = false;
							show_gainperk = false;
						}
						else if (selected_class == 8)
						{
							show_inventory = false;
							show_pipboy = false;
							show_trader = false;
							show_perks = false;
							show_items = false;
							show_gainperk = true;
							perkslot = 1;
						}
						else if (selected_class == 9)
						{
							show_inventory = false;
							show_pipboy = false;
							show_trader = false;
							show_perks = false;
							show_items = false;
							show_gainperk = true;
							perkslot = 2;
						}
					}
					else if (slotnum >= 1 && slotnum <= 22)
					{
						//if there's actually an item there		
						if ((show_inventory) && (getstati(32+slotnum-1) != 0))	
						{
							showcontextmenu = true;	//show the context menu
							contextpos = mousepos;
						}
						else if ((show_trader) && (getstati(70+slotnum) != 0))	
						{
							showcontextmenu = true;	//show the context menu
							contextpos = mousepos;
						}
					}
					else if (slotnum >= 23 && slotnum <= 24)
					{
						//if there's actually an item there		
						if ((show_inventory) && (getstati(97) != 0) && (getstati(98) != 0))	
						{
							showmixmenu = true;	//show the context menu
							contextpos = mousepos;
						}
					}
					else
					{
						show_inventory = false;	//they clicked outside, fools!
						showcontextmenu = false;
						showmixmenu = false;
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
	if (eventtype == 2 && (show_inventory || show_trader || show_pipboy || show_items || show_perks || show_gainperk))	//mouse
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
	k_mwheelup = stringtokeynum("K_MWHEELUP");
	k_mwheeldown = stringtokeynum("K_MWHEELDOWN");
};


void(vector pos, float slotno) ShopImage =
{
	local float it, mod_s, mod_e;
	local string itname;


	if (slotno == downslotnum && slotnum)
		slotno = slotnum;
	else if (slotno == slotnum && downslotnum)
		slotno = downslotnum;


	it = getstati(99+slotno);
	itname = GetItemImage(ToIID(it));
	drawpic(pos, "gui/blank.jpg", IMGSIZEV, '1 1 1', 1);

	it = ToStatus(it);
	if (it <= 1)
		return;
	itname = ftos(it);
	it = strlen(itname);
	drawstring(pos + IMGSIZEV - '0 8 0' - '8 0 0'*it, itname, '8 8 8', '1 1 1', 1);
};

void() Invent_Draw2 =
{
	local float i;
	local float it;
	local string itname;
	local float op;
	
	local float width, height;
	width = cvar("vid_conwidth");
	height = cvar("vid_conheight");
	//how much space have we got for the slider?


	local float slotofs;
	slotofs = sliderpos*IMGSIZEF*8 + IMGSIZEF*3;

	for (i = 1; i < 8; i++)
	{
		ShopImage(((i*(width-64)-slotofs) * '0 1 0'), i+1);
	}




	drawpic(('1 0 0'*(width-64)), "gui/scrollup.jpg", '16 16 0', '1 1 1', 1);
	for (i = 0; i < height; i+=16)
		drawpic('1 0 0'*(width-64)+'0 16 0'+i*'0 1 0', "gui/scrollbar.jpg", '16 16 0', '1 1 1', 1);
	drawpic(('1 0 0'*(width-64))+(height-16)*'0 1 0', "gui/scrolldown.jpg", '16 16 0', '1 1 1', 1);

	drawpic(('1 0 0'*(width-64))+'0 16 0'+sliderpos*(height-48)*'0 1 0', "gui/scrollbox.jpg", '16 16 0', '1 1 1', 1);


	if (showcontextmenu)
	{
		op = floor((mousepos_y - contextpos_y)/8);
		drawfill(contextpos - '8 8 0', '88 24 0'+'16 16 0', '0 0 0', 0.7);
		drawstring(contextpos + (0 * '0 8 0'), "Purchase", '8 8 0', '1 1 0' + (op!=0)*'0 0 1', 1);
		drawstring(contextpos + (1 * '0 8 0'), "Barter", '8 8 0', '1 1 0' + (op!=1)*'0 0 1', 1);
	}
	else
	{
		if (mousepos_x >= IMGSIZEF)
		{
			slotnum = 0;
			if (mousepos_x >= 112 && mousepos_x <= 112+3*IMGSIZEF)
				if (mousepos_y >= 256 && mousepos_y <= 256+IMGSIZEF)
					slotnum = floor((mousepos_x - 112)/IMGSIZEF) + 1;
		}
		else
		{
			slotnum = floor((mousepos_y + slotofs)/IMGSIZEF) + 1;
		}
//			slotnum = floor((mousepos_y - toppos_y)/8) + 1;
	}

	drawstring('128 0 0', ftos(slotnum), '8 8 0', '1 1 1', 1);

	it = getstati(99+slotnum);
	itname = GetItemName(ToIID(it));
	drawstring('128 32 0', itname, '8 8 0', '1 1 1', 1);
	itname = GetItemDesc(ToIID(it));
	drawstring('128 48 0', itname, '8 8 0', '1 1 1', 1);

	drawpic(mousepos, "gui/cursor.jpg", '1 1 0'*CURSORSIZE, '1 1 1', 1);
};