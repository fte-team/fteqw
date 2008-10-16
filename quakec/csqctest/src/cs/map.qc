.string target;
.string targetname;

string() getentitytoken = #355;

void() spawn_trigger_teleport =
{
	setmodel(self, self.model);
	self.model = "";

	print("teleport at ", vtos(self.mins), vtos(self.maxs), " to ", self.target, "\n");
};

void() spawn_info_teleport_destination =
{
	print("teleport_dest at ", vtos(self.origin), " called ", self.targetname, "\n");
};

float() parsenewmapentity =
{
	local string field, value;
	local entity nent;
	local void() spawnfunc;
	nent = spawn();

//	print("new entity\n");
	while (1)
	{
		field = getentitytoken();
		if not (field)
			break;

		if (field == "}")
		{
			if (!nent.classname)
				break;
			if (spawnfunc)
			{
				self = nent;
				spawnfunc();
			}
			return true;
		}

		value = getentitytoken();
		if not (value)
			break;

//		print("ent field ", field, " has value ", value, "\n");

		if (field == "classname")
		{
			if (value == "trigger_teleport")
				spawnfunc = spawn_trigger_teleport;
			if (value == "info_teleport_destination")
				spawnfunc = spawn_info_teleport_destination;
			nent.classname = value;
		}
		else if (field == "targetname")
			nent.targetname = value;
		else if (field == "target")
			nent.target = value;
		else if (field == "origin")
			nent.origin = stov(value);
		else if (field == "angles")
			nent.angles = stov(value);
		else if (field == "angle")
		{
			nent.angles_x = 0;
			nent.angles_y = stof(value);
			nent.angles_z = 0;
		}
		else if (field == "model")
			nent.model = value;
/*		else if (field == "light");
		else if (field == "mangle");
		else if (field == "killtarget");
		else if (field == "wad");
		else if (field == "height");
		else if (field == "lip");
		else if (field == "count");
		else if (field == "dmg");
		else if (field == "delay");
		else if (field == "message");
		else if (field == "worldtype");
		else if (field == "sounds");
		else if (field == "map");
		else if (field == "spawnflags");
		else if (field == "wait");
		else if (field == "style");
		else if (field == "health");
		else if (field == "speed");
		else
			print("ignoring field ", field, "\n");
*/
	}

	remove(nent);

	return false;
};

void() CSQC_WorldLoaded =
{
	local string tok;

	while (1)
	{
		tok = getentitytoken();

		if (tok == "")
			break;

		if (tok != "{")
		{
			print("bad entity data\n");
			return;
		}
		
		if (!parsenewmapentity())
		{
			print("bad entity data\n");
			return;
		}
	}
};
