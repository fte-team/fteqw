void() SV_PlayerPhysics =
{
	if (time != lastclientthink)
	{
		lastclientthink = time;
		sv_maxspeed = cvar("sv_maxspeed");
		sv_friction = cvar("sv_friction");
		sv_accelerate = cvar("sv_accelerate");
		sv_stopspeed = cvar("sv_stopspeed");
		sv_edgefriction = cvar("edgefriction");
		// LordHavoc: this * 4 is an optimization
		cl_rollangle = cvar("cl_rollangle") * 4;
		// LordHavoc: this 1 / is an optimization
		cl_divspeed = 1 / cvar("cl_rollspeed");
	}


	phys_movetype = self.movetype;
	phys_health = self.health;
	phys_fixangle = self.fixangle;
	phys_onground = self.flags & FL_ONGROUND;
	phys_waterjump = self.flags * FL_WATERJUMP;
//	phys_angles = self.angles;
	phys_v_angles = self.v_angles;
	phys_velocity = self.velocity;
	phys_movement = self.movement;
	phys_punchangle = self.punchangle;

	RunPlayerPhysics();

	if (phys_onground)
		self.flags = self.flags | FL_ONGROUND;
	else
		self.flags = self.flags - (self.flags & FL_ONGROUND);
	if (phys_waterjump)
		self.flags = self.flags | FL_WATERJUMP;
	else
		self.flags = self.flags - (self.flags & FL_WATERJUMP);
	self.angles = phys_angles;
	self.velocity = phys_velocity;
	self.punchangle = phys_punchangle;
};