/*
this file handles the local player, and marshalling between the different sorts of player models.

RefreshPlayer: Called from the engine each time a player (ent with player.mdl) is about to be drawn
ParsePlayer: Called from CSQC_Ent_Parse for csqc protocol ents
*/

#include "playerframes.inc"

static void() RemovePlayer;

enum
{
	MF_BAD,
#ifdef MD3PMODELS
	MF_QUAKE3,
#endif
#ifdef HLPMODELS
	MF_HLPM,
#endif
	MF_QUAKE
};

#ifdef NOEXTENSIONS	//FRIK_FILE is an extension still, unfortunatly. but hey, the model code depends upon it more
#define strunzone(s)	//so really we can get away with this for this module
#define strzone(s) ""
#endif

static void() Player_Interpolate =
{	//do some frame interpolation.
	if (self.entnum == player_localentnum)
	{
		Pred_UpdateLocalMovement(self);
	}

	switch(self.modelstyle)
	{
#ifdef MD3PMODELS
	case MF_QUAKE3:
		Anim_Draw();
		break;
#endif
#ifdef HLPMODELS
	case MF_HLPM:
		HLPM_Draw();
		break;
#endif
	default:
		if (self.lerptime)
			self.lerpfrac = 1-(time-self.lerptime)*10;

		if (chasecam || self.entnum != player_localentnum)
			self.renderflags = 0;
		else
			self.renderflags = RF_EXTERNALMODEL;

#ifdef POWERUP_SHELLS
		if (!isdp)
		{
			if (self.sveffects & SVE_INVIS)
				self.forceshader = shaderforname("powerups/invisibility");
			else
				self.forceshader = 0;

			self.fatness = 0;

			if (!(self.sveffects & SVE_INVIS))
			{
				if (self.sveffects & SVE_QUAD)
				{
					addentity(self);

					self.fatness = -2;
					self.forceshader = shaderforname("powerups/quad");
				}
				if (self.sveffects & SVE_GOD)
				{
					addentity(self);

					self.fatness = -2.8;
					self.forceshader = shaderforname("powerups/regen");
				}
			}
			break;
		}
#endif
		if (self.sveffects & SVE_INVIS)
			setmodel(self, "progs/eyes.mdl");
		else
			setmodel(self, "progs/player.mdl");
		break;
	}
};

static void(float g) Player_SetLocalInfoGender =
{
	if (player_local == self)
	{
		//if it was forced, don't lie to everyone else.
		if (cvar_string("cg_forceskin") != "")
			return;

		switch(g)
		{
		case GENDER_FEMALE:
			localcmd("setinfo s f\n");
			break;
		case GENDER_MALE:
			localcmd("setinfo s \"\"\n");
			break;
		default:
		case GENDER_NEUTER:
			localcmd("setinfo s n\n");
			break;
		}
	}
}

static void() Player_SetQ1Model =
{
	self.modelstyle = MF_QUAKE;

	Player_SetLocalInfoGender(GENDER_MALE);
}

#ifdef MD3PMODELS
static void(string skinname) Player_SetQ3Model =
{
	if (!Anim_SetModel(skinname))
	{
		Anim_UnsetModel();
		return;
	}
	Player_SetLocalInfoGender(Anim_GetGender());
	self.modelstyle = MF_QUAKE3;
};
#endif

#ifdef HLPMODELS
static void(string skinname) Player_SetHLModel =
{
	if (!HLPM_SetModel(skinname))
	{
		HLPM_UnsetModel();
		return;
	}
	Player_SetLocalInfoGender(GENDER_MALE);
	self.modelstyle = MF_HLPM;
};
#endif

static void() Player_UnsetModel =
{
	switch(self.modelstyle)
	{
#ifdef MD3PMODELS
	case MF_QUAKE3:
		Anim_UnsetModel();
		break;
#endif
#ifdef HLPMODELS
	case MF_HLPM:
		HLPM_UnsetModel();
		break;
#endif
	default:
		//nothing to do
		break;
	}
	self.modelstyle = MF_BAD;
	setmodel(self, "");
};

static entity() Player_DupModel =
{
	local entity e;

	switch(self.modelstyle)
	{
#ifdef MD3PMODELS
	case MF_QUAKE3:
		e = Anim_DupModel();
		break;
#endif
#ifdef HLPMODELS
	case MF_HLPM:
		e = HLPM_DupModel();
		break;
#endif
	default:
		e = spawn();
		break;
	}
	e.modelstyle = self.modelstyle;
	return e;
};

.float starttime;
.float oldframe;
static void() bodythink =
{
	local float final;
	if (self.frame >= $axdeth1 && self.frame <= $axdeth9)
		final = $axdeth9;
	else if (self.frame >= $deatha1 && self.frame <= $deatha11)
		final = $deatha11;
	else if (self.frame >= $deathb1 && self.frame <= $deathb9)
		final = $deathb9;
	else if (self.frame >= $deathc1 && self.frame <= $deathc15)
		final = $deathc15;
	else if (self.frame >= $deathd1 && self.frame <= $deathd9)
		final = $deathd9;
	else if (self.frame >= $deathe1 && self.frame <= $deathe9)
		final = $deathe9;

	self.frame = self.frame+1;

	if (self.frame > final)
		self.frame = final;

	self.nextthink = time + 0.1;
};
static void() DeadBodyPredraw =
{
	float ftime;
	ftime = time - self.starttime;

	if (self.removetime < time)
	{
		Player_UnsetModel();
		remove(self);
		return;
	}

	Movetype_Bounce();

	Player_Interpolate();
	self.origin_z -= ftime*0.5;
};

static void() JustRessed =
{
	local entity e;

	e = Player_DupModel();
	e.frame = self.oldframe;	//and stay dead!
	e.frame2 = self.oldframe;	//and stay dead!
	setmodel(e, "progs/player.mdl");
	e.origin = self.lastorg;
	e.velocity = self.lastvel;
	e.predraw = DeadBodyPredraw;
	e.removetime = time + 30;
	e.lerptime = time;

	e.gibbable = GIB_PLAYER;
	e.solid = SOLID_TRIGGER;

	e.classname = "deadbody";
	e.movetype = MOVETYPE_TOSS;

	e.starttime = time;
	e.nextthink = time + 0.1;
	e.think = bodythink;
	e.drawmask = self.drawmask;
};

static void() RemovePlayer =
{
	if (player_local == self)
		player_local = world;
		
	if (self.haddied)
	{
		JustRessed();
		self.haddied = false;
	}

	Player_UnsetModel();

	strunzone(self.oldskin);
	self.oldskin = "";

	self.predraw = __NULL__;
};

void(float isnew) RefreshPlayer =
{
	local string newskin;

	self.predraw = Player_Interpolate;

	if (isnew)
		self.haddied = false;

	//if its not new and they're not dead, but they were before..
	if (self.frame < $axdeth1 || self.frame > $deathe9)
	{
		if (self.haddied)
		{
			JustRessed();
			self.haddied = false;
		}
		self.solid = SOLID_BBOX;
	}
	else
	{
		self.haddied = true;
		self.solid = SOLID_NOT;
	}

	self.lastorg = self.origin;
	self.lastvel = self.velocity;

	if (isnew)
	{
		if (self.entnum == player_localentnum)
		{
			player_local = self;
		}

		self.modelstyle = MF_BAD;

		self.oldskin = strzone("");
		self.drawmask = MASK_NORMAL;

		self.removefunc = RemovePlayer;
	}

	if (self.sveffects & SVE_INVIS)
	{
		if (self.modelstyle != MF_QUAKE)
		{
			Player_UnsetModel();
			Player_SetQ1Model();	//every model uses eyes
		}
	}
	else
	{
		newskin = cvar_string("cg_forceskin");
		if (newskin == "")
			newskin = getplayerkeyvalue(self.entnum-1, "model");
		if (newskin == "")
			newskin = getplayerkeyvalue(self.entnum-1, "skin");
		if (newskin != self.oldskin)
		{
			Player_UnsetModel();

			strunzone(self.oldskin);
			self.oldskin = strzone(newskin);
		}

		if (self.modelstyle == MF_BAD)
		{
			newskin = self.oldskin;

#ifdef MD3PMODELS
			if (self.modelstyle == MF_BAD && newskin != "")
				Player_SetHLModel(newskin);
#endif
#ifdef HLPMODELS
			if (self.modelstyle == MF_BAD && newskin != "")
				Player_SetQ3Model(newskin);
#endif
			if (self.modelstyle == MF_BAD)
				Player_SetQ1Model();
		}
	}

	setsize(self, VEC_HULL_MIN, VEC_HULL_MAX);

	self.oldframe = self.frame;

	if (player_local != self)
	{
		return;
	}

	Pred_PlayerUpdated(self);
};

//this is sent after the server has run our movement command.
nonstatic void(float isnew) ParsePlayer =
{
	local float f;

	f = readbyte();

	if (f != self.frame || isnew)
	{
		self.frame2 = self.frame;
		self.lerptime = time;
		self.frame = f;
	}

	self.angles_x = readbyte()*(360/256);
	self.angles_y = readbyte()*(360/256);
	self.origin_x = readcoord();
	self.origin_y = readcoord();
	self.origin_z = readcoord();
	self.velocity_x = readshort();
	self.velocity_y = readshort();
	self.velocity_z = readshort();
	self.colormap = self.entnum;

	self.sveffects = readbyte();

	RefreshPlayer(isnew);
};

#ifdef NOEXTENSIONS
#undef strunzone
#undef strzone
#endif
