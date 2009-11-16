entity viewentity;
float viewmodelindex;

.float lerptime;

float scrwidth;
float scrheight;



void() DrawScreen =
{
      ScreenImage();
};
 

void() Crosshair_Draw =
{
	local float x, a;
	local vector y;


	if (getstati(54) == 0)
		a = 16;
	else if (getstati(54) == 1)
		a = 12;
	else if (getstati(54) == 2)
		a = 8;

	y = '1 0 0'*scrwidth*0.50 + '0 1 0'*scrheight*0.50;

	x = a + (4 * getstati(99));
	Crosshair(y + '1 0 0'*x, "gui/xhair.jpg");
	Crosshair(y + '-1 0 0'*x, "gui/xhair.jpg");
	Crosshair(y + '0 1 0'*x, "gui/xhair.jpg");
	Crosshair(y + '0 -1 0'*x, "gui/xhair.jpg");

	x = (a+2) + (4 * getstati(99));
	Crosshair(y + '1 0 0'*x, "gui/xhair.jpg");
	Crosshair(y + '-1 0 0'*x, "gui/xhair.jpg");
	Crosshair(y + '0 1 0'*x, "gui/xhair.jpg");
	Crosshair(y + '0 -1 0'*x, "gui/xhair.jpg");

	x = (a-2) + (4 * getstati(99));
	Crosshair(y + '1 0 0'*x, "gui/xhair.jpg");
	Crosshair(y + '-1 0 0'*x, "gui/xhair.jpg");
	Crosshair(y + '0 1 0'*x, "gui/xhair.jpg");
	Crosshair(y + '0 -1 0'*x, "gui/xhair.jpg");

};

void() Position_Draw =
{
	float pos, s;
	string sn;

	pos = getstati(54);
	s = getstati(58);
	sn = ftos(s);

	if (pos == 0)
		drawpic('0 0 0'+('1 0 0'*(scrwidth*0.93)) +('0 1 0'*(scrheight*0.87)), "gui/hud/stand.png", '0 0 0'+'32 60 0'*(scrwidth/640), '1 1 1');
	else if (pos == 1)
		drawpic('0 0 0'+('1 0 0'*(scrwidth*0.93)) +('0 1 0'*(scrheight*0.87)), "gui/hud/duck.png", '0 0 0'+'32 60 0'*(scrwidth/640), '1 1 1');
	else if (pos == 2)
		drawpic('0 0 0'+('1 0 0'*(scrwidth*0.93)) +('0 1 0'*(scrheight*0.87)), "gui/hud/prone.png", '0 0 0'+'32 60 0'*(scrwidth/640), '1 1 1');

	if (s > 0)
	{
		drawpic('0 0 0'+('1 0 0'*(scrwidth*0.20)) +('0 1 0'*(scrheight*0.79)), "gui/hud/sneak.png", '0 0 0'+'72 25 0'*(scrwidth/640), '1 1 1');
		//drawstring('0 0 0'+('1 0 0'*(scrwidth*0.20)) +('0 1 0'*(scrheight*0.80)),  strcat("%", sn), '8 8 8', '1 1 1', 1);
	}
};

void() Chem_Draw =
{
	local float rage, heal;

	rage = getstati(55);
	heal = getstati(56);

	if (rage == 255)
		ChemImage('0 -24 0'+('1 0 0'*(scrwidth*0.60)) +('0 1 0'*(scrheight*0.20)), 0);
	else if (rage == IID_CHEM_ADRENALINE)
		ChemImage('0 -24 0'+('1 0 0'*(scrwidth*0.60)) +('0 1 0'*(scrheight*0.20)), 1);
	else if (rage == IID_CHEM_PSYCHO)
		ChemImage('0 -24 0'+('1 0 0'*(scrwidth*0.60)) +('0 1 0'*(scrheight*0.20)), 2);
	else if (rage == IID_CHEM_BESERK)
		ChemImage('0 -24 0'+('1 0 0'*(scrwidth*0.60)) +('0 1 0'*(scrheight*0.20)), 3);
	else if (heal > 0)
		ChemImage('32 -24 0'+('1 0 0'*(scrwidth*0.60)) +('0 1 0'*(scrheight*0.20)), 4);
	else if (rage == 777)
		ChemImage('0 -24 0'+('1 0 0'*(scrwidth*0.60)) +('0 1 0'*(scrheight*0.20)), 5);

};



void() Time_Draw =
{
	local float xtime, ytime, ztime;
	local string qtime, rtime, shadowtime;

	xtime = getstati(104);
	ytime = floor(xtime/60);
	ztime = xtime - (ytime*60);
	qtime = ftos(ytime);
	rtime = ftos(ztime);
	shadowtime = ftos(getstati(104));

	drawstring('0 32 0'+('1 0 0'*(scrwidth*0.10))+('0 1 0'*(scrheight*0.90)),  strcat("[", qtime, ":", rtime, "]\n"), '8 8 8', '1 1 1', 1);
};





void() Weight_Draw =
{
	float carry,max,enc;
	string image;

	carry = getstati(52);
	max = getstati(53);

	enc = carry/max;

	if (enc <= 0.10)
		image = "weight/weight_zero.jpg";
	else if (enc <= 0.20)
		image = "weight/weight_one.jpg";
	else if (enc <= 0.35)
		image = "weight/weight_two.jpg";
	else if (enc <= 0.50)
		image = "weight/weight_three.jpg";
	else if (enc <= 0.65)
		image = "weight/weight_four.jpg";
	else if (enc <= 0.80)
		image = "weight/weight_five.jpg";
	else if (enc <= 0.95)
		image = "weight/weight_six.jpg";
	else
		image = "weight/weight_seven.jpg";

      MoneyImage('0 -24 0'+('1 0 0'*(scrwidth*0.50)) +('0 1 0'*(scrheight*0.90)), image);

};


void() Health_Draw =
{
	local float bar, hp, maxhp, top;

	hp = getstati(48);
	maxhp = getstati(49);

	bar = 0;
	top = ceil(150 * (hp/maxhp));

	if (top > 150)
		top = 150;

	while (bar <= top)
	{

			drawpic('0 0 0'+('1 0 0'*(scrwidth*0.07)) +('0 1 0'*(scrheight*0.75)) - ('0 1 0'*bar*(scrheight/640)),  "gui/graph/green.jpg", '0 0 0'+'12 1 0'*(scrwidth/640), '1 1 1', 1);				
		

		bar = bar + 1;
	}

};


void() Radiation_Draw =
{
	local float bar, rad, top;

	rad = getstati(106);

	bar = 0;
	top = ceil(150 * (rad/100));

	if (top > 150)
		top = 150;

	while (bar <= top)
	{

			drawpic('0 0 0'+('1 0 0'*(scrwidth*0.02)) +('0 1 0'*(scrheight*0.75)) - ('0 1 0'*bar*(scrheight/640)),  "gui/graph/yellow.jpg", '0 0 0'+'12 1 0'*(scrwidth/640), '1 1 1', 1);				
		

		bar = bar + 1;
	}

};



void() DoThatViewModelThing =
{
	float newframe, newmodel;

	newframe = getstati(STAT_WEAPONFRAME);
	newmodel = getstati(STAT_WEAPONMODEL);

	if (newmodel != viewmodelindex)
	{	//changed entirly.
		viewmodelindex = newmodel;
		self.frame2 = self.frame = newframe;
		self.lerptime = time;
	}
	else if (newframe != self.frame)
	{
		self.frame2 = self.frame;
		self.frame = newframe;
		self.lerptime = time;
	}
	self.lerpfrac = 1-(time-self.lerptime)*10;

	if (self.lerpfrac < 0) self.lerpfrac = 0;


	self.modelindex = viewmodelindex;
};

void() CSQC_Init =
{
	viewentity = spawn();
	viewentity.predraw = DoThatViewModelThing;
	viewentity.drawmask = MASK_NORMAL;
	viewentity.renderflags = RF_VIEWMODEL | RF_DEPTHHACK;

	FigureOutButtons();
};


void(vector pos, float x) HUD_Image =
{
	local float it, mod_s, mod_e;
	local string itname;


	if (x <= 16)
		it = getstati(31+x);


	itname = GetItemImage(ToIID(it));
	drawpic(pos, strcat("gui/", itname), '48 48 0', '1 1 1', 1);

	it = ToStatus(it);
	if (it <= 1)
		return;
	itname = ftos(it);
	it = strlen(itname);
	drawstring(pos + '48 48 0' - '0 8 0' - '8 0 0'*it, itname, '8 8 8', '1 1 1', 1);
};


void(vector pos, float slotno) HUD_Weapon_Image =
{
	local float it, mod_s, mod_e;
	local string itname;


	if (slotno == downslotnum && slotnum)
		slotno = slotnum;
	else if (slotno == slotnum && downslotnum)
		slotno = downslotnum;

	if (slotno <= 16)
		it = getstati(31+slotno);


	itname = GetItemImage(ToIID(it));
	drawpic(pos, strcat("gui/weapons/", itname), '180 59 0', '1 1 1', 1);

};

void() Ammo_Draw =
{
	local float hand, weap, ammo, amount, max, bar, top;
	local string reveal;

	hand = getstati(61);

	if (hand == 1)
	{
           weap = ToIID(getstati(32));
           ammo = ToStatus(getstati(32));
           max = WeaponMagQuant(weap);


	bar = 0;
	top = ceil(35 * (ammo/max));

		if (top > 35)
			top = 35;

		while (bar <= top)
		{

				drawpic('0 0 0'+('1 0 0'*(scrwidth*0.38)) +('0 1 0'*(scrheight*0.99)) - ('0 2 0'*bar*(scrheight/640)), "gui/graph/green.jpg", '0 0 0'+'8 1 0'*(scrwidth/640), '1 1 1', 1);				
		

			bar = bar + 1;
		}

	}

	if (hand == 2)
	{
           weap = ToIID(getstati(33));
           ammo = ToStatus(getstati(33));
           max = WeaponMagQuant(weap);


	bar = 0;
	top = ceil(35 * (ammo/max));

		if (top > 35)
			top = 35;

		while (bar <= top)
		{

				drawpic('0 0 0'+('1 0 0'*(scrwidth*0.38)) +('0 1 0'*(scrheight*0.99)) - ('0 2 0'*bar*(scrheight/640)), "gui/graph/green.jpg", '0 0 0'+'8 1 0'*(scrwidth/640), '1 1 1', 1);					
		

			bar = bar + 1;
		}

	}

};


void(float vwidth, float vheight, float do2d) CSQC_UpdateView =
{
	float usehud;

	scrwidth = vwidth;
	scrheight = vheight;

	clearscene();

	if (0)//!cvar("cg_usehud"))
	{
		setviewprop(VF_DRAWENGINESBAR, 0);
		usehud = true;
	}
	else
	{
		setviewprop(VF_DRAWENGINESBAR, 1);
	}

	setviewprop(VF_DRAWCROSSHAIR, 1);

	//force fullscreen views.
	setviewprop(VF_MIN, '0 0 0');
	setviewprop(VF_SIZE_X, scrwidth);
	setviewprop(VF_SIZE_Y, scrheight);

	addentities(MASK_ENGINE | MASK_NORMAL);

	renderscene();


	if (getstati(57) == 1)
		BriefImage();
	else if (getstati(57) == 2)
		DrawScreen();
	else if (getstati(57) == 3)
		DrawScreen();
	else if (getstati(57) == 4)//pain
		DrawScreen();
	else if (getstati(57) == 5)//flash
		DrawScreen();
	else if (getstati(109) == 0)
		DrawScreen();
	else if (show_trader)
		Trader_Draw();
	else if (show_inventory)
		Invent_Draw();
	else if (show_pipboy)
		Pipboy_Draw();
	else if (show_perks)
		Perks_Draw();
	else if (show_items)
		Items_Draw();
	else if (show_gainperk)
		GainPerk_Draw();
	else
	{
		drawpic('0 0 0'+('1 0 0'*(scrwidth*0.06)) +('0 1 0'*(scrheight*0.45)), "gui/hud/health.png", '0 0 0'+'25 155 0'*(scrwidth/640), '1 1 1');
		drawpic('0 0 0'+('1 0 0'*(scrwidth*0.01)) +('0 1 0'*(scrheight*0.45)), "gui/hud/radiation.png", '0 0 0'+'25 155 0'*(scrwidth/640), '1 1 1');
		drawpic('0 0 0' + ('0 1 0'*(scrheight*0.84)), "gui/hud/hud.png", '0 0 0'+'80 10 0'*(scrwidth/80), '1 1 1');

		Health_Draw();
		Radiation_Draw();

		HUD_Weapon_Image('0 0 0'+('1 0 0'*(scrwidth*0.38)) +('0 1 0'*(scrheight*0.87)), getstati(61));
		Ammo_Draw();

		SlotImageSmall('0 0 0'+('1 0 0'*(scrwidth*0.72)) +('0 1 0'*(scrheight*0.87)), getstati(5));
		SlotImageSmall('0 0 0'+('1 0 0'*(scrwidth*0.79)) +('0 1 0'*(scrheight*0.87)), getstati(6));

		Position_Draw();
		Chem_Draw();
		Time_Draw();
		Crosshair_Draw();
	}
};
