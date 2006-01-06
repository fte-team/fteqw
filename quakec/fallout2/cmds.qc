void(entity e, string s) clientcommand = #440
float(string s) tokenize = #441;
string(float n) argv = #442;

float(string desc) itemtoslot =
{
	local float slot;
	slot = stof(desc);
	if (slot >= 1 && slot <= MAXSLOTS)
		return slot;

	slot = ItemIDOfName(desc);
	if (!slot)
	{
		sprint(self, PRINT_HIGH, "Not an item name\n");
		return 0;
	}
	else
	{
		slot = SlotOfItem(self, slot);
		if (slot)
			return slot;

		sprint(self, PRINT_HIGH, "You don't have one of those\n");
		return 0;
	}
};

float(float slotno) DecreaseDestroySlot =
{
	local float it;
	local float iid;
	local float num;
	it = ItemInSlot(self, slotno);
	if (ToStatus(it) <= 1)
	{
		SetItemSlot(self, slotno, 0);
		return true;
	}
	else
	{
		iid = ToIID(it);
		num = ToStatus(it);
		num = num - 1;
		it = SlotVal(iid, num);
		SetItemSlot(self, slotno, it);
		return false;
	}
};

void(string arg1) Cmd_InvUse =
{
	local float it, iid;
	local float slotno;
	slotno = itemtoslot(arg1);
	if (!slotno)
	{
		return;
	}


	it = ItemInSlot(self, slotno);
	iid = ToIID(it);

	if (iid == 0)
		return;

	if (WeaponAmmoType(iid))
	{
		//weapons are reloaded
		ReloadWeapon(slotno);
		return;
	}

	if (iid == IID_CHEM_STIMPACK ||
	    iid == IID_CHEM_MEDICALBAG ||
	    iid == IID_CHEM_SUPERSTIM)
	{
		if (UseHealingChem(iid))
			DecreaseDestroySlot(slotno);
		return;
	}

	if (iid == IID_CHEM_ADRENALINE ||
	    iid == IID_CHEM_PSYCHO ||
	    iid == IID_CHEM_BESERK)
	{
		if (UseBoostingChem(iid))
			DecreaseDestroySlot(slotno);
		return;
	}

	if (iid == IID_BUILD_MRAMMO ||
	    iid == IID_BUILD_SHIELDGEN ||
	    iid == IID_BUILD_AUTODOC ||
	    iid == IID_BUILD_ROBOFANG)
	{
		if (spawn_station(iid))
			DecreaseDestroySlot(slotno);
		return;
	}

	sprint(self, PRINT_HIGH, "Don't know how to 'use' item\n");
};

void(string arg1) Cmd_InvDrop =
{
	local float it, iid;
	local float slotno;
	slotno = itemtoslot(arg1);
	if (!slotno)
	{
		return;
	}


	it = ItemInSlot(self, slotno);
	iid = ToIID(it);

	if (iid == 0)
		return;

	DropFromSlot(slotno, true, false);
};

void(string arg1, string arg2) Cmd_InvSwap =
{
	local float old1, old2;
	local float slotno1;
	local float slotno2;
	slotno1 = itemtoslot(arg1);
	if (!slotno1)
	{
		return;
	}
	slotno2 = itemtoslot(arg2);
	if (!slotno2)
	{
		return;
	}

	old1 = ItemInSlot(self, slotno1);
	old2 = ItemInSlot(self, slotno2);
	if (!FitsInSlot(slotno1, ToIID(old2)))
	{
		sprint(self, PRINT_HIGH, "Not allowed to exchange items\n");
		return;
	}
	if (!FitsInSlot(slotno2, ToIID(old1)))
	{
		sprint(self, PRINT_HIGH, "Not allowed to exchange items\n");
		return;
	}
	SetItemSlot(self, slotno1, old2);
	SetItemSlot(self, slotno2, old1);

	if (slotno1 == self.current_slot || slotno2 == self.current_slot)
		W_SetCurrentAmmo();
};

void(string arg1, float iid, float num) Cmd_InvGive =
{
	local float slotno;
	slotno = itemtoslot(arg1);
	if (!slotno)
	{
		return;
	}

	if (num <= 0)
		num = 1;
	SetItemSlot(self, slotno, SlotVal(iid, num));

	if (slotno == self.current_slot)
		W_SetCurrentAmmo();
};

void(string line) SV_ParseClientCommand =
{
	local string cmd;
	tokenize(line);

	cmd = argv(0);
	if (self.deadflag)
	{	//no inventory stuff while dead.
		clientcommand(self, line);
	}
	else if (cmd == "invuse")
	{
		Cmd_InvUse(argv(1));
	}
	else if (cmd == "invdrop")
	{
		Cmd_InvDrop(argv(1));
	}
	else if (cmd == "invswap")
	{
		Cmd_InvSwap(argv(1), argv(2));
	}
	else if (cmd == "invgive")
	{
		Cmd_InvGive(argv(1), stof(argv(2)), stof(argv(3)));
	}
	else if (cmd == "god")
	{
		sprint(self, PRINT_HIGH, "sorry, but I'm an athiest\n");
	}
	else
		clientcommand(self, line);
};