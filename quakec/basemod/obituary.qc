// client death messages
#define MOD_NONE             0
#define MOD_AXE              1
#define MOD_SHOTGUN          2
#define MOD_SUPERSHOTGUN     3
#define MOD_SPIKE            4
#define MOD_SUPERSPIKE       5
#define MOD_GRENADE          6
#define MOD_ROCKET           8
#define MOD_ROCKETRADIUS     9
#define MOD_SHAFT           10
#define MOD_SHAFTWATER      11
#define MOD_SHAFTSLIME      12
#define MOD_SHAFTLAVA       13
#define MOD_TELEFRAG        14
#define MOD_TELEFRAGDEFLECT 15
#define MOD_SQUISH          16
#define MOD_DROWN           17
#define MOD_SLIME           18
#define MOD_LAVA            19
#define MOD_EXPLOBOX        20
#define MOD_FALL            21
#define MOD_FIREBALL        22
#define MOD_EXIT            23
#define MOD_LASER           24
#define MOD_SELFWATER       25
#define MOD_HURT            26

void(string targ, INTEGER mod) SuicideMessage =
{
	string s, t, u;

	s = "";
	t = "";
	u = "";

	switch (mod)
	{
	case MOD_GRENADE:
		s = targ;
		t = " tries to put the pin back in";
		break;
	case MOD_SHAFTWATER:
		s = targ;
		t = " discharges into the water.";
		break;
	case MOD_SHAFTSLIME:
		s = targ;
		t = " discharges into the slime";
		break;
	case MOD_SHAFTLAVA:
		s = targ;
		t = " discharges into the lava";
		break;
	case MOD_TELEFRAGDEFLECT:
		s = "Satan's power deflects ";
		t = targ;
		u = "'s telefrag";
		break;
	case MOD_SELFWATER:
		s = targ;
		t = " electrocutes himself.";
		break;
	case MOD_EXPLOBOX:
		s = targ;
		t = " blew himself up";
		break;
	default:
		s = targ;
		t = " becomes bored with life";
	}

	bprint4(PRINT_MEDIUM, s, t, u, "\n");
}

void(string targ, string attacker, INTEGER mod) KillMessage =
{
	string s, t, u, v, w;

	s = "";
	t = "";
	u = "";
	v = "";
	w = "";

	switch (mod)
	{
	case MOD_AXE:
		s = targ;
		t = " was ax-murdered by ";
		u = attacker;
		break;
	case MOD_SHOTGUN:
		s = targ;
		t = " chewed on ";
		u = attacker;
		v = "'s boomstick";
		break;
	case MOD_SUPERSHOTGUN:
		s = targ;
		t = " ate two loads of ";
		u = attacker;
		v = "'s buckshot";
		break;
	case MOD_SPIKE:
		s = targ;
		t = " was nailed by ";
		u = attacker;
		break;
	case MOD_SUPERSPIKE:
		s = targ;
		t = " was punctured by ";
		u = attacker;
		break;
	case MOD_GRENADE:
		s = targ;
		t = " eats ";
		u = attacker;
		v = "'s pineapple";
		break;
	case MOD_ROCKET:
	case MOD_ROCKETRADIUS:
		s = targ;
		t = " rides ";
		u = attacker;
		v = "'s rocket";
		break;
	case MOD_SHAFT:
		s = targ;
		t = " accepts ";
		u = attacker;
		v = "'s shaft";
		break;
	case MOD_SHAFTWATER:
	case MOD_SHAFTSLIME:
	case MOD_SHAFTLAVA:
		s = targ;
		t = " accepts ";
		u = attacker;
		v = "'s discharge";
		break;
	case MOD_TELEFRAG:
		s = targ;
		t = " was telefragged by ";
		u = attacker;
		break;
	case MOD_TELEFRAGDEFLECT:
		s = targ;
		t = " was telefragged by ";
		u = attacker;
		v = "'s Satan's power";
		break;
	case MOD_SQUISH:
		s = attacker;
		t = " squishes ";
		u = targ;
		break;
	case MOD_EXPLOBOX:
		s = targ;
		t = " was blown up by ";
		u = attacker;
		break;
	default:
		s = targ;
		t = " was killed by ";
		u = attacker;
	}

	bprint6(PRINT_MEDIUM, s, t, u, v, w, "\n");
}

void(string targ, string attacker, INTEGER mod) TeamKillMessage =
{
	string s, t, u, v, w;
	float rnum;

	s = "";
	t = "";
	u = "";
	v = "";
	w = "";

	switch (mod)
	{
	case MOD_SQUISH:
		s = attacker;
		t = " squishes teammate ";
		u = targ;
		break;
	default:
		rnum = random();

		s = attacker;

		if (rnum < 0.25)
			t = " mows down teammate ";
		else if (rnum < 0.5)
		{
			t = " checks his glasses and sees ";
			v = " dead";
		}
		else if (rnum < 0.75)
		{
			t = " frags ";
			v = " for the other team";
		}
		else
			t = " loses his friend ";

		u = targ;
	}

	bprint6(PRINT_MEDIUM, s, t, u, v, w, "\n");
}

void(string targ, INTEGER mod) WorldKillMessage =
{
	string s, t, u;

	s = "";
	t = "";
	u = "";

	switch (mod)
	{
	case MOD_SPIKE:
	case MOD_SUPERSPIKE:
		s = targ;
		t = " was spiked";
		break;
	case MOD_SQUISH:
		s = targ;
		t = "  was squished";
		break;
	case MOD_DROWN:
		s = targ;

		if (random() < 0.5)
			t = " sleeps with the fishes";
		else
			t = " sucks it down";
		break;
	case MOD_SLIME:
		s = targ;

		if (random() < 0.5)
			t = " gulped a load of slime";
		else
			t = " can't exist on slime alone";
		break;
	case MOD_LAVA:
		s = targ;

		if (random() < 0.5)
			t = " turned into hot slag";
		else
			t = " visits the Volcano God";
		break;
	case MOD_EXPLOBOX:
		s = targ;
		t = " blew up";
		break;
	case MOD_FALL:
		s = targ;
		t = " fell to his death";
		break;
	case MOD_FIREBALL:
		s = targ;
		t = " ate a lavaball";
		break;
	case MOD_EXIT:
		s = targ;
		t = " tried to leave";
		break;
	case MOD_LASER:
		s = targ;
		t = " was zapped";
		break;
	default:
		s = targ;
		t = " died";
	}

	bprint4(PRINT_MEDIUM, s, t, u, "\n");
}