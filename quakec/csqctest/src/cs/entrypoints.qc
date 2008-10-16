
void(float isnew) RefreshPlayer;

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
	registercommand("cin_command");
	registercommand("cin_resize");
	registercommand("cin_mousemove");
	registercommand("cin_keypress");
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
	case "test":
		test();
		break;
	case "+showscores":
	case "-showscores":
	case "+showteamscores":
	case "-showteamscores":
		break;
	case "osgk":
		Menu_OSGK(argv(1));
		break;
	case "cin_command":
		gecko_navigate(argv(1), argv(2));
		break;
	case "cin_keypress":
		gecko_keyevent(argv(1), stof(argv(2)), 2);
		break;
	case "cin_resize":
		gecko_resize(argv(1), stof(argv(2)), stof(argv(3)));
		break;
	case "cin_mousemove":
		gecko_mousemove(argv(1), stof(argv(2)), stof(argv(3)));
		break;
	default:
		return false;
	}
	return true;
};

void() CSQC_Ent_Remove =	//the ent disappeared on the server.
{
	if (self == player_local)
		print("WARNING: Player disappeared!\n");

//	print("csqc ", ftos(self.modelindex), "(", self.model, ") removed\n");

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
	//set some cvars to their defaults, and create them at the same time
	if (cvar_string("cg_giblevel") == "")	localcmd("set cg_giblevel 1\n");
	if (cvar_string("cg_thirdPerson") == "")	localcmd("set cg_thirdPerson 0\n");
	if (cvar_string("cg_forceskin") == "")	localcmd("set cg_forceskin \"\"\n");
	if (cvar_string("cg_hudtype") == "")	localcmd("set cg_hudtype 0\n");
	if (cvar_string("cg_nopred") == "")		localcmd("set cg_nopred 0\n");
	if (cvar_string("v_viewheight") == "")	localcmd("set v_viewheight 0\n");//normally an engine cvar in quakeworld.
	if (cvar_string("cg_nostep") == "")		localcmd("set cg_nostep 0\n");
	if (cvar_string("cg_noerror") == "")	localcmd("set cg_noerror 0\n");
	if (cvar_string("cg_noselfsounds") == "")	localcmd("set cg_noselfsounds 0\n");
	if (cvar_string("cg_noselfjumpsound") == "")	localcmd("set cg_noselfjumpsound 0\n");

	viewentity = spawn();
	viewentity.renderflags = RF_VIEWMODEL;

	drawloadingscreen ();

	ResetPlayerPrediction();
	regcommands();
};

void(entity ent) CSQC_DrawViewModel =
{
	if (player_local.sveffects & SVE_INVIS)
		ent.forceshader = shaderforname("powerups/invisibility");
	else
		ent.forceshader = 0;

	ent.fatness = 0;
	DoThatViewModelThing(ent);
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

void(float isnew) CSQC_Delta_Update =
{
	self.drawmask = MASK_NORMAL;
	setmodelindex(self, self.modelindex);
//	if (isnew)
//	{
//		print("delt ", self.model, " updated at ", vtos(self.origin), "\n");
//	}

	if (self.entnum <= maxclients)
		RefreshPlayer(isnew);
	else if (self.colormap)
		self.modelindex = 0;	//don't show dead bodies, we do dying ourselves.
};

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

void(float flags, float simtime) readserverentitystate = #369;
#define RSES_AUTOLERP 0
#define RSES_NOLERP 1
#define RSES_AUTOROTATE 0
#define RSES_NOROTATE 2
#define RSES_AUTOTRAILS 0
#define RSES_NOTRAILS 4
#define RSES_AUTOLIGHTS 0
#define RSES_NOLIGHTS 8

void(float width, float height, float do2d) CSQC_UpdateView =
{
	float lagged;
	float i;
	float hudtype = cvar("cg_hudtype");

	chasecam = cvar("cg_thirdPerson");
	if (getstati(STAT_HEALTH) <= 0)
		chasecam = 1;

#ifndef WORKINDP
	readserverentitystate(RSES_AUTOLERP|RSES_AUTOROTATE|RSES_AUTOTRAILS|RSES_AUTOLIGHTS, 0);
#endif

	clearscene();

	if (hudtype)
	{
		setviewprop(VF_DRAWENGINESBAR, 0);
	}
	else
	{
		setviewprop(VF_DRAWENGINESBAR, 1);
	}
	setviewprop(VF_DRAWCROSSHAIR, 1);

	if (player_local)
	{
		UpdateLocalMovement();

		setviewprop(VF_ORIGIN, vieworg);
		setviewprop(VF_ANGLES, view_angles);

		makevectors(view_angles);
		SetListener(vieworg, v_forward, v_right, v_up);

		if (!chasecam)
			CSQC_DrawViewModel(viewentity);
	}

	//force fullscreen views.
	setviewprop(VF_MIN, '0 0 0');	
	setviewprop(VF_SIZE_X, width);	
	setviewprop(VF_SIZE_Y, height);

#ifdef WORKINDP
	addentities(MASK_NORMAL|MASK_ENGINE);
#else
	addentities(MASK_NORMAL);
#endif

	renderscene();

	if (do2d)
	{
		if (hudtype)
			Hud_Draw(hudtype, width, height);

		Menu_Think();
	}
};

