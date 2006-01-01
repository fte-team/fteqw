string (entity guy, float slot) GetWeaponName =
{
	local string name;

	if (slot == 0)
		name = "bare hands";
	if (slot == 1)
		name = "knife";
	else if (slot == 2)
		name = "axe";
	else if (slot == 3)
		name = "ripper";
	else if (slot == 4)
		name = "power axe";
	else if (slot == 5)
		name = "1911";
	else if (slot == 6)
		name = "desert eagle";
	else if (slot == 7)
		name = "needler";
	else if (slot == 8)
		name = "alien blaster";
	else if (slot == 9)
		name = "pipe rifle";
	else if (slot == 10)
		name = "winchester";
	else if (slot == 11)
		name = "mossberg";
	else if (slot == 12)
		name = "citykiller";
	else if (slot == 13)
		name = "mp9";
	else if (slot == 14)
		name = "grease gun";
	else if (slot == 15)
		name = "rangemaster";
	else if (slot == 16)
		name = "ak-112";
	else if (slot == 17)
		name = "ak-74";
	else if (slot == 18)
		name = "dks-1";
	else if (slot == 19)
		name = "moonlight";
	else if (slot == 20)
		name = "sa-80";
	else if (slot == 21)
		name = "gauss rifle";
	else if (slot == 22)
		name = "laser carbine";

	return name;
};

string () GetArmorName =
{
	local string name;
	local float slot;

	slot = self.armor;

	if (slot == 0)
		name = "none";
	else if (slot == 1)
		name = "bulletproof shirt";
	else if (slot == 2)
		name = "leather armor";
	else if (slot == 3)
		name = "kevlar armor";
	else if (slot == 4)
		name = "metal armor";
	else if (slot == 5)
		name = "combat armor";
	else if (slot == 6)
		name = "brotherhood armor";
	else if (slot == 7)
		name = "force armor";
	else if (slot == 8)
		name = "light power armor";

	return name;
};


string () GetEquipmentName =
{
	local string name;
	local float slot;

	slot = self.equipment;

	if (slot == 0)
		name = "none";
	else if (slot == 1)
		name = "medic's bag";
	else if (slot == 2)
		name = "security alarm";
	else if (slot == 3)
		name = "remote camera";
	else if (slot == 4)
		name = "belt pouch";
	else if (slot == 5)
		name = "backpack";
	else if (slot == 6)
		name = "toolkit mark ii";
	else if (slot == 7)
		name = "climbing gear";
	else if (slot == 8)
		name = "enhanced battery";
	else if (slot == 9)
		name = "stealth-boy";

	return name;
};

string () GetProtectName =
{
	local string name;
	local float slot;

	slot = self.protect;

	if (slot == 0)
		name = "none";
	if (slot == 1)
		name = "";
	else if (slot == 2)
		name = "sacred talisman";
	else if (slot == 3)
		name = "forcefield ring";
	else if (slot == 4)
		name = "";
	else if (slot == 5)
		name = "";

	return name;
};

string () GetPerkName =
{
	local string name;
	local float slot;

	slot = self.perk;

	if (slot == 0)
		name = "none";
	if (slot == 1)
		name = "medic's bag";
	else if (slot == 2)
		name = "security alarm";
	else if (slot == 3)
		name = "remote camera";
	else if (slot == 4)
		name = "belt pouch";
	else if (slot == 5)
		name = "backpack";
	else if (slot == 6)
		name = "toolkit mark ii";
	else if (slot == 7)
		name = "climbing gear";
	else if (slot == 8)
		name = "enhanced battery";
	else if (slot == 9)
		name = "stealth-boy";
	return name;
};

string () GetTraitName =
{
	local string name;
	local float slot;

	slot = self.trait;

	if (slot == 0)
		name = "none";
	else if (slot == 1)
		name = "medic's bag";
	else if (slot == 2)
		name = "security alarm";
	else if (slot == 3)
		name = "remote camera";
	else if (slot == 4)
		name = "belt pouch";
	else if (slot == 5)
		name = "backpack";
	else if (slot == 6)
		name = "toolkit mark ii";
	else if (slot == 7)
		name = "climbing gear";
	else if (slot == 8)
		name = "enhanced battery";
	else if (slot == 9)
		name = "stealth-boy";

	return name;
};