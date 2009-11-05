void bprint(float plev, string st, string s2){}
float PRINT_MEDIUM = 1;
#include "inventory.qc"

float show_inventory;

vector mousepos;

float showcontextmenu;
vector contextpos;

float slotnum;	//the current slot under the mousecursor

float sliderpos;

float downslotnum;	//the slot number when the mousecursor when down

vector toppos = '0 32 0';

float k_mouse1;
float k_mwheelup;
float k_mwheeldown;
float mouseisdown;

#define IMGSIZEF 64
#define IMGSIZEV ('1 1 0'*IMGSIZEF)
#define IMGSIZEK 32
#define IMGSIZEH ('1 1 0'*IMGSIZEK)
#define IMGSIZEX 48
#define IMGSIZEZ ('1 1 0'*IMGSIZEX)
#define IMGSIZEQ ('1 1 0'*2)

#define CURSORSIZE 8

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

	}

	itname = GetItemImage(ToIID(it));
	drawpic(pos, strcat("gui/", itname), IMGSIZEV, '1 1 1', 1);

	mod_s = getstati(70);
	mod_e = getstati(71);

	it = ToStatus(it);
	if (it < 1)
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
	}

	itname = GetItemImage(ToIID(it));
	drawpic(pos, strcat("gui/", itname), IMGSIZEZ, '1 1 1', 1);

	mod_s = getstati(70);
	mod_e = getstati(71);


	if (it > 0)
	{
		if (mod_s == it)
			drawstring(pos, "sil", '8 8 8', '1 1 1', 1);

		if (mod_e == it)
			drawstring(pos, "ext", '8 8 8', '1 1 1', 1);
	}

	it = ToStatus(it);
	if (it <= 1)
		return;
	itname = ftos(it);
	it = strlen(itname);
	drawstring(pos + IMGSIZEZ - '0 8 0' - '8 0 0'*it, itname, '8 8 8', '1 1 1', 1);
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

void() Invent_Draw =
{
	local float i;
	local float it;
	local string itname, s1, s2, s3, s4, s5, s6, s7, s8, s9, perki, perk2i, p1, p2, pn, pd;
	local float op;
	
	local float height,width;
	height = cvar("vid_conheight");
	width = cvar("vid_conwidth");
	//how much space have we got for the slider?


	local float slotofs;
	slotofs = sliderpos*(height - (IMGSIZEF*6)) + (IMGSIZEF*6);

	for (i = 6; i <= MAXSLOTS; i++)
	{
		SlotImage(((i*IMGSIZEF-slotofs) * '0 1 0'), i+1);

//		itname = GetItemName(ToIID(it));
//		drawstring(toppos + ((i*8-slotofs) * '0 1 0'), strcat(itname, " (", ftos(ToStatus(it)), ")"), '8 8 0', '1 1 1', 1);
	}

	drawstring('112 248 0', " HAND1 ", '8 8 8', '1 1 1', 1);
	SlotImage('112 256 0', 1);
	drawstring('112 248 0'+('1 0 0'*IMGSIZEF), " HAND2 ", '8 8 8', '1 1 1', 1);
	SlotImage('112 256 0'+('1 0 0'*IMGSIZEF), 2);
	drawstring('112 248 0'+('2 0 0'*IMGSIZEF), " ARMOUR ", '8 8 8', '1 1 1', 1);
	SlotImage('112 256 0'+('2 0 0'*IMGSIZEF), 3);


	drawstring('112 120 0', " EQUIP ", '8 8 8', '1 1 1', 1);
	SlotImage('112 128 0', 4);
	drawstring('112 120 0'+('1 0 0'*IMGSIZEF), " POUCH1 ", '8 8 8', '1 1 1', 1);
	SlotImage('112 128 0'+('1 0 0'*IMGSIZEF), 5);
	drawstring('112 120 0'+('2 0 0'*IMGSIZEF), " POUCH2 ", '8 8 8', '1 1 1', 1);
	SlotImage('112 128 0'+('2 0 0'*IMGSIZEF), 6);

	drawstring('120 72 0'+('4 0 0'*IMGSIZEF), " JUNK ", '8 8 8', '1 1 1', 1);
	SlotImageSmall('112 80 0'+('4 0 0'*IMGSIZEF), 17);
	SlotImageSmall('112 128 0'+('4 0 0'*IMGSIZEF), 18);
	SlotImageSmall('112 176 0'+('4 0 0'*IMGSIZEF), 19);
	SlotImageSmall('112 224 0'+('4 0 0'*IMGSIZEF), 20);
	SlotImageSmall('96 80 0'+('5 0 0'*IMGSIZEF), 21);
	SlotImageSmall('96 128 0'+('5 0 0'*IMGSIZEF), 22);
	SlotImageSmall('96 176 0'+('5 0 0'*IMGSIZEF), 23);
	SlotImageSmall('96 224 0'+('5 0 0'*IMGSIZEF), 24);


	perki = GetPerkImage(getstati(60));
	perk2i = GetPerkImage(getstati(59));

	drawstring('112 328 0'+('3 0 0'*IMGSIZEF), " PERK1 ", '8 8 8', '1 1 1', 1);
	drawpic('112 336 0'+('3 0 0'*IMGSIZEF),  perki, IMGSIZEV, '1 1 1', 1);
	drawstring('112 328 0'+('5 0 0'*IMGSIZEF), " PERK2 ", '8 8 8', '1 1 1', 1);
	drawpic('112 336 0'+('5 0 0'*IMGSIZEF), perk2i, IMGSIZEV, '1 1 1', 1);

	p1 = PerkName(1, getstati(60));
	p2 = PerkName(2, getstati(59));

	drawstring('112 408 0'+('3 0 0'*IMGSIZEF), p1, '8 8 8', '1 1 1', 1);
	drawstring('112 408 0'+('5 0 0'*IMGSIZEF), p2, '8 8 8', '1 1 1', 1);

	i = getstati(62);
	if (i == 1)
		itname = "team: rangers";
	else if (i == 2)
		itname = "team: raiders";
	else
		itname = "team: none";
	drawstring('112 264 0'+('0 1 0'*IMGSIZEF), itname, '8 8 8', '1 1 1', 1);
	i = getstati(63);
	if (i == 1)
	{	
		itname = "skill set: first aid";
		s1 = "small arms: 65%";
		s2 = "big guns:   35%";
		s3 = "melee:      45%";
		s4 = "sneak:      40%";
		s5 = "first aid:  90%";
		s6 = "doctor:     75%";
		s7 = "science:    25%";
		s8 = "repair:     35%";
		s9 = "thrown:     50%";
	}
	else if (i == 2)
	{	
		itname = "skill set: stealth";
		s1 = "small arms: 75%";
		s2 = "big guns:   25%";
		s3 = "melee:      80%";
		s4 = "sneak:      95%";
		s5 = "first aid:  55%";
		s6 = "doctor:     15%";
		s7 = "science:    55%";
		s8 = "repair:     25%";
		s9 = "thrown:     75%";
	}
	else if (i == 3)
	{	
		itname = "skill set: combat";
		s1 = "small arms: 90%";
		s2 = "big guns:   75%";
		s3 = "melee:      80%";
		s4 = "sneak:      55%";
		s5 = "first aid:  55%";
		s6 = "doctor:     15%";
		s7 = "science:    15%";
		s8 = "repair:     15%";
		s9 = "thrown:     75%";
	}
	else if (i == 4)
	{	
		itname = "skill set: science";
		s1 = "small arms: 25%";
		s2 = "big guns:   10%";
		s3 = "melee:      15%";
		s4 = "sneak:      35%";
		s5 = "first aid:  65%";
		s6 = "doctor:     25%";
		s7 = "science:    95%";
		s8 = "repair:     90%";
		s9 = "thrown:     25%";
	}
	else
	{
		itname = "class: none";
		s1 = "small arms: 10%";
		s2 = "big guns:    5%";
		s3 = "melee:       5%";
		s4 = "sneak:      10%";
		s5 = "first aid:  10%";
		s6 = "doctor:      5%";
		s7 = "science:    15%";
		s8 = "repair:      5%";
	}

	drawstring('112 272 0'+('0 1 0'*IMGSIZEF), itname, '8 8 8', '1 1 1', 1);
	drawstring('112 280 0'+('0 1 0'*IMGSIZEF), s1, '8 8 8', '1 1 1', 1);
	drawstring('112 288 0'+('0 1 0'*IMGSIZEF), s2, '8 8 8', '1 1 1', 1);
	drawstring('112 296 0'+('0 1 0'*IMGSIZEF), s3, '8 8 8', '1 1 1', 1);
	drawstring('112 304 0'+('0 1 0'*IMGSIZEF), s4, '8 8 8', '1 1 1', 1);
	drawstring('112 312 0'+('0 1 0'*IMGSIZEF), s5, '8 8 8', '1 1 1', 1);
	drawstring('112 320 0'+('0 1 0'*IMGSIZEF), s6, '8 8 8', '1 1 1', 1);
	drawstring('112 328 0'+('0 1 0'*IMGSIZEF), s7, '8 8 8', '1 1 1', 1);
	drawstring('112 336 0'+('0 1 0'*IMGSIZEF), s8, '8 8 8', '1 1 1', 1);
	drawstring('112 344 0'+('0 1 0'*IMGSIZEF), s9, '8 8 8', '1 1 1', 1);



	drawpic(('1 0 0'*IMGSIZEF), "gui/scrollup.jpg", '16 16 0', '1 1 1', 1);
	for (i = 0; i < height; i+=16)
		drawpic('1 0 0'*IMGSIZEF+'0 16 0'+i*'0 1 0', "gui/scrollbar.jpg", '16 16 0', '1 1 1', 1);
	drawpic(('1 0 0'*IMGSIZEF)+(height-16)*'0 1 0', "gui/scrolldown.jpg", '16 16 0', '1 1 1', 1);

	drawpic(('1 0 0'*IMGSIZEF)+'0 16 0'+sliderpos*(height-48)*'0 1 0', "gui/scrollbox.jpg", '16 16 0', '1 1 1', 1);


	if (showcontextmenu)
	{
			drawfill(contextpos - '8 8 0', '88 24 0'+'96 96 0', '0 0 0', 0.9);

		op = floor((mousepos_y - contextpos_y)/8);
		drawstring(contextpos + (0 * '0 8 0'), "use/reload", '8 8 0', '1 1 0' + (op!=0)*'0 0 1', 1);
		drawstring(contextpos + (1 * '0 8 0'), "put in hand", '8 8 0', '1 1 0' + (op!=1)*'0 0 1', 1);
		if (getstati(108) == 0)
			drawstring(contextpos + (2 * '0 8 0'), "drop", '8 8 0', '1 1 0' + (op!=2)*'0 0 1', 1);
		if (getstati(108) == 1)
			drawstring(contextpos + (2 * '0 8 0'), "sell", '8 8 0', '1 1 0' + (op!=2)*'0 0 1', 1);
		if (getstati(60) == 14 || getstati(59) == 14)
			drawstring(contextpos + (3 * '0 8 0'), "mix chems", '8 8 0', '1 1 0' + (op!=3)*'0 0 1', 1);

			drawstring(contextpos + (4 * '0 8 0'), "build: flash bomb", '8 8 0', '1 1 0' + (op!=4)*'0 0 1', 1);
			drawstring(contextpos + (5 * '0 8 0'), "build: frag grenade", '8 8 0', '1 1 0' + (op!=5)*'0 0 1', 1);
			drawstring(contextpos + (6 * '0 8 0'), "build: stun grenade", '8 8 0', '1 1 0' + (op!=6)*'0 0 1', 1);
			drawstring(contextpos + (7 * '0 8 0'), "build: silencer", '8 8 0', '1 1 0' + (op!=7)*'0 0 1', 1);

			if (getstati(63) == 4)
			{
				drawstring(contextpos + (8 * '0 8 0'), "build: scope", '8 8 0', '1 1 0' + (op!=8)*'0 0 1', 1);
				drawstring(contextpos + (9 * '0 8 0'), "build: x-ray device", '8 8 0', '1 1 0' + (op!=9)*'0 0 1', 1);
				drawstring(contextpos + (10 * '0 8 0'), "build: modification", '8 8 0', '1 1 0' + (op!=10)*'0 0 1', 1);
			}
	}
	else
	{
		if (mousepos_x >= IMGSIZEF)
		{
			slotnum = 0;
			if (mousepos_x >= 112 && mousepos_x <= 112+3*IMGSIZEF)
				if (mousepos_y >= 256 && mousepos_y <= 256+IMGSIZEF)
					slotnum = floor((mousepos_x - 112)/IMGSIZEF) + 1;

			if (mousepos_x >= 112 && mousepos_x <= 112+3*IMGSIZEF)
				if (mousepos_y >= 128 && mousepos_y <= 128+IMGSIZEF)
					slotnum = floor((mousepos_x - 112)/IMGSIZEF) + 4;

			if (mousepos_x >= 112+3*IMGSIZEF && mousepos_x <= 112+4*IMGSIZEF)
				if (mousepos_y >= 336 && mousepos_y <= 336+IMGSIZEF)
					slotnum = 98;

			if (mousepos_x >= 112+5*IMGSIZEF && mousepos_x <= 112+6*IMGSIZEF)
				if (mousepos_y >= 336 && mousepos_y <= 336+IMGSIZEF)
					slotnum = 99;

			
			if (mousepos_x >= 112+4*IMGSIZEF && mousepos_x <= 112+5*IMGSIZEF)
				if (mousepos_y >= 80 && mousepos_y <= 80+4*IMGSIZEX)
					slotnum = floor((mousepos_y - 80)/IMGSIZEX) + 17;

			if (mousepos_x >= 112+5*IMGSIZEF && mousepos_x <= 112+6*IMGSIZEF)
				if (mousepos_y >= 80 && mousepos_y <= 80+4*IMGSIZEX)
					slotnum = floor((mousepos_y - 80)/IMGSIZEX) + 21;
		}
		else
		{
			slotnum = floor((mousepos_y + slotofs)/IMGSIZEF) + 1;
		}
//			slotnum = floor((mousepos_y - toppos_y)/8) + 1;
	}

	drawstring('128 0 0', ftos(slotnum), '8 8 0', '1 1 1', 1);

	drawfill('120 24 0', '512 40 0', '0 0 0', 0.7);

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
	drawstring('128 32 0', itname, '8 8 0', '1 1 1', 1);
	itname = GetItemDesc(ToIID(it));
	drawstring('128 48 0', itname, '8 8 0', '1 1 1', 1);
}
else if (slotnum == 98)
{
	pn = PerkName(1, getstati(60));
	pd = GetPerkDesc(getstati(60));
	drawstring('128 32 0', pn, '8 8 0', '1 1 1', 1);
	drawstring('128 48 0', pd, '8 8 0', '1 1 1', 1);
}
else if (slotnum == 99)
{
	pn = PerkName(2, getstati(59));
	pd = GetPerkDesc(getstati(59));
	drawstring('128 32 0', pn, '8 8 0', '1 1 1', 1);
	drawstring('128 48 0', pd, '8 8 0', '1 1 1', 1);
}
//	drawfill(mousepos, '8 8 0', '0 0 0', 0.7);
//	drawstring(mousepos, "^", '8 8 0', '1 1 1', 1);
	drawpic(mousepos, "gui/cursor.jpg", '1 1 0'*CURSORSIZE, '1 1 1', 1);
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
				else if ((op == 2) && (getstati(108) == 0))
					localcmd(strcat("cmd invdrop ", ftos(slotnum), "\n"));
				else if ((op == 2) && (getstati(108) == 1))
					localcmd(strcat("cmd invsell ", ftos(slotnum), "\n"));
				else if (op == 3)
					localcmd(strcat("cmd invmix ", ftos(slotnum), "\n"));
				else if (op == 4)
					localcmd("cmd create flash\n");
				else if (op == 5)
					localcmd("cmd create grenade\n");
				else if (op == 6)
					localcmd("cmd create stun\n");
				else if (op == 7)
					localcmd("cmd create silencer\n");
				else if (op == 8)
					localcmd("cmd create extender\n");
				else if (op == 9)
					localcmd("cmd create xray\n");
				else if (op == 10)
					localcmd("cmd create modify\n");

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
					if (slotnum >= 1 && slotnum <= 24)
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