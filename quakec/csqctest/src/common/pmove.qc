
float clientcommandframe;
float input_timelength;
vector input_angles;
vector input_movevalues;
float input_buttons;
float input_impulse;
//float input_lightlevel;
//float input_weapon;
//float input_servertime;

float pmove_type;
vector pmove_org;
vector pmove_vel;
vector pmove_mins;
vector pmove_maxs;
float pmove_jump_held;
float pmove_waterjumptime;

float movevars_friction;
float movevars_gravity;

float movevars_stepheight = 22;

#ifdef OWNPLAYERPHYSICS

void PMove_Move(vector dest)	//move forwards (preferably on the level) (does step ups)
{
	vector src;
	float stepped;
	tracebox(pmove_org, pmove_mins, pmove_maxs, dest, false, self);	//try going straight there

	if (trace_fraction < 1)
	{	//step up
		src = trace_endpos;
		trace_endpos_z += movevars_stepheight;
		tracebox(src, pmove_mins, pmove_maxs, dest, false, self);
		stepped = trace_endpos_z - src_z;
		dest_z += stepped;

		//move forwards
		tracebox(trace_endpos, pmove_mins, pmove_maxs, dest, false, self);

		//move down
		dest_z -= stepped;
		tracebox(trace_endpos, pmove_mins, pmove_maxs, dest, false, self);
	}
	pmove_org = trace_endpos;
};

void PMove(void)
{
	makevectors(input_angles);


	pmove_vel *= movevars_friction*input_timelength;
	pmove_vel += input_timelength*(
			v_forward * input_movevalues_x +
			v_right * input_movevalues_y +
			v_up * input_movevalues_z);

	switch(pmove_type)
	{
	case MOVETYPE_WALK:
		pmove_vel_z += movevars_gravity*input_timelength;
		PMove_Move(pmove_org + pmove_vel*input_timelength);
		break;
	case MOVETYPE_NOCLIP:
		pmove_org += pmove_vel*input_timelength;
		break;
	case MOVETYPE_NONE:
		break;
	}
};

#endif
