/*
UpdateLocalMovement: runs the local prediction (called from player.qc - local players are drawn for mirrors etc)
	out: vieworg (view origin, with chasecam/viewheight added)
	out: pmove_org (raw player ent origin)

PlayerUpdated: updates internal state, called from player.qc any time we got an update from the server.
	in: self (the player ent origin/velocity/mins/maxs)

ResetPlayerPrediction: call if you broke the special pmove globals
*/

#define ERRORTIME 20
#define STEPTIME 8

vector player_org;
vector player_vel;
float player_jump_held;
float player_sequence;
float player_steptime;
float player_step;

vector pmove_error;
float pmove_errortime;

float pmove_step;
float pmove_steptime;
float pmove_step_oldz;

void() pmovetouchtriggers;

float pmoveframe;
nonstatic void() Pred_ResetPlayerPrediction = 
{
//reset the pmove to lerp from the new position
	pmove_org = player_org;
	pmove_vel = player_vel;
	pmoveframe = player_sequence+1;	//+1 because the recieved frame has the move already done (server side)
	pmove_jump_held = player_jump_held;

	if (pmoveframe < clientcommandframe-128)
		pmoveframe = clientcommandframe-128;	//avoid an infinate loop
};

//called from map.qc when a teleport trigger was predicted to be touched.
void(vector newteleorg, vector newtelevel, vector newteleang) Pred_Predict_Teleport =
{
	float teleframe;
	teleframe = pmoveframe;

	//if this is the first client frame that shows us moving into the teleporter
	if (teleframe == clientcommandframe-1)
	{
		//update the client's view angles
		setviewprop(33, newteleang);
		view_angles = newteleang;
	}
	pmove_org = newteleorg;
	pmove_vel = newtelevel;
	input_angles = newteleang;
};

void(float endframe) Pred_RunMovement;

nonstatic void() Pred_PlayerUpdated =
{
	local float noerror;
	local vector o;

	noerror = cvar("cg_noerror");

	//reset the prediction to last-known-good state
	Pred_ResetPlayerPrediction();
	Pred_RunMovement(servercommandframe+1);
	player_jump_held = pmove_jump_held;
	player_step = pmove_step;
	player_steptime = pmove_steptime;

	//pull out the new values
	player_org = self.origin;
	player_vel = self.velocity;
	pmove_mins = self.mins;
	pmove_maxs = self.maxs;
	player_sequence = servercommandframe;

	if (noerror)
	{
		pmove_error = '0 0 0';
		pmove_errortime = time;

		Pred_ResetPlayerPrediction();
	}
	else
	{
		Pred_RunMovement(clientcommandframe); //make sure we're up to date
		o = pmove_org;	//save off the old for the teleport check below.

		//reset it, then update to now to guage how much our previous prediction was incorrect by
		Pred_ResetPlayerPrediction();
		Pred_RunMovement(clientcommandframe);

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

void(float endframe) Pred_RunMovement =
{
	if (servercommandframe >= player_sequence+63)
	{
		//we're meant to be updating the player faster than this
		//hopefully its just that we're throttled...

		//you can comment out this block and the player will continue to be predicted locally. But its best to freeze them

		player_sequence = servercommandframe-63;

		return;
	}

#ifdef WORKINDP
	//DP handily corrupts this for us, so waste some more cpu cycles to compensate
	if (isdp)
		Pred_ResetPlayerPrediction();
#endif

	if (getstati(STAT_HEALTH) <= 0)
	{
		pmoveframe = clientcommandframe;
		//just update the angles
		if (!getinputstate(pmoveframe-1))
		{
		}
		return;	//dead, so don't run prediction. :D
	}

	if (cvar("cg_nopred"))
	{
		pmoveframe = clientcommandframe;
		//just update the angles
		if (!getinputstate(pmoveframe-1))
		{
		}
		input_angles = view_angles;
		return;
	}

	while(pmoveframe < endframe)
	{
		if (!getinputstate(pmoveframe))
		{
			break;
		}
		runstandardplayerphysics();

		pmovetouchtriggers();

		pmoveframe++;
	}

	//add in anything that was applied after (for low packet rate protocols)
	input_angles = view_angles;
};

nonstatic void() Pred_UpdateLocalMovement =
{
	local float viewheight;

	Pred_RunMovement(clientcommandframe);

	if (pmove_org_z > pmove_step_oldz+8 && pmove_org_z < pmove_step_oldz+24 && pmove_vel_z == 0)
	{
		//evaluate out the remaining old step
		if (pmove_steptime - time > 0)
			pmove_step = (pmove_steptime - time)*STEPTIME*pmove_step;
		else
			pmove_step = 0;

		//work out the new step
		pmove_step += (pmove_step_oldz-pmove_org_z);
		pmove_steptime = time + 1/STEPTIME;
	}
	pmove_step_oldz = pmove_org_z;

	//allow the user to move the viewheight down 6 units so it's at +16, where projectiles come from.
	viewheight = cvar("v_viewheight");
	if (viewheight < -7)
		viewheight = -7;
	else if (viewheight > 7)
		viewheight = 7;

	vieworg = pmove_org;	//the default view height
	vieworg_z += getstati(STAT_VIEWHEIGHT) + viewheight;

	//correct the view position to compensate for any errors, slowly over time, 0.1 seconds.
	if (pmove_errortime - time > 0)
		vieworg += (pmove_errortime - time)*ERRORTIME * pmove_error;

	if (!cvar("cg_nostep"))
		if (pmove_steptime - time > 0)
			vieworg_z += (pmove_steptime - time)*STEPTIME * pmove_step;

	if (chasecam)
	{
		view_angles_y += 180;
		makevectors(view_angles);
		traceline(pmove_org, vieworg - v_forward * 72+v_up*32, TRUE, self);
		vieworg = trace_endpos + v_forward*8;
	}
};
