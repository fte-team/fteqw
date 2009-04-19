void(string model, void(float isnew) updatefunc, float flags) deltalisten = #371;
void(float flags, float simtime) readserverentitystate = #369;
#define RSES_NOLERP 1	//only really for use on predicted player entities
#define RSES_AUTOROTATE 0
#define RSES_NOROTATE 2
#define RSES_AUTOTRAILS 0
#define RSES_NOTRAILS 4
#define RSES_AUTOLIGHTS 0
#define RSES_NOLIGHTS 8

#ifdef WORKINDP
	#define show_scoreboard sb_showscores
#endif

float show_scoreboard;

//check engine extensions, and give suitable warning
void() checkengineversion =
{
	if (checkextension("FTE_STRINGS"))
	{
		isdp = substring(" ", 0, -1) == "";
		if (isdp)
			print("broken FTE_STRINGS support\n");
	}
	else
		print("no FTE_STRINGS support, this mod might crash... sorry\n");
};


//check compiler, and give suitable warnings (unfortunatly at run time)
float(float f) testfunc =
{
	return f;
}
float(float f1, float f2) testfunc2 =
{
	return f1+f2;
}
float testarray[2];
void() checkcompilerversion =
{
	local entity testent;

	testent = spawn();
	testent.entnum = -5;
	testent.owner = testent;
	testent.owner.entnum += 5;
	if (testent.entnum)
		print("compiler failed on e.e.f += \n");
	remove(testent);

	testarray[0] = 5;
	testarray[1] = 6;

	if (testfunc(5)+testfunc(6) != 11)
		print("compiler failed on f()+f()\n");
	if (testfunc2(5, testfunc(6)) != 11)
		print("compiler failed on f2(0,f1(1))\n");
	if (testfunc2(testfunc(5),testfunc(6)) != 11)
		print("compiler failed on f2(f(),f())\n");
	if (testarray[0]+testarray[1] != 11)
		print("compiler failed on a[0]+a[1]\n");
};



void(float isnew) RefreshPlayer;


void(float isnew) RefreshOther =
{
	self.drawmask = MASK_ENGINE;
	setmodelindex(self, self.modelindex);

	if (self.entnum <= maxclients)
		RefreshPlayer(isnew);
	else if (self.colormap)
		self.modelindex = 0;	//don't show dead bodies, we do dying ourselves.
};



float(string centertext) centerprinted =
{
//warning!
//centertext is a temporary string!
//warning!

	return false;	//we don't want to handle it, let the engine do it.
};

.float classnum;

void(float isnew) CSQC_Ent_Update =
{
	local float classtype;
	local var void(float isnew) fnc;

	classtype = readbyte();

//remove if the class changed.
	if (self.classnum != classtype)
	{
		if (self.removefunc)
			self.removefunc();
		isnew = true;
		self.classnum = classtype;
	}

	fnc = ParseEntityClass[classtype];
	if (!fnc)
		error("Unrecognised entity class: Your csqc needs updating");
	fnc(isnew);

//	if (isnew)
//	print("csqc ", self.model, " updated at ", vtos(self.origin), "\n");
};

void() regcommands =
{
	registercommand("tetris");
	registercommand("skinchooser");
	registercommand("randomskin");
	registercommand("test");
	registercommand("+showscores");
	registercommand("-showscores");
	registercommand("+showteamscores");
	registercommand("-showteamscores");

	registercommand("osgk");
	registercommand("osgk_command");
	registercommand("osgk_resize");
	registercommand("osgk_mousemove");
	registercommand("osgk_keypress");
};

float(string str) CSQC_ConsoleCommand =
{
	local float args;
	args = tokenize(str);
	switch(argv(0))
	{
	case "tetris":
		Menu_Tetris();
		break;
	case "skinchooser":
		Menu_SkinChooser();
		break;
	case "randomskin":
		SelectRandomSkin();
		break;
	case "+showscores":
		show_scoreboard = 1;
		return false;
	case "+showteamscores":
		show_scoreboard = 2;
		return false;
	case "-showscores":
	case "-showteamscores":
		show_scoreboard = 0;
		return false;
	case "osgk":
		Menu_OSGK(argv(1));
		break;
	case "osgk_command":
		gecko_navigate(argv(1), argv(2));
		break;
	case "osgk_keypress":
		gecko_keyevent(argv(1), stof(argv(2)), 2);
		break;
	case "osgk_resize":
		gecko_resize(argv(1), stof(argv(2)), stof(argv(3)));
		break;
	case "osgk_mousemove":
		gecko_mousemove(argv(1), stof(argv(2)), stof(argv(3)));
		break;
	default:
		return false;
	}
	return true;
};

void() CSQC_Ent_Remove =	//the ent disappeared on the server.
{
	if (self.removefunc)
		self.removefunc();
	remove(self);
};

entity viewentity;
//note that a better way of animating weapon firing would be as a response to an event, or as a clientside predicted thing.
void(entity ve) DoThatViewModelThing =
{
	float newframe, newmodel;

	newframe = getstati(STAT_WEAPONFRAME);
	newmodel = getstati(STAT_WEAPONMODEL);

	if (newmodel != ve.modelindex)
	{	//changed entirly.
		ve.modelindex = newmodel;
		ve.frame2 = ve.frame = newframe;
		ve.lerptime = time;
	}
	else if (newframe != ve.frame)
	{
		ve.frame2 = ve.frame;
		ve.frame = newframe;
		ve.lerptime = time;
	}
	ve.lerpfrac = 1-(time-ve.lerptime)*10;

	ve.origin = '0 0 0';
	ve.angles = '0 0 0';
	ve.angles_x *= -1;
};

void() drawloadingscreen =
{
//work in progress here
//	drawfill('0 0', '320 240', '1 1 1', 1);
//	drawstring('0 0', "loading screen", '1 1', '1 1 1', 1);
};

void() CSQC_Init =
{
	checkengineversion();
	checkcompilerversion();

	//set some cvars to their defaults, and create them at the same time
	if (cvar_string("cg_giblevel") == "")	localcmd("set cg_giblevel 1\n");
	if (cvar_string("cg_thirdPerson") == "")	localcmd("set cg_thirdPerson 0\n");
	if (cvar_string("cg_forceskin") == "")	localcmd("set cg_forceskin \"\"\n");
	if (cvar_string("cg_hudtype") == "")	localcmd("set cg_hudtype 1\n");
	if (cvar_string("cg_nopred") == "")		localcmd("set cg_nopred 0\n");
	if (cvar_string("v_viewheight") == "")	localcmd("set v_viewheight 0\n");//normally an engine cvar in quakeworld.
	if (cvar_string("cg_nostep") == "")		localcmd("set cg_nostep 0\n");
	if (cvar_string("cg_noerror") == "")	localcmd("set cg_noerror 0\n");
	if (cvar_string("cg_noselfsounds") == "")	localcmd("set cg_noselfsounds 0\n");
	if (cvar_string("cg_noselfjumpsound") == "")	localcmd("set cg_noselfjumpsound 0\n");

	viewentity = spawn();
	viewentity.renderflags = RF_VIEWMODEL|RF_DEPTHHACK;

	drawloadingscreen ();

	regcommands();

#ifndef FTEDEPENDANT
	if (checkextension("EXT_CSQC_1"))
#endif
	{
//		deltalisten("*", RefreshOther, 0);	//catch-all
		deltalisten("progs/player.mdl", RefreshPlayer, RSES_NOLERP|RSES_NOROTATE);	//animate players/player-corpses
	}
};

void(entity ent) CSQC_DrawViewModel =
{
	if (player_local.sveffects & SVE_INVIS)
		ent.forceshader = shaderforname("powerups/invisibility");
	else
		ent.forceshader = 0;

	ent.fatness = 0;
	DoThatViewModelThing(ent);

#ifdef WORKINDP
	//DP doesn't support RF_VIEWMODEL
	//so hack around that
	ent.renderflags = RF_DEPTHHACK;	//nope, not a view model, that just breaks things
	ent.origin = vieworg;
	ent.angles = input_angles;
	ent.angles_x = -ent.angles_x;
#endif

	addentity(ent);
	if (player_local.sveffects & SVE_QUAD)
	{
		ent.fatness = -2;
		ent.forceshader = shaderforname("powerups/quad");
		addentity(ent);
	}
	if (player_local.sveffects & SVE_GOD)
	{
		ent.fatness = -2.8;
		ent.forceshader = shaderforname("powerups/regen");
		addentity(ent);
	}
}

//a bit of fun
void() CSQC_Input_Frame =
{
/*	if (input_buttons == 1)
		input_buttons = 2;
	else if (input_buttons == 2)
		input_buttons = 1;*/
};

void() CSQC_Delta_Remove =
{
	if (self == player_local)
		player_local = world;

//	print("delt ", ftos(self.modelindex), "(", self.model, ") removed\n");
	if (self.removefunc)
		self.removefunc();
	remove(self);
};

void(float width, float height, float do2d) CSQC_UpdateView =
{
	float hudtype = cvar("cg_hudtype");

	chasecam = cvar("cg_thirdPerson");
	if (getstati(STAT_HEALTH) <= 0)
		chasecam = 1;

	clearscene();

	if (hudtype != 1)
	{
		setviewprop(VF_DRAWENGINESBAR, 0);
	}
	else
	{
		setviewprop(VF_DRAWENGINESBAR, 1);
	}
	setviewprop(VF_DRAWCROSSHAIR, 1);

	//force fullscreen views (ignore viewsize).
	setviewprop(VF_MIN, '0 0 0');
	setviewprop(VF_SIZE_X, width);
	setviewprop(VF_SIZE_Y, height);

	addentities(MASK_NORMAL|MASK_ENGINE);

	if (player_local)
	{
		setviewprop(VF_ORIGIN, vieworg);
		setviewprop(VF_ANGLES, view_angles);

		makevectors(view_angles);
		SetListener(vieworg, v_forward, v_right, v_up);

		if (!chasecam)
		{
			CSQC_DrawViewModel(viewentity);
		}
	}
	else
	{
		//engine didn't tell us about our player entity. that's not right...
		addentities(MASK_ENGINEVIEWMODEL);
	}

	renderscene();

	if (do2d)
	{
		if (hudtype)
			Hud_Draw(hudtype, show_scoreboard, width, height);

		Menu_Think();
	}
};

