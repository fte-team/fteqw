entity viewentity;
float viewmodelindex;

.float lerptime;

float scrwidth;
float scrheight;

void() Quick_Draw =
{

      drawstring('32 -80 0'+('1 0 0'*(scrwidth*0.80)) +('0 1 0'*(scrheight*0.90)), " q1 ", '8 8 8', '1 1 1', 1);
      QuickImage('32 -72 0'+('1 0 0'*(scrwidth*0.80)) +('0 1 0'*(scrheight*0.90)), 5);

      drawstring('64 -80 0'+('1 0 0'*(scrwidth*0.80)) +('0 1 0'*(scrheight*0.90)), " q2 ", '8 8 8', '1 1 1', 1);
      QuickImage('64 -72 0'+('1 0 0'*(scrwidth*0.80)) +('0 1 0'*(scrheight*0.90)), 6);
};

void() DrawScreen =
{
      ScreenImage();
};

void() Weapon_Draw =
{
	local float hand;

	hand = getstati(61);

	if (hand == 1)
	{
		drawstring('0 -32 0'+('1 0 0'*(scrwidth*0.80)) +('0 1 0'*(scrheight*0.90)), "   HAND 1   ", '8 8 8', '1 1 1', 1);
		SlotImage('0 -24 0'+('1 0 0'*(scrwidth*0.80)) +('0 1 0'*(scrheight*0.90)), 1);
	}
	if (hand == 2)
	{
		drawstring('0 -32 0'+('1 0 0'*(scrwidth*0.80)) +('0 1 0'*(scrheight*0.90)), "   HAND 2   ", '8 8 8', '1 1 1', 1);
		SlotImage('0 -24 0'+('1 0 0'*(scrwidth*0.80)) +('0 1 0'*(scrheight*0.90)), 2);
	}
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

	if (s > 0)
	{
		drawstring('0 -32 0'+('1 0 0'*(scrwidth*0.40)) +('0 1 0'*(scrheight*0.90)),  "sneak", '8 8 8', '1 1 1', 1);
		PosImage('0 -24 0'+('1 0 0'*(scrwidth*0.40)) +('0 1 0'*(scrheight*0.90)), 1);
		drawstring('0 8 0'+('1 0 0'*(scrwidth*0.40)) +('0 1 0'*(scrheight*0.90)),  strcat("%", sn), '8 8 8', '1 1 1', 1);
	}
	else if (pos == 1)
	{
		drawstring('0 -32 0'+('1 0 0'*(scrwidth*0.40)) +('0 1 0'*(scrheight*0.90)),  "duck", '8 8 8', '1 1 1', 1);
		PosImage('0 -24 0'+('1 0 0'*(scrwidth*0.40)) +('0 1 0'*(scrheight*0.90)), 1);
	}
	else if (pos == 2)
	{
		drawstring('0 -32 0'+('1 0 0'*(scrwidth*0.40)) +('0 1 0'*(scrheight*0.90)),  "prone", '8 8 8', '1 1 1', 1);
		PosImage('0 -24 0'+('1 0 0'*(scrwidth*0.40)) +('0 1 0'*(scrheight*0.90)), 2);
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

void() Ammo_Draw =
{
	local float hand, weap, ammo, amount;
	local string reveal;

	hand = getstati(61);

	if (hand == 1)
	{
           weap = ToIID(getstati(32));
           ammo = WeaponAmmoType(weap);
           amount = ShadowSlotOfItem(self, ammo);
           //drawstring('300 350 0'+('1 0 0'*IMGSIZEF), " AMMO ", '8 8 8', '1 1 1', 1);
           //SlotImage('300 358 0'+('1 0 0'*IMGSIZEF), amount);
           drawstring('-64 -32 0'+('1 0 0'*(scrwidth*0.80)) +('0 1 0'*(scrheight*0.90)), "  AMMO  ", '8 8 8', '1 1 1', 1);
           if (amount > 0)
			SlotImage('-64 -24 0'+('1 0 0'*(scrwidth*0.80)) +('0 1 0'*(scrheight*0.90)), amount);
           else
			BlankImage('-64 -24 0'+('1 0 0'*(scrwidth*0.80)) +('0 1 0'*(scrheight*0.90)), 0);
	}
	else if (hand == 2)
	{
           weap = ToIID(getstati(33));
           ammo = WeaponAmmoType(weap);
           amount = ShadowSlotOfItem(self, ammo);
           reveal = ftos(amount);
           //drawstring('300 350 0'+('1 0 0'*IMGSIZEF), " AMMO ", '8 8 8', '1 1 1', 1);
           //SlotImage('300 358 0'+('1 0 0'*IMGSIZEF), amount);
           drawstring('-64 -32 0'+('1 0 0'*(scrwidth*0.80)) +('0 1 0'*(scrheight*0.90)), "  AMMO  ", '8 8 8', '1 1 1', 1);
           if (amount > 0)
			SlotImage('-64 -24 0'+('1 0 0'*(scrwidth*0.80)) +('0 1 0'*(scrheight*0.90)), amount);
           else
			BlankImage('-64 -24 0'+('1 0 0'*(scrwidth*0.80)) +('0 1 0'*(scrheight*0.90)), 0);
	}
};

void() Money_Draw =
{
	string cash;

	cash = ftos(getstati(50));

      drawstring('64 -32 0'+('1 0 0'*(scrwidth*0.50)) +('0 1 0'*(scrheight*0.90)), cash, '8 8 8', '1 1 1', 1);
      MoneyImage('64 -24 0'+('1 0 0'*(scrwidth*0.50)) +('0 1 0'*(scrheight*0.90)), "money.jpg");

};

void() Energy_Draw =
{
	string cash;

	cash = ftos(getstati(51));

      drawstring('34 -32 0'+('1 0 0'*(scrwidth*0.50)) +('0 1 0'*(scrheight*0.90)), cash, '8 8 8', '1 1 1', 1);
      MoneyImage('32 -24 0'+('1 0 0'*(scrwidth*0.50)) +('0 1 0'*(scrheight*0.90)), "energy.jpg");

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

void() Armor_Draw =
{
	//drawstring('300 350 0'+('3 0 0'*IMGSIZEF), "   ARMOR   ", '8 8 8', '1 1 1', 1);
	//SlotImage('300 358 0'+('3 0 0'*IMGSIZEF), 3);
      drawstring('64 -32 0'+('1 0 0'*(scrwidth*0.80)) +('0 1 0'*(scrheight*0.90)), "  ARMOR  ", '8 8 8', '1 1 1', 1);
      SlotImage('64 -24 0'+('1 0 0'*(scrwidth*0.80)) +('0 1 0'*(scrheight*0.90)), 3);

};

void() Health_Draw =
{
	float hp, maxhp, status, perc, hpos, tpos, opos, ostatus, ones;
	string string_hpos, string_tpos, string_opos;

	hp = getstati(48);
	maxhp = getstati(49);

	status = (floor(hp) * 100) / maxhp;
	perc = status;

	if (hp >= maxhp)
		string_hpos = "one.jpg";
	else
		string_hpos = "zero.jpg";

	ostatus = status;
	status = status * 0.10;
	status = floor(status);

	if (status == 0)
		string_tpos = "zero.jpg";
	if (status == 1)
		string_tpos = "one.jpg";
	if (status == 2)
		string_tpos = "two.jpg";
	if (status == 3)
		string_tpos = "three.jpg";
	if (status == 4)
		string_tpos = "four.jpg";
	if (status == 5)
		string_tpos = "five.jpg";
	if (status == 6)
		string_tpos = "six.jpg";
	if (status == 7)
		string_tpos = "seven.jpg";
	if (status == 8)
		string_tpos = "eight.jpg";
	if (status == 9)
		string_tpos = "nine.jpg";
	if (status == 10)
		string_tpos = "zero.jpg";

	ones = (ostatus - (status*10));
	ones = floor(ones);

	if (ones == 0)
		string_opos = "zero.jpg";
	if (ones == 1)
		string_opos = "one.jpg";
	if (ones == 2)
		string_opos = "two.jpg";
	if (ones == 3)
		string_opos = "three.jpg";
	if (ones == 4)
		string_opos = "four.jpg";
	if (ones == 5)
		string_opos = "five.jpg";
	if (ones == 6)
		string_opos = "six.jpg";
	if (ones == 7)
		string_opos = "seven.jpg";
	if (ones == 8)
		string_opos = "eight.jpg";
	if (ones == 9)
		string_opos = "nine.jpg";
	if (status == 10)
		string_opos = "zero.jpg";

	if (hp <= 0)
	{
		string_hpos = "zero.jpg";
		string_tpos = "zero.jpg";
		string_opos = "zero.jpg";
	}

	if (perc > 50)
	{
      	HealthImage('0 -24 0'+('0 1 0'*(scrheight*0.90)), 0, "percent.jpg");
      	HealthImage('32 -24 0'+('0 1 0'*(scrheight*0.90)), 0, string_hpos);
      	HealthImage('64 -24 0'+('0 1 0'*(scrheight*0.90)), 0, string_tpos);
      	HealthImage('96 -24 0'+('0 1 0'*(scrheight*0.90)), 0, string_opos);
	}
	else if (perc > 25)
	{
      	HealthImage('0 -24 0'+('0 1 0'*(scrheight*0.90)), 1, "percent.jpg");
      	HealthImage('32 -24 0'+('0 1 0'*(scrheight*0.90)), 1, string_hpos);
      	HealthImage('64 -24 0'+('0 1 0'*(scrheight*0.90)), 1, string_tpos);
      	HealthImage('96 -24 0'+('0 1 0'*(scrheight*0.90)), 1, string_opos);
	}
	else
	{
      	HealthImage('0 -24 0'+('0 1 0'*(scrheight*0.90)), 2, "percent.jpg");
      	HealthImage('32 -24 0'+('0 1 0'*(scrheight*0.90)), 2, string_hpos);
      	HealthImage('64 -24 0'+('0 1 0'*(scrheight*0.90)), 2, string_tpos);
      	HealthImage('96 -24 0'+('0 1 0'*(scrheight*0.90)), 2, string_opos);
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

//	if (usehud)
//		Hud_Draw();

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
		Time_Draw();
		Health_Draw();
		Weight_Draw();
		Money_Draw();
		Ammo_Draw();
		Energy_Draw();
		Weapon_Draw();
		Armor_Draw();
		Position_Draw();
		Quick_Draw();
		Chem_Draw();
		Crosshair_Draw();
	}
};
