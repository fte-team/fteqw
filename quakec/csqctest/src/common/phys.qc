//note that this file uses various engine pastes and is thus subject to the terms of the GNU GPL.
//use with caution

float lastclientthink, sv_maxspeed, sv_friction, sv_accelerate, sv_stopspeed;
float sv_edgefriction, cl_rollangle, cl_divspeed;

vector phys_origin;	//in/out
float phys_movetype;	//in
float phys_health;	//in
float phys_fixangle;	//in
float phys_onground;	//in/out
float phys_waterjump;	//in/out
vector phys_angles;	//out
vector phys_v_angles;	//in
vector phys_velocity;	//in/out
vector phys_movement;	//in
vector phys_punchangle;	//in/out
float phys_gravity;	//in (should be sv_gravity * self.gravity)

#define bound(min,val,max) if (val < min) val = min; else if (val > max) val = max

#define sv_stepheight 22

var void() thinkfunc;

float(entity ent, float ftime) SV_FlyMove =
{
}

void() WalkMove =
{
	float clip, oldonground, originalmove_clip, originalmove_flags, originalmove_groundentity;
	vector upmove, downmove, start_origin, start_velocity, stepnormal, originalmove_origin, originalmove_velocity;

//	SV_CheckVelocity(ent);

	// do a regular slide move unless it looks like you ran into a step
	oldonground = phys_onground;
	phys_onground = false;

	start_origin = phys_origin;
	start_velocity = phys_velocity;

	clip = SV_FlyMove (ent, sv.frametime, NULL);

	SV_SetOnGround (ent);
	SV_CheckVelocity(ent);

	VectorCopy(phys_origin, originalmove_origin);
	VectorCopy(phys_velocity, originalmove_velocity);
	originalmove_clip = clip;
	originalmove_flags = (int)ent->fields.server->flags;
	originalmove_groundentity = phys_groundentity;

	if (phys_waterjump)
		return;

//	if (sv_nostep.integer)
//		return;

	// if move didn't block on a step, return
	if (clip & 2)
	{
		// if move was not trying to move into the step, return
		if (fabs(start_velocity[0]) < 0.03125 && fabs(start_velocity[1]) < 0.03125)
			return;

		if (phys_movetype != MOVETYPE_FLY)
		{
			// return if gibbed by a trigger
			if (phys_movetype != MOVETYPE_WALK)
				return;

			// only step up while jumping if that is enabled
//			if (!(sv_jumpstep.integer && sv_gameplayfix_stepwhilejumping.integer))
				if (!oldonground && phys_waterlevel == 0)
					return;
		}

		// try moving up and forward to go up a step
		// back to start pos
		VectorCopy (start_origin, phys_origin);
		VectorCopy (start_velocity, phys_velocity);

		// move up
		VectorClear (upmove);
		upmove[2] = sv_stepheight;
		// FIXME: don't link?
		SV_PushEntity(ent, upmove);

		// move forward
		phys_velocity[2] = 0;
		clip = SV_FlyMove (ent, sv.frametime, stepnormal);
		phys_velocity[2] += start_velocity[2];

//		SV_CheckVelocity(ent);

		// check for stuckness, possibly due to the limited precision of floats
		// in the clipping hulls
		if (clip
		 && fabs(originalmove_origin[1] - phys_origin[1]) < 0.03125
		 && fabs(originalmove_origin[0] - phys_origin[0]) < 0.03125)
		{
			//Con_Printf("wall\n");
			// stepping up didn't make any progress, revert to original move
			VectorCopy(originalmove_origin, phys_origin);
			VectorCopy(originalmove_velocity, phys_velocity);
			//clip = originalmove_clip;
			phys_flags = originalmove_flags;
			phys_groundentity = originalmove_groundentity;
			// now try to unstick if needed
			//clip = SV_TryUnstick (ent, oldvel);
			return;
		}

		//Con_Printf("step - ");

		// extra friction based on view angle
		if (clip & 2 && sv_wallfriction.integer)
			SV_WallFriction (ent, stepnormal);
	}
	// skip out if stepdown is enabled, moving downward, not in water, and the move started onground and ended offground
//	else if (!(sv_gameplayfix_stepdown.integer && phys_waterlevel < 2 && start_velocity[2] < (1.0 / 32.0) && oldonground && !((int)ent->fields.server->flags & FL_ONGROUND)))
//		return;
/*
	// move down
	VectorClear (downmove);
	downmove[2] = -sv_stepheight + start_velocity[2]*sv.frametime;
	// FIXME: don't link?
	downtrace = SV_PushEntity (ent, downmove);

	if (downtrace.fraction < 1 && downtrace.plane.normal[2] > 0.7)
	{
		// LordHavoc: disabled this check so you can walk on monsters/players
		//if (phys_solid == SOLID_BSP)
		{
			//Con_Printf("onground\n");
			phys_onground = true;
			phys_groundentity = PRVM_EDICT_TO_PROG(downtrace.ent);
		}
	}
	else
	{
		//Con_Printf("slope\n");
		// if the push down didn't end up on good ground, use the move without
		// the step up.  This happens near wall / slope combinations, and can
		// cause the player to hop up higher on a slope too steep to climb
		VectorCopy(originalmove_origin, ent->fields.server->origin);
		VectorCopy(originalmove_velocity, ent->fields.server->velocity);
		//clip = originalmove_clip;
		ent->fields.server->flags = originalmove_flags;
		ent->fields.server->groundentity = originalmove_groundentity;
	}

	SV_SetOnGround (ent);
	SV_CheckVelocity(ent);
*/
};

// LordHavoc:
// Highly optimized port of SV_ClientThink from engine code to QuakeC.

// note that the engine will call this function if it finds it,
// so modify for your own mods and enjoy...

void() RunPlayerPhysics =
{
	local vector wishvel, wishdir, v;
	local float wishspeed, f;

	if (phys_movetype == MOVETYPE_NONE)
		return;

	if (phys_punchangle != '0 0 0')
	{
		f = vlen(phys_punchangle) - 10 * frametime;
		if (f > 0)
			phys_punchangle = normalize(phys_punchangle) * f;
		else
			phys_punchangle = '0 0 0';
	}

	// if dead, behave differently
	if (phys_health <= 0)
		return;

	// show 1/3 the pitch angle and all the roll angle
	phys_angles_z = bound(-1, phys_velocity * v_right * cl_divspeed, 1) * cl_rollangle;
	if (!phys_fixangle)
	{
		phys_angles_x = (phys_v_angle_x + phys_punchangle_x) * 0.333 * autocvar(r_meshpitch, -1);
		phys_angles_y = phys_v_angle_y + phys_punchangle_y;
	}

	if (phys_waterjump)
	{
		phys_velocity_x = phys_movedir_x;
		phys_velocity_y = phys_movedir_y;
		if (time > phys_teleport_time || phys_waterlevel == 0)
		{
			phys_waterjump = false;
			phys_teleport_time = 0;
		}
		return;
	}

	makevectors(phys_v_angle);

	// swim
	if (phys_waterlevel >= 2)
	if (phys_movetype != MOVETYPE_NOCLIP)
	{
		phys_onground = false;

		if (phys_movement == '0 0 0')
			wishvel = '0 0 -60'; // drift towards bottom
		else
			wishvel = v_forward * phys_movement_x + v_right * phys_movement_y + '0 0 1' * phys_movement_z;

		wishspeed = vlen (wishvel);
		if (wishspeed > sv_maxspeed)
			wishspeed = sv_maxspeed;

		wishspeed = wishspeed * 0.7;

		// water friction
		if (phys_velocity != '0 0 0')
		{
			f = vlen(phys_velocity) * (1 - frametime * sv_friction);
			if (f > 0)
				phys_velocity = normalize(phys_velocity) * f;
			else
				phys_velocity = '0 0 0';
		}
		else
			f = 0;

		// water acceleration
		if (wishspeed <= f)
			return;

		f = min(wishspeed - f, sv_accelerate * wishspeed * frametime);
		phys_velocity = phys_velocity + normalize(wishvel) * f;
		return;
	}

	// hack to not let you back into teleporter
	if (time < phys_teleport_time && phys_movement_x < 0)
		wishvel = v_right * phys_movement_y;
	else
	{
		if (phys_onground)
			makevectors (phys_v_angle_y * '0 1 0');

		wishvel = v_forward * phys_movement_x + v_right * phys_movement_y;
	}

	if (phys_movetype != MOVETYPE_WALK)
		wishvel_z = phys_movement_z;
	else
		wishvel_z = 0;

	wishdir = normalize(wishvel);
	wishspeed = vlen(wishvel);
	if (wishspeed > sv_maxspeed)
		wishspeed = sv_maxspeed;

	if (phys_movetype == MOVETYPE_NOCLIP) // noclip
	{
		phys_onground = false;
		phys_velocity = wishdir * wishspeed;
	}
	else if (phys_onground) // walking
	{
		// friction
		if (phys_velocity_x || phys_velocity_y)
		{
			v = phys_velocity;
			v_z = 0;
			f = vlen(v);

			// if the leading edge is over a dropoff, increase friction
			v = phys_origin + normalize(v) * 16 + '0 0 1' * phys_mins_z;

			traceline(v, v + '0 0 -34', TRUE, self);

			// apply friction
			if (trace_fraction == 1.0)
			{
				if (f < sv_stopspeed)
					f = 1 - frametime * (sv_stopspeed / f) * sv_friction * sv_edgefriction;
				else
					f = 1 - frametime * sv_friction * sv_edgefriction;
			}
			else
			{
				if (f < sv_stopspeed)
					f = 1 - frametime * (sv_stopspeed / f) * sv_friction;
				else
					f = 1 - frametime * sv_friction;
			}

			if (f < 0)
				phys_velocity = '0 0 0';
			else
				phys_velocity = phys_velocity * f;
		}

		// acceleration
		f = wishspeed - (phys_velocity * wishdir);
		if (f > 0)
			phys_velocity = phys_velocity + wishdir * min(f, sv_accelerate * frametime * wishspeed);
	}
	else // airborn
	{
		if (wishspeed < 30)
			f = wishspeed - (phys_velocity * wishdir);
		else
			f = 30 - (phys_velocity * wishdir);
		if (f > 0)
			phys_velocity = phys_velocity + wishdir * (min(f, sv_accelerate) * wishspeed * frametime);
	}

	PlayerPreThink();
	if (thinkfunc)
		thinkfunc();

	if (!SV_CheckWater (ent) && !phys_waterjump) )
		phys_velocity_z -= phys_gravity * frametime;
//		SV_CheckStuck (ent);
	SV_WalkMove (ent);
	PlayerPostThink();
}

