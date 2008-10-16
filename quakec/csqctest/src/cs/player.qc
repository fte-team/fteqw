//
// running
//
$frame axrun1 axrun2 axrun3 axrun4 axrun5 axrun6

$frame rockrun1 rockrun2 rockrun3 rockrun4 rockrun5 rockrun6

//
// standing
//
$frame stand1 stand2 stand3 stand4 stand5

$frame axstnd1 axstnd2 axstnd3 axstnd4 axstnd5 axstnd6
$frame axstnd7 axstnd8 axstnd9 axstnd10 axstnd11 axstnd12


//
// pain
//
$frame axpain1 axpain2 axpain3 axpain4 axpain5 axpain6

$frame pain1 pain2 pain3 pain4 pain5 pain6


//
// death
//

$frame axdeth1 axdeth2 axdeth3 axdeth4 axdeth5 axdeth6
$frame axdeth7 axdeth8 axdeth9

$frame deatha1 deatha2 deatha3 deatha4 deatha5 deatha6 deatha7 deatha8
$frame deatha9 deatha10 deatha11

$frame deathb1 deathb2 deathb3 deathb4 deathb5 deathb6 deathb7 deathb8
$frame deathb9

$frame deathc1 deathc2 deathc3 deathc4 deathc5 deathc6 deathc7 deathc8
$frame deathc9 deathc10 deathc11 deathc12 deathc13 deathc14 deathc15

$frame deathd1 deathd2 deathd3 deathd4 deathd5 deathd6 deathd7
$frame deathd8 deathd9

$frame deathe1 deathe2 deathe3 deathe4 deathe5 deathe6 deathe7
$frame deathe8 deathe9

//
// attacks
//
$frame nailatt1 nailatt2

$frame light1 light2

$frame rockatt1 rockatt2 rockatt3 rockatt4 rockatt5 rockatt6

$frame shotatt1 shotatt2 shotatt3 shotatt4 shotatt5 shotatt6

$frame axatt1 axatt2 axatt3 axatt4 axatt5 axatt6

$frame axattb1 axattb2 axattb3 axattb4 axattb5 axattb6

$frame axattc1 axattc2 axattc3 axattc4 axattc5 axattc6

$frame axattd1 axattd2 axattd3 axattd4 axattd5 axattd6

#define ERRORTIME 20
#define STEPTIME 10

.vector lastorg;
.vector lastvel;
.float haddied;

vector player_org, vieworg;
vector player_vel;
float player_jump_held;
float player_sequence;
float player_steptime;
float player_step;
entity player_local;	//refers to the local player
float player_localentnum;
float recframe;

vector pmove_error;
float pmove_errortime;
float pmove_step;
float pmove_steptime;

.void() removefunc;

float pmoveframe;
void() ResetPlayerPrediction = 
{
//reset the pmove to lerp from the new position
	pmove_org = player_org;
	pmove_vel = player_vel;
	pmoveframe = player_sequence+1;	//the recieved frame has the move already done (server side)
	pmove_jump_held = player_jump_held;
	pmove_steptime = player_steptime;
	pmove_step = player_step;

	if (pmoveframe < clientcommandframe-128)
		pmoveframe = clientcommandframe-128;	//avoid an infinate loop

//print("reset, porg is ", ftos(pmove_org_z), "\n");
};

void(float endframe) RunMovement =
{
	float oz;

	if (servercommandframe >= player_sequence+63)
	{
		//we're meant to be updating the player faster than this
		//hopefully its just that we're throttled...
		player_sequence = servercommandframe-63;

		return;
	}

	oz = pmove_org_z;
ResetPlayerPrediction ();
	if (getstati(STAT_HEALTH) <= 0)
	{
		pmoveframe = clientcommandframe;
		//just update the angles
		if (!getinputstate(pmoveframe-1))
		{
		}
		return;	//dead, so don't run prediction. :D
	}

	if (cvar("cg_nopred") || cvar("cl_nopred"))
	{
		pmoveframe = clientcommandframe;
		//just update the angles
		if (!getinputstate(pmoveframe-1))
		{
		}
		return;
	}
//print("start, porg is ", ftos(pmove_org_z), "\n");
	while(pmoveframe < endframe)
	{
		if (!getinputstate(pmoveframe))
		{
#ifndef WORKINDP
			break;
#endif
		}
		runstandardplayerphysics();
		pmoveframe++;
	}
//print("  ran, porg is ", ftos(pmove_org_z), "\n");

	if (pmove_org_z > oz+8 && pmove_org_z < oz+24 && pmove_vel_z == 0)
	{
		//evaluate out the remaining old step
		if (pmove_steptime - time > 0)
			pmove_step = (pmove_steptime - time)*STEPTIME*pmove_step;
		else
			pmove_step = 0;

		//work out the new step
		pmove_step += (oz-pmove_org_z);
		pmove_steptime = time + 1/STEPTIME;
	}

	//add in anything that was applied after (for low packet rate protocols)
	input_angles = view_angles;
};

void() UpdateLocalMovement =
{
	local float viewheight;

	RunMovement(clientcommandframe);

	//allow the user to move the viewheight down 6 units so it's at +16, where projectiles come from.
	viewheight = cvar("v_viewheight");
	if (viewheight < -7)
		viewheight = -7;
	if (viewheight > 7)
		viewheight = 7;

	vieworg = pmove_org;	//the default view height
	vieworg_z +=  22 + viewheight;

	//correct the view position to compensate for any errors, slowly over time, 0.1 seconds.
	if (pmove_errortime - time > 0)
		vieworg += (pmove_errortime - time)*ERRORTIME * pmove_error;

	if (!cvar("cg_nostep"))
		if (pmove_steptime - time > 0)
			vieworg_z += (pmove_steptime - time)*STEPTIME * pmove_step;

	if (chasecam)
	{
		makevectors(input_angles);
		traceline(pmove_org, vieworg - v_forward * 72+v_up*32, FALSE, self);
		vieworg = trace_endpos + v_forward*8;
	}
};

.string oldskin;
void() RemovePlayer;

void() Player_Interpolate =
{	//do some frame interpolation.
	if (self.modelnum != -1 && self.model!="")
		Anim_Draw();
	else
	{
		self.lerpfrac = 1-(time-self.lerptime)*10;

		if (chasecam || self.entnum != player_localentnum)
			self.renderflags = 0;
		else
			self.renderflags = RF_EXTERNALMODEL;

		if (self.sveffects & SVE_INVIS)
			self.forceshader = shaderforname("powerups/invisibility");
		else
			self.forceshader = 0;
		self.fatness = 0;

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
};

void() BounceProject;
.float removetime;
.float starttime;
.float oldframe;
void() bodythink =
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
void() DeadBodyPredraw =
{
	float ftime;
	ftime = time - self.starttime;

	if (self.removetime < time)
	{
		Anim_UnsetModel();
		remove(self);
		return;
	}

	BounceProject();

	Player_Interpolate();
	self.origin_z -= ftime*0.5;
};

void() JustRessed =
{
	local entity e;

	e = Anim_DupModel();
	e.frame = self.oldframe;	//and stay dead!
	e.frame2 = self.oldframe;	//and stay dead!
	setmodel(e, "progs/player.mdl");
	e.origin = self.lastorg;
	e.velocity = self.lastvel;
	e.predraw = DeadBodyPredraw;
	e.removetime = time + 30;

	e.gibbable = GIB_PLAYER;
	e.solid = SOLID_TRIGGER;

	e.classname = "deadbody";
	e.movetype = MOVETYPE_TOSS;

	e.starttime = time;
	e.nextthink = time + 0.1;
	e.think = bodythink;
	e.drawmask = self.drawmask;
};

void() PlayerUpdated =
{
	local float noerror;
	local vector o;

	noerror = cvar("cg_noerror");

	//reset the prediction to last-known-good state
	ResetPlayerPrediction();
	RunMovement(servercommandframe+1);
	player_jump_held = pmove_jump_held;
	player_step = pmove_step;
	player_steptime = pmove_steptime;

	player_org = self.origin;
	player_vel = self.velocity;
	pmove_mins = self.mins;
	pmove_maxs = self.maxs;
	player_sequence = servercommandframe;

	if (noerror)
	{
		pmove_error = '0 0 0';
		pmove_errortime = time;

		ResetPlayerPrediction();
	}
	else
	{
		RunMovement(clientcommandframe); //make sure we're up to date
		o = pmove_org;	//save off the old for the teleport check below.

		//reset it, then update to now to guage how much our previous prediction was incorrect by
		ResetPlayerPrediction();
		RunMovement(clientcommandframe);

		if (vlen(o - pmove_org) > 64)
		{//teleport
			pmove_error = '0 0 0';
			pmove_errortime = time;
		}
		else
		{	//figure out the error ammount, and lerp back to it, without forgetting about any current inaccuracies.
			pmove_error = (pmove_errortime - time)*ERRORTIME * pmove_error + (o - pmove_org);
			if (vlen(pmove_error) < 1)
				pmove_error = '0 0 0';
			pmove_errortime = time + 1/ERRORTIME;
		}
	}
};

void() RemovePlayer =
{
	if (self.haddied)
	{
		JustRessed();
		self.haddied = false;
	}

	strunzone(self.oldskin);
	self.oldskin = "";
	Anim_UnsetModel();

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
	}
	else
	{
		self.haddied = true;
	}

	self.lastorg = self.origin;
	self.lastvel = self.velocity;

	if (isnew)
	{
		if (self.entnum == player_localentnum)
		{
			player_local = self;
		}

		self.model = "progs/player.mdl";
		self.oldskin = strzone("");
		Anim_UnsetModel();
		self.drawmask = MASK_NORMAL;

		self.removefunc = RemovePlayer;
	}

	newskin = cvar_string("cg_forceskin");
	if (newskin == "")
		newskin = getplayerkeyvalue(self.entnum-1, "skin");
	if (newskin != self.oldskin)
	{
		strunzone(self.oldskin);
		self.oldskin = strzone(newskin);
		if (!Anim_SetModel(self.oldskin))
		{			Anim_UnsetModel();
		}
/*
		switch(Anim_GetGender())
		{
		case GENDER_FEMALE:
			localcmd("setinfo sex f\n");
			break;
		case GENDER_MALE:
			localcmd("setinfo sex m\n");
			break;
		case GENDER_NEUTER:
			localcmd("setinfo sex n\n");
			break;
		}
*/
	}

	setsize(self, VEC_HULL_MIN, VEC_HULL_MAX);

	self.oldframe = self.frame;

	if (player_local != self)
		return;

	PlayerUpdated();
};

//this is sent after the server has run our movement command.
void(float isnew) ParsePlayer =
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
	self.velocity_x = readshort()/64;
	self.velocity_y = readshort()/64;
	self.velocity_z = readshort()/64;
	self.colormap = self.entnum;

	self.sveffects = readbyte();

	RefreshPlayer(isnew);
};

