void() tesla_think =
{
	local entity e;
	local vector org;

	self.nextthink = time + 0.1;
	self.frame = self.frame + 1;

	if (!self.aflag)
	{
		if (!self.enemy || self.enemy.health <= 0)
		{
			e = findradius(self.origin, 800);
			while (e != world)
			{
				if (e.health > 0)
				{
					if (e.team != self.team)
					{
						if (e.solid)
						{
							if (e.takedamage)
							{
								traceline (self.origin, e.origin, FALSE, self);
								if (trace_ent == e)
								{
									break;
								}
							}
						}
					}
				}
				e = e.chain;
			}
			self.enemy = e;
			if (e != world)
				sound (self, CHAN_AUTO, "weapons/lstart.wav", 1, ATTN_NORM);
		}
		if (self.enemy)
		{
			if (self.frame < 7)
				self.frame = 7;

			org = self.origin + '0 0 32';

			traceline (org, self.enemy.origin, FALSE, self);

			if (self.t_width < time)
			{
				sound (self, CHAN_WEAPON, "weapons/lhit.wav", 1, ATTN_NORM);
				self.t_width = time + 0.6;
			}

			//mwa ha ha!!
			WriteByte (MSG_MULTICAST, SVC_TEMPENTITY);
			WriteByte (MSG_MULTICAST, TE_LIGHTNING2);
			WriteEntity (MSG_MULTICAST, self);
			WriteCoord (MSG_MULTICAST, org_x);
			WriteCoord (MSG_MULTICAST, org_y);
			WriteCoord (MSG_MULTICAST, org_z);
			WriteCoord (MSG_MULTICAST, trace_endpos_x);
			WriteCoord (MSG_MULTICAST, trace_endpos_y);
			WriteCoord (MSG_MULTICAST, trace_endpos_z);
			multicast (org, MULTICAST_PHS);

			if (trace_ent != self.enemy)
				self.enemy = world;
			trace_endpos = normalize(trace_endpos - org)*8 + trace_endpos;
			LightningDamage (self.origin, trace_endpos, self, 10);

			if (self.frame > 12)
				self.frame = 0;
			return;
		}
	}
	if (self.frame >= 7)
		self.frame = 0;
};

void() FinishTesla =
{
	self.netname = "tesla turret";
	self.think = tesla_think;
	setmodel (self, "progs/coil.mdl");
	setsize (self, '-16 -16 -24', '16 16 32');
	self.nextthink = time + 0.1;
};
