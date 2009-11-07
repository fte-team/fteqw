void(entity e, string s) clientcommand = #440
float(string s) tokenize = #441;
string(float n) argv = #442;

string(string s, float start, float length) substring = #116;
float(string str, string sub) strstrofs = #221;



float(string desc) itemtoslot =
{
	local float slot;
	local float coma;

	while((coma = strstrofs(desc, "+")) > 0)
	{
		slot = itemtoslot(substring(desc, 0, coma));
		if (slot)
			return slot;
		desc = substring(desc, coma+1, -1);
	}

	slot = stof(desc);
	if (slot >= 1 && slot <= 26)
		return slot;

	if (desc == "current")
		return self.current_slot;
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
		return 0;
	}
};

float(entity x, float slotno) DecreaseDestroySlotOther =
{
	local float it;
	local float iid;
	local float num;
	local entity oself;

	oself = self;
	self = x;

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

	self = oself;
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
	local float slotno, w;
	slotno = itemtoslot(arg1);
	if (!slotno)
	{
		sprint(self, PRINT_MEDIUM, "can't use - you don't have one\n");
		return;
	}


	it = ItemInSlot(self, slotno);
	iid = ToIID(it);
	w = SlotOfItem(self, iid);

	if (iid == 0)
		return;

	if (iid >= IID_GREN_FRAG && iid <= IID_GREN_STUN)
	{
		if (w != 5 && w != 6)
			return;

		self.rtime = time + 1;
		self.grenslot = w;
		player_pull1();
		return;
	}

	if (WeaponAmmoType(iid))
	{
		//weapons are reloaded
		ReloadWeapon(slotno);
		return;
	}


	if (iid == IID_CHEM_MEDICALBAG)
	{
		UseMedicalBag();
		return;
	}

	if (iid == IID_CHEM_STIMPACK)
	{
		UseStimpack();
		return;
	}

	if (iid == IID_GREN_FLARE)
	{
		ToggleFlare();
		return;
	}

	if (iid == IID_CHEM_SUPERSTIM)
	{
		UseSuperStim();
		return;
	}

	if (iid == IID_CHEM_ADRENALINE ||
	    iid == IID_CHEM_PSYCHO ||
	    iid == IID_CHEM_RADX ||
	    iid == IID_CHEM_BESERK)
	{
		if (UseBoostingChem(iid))
			DecreaseDestroySlot(slotno);
		return;
	}

	if (iid == IID_BUILD_MRAMMO ||
	    iid == IID_BUILD_SHIELDGEN ||
	    iid == IID_BUILD_AUTODOC ||
	    iid == IID_BUILD_ROBOFANG ||
	    iid == IID_BUILD_TTURRET || 
	    iid == IID_BUILD_RTURRET ||
	    iid == IID_BUILD_GTURRET)
	{
		if (spawn_station(iid))
			DecreaseDestroySlot(slotno);
		return;
	}

	if (iid == IID_EQUIP_CLIMBINGGEAR)
	{
		if (self.equipment_slot == slotno)
		{
			self.equipment_slot = 0;
			sprint(self, PRINT_HIGH, "Climbing gear deactivated\n");
		}
		else
		{
			self.equipment_slot = slotno;
			sprint(self, PRINT_HIGH, "Climbing gear activated\n");
		}
		return;
	}
	if (iid == IID_EQUIP_SPRINTKIT)
	{
		if (self.equipment_slot == slotno)
		{
			self.equipment_slot = 0;
			sprint(self, PRINT_HIGH, "What a downer\n");
		}
		else
		{
			self.equipment_slot = slotno;
			sprint(self, PRINT_HIGH, "SUGAR RUSH!!!\n");
		}
		return;
	}
	if (iid == IID_EQUIP_STEALTHBOY)
	{
		Sneak();
		return;
	}
	if (iid == IID_EQUIP_SILENCER)
	{
		AttachSilencer();
		return;
	}
	if (iid == IID_WP_WRENCH)
	{
		FireToolkit();
		return;
	}
	if (iid == IID_EQUIP_HOVERBOOTS)
	{
		if (self.equipment_slot == slotno)
		{
			self.equipment_slot = 0;
			sprint(self, PRINT_HIGH, "Hoverboots deactivated\n");
		}
		else
		{
			self.equipment_slot = slotno;
			sprint(self, PRINT_HIGH, "Hoverboots activated\n");
		}
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
		sprint(self, PRINT_MEDIUM, "Can't drop - you don't have one\n");
		return;
	}


	it = ItemInSlot(self, slotno);
	iid = ToIID(it);

	if (iid == 0)
		return;

	DropFromSlot(slotno, true, false);
};

void(string arg1) Cmd_InvBuy =
{
	local float x,y, iid, amount, money;


	x = FindEmptySlot(self);

	if (x == 0)
	{
		sprint(self, 2, "no more room in inventory!\n");
		sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
		return;
	}



	if (arg1 == "1")
	{
		iid = ToIID(self.xslot1);
		amount = ToStatus(self.xslot1);
		money = GetBaseValue(iid);
		if (self.ammo_shells < money)
		{
			sprint(self, 2, "not enough money.\n");
			sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
			return;
		}
		else
			self.ammo_shells = self.ammo_shells - money;

		if (NotStackable(iid))
			SetItemSlot(self, x, self.xslot1);
		else
			
			if (!TryGiveStackable(self, iid, amount))
			{
				sprint(self, 2, "full inventory.\n");
				sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
				return;
			}
		sound (self, CHAN_BODY, "misc/item2.wav", 1, ATTN_NORM);
	}

	if (arg1 == "2")
	{
		iid = ToIID(self.xslot2);
		amount = ToStatus(self.xslot2);
		money = GetBaseValue(iid);
		if (self.ammo_shells < money)
		{
			sprint(self, 2, "not enough money.\n");
			sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
			return;
		}
		else
			self.ammo_shells = self.ammo_shells - money;

		if (NotStackable(iid))
			SetItemSlot(self, x, self.xslot2);
		else
			
			if (!TryGiveStackable(self, iid, amount))
			{
				sprint(self, 2, "full inventory.\n");
				sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
				return;
			}
		sound (self, CHAN_BODY, "misc/item2.wav", 1, ATTN_NORM);
	}

	if (arg1 == "3")
	{
		iid = ToIID(self.xslot3);
		amount = ToStatus(self.xslot3);
		money = GetBaseValue(iid);
		if (self.ammo_shells < money)
		{
			sprint(self, 2, "not enough money.\n");
			sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
			return;
		}
		else
			self.ammo_shells = self.ammo_shells - money;

		if (NotStackable(iid))
			SetItemSlot(self, x, self.xslot3);
		else
			
			if (!TryGiveStackable(self, iid, amount))
			{
				sprint(self, 2, "full inventory.\n");
				sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
				return;
			}
		sound (self, CHAN_BODY, "misc/item2.wav", 1, ATTN_NORM);
	}

	if (arg1 == "4")
	{
		iid = ToIID(self.xslot4);
		amount = ToStatus(self.xslot4);
		money = GetBaseValue(iid);
		if (self.ammo_shells < money)
		{
			sprint(self, 2, "not enough money.\n");
			sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
			return;
		}
		else
			self.ammo_shells = self.ammo_shells - money;

		if (NotStackable(iid))
			SetItemSlot(self, x, self.xslot4);
		else
			
			if (!TryGiveStackable(self, iid, amount))
			{
				sprint(self, 2, "full inventory.\n");
				sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
				return;
			}
		sound (self, CHAN_BODY, "misc/item2.wav", 1, ATTN_NORM);
	}

	if (arg1 == "5")
	{
		iid = ToIID(self.xslot5);
		amount = ToStatus(self.xslot5);
		money = GetBaseValue(iid);
		if (self.ammo_shells < money)
		{
			sprint(self, 2, "not enough money.\n");
			sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
			return;
		}
		else
			self.ammo_shells = self.ammo_shells - money;

		if (NotStackable(iid))
			SetItemSlot(self, x, self.xslot5);
		else
			
			if (!TryGiveStackable(self, iid, amount))
			{
				sprint(self, 2, "full inventory.\n");
				sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
				return;
			}
		sound (self, CHAN_BODY, "misc/item2.wav", 1, ATTN_NORM);
	}

	if (arg1 == "6")
	{
		iid = ToIID(self.xslot6);
		amount = ToStatus(self.xslot6);
		money = GetBaseValue(iid);
		if (self.ammo_shells < money)
		{
			sprint(self, 2, "not enough money.\n");	
			sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
			return;
		}
		else
			self.ammo_shells = self.ammo_shells - money;

		if (NotStackable(iid))
			SetItemSlot(self, x, self.xslot6);
		else
			
			if (!TryGiveStackable(self, iid, amount))
			{
				sprint(self, 2, "full inventory.\n");
				sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
				return;
			}
		sound (self, CHAN_BODY, "misc/item2.wav", 1, ATTN_NORM);
	}

	if (arg1 == "7")
	{
		iid = ToIID(self.xslot7);
		amount = ToStatus(self.xslot7);
		money = GetBaseValue(iid);
		if (self.ammo_shells < money)
		{
			sprint(self, 2, "not enough money.\n");
			sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
			return;
		}
		else
			self.ammo_shells = self.ammo_shells - money;

		if (NotStackable(iid))
			SetItemSlot(self, x, self.xslot7);
		else
			
			if (!TryGiveStackable(self, iid, amount))
			{
				sprint(self, 2, "full inventory.\n");
				sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
				return;
			}
		sound (self, CHAN_BODY, "misc/item2.wav", 1, ATTN_NORM);
	}

	if (arg1 == "8")
	{
		iid = ToIID(self.xslot8);
		amount = ToStatus(self.xslot8);
		money = GetBaseValue(iid);
		if (self.ammo_shells < money)
		{
			sprint(self, 2, "not enough money.\n");
			sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
			return;
		}
		else
			self.ammo_shells = self.ammo_shells - money;

		if (NotStackable(iid))
			SetItemSlot(self, x, self.xslot8);
		else
			
			if (!TryGiveStackable(self, iid, amount))
			{
				sprint(self, 2, "full inventory.\n");
				sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
				return;
			}
		sound (self, CHAN_BODY, "misc/item2.wav", 1, ATTN_NORM);
	}
	if (arg1 == "9")
	{
		iid = ToIID(self.xslot9);
		amount = ToStatus(self.xslot9);
		money = GetBaseValue(iid);
		if (self.ammo_shells < money)
		{
			sprint(self, 2, "not enough money.\n");
			sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
			return;
		}
		else
			self.ammo_shells = self.ammo_shells - money;

		if (NotStackable(iid))
			SetItemSlot(self, x, self.xslot9);
		else
			
			if (!TryGiveStackable(self, iid, amount))
			{
				sprint(self, 2, "full inventory.\n");
				sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
				return;
			}
		sound (self, CHAN_BODY, "misc/item2.wav", 1, ATTN_NORM);
	}
	if (arg1 == "10")
	{
		iid = ToIID(self.xslot10);
		amount = ToStatus(self.xslot10);
		money = GetBaseValue(iid);
		if (self.ammo_shells < money)
		{
			sprint(self, 2, "not enough money.\n");
			sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
			return;
		}
		else
			self.ammo_shells = self.ammo_shells - money;

		if (NotStackable(iid))
			SetItemSlot(self, x, self.xslot10);
		else
			
			if (!TryGiveStackable(self, iid, amount))
			{
				sprint(self, 2, "full inventory.\n");
				sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
				return;
			}
		sound (self, CHAN_BODY, "misc/item2.wav", 1, ATTN_NORM);
	}
	if (arg1 == "11")
	{
		iid = ToIID(self.xslot11);
		amount = ToStatus(self.xslot11);
		money = GetBaseValue(iid);
		if (self.ammo_shells < money)
		{
			sprint(self, 2, "not enough money.\n");
			sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
			return;
		}
		else
			self.ammo_shells = self.ammo_shells - money;

		if (NotStackable(iid))
			SetItemSlot(self, x, self.xslot11);
		else
			
			if (!TryGiveStackable(self, iid, amount))
			{
				sprint(self, 2, "full inventory.\n");
				sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
				return;
			}
		sound (self, CHAN_BODY, "misc/item2.wav", 1, ATTN_NORM);
	}
	if (arg1 == "12")
	{
		iid = ToIID(self.xslot12);
		amount = ToStatus(self.xslot12);
		money = GetBaseValue(iid);
		if (self.ammo_shells < money)
		{
			sprint(self, 2, "not enough money.\n");
			sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
			return;
		}
		else
			self.ammo_shells = self.ammo_shells - money;

		if (NotStackable(iid))
			SetItemSlot(self, x, self.xslot12);
		else
			
			if (!TryGiveStackable(self, iid, amount))
			{
				sprint(self, 2, "full inventory.\n");
				sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
				return;
			}
		sound (self, CHAN_BODY, "misc/item2.wav", 1, ATTN_NORM);
	}
	if (arg1 == "13")
	{
		iid = ToIID(self.xslot13);
		amount = ToStatus(self.xslot13);
		money = GetBaseValue(iid);
		if (self.ammo_shells < money)
		{
			sprint(self, 2, "not enough money.\n");
			sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
			return;
		}
		else
			self.ammo_shells = self.ammo_shells - money;

		if (NotStackable(iid))
			SetItemSlot(self, x, self.xslot13);
		else
			
			if (!TryGiveStackable(self, iid, amount))
			{
				sprint(self, 2, "full inventory.\n");
				sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
				return;
			}
		sound (self, CHAN_BODY, "misc/item2.wav", 1, ATTN_NORM);
	}
	if (arg1 == "14")
	{
		iid = ToIID(self.xslot14);
		amount = ToStatus(self.xslot14);
		money = GetBaseValue(iid);
		if (self.ammo_shells < money)
		{
			sprint(self, 2, "not enough money.\n");
			sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
			return;
		}
		else
			self.ammo_shells = self.ammo_shells - money;

		if (NotStackable(iid))
			SetItemSlot(self, x, self.xslot14);
		else
			
			if (!TryGiveStackable(self, iid, amount))
			{
				sprint(self, 2, "full inventory.\n");
				sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
				return;
			}
		sound (self, CHAN_BODY, "misc/item2.wav", 1, ATTN_NORM);
	}
	if (arg1 == "15")
	{
		iid = ToIID(self.xslot15);
		amount = ToStatus(self.xslot15);
		money = GetBaseValue(iid);
		if (self.ammo_shells < money)
		{
			sprint(self, 2, "not enough money.\n");
			sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
			return;
		}
		else
			self.ammo_shells = self.ammo_shells - money;

		if (NotStackable(iid))
			SetItemSlot(self, x, self.xslot15);
		else
			
			if (!TryGiveStackable(self, iid, amount))
			{
				sprint(self, 2, "full inventory.\n");
				sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
				return;
			}
		sound (self, CHAN_BODY, "misc/item2.wav", 1, ATTN_NORM);
	}
	if (arg1 == "16")
	{
		iid = ToIID(self.xslot16);
		amount = ToStatus(self.xslot16);
		money = GetBaseValue(iid);
		if (self.ammo_shells < money)
		{
			sprint(self, 2, "not enough money.\n");
			sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
			return;
		}
		else
			self.ammo_shells = self.ammo_shells - money;

		if (NotStackable(iid))
			SetItemSlot(self, x, self.xslot16);
		else
			
			if (!TryGiveStackable(self, iid, amount))
			{
				sprint(self, 2, "full inventory.\n");
				sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
				return;
			}
		sound (self, CHAN_BODY, "misc/item2.wav", 1, ATTN_NORM);
	}
	if (arg1 == "17")
	{
		iid = ToIID(self.xslot17);
		amount = ToStatus(self.xslot17);
		money = GetBaseValue(iid);
		if (self.ammo_shells < money)
		{
			sprint(self, 2, "not enough money.\n");
			sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
			return;
		}
		else
			self.ammo_shells = self.ammo_shells - money;

		if (NotStackable(iid))
			SetItemSlot(self, x, self.xslot17);
		else
			
			if (!TryGiveStackable(self, iid, amount))
			{
				sprint(self, 2, "full inventory.\n");
				sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
				return;
			}
		sound (self, CHAN_BODY, "misc/item2.wav", 1, ATTN_NORM);
	}
	if (arg1 == "18")
	{
		iid = ToIID(self.xslot18);
		amount = ToStatus(self.xslot18);
		money = GetBaseValue(iid);
		if (self.ammo_shells < money)
		{
			sprint(self, 2, "not enough money.\n");
			sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
			return;
		}
		else
			self.ammo_shells = self.ammo_shells - money;

		if (NotStackable(iid))
			SetItemSlot(self, x, self.xslot18);
		else
			
			if (!TryGiveStackable(self, iid, amount))
			{
				sprint(self, 2, "full inventory.\n");
				sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
				return;
			}
		sound (self, CHAN_BODY, "misc/item2.wav", 1, ATTN_NORM);
	}
	if (arg1 == "19")
	{
		iid = ToIID(self.xslot19);
		amount = ToStatus(self.xslot19);
		money = GetBaseValue(iid);
		if (self.ammo_shells < money)
		{
			sprint(self, 2, "not enough money.\n");
			sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
			return;
		}
		else
			self.ammo_shells = self.ammo_shells - money;

		if (NotStackable(iid))
			SetItemSlot(self, x, self.xslot19);
		else
			
			if (!TryGiveStackable(self, iid, amount))
			{
				sprint(self, 2, "full inventory.\n");
				sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
				return;
			}
		sound (self, CHAN_BODY, "misc/item2.wav", 1, ATTN_NORM);
	}
	if (arg1 == "20")
	{
		iid = ToIID(self.xslot20);
		amount = ToStatus(self.xslot20);
		money = GetBaseValue(iid);
		if (self.ammo_shells < money)
		{
			sprint(self, 2, "not enough money.\n");
			sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
			return;
		}
		else
			self.ammo_shells = self.ammo_shells - money;

		if (NotStackable(iid))
			SetItemSlot(self, x, self.xslot20);
		else
			
			if (!TryGiveStackable(self, iid, amount))
			{
				sprint(self, 2, "full inventory.\n");
				sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
				return;
			}
		sound (self, CHAN_BODY, "misc/item2.wav", 1, ATTN_NORM);
	}
};

void(string arg1) Cmd_InvSell =
{
	local float it, iid, x;
	local float slotno;
	local string y, z;

	slotno = itemtoslot(arg1);
	if (!slotno)
	{
		sprint(self, 2, "can't sell - you don't have one\n");
		return;
	}

	if (slotno <= 4)
	{
		sprint(self, 2, "put it in your inventory first!\n");
		return;
	}

	it = ItemInSlot(self, slotno);
	iid = ToIID(it);

	if (iid == 0)
		return;

	self.missionbrief = 0;

	SetItemSlot(self, slotno, 0);
	x = GetBaseValue(iid);
	y = ftos(x);
	z = GetItemName(iid);

	self.ammo_shells += x;
	sprint(self, 2, "you sell ");
	sprint(self, 2, z);
	sprint(self, 2, " for $");
	sprint(self, 2, y);
	sprint(self, 2, ".\n");

};

void(string arg1) Cmd_InvMix =
{
	local float it, iid1, iid2, item1, item2;
	local float slotno, x;



	it = ItemInSlot(self, 23);
	iid1 = ToIID(it);
	if (iid1 == 0)
	{
		sprint(self, PRINT_MEDIUM, "need two items!\n");
		sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
		return;
	}

	it = ItemInSlot(self, 24);
	iid2 = ToIID(it);
	if (iid2 == 0)
	{
		sprint(self, PRINT_MEDIUM, "need two items!\n");
		sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
		return;
	}

	if ((iid1 == IID_MISC_NUKACOLA && iid2 == IID_MISC_CHEMICALS) ||
            (iid2 == IID_MISC_NUKACOLA && iid1 == IID_MISC_CHEMICALS))
	{

		DecreaseDestroySlot(23);
		DecreaseDestroySlot(24);

		if (random()*100 >= (self.skill_science*10))
		{
			sprint(self, PRINT_MEDIUM, "your concoction exploded!\n");
			Explosion();
			return;
		}

		if (random()*100 < ((self.skill_science*10) - 100))
		{
			sprint(self, PRINT_MEDIUM, "2 stimpacks created!\n");
			sound (self, CHAN_BODY, "misc/yourturn.wav", 1, ATTN_NORM);
			TryGiveStackable(self, IID_CHEM_STIMPACK, 2);
			return;
		}
		TryGiveStackable(self, IID_CHEM_STIMPACK, 1);
		sprint(self, PRINT_MEDIUM, "1 stimpack created\n");
		sound (self, CHAN_BODY, "misc/item1.wav", 1, ATTN_NORM);
	}
	

	else if ((iid1 == IID_MISC_AEROSOL && iid2 == IID_MISC_CHEMICALS) ||
            (iid2 == IID_MISC_AEROSOL && iid1 == IID_MISC_CHEMICALS))
	{

		DecreaseDestroySlot(23);
		DecreaseDestroySlot(24);

                if (random()*100 > (self.skill_science*10))
		{
			sprint(self, PRINT_MEDIUM, "your concoction exploded!\n");
			Explosion();
			return;
		}

		if (random()*100 < ((self.skill_science*10) - 100))
		{
			sprint(self, PRINT_MEDIUM, "2 flash bombs created!\n");
			sound (self, CHAN_BODY, "misc/yourturn.wav", 1, ATTN_NORM);
			TryGiveStackable(self, IID_GREN_FLASH, 2);
			return;
		}
		TryGiveStackable(self, IID_GREN_FLASH, 1);
		sprint(self, PRINT_MEDIUM, "1 flash bomb created\n");
		sound (self, CHAN_BODY, "misc/item1.wav", 1, ATTN_NORM);
	}
	else if ((iid1 == IID_MISC_NUKACOLA && iid2 == IID_MISC_GUM) ||
            (iid2 == IID_MISC_NUKACOLA && iid1 == IID_MISC_GUM))
	{

		DecreaseDestroySlot(23);
		DecreaseDestroySlot(24);

		if (random()*100 > (self.skill_science*10))
		{
			sprint(self, PRINT_MEDIUM, "your concoction exploded!\n");
			Explosion();
			return;
		}

		if (random()*100 < ((self.skill_science*10) - 100))
		{
			sprint(self, PRINT_MEDIUM, "2 stun bombs created!\n");
			sound (self, CHAN_BODY, "misc/yourturn.wav", 1, ATTN_NORM);
			TryGiveStackable(self, IID_GREN_STUN, 2);
			return;
		}
		TryGiveStackable(self, IID_GREN_STUN, 1);
		sprint(self, PRINT_MEDIUM, "1 stun bomb created\n");
		sound (self, CHAN_BODY, "misc/item1.wav", 1, ATTN_NORM);
	}
	else if ((iid1 == IID_MISC_RDXCRYSTAL && iid2 == IID_MISC_HMXCOMPOUND) ||
            (iid2 == IID_MISC_RDXCRYSTAL && iid1 == IID_MISC_HMXCOMPOUND))
	{

		DecreaseDestroySlot(23);
		DecreaseDestroySlot(24);

		if (random()*100 > (self.skill_science*5))
		{
			sprint(self, PRINT_MEDIUM, "your concoction exploded!\n");
			Explosion();
			return;
		}

		if (random()*100 < ((self.skill_science*4) - 100))
		{
			sprint(self, PRINT_MEDIUM, "2 rockets created!\n");
			sound (self, CHAN_BODY, "misc/yourturn.wav", 1, ATTN_NORM);
			TryGiveStackable(self, IID_AM_ROCKET, 2);
			return;
		}
		TryGiveStackable(self, IID_AM_ROCKET, 1);
		sprint(self, PRINT_MEDIUM, "1 rocket created\n");
		sound (self, CHAN_BODY, "misc/item1.wav", 1, ATTN_NORM);
	}
	else if ((iid1 == IID_MISC_CHEMICALS && iid2 == IID_MISC_HMXCOMPOUND) ||
            (iid2 == IID_MISC_CHEMICALS && iid1 == IID_MISC_HMXCOMPOUND))
	{

		DecreaseDestroySlot(23);
		DecreaseDestroySlot(24);

		if (random()*100 > (self.skill_science*8))
		{
			sprint(self, PRINT_MEDIUM, "your concoction exploded!\n");
			Explosion();
			return;
		}

		if (random()*100 < ((self.skill_science*8) - 100))
		{
			sprint(self, PRINT_MEDIUM, "2 grenades created!\n");
			sound (self, CHAN_BODY, "misc/yourturn.wav", 1, ATTN_NORM);
			TryGiveStackable(self, IID_GREN_FRAG, 2);
			return;
		}
		TryGiveStackable(self, IID_GREN_FRAG, 1);
		sprint(self, PRINT_MEDIUM, "1 grenade created\n");
		sound (self, CHAN_BODY, "misc/item1.wav", 1, ATTN_NORM);
	}
	else if ((iid1 == IID_MISC_CHEMICALS && iid2 == IID_MISC_RDXCRYSTAL) ||
            (iid2 == IID_MISC_CHEMICALS && iid1 == IID_MISC_RDXCRYSTAL))
	{

		DecreaseDestroySlot(23);
		DecreaseDestroySlot(24);

		if (random()*100 > (self.skill_science*3))
		{
			sprint(self, PRINT_MEDIUM, "your concoction exploded!\n");
			Explosion();
			return;
		}

		if (random()*100 < ((self.skill_science*4) - 100))
		{
			sprint(self, PRINT_MEDIUM, "2 super stims created!\n");
			sound (self, CHAN_BODY, "misc/yourturn.wav", 1, ATTN_NORM);
			TryGiveStackable(self, IID_CHEM_SUPERSTIM, 2);
			return;
		}
		TryGiveStackable(self, IID_CHEM_SUPERSTIM, 1);
		sprint(self, PRINT_MEDIUM, "1 super stim created\n");
		sound (self, CHAN_BODY, "misc/item1.wav", 1, ATTN_NORM);
	}

	else
	{
		sprint(self, PRINT_MEDIUM, "cannot merge items!\n");
		sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
		return;
	}
};

void(string arg1) Cmd_InvCreate =
{
	local float it, iid;
	local float slotno, q, w, x, y, z;
	local float weap;

	if (arg1 == "flash")
	{

		x = SlotOfItem(self, IID_MISC_NUKACOLA);
		if (x == 0)
		{
			sprint(self, PRINT_MEDIUM, "you need nukacola + chemicals + aerosol can\n");
			return;
		}

		y = SlotOfItem(self, IID_MISC_CHEMICALS);
		if (y == 0)
		{
			sprint(self, PRINT_MEDIUM, "you need nukacola + chemicals + aerosol can\n");
			return;
		}

		z = SlotOfItem(self, IID_MISC_AEROSOL);
		if (z == 0)
		{
			sprint(self, PRINT_MEDIUM, "you need nukacola + chemicals + aerosol can\n");
			return;
		}

		DecreaseDestroySlot(x);
		DecreaseDestroySlot(y);
		DecreaseDestroySlot(z);

		if ((random()<0.50) && (self.class == 1))
		{
			sprint(self, PRINT_MEDIUM, "you failed to create a flash bomb.\n");
			return;
		}
		if ((random()<0.25) && (self.class == 2))
		{
			sprint(self, PRINT_MEDIUM, "you failed to create a flash bomb.\n");
			return;
		}
		if ((random()<0.75) && (self.class == 3))
		{
			sprint(self, PRINT_MEDIUM, "you failed to create a flash bomb.\n");
			return;
		}

		TryGiveStackable(self, IID_GREN_FLASH, 1);
		sprint(self, PRINT_MEDIUM, "flash bomb created.\n");
	}
	if (arg1 == "grenade")
	{
		x = SlotOfItem(self, IID_MISC_RDXCRYSTAL);
		if (x == 0)
		{
			sprint(self, PRINT_MEDIUM, "you need rdx crystals, chemicals, aerosol can\n");
			return;
		}

		y = SlotOfItem(self, IID_MISC_CHEMICALS);
		if (y == 0)
		{
			sprint(self, PRINT_MEDIUM, "you need rdx crystals, chemicals, aerosol can\n");
			return;
		}

		z = SlotOfItem(self, IID_MISC_AEROSOL);
		if (z == 0)
		{
			sprint(self, PRINT_MEDIUM, "you need rdx crystals, chemicals, aerosol can\n");
			return;
		}

		DecreaseDestroySlot(x);
		DecreaseDestroySlot(y);
		DecreaseDestroySlot(z);

		if ((random()<0.75) && (self.class == 1))
		{
			sprint(self, PRINT_MEDIUM, "you failed to create a grenade.\n");
			return;
		}
		if ((random()<0.75) && (self.class == 2))
		{
			sprint(self, PRINT_MEDIUM, "you failed to create a grenade.\n");
			return;
		}
		if ((random()<0.75) && (self.class == 3))
		{
			sprint(self, PRINT_MEDIUM, "you failed to create a grenade.\n");
			return;
		}

		TryGiveStackable(self, IID_GREN_FRAG, 1);
		sprint(self, PRINT_MEDIUM, "homemade bomb created.\n");
	}
	if (arg1 == "silencer")
	{
		x = SlotOfItem(self, IID_MISC_DUCKTAPE);
		if (x == 0)
		{
			sprint(self, PRINT_MEDIUM, "you need junk + aerosol can + duck tape\n");
			return;
		}
		y = SlotOfItem(self, IID_MISC_JUNK);
		if (y == 0)
		{
			sprint(self, PRINT_MEDIUM, "you need junk + aerosol can + duck tape\n");
			return;
		}
		z = SlotOfItem(self, IID_MISC_AEROSOL);
		if (z == 0)
		{
			sprint(self, PRINT_MEDIUM, "you need junk + aerosol can + duck tape\n");
			return;
		}

		DecreaseDestroySlot(x);
		DecreaseDestroySlot(y);
		DecreaseDestroySlot(z);

		if ((random()<0.25) && (self.class == 1))
		{
			sprint(self, PRINT_MEDIUM, "you failed to create a silencer.\n");
			return;
		}
		if ((random()<0.10) && (self.class == 2))
		{
			sprint(self, PRINT_MEDIUM, "you failed to create a silencer.\n");
			return;
		}
		if ((random()<0.50) && (self.class == 3))
		{
			sprint(self, PRINT_MEDIUM, "you failed to create a silencer.\n");
			return;
		}
		AddNonStackable(self, IID_EQUIP_SILENCER, 1);
		sprint(self, PRINT_MEDIUM, "silencer created.\n");
	}
	if (arg1 == "stun")
	{
		x = SlotOfItem(self, IID_MISC_NUKACOLA);
		if (x == 0)
		{
			sprint(self, PRINT_MEDIUM, "you need nukacola + chemicals\n");
			return;
		}

		y = SlotOfItem(self, IID_MISC_CHEMICALS);
		if (y == 0)
		{
			sprint(self, PRINT_MEDIUM, "you need nukacola + chemicals\n");
			return;
		}
		DecreaseDestroySlot(x);
		DecreaseDestroySlot(y);

		if ((random()<0.40) && (self.class < 4))
		{
			sprint(self, PRINT_MEDIUM, "you failed to create a stun grenade.\n");
			return;
		}

		TryGiveStackable(self, IID_GREN_STUN, 1);
		sprint(self, PRINT_MEDIUM, "stun grenade created.\n");
	}
	if (arg1 == "extender")
	{
		y = SlotOfItem(self, IID_MISC_STEELPIPE);
		if (y == 0)
		{
			sprint(self, PRINT_MEDIUM, "you need steel pipe + duck tape\n");
			return;
		}

		z = SlotOfItem(self, IID_MISC_DUCKTAPE);
		if (z == 0)
		{
			sprint(self, PRINT_MEDIUM, "you need junk, aerosol can\n");
			return;
		}

		DecreaseDestroySlot(y);
		DecreaseDestroySlot(z);

		AddNonStackable(self, IID_EQUIP_EXTENDER, 1);
		sprint(self, PRINT_MEDIUM, "barrel extender created.\n");
	}
	if (arg1 == "xray")
	{
		y = SlotOfItem(self, IID_MISC_LCD);
		if (y == 0)
		{
			sprint(self, PRINT_MEDIUM, "you need the proper ingredients...\n");
			return;
		}
		y = SlotOfItem(self, IID_MISC_CIRCUITBOARD);
		if (y == 0)
		{
			sprint(self, PRINT_MEDIUM, "you need the proper ingredients...\n");
			return;
		}

		z = SlotOfItem(self, IID_MISC_GUM);
		if (z == 0)
		{
			sprint(self, PRINT_MEDIUM, "you need the proper ingredients...\n");
			return;
		}
		q = SlotOfItem(self, IID_MISC_XRAYTUBE);
		if (q == 0)
		{
			sprint(self, PRINT_MEDIUM, "you need the proper ingredients...\n");
			return;
		}
		w = SlotOfItem(self, IID_MISC_COPPERWIRE);
		if (w == 0)
		{
			sprint(self, PRINT_MEDIUM, "you need the proper ingredients...\n");
			return;
		}

		DecreaseDestroySlot(q);
		DecreaseDestroySlot(w);
		DecreaseDestroySlot(x);
		DecreaseDestroySlot(y);
		DecreaseDestroySlot(z);

		if ((random()<0.90) && (self.class < 4))
		{
			sprint(self, PRINT_MEDIUM, "you failed to create an x-ray device.\n");
			return;
		}

		AddNonStackable(self, IID_EQUIP_XRAY, 1);
		sprint(self, PRINT_MEDIUM, "x-ray device created!\n");
	}
	if (arg1 == "modify")
	{
			weap = ToIID(self.(SlotField(self.current_slot)));

		if (weap == IID_WP_PULSERIFLE)
		{
			y = SlotOfItem(self, IID_MISC_DUCKTAPE);
			if (y == 0)
			{
				sprint(self, PRINT_MEDIUM, "you need the proper ingredients...\n");
				return;
			}
			x = SlotOfItem(self, IID_MISC_CIRCUITBOARD);
			if (x == 0)
			{
				sprint(self, PRINT_MEDIUM, "you need the proper ingredients...\n");
				return;
			}
			z = SlotOfItem(self, IID_MISC_NUKACOLA);
			if (z == 0)
			{
				sprint(self, PRINT_MEDIUM, "you need the proper ingredients...\n");
				return;
			}
			q = SlotOfItem(self, IID_MISC_XRAYTUBE);
			if (q == 0)
			{
				sprint(self, PRINT_MEDIUM, "you need the proper ingredients...\n");
				return;
			}
			w = SlotOfItem(self, IID_MISC_COPPERWIRE);
			if (w == 0)
			{
				sprint(self, PRINT_MEDIUM, "you need the proper ingredients...\n");
				return;
			}

			DecreaseDestroySlot(q);
			DecreaseDestroySlot(w);
			DecreaseDestroySlot(x);
			DecreaseDestroySlot(y);
			DecreaseDestroySlot(z);

			if (ToIID(self.islot1) == IID_WP_PULSERIFLE)
				SetItemSlot(self, 1, SlotVal(IID_WP_TURBOPLASMA, 40));
			else if (ToIID(self.islot2) == IID_WP_PULSERIFLE)
				SetItemSlot(self, 2, SlotVal(IID_WP_TURBOPLASMA, 40));


			if ((random()<0.25))
			{
				sprint(self, PRINT_MEDIUM, "you try to modify your plasma rifle by hotwiring\n");
				sprint(self, PRINT_MEDIUM, "the bolt carrier and venting off excess gasses  \n");
				sprint(self, PRINT_MEDIUM, "but you end up destroying it...\n");
				return;
			}
			AddNonStackable(self, IID_WP_TURBOPLASMA, 1);
			sprint(self, PRINT_MEDIUM, "turbo plasma rifle created\n");
		}
		if (weap == IID_WP_AK112)
		{
			y = SlotOfItem(self, IID_MISC_DUCKTAPE);
			if (y == 0)
			{
				sprint(self, PRINT_MEDIUM, "you need the proper ingredients...\n");
				return;
			}
			x = SlotOfItem(self, IID_MISC_JUNK);
			if (x == 0)
			{
				sprint(self, PRINT_MEDIUM, "you need the proper ingredients...\n");
				return;
			}
			z = SlotOfItem(self, IID_MISC_AEROSOL);
			if (z == 0)
			{
				sprint(self, PRINT_MEDIUM, "you need the proper ingredients...\n");
				return;
			}
			w = SlotOfItem(self, IID_MISC_COPPERWIRE);
			if (w == 0)
			{
				sprint(self, PRINT_MEDIUM, "you need the proper ingredients...\n");
				return;
			}

			DecreaseDestroySlot(y);
			DecreaseDestroySlot(w);
			DecreaseDestroySlot(x);
			DecreaseDestroySlot(z);

			if (ToIID(self.islot1) == IID_WP_AK112)
				SetItemSlot(self, 1, SlotVal(IID_WP_AK112_M, 50));
			else if (ToIID(self.islot2) == IID_WP_AK112)
				SetItemSlot(self, 2, SlotVal(IID_WP_AK112_M, 50));

			sprint(self, PRINT_MEDIUM, "created expanded magazine for ak-112\n");
		}
		if (weap == IID_WP_DEAGLE)
		{
			y = SlotOfItem(self, IID_MISC_DUCKTAPE);
			if (y == 0)
			{
				sprint(self, PRINT_MEDIUM, "you need the proper ingredients...\n");
				return;
			}
			x = SlotOfItem(self, IID_MISC_JUNK);
			if (x == 0)
			{
				sprint(self, PRINT_MEDIUM, "you need the proper ingredients...\n");
				return;
			}
			z = SlotOfItem(self, IID_MISC_AEROSOL);
			if (z == 0)
			{
				sprint(self, PRINT_MEDIUM, "you need the proper ingredients...\n");
				return;
			}
			w = SlotOfItem(self, IID_MISC_COPPERWIRE);
			if (w == 0)
			{
				sprint(self, PRINT_MEDIUM, "you need the proper ingredients...\n");
				return;
			}

			DecreaseDestroySlot(y);
			DecreaseDestroySlot(w);
			DecreaseDestroySlot(x);
			DecreaseDestroySlot(z);

			if (ToIID(self.islot1) == IID_WP_DEAGLE)
				SetItemSlot(self, 1, SlotVal(IID_WP_DEAGLE_M, 10));
			else if (ToIID(self.islot2) == IID_WP_DEAGLE)
				SetItemSlot(self, 2, SlotVal(IID_WP_DEAGLE_M, 10));

			sprint(self, PRINT_MEDIUM, "created expanded magazine for deagle\n");
		}
		else
		{
			sprint(self, PRINT_MEDIUM, "can't modify currently held weapon.\n");
			return;
		}
	}
};

void(string arg1, string arg2) Cmd_InvSwap =
{
	local float old1, old2;
	local float slotno1;
	local float slotno2;
	slotno1 = itemtoslot(arg1);
	if (!slotno1)
	{
		sprint(self, PRINT_HIGH, "No item\n");
		return;
	}
	slotno2 = itemtoslot(arg2);
	if (!slotno2)
	{
		sprint(self, PRINT_HIGH, "No item\n");
		return;
	}

	old1 = ItemInSlot(self, slotno1);
	old2 = ItemInSlot(self, slotno2);
	if (old1 == old2)
		return;

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

//swap the equipment_slot over too.
	if (self.equipment_slot == slotno1)
		self.equipment_slot = slotno2;
	else if (self.equipment_slot == slotno2)
		self.equipment_slot = slotno1;

	self.rtime = time + 2;	//we don't check this here though - cost to get into inventory.
};

void(string arg1, float iid, float num) Cmd_InvGive =
{
	local float slotno;
	slotno = itemtoslot(arg1);
	if (!slotno)
	{
		sprint(self, PRINT_MEDIUM, "Can't give to that slot\n");
		return;
	}

	if (!FitsInSlot(slotno, iid))
	{
		sprint(self, PRINT_MEDIUM, "Can't give that item in that slot\n");
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
	if (cmd == "invuse")
	{
		if (self.deadflag || self.current_slot==0)
			return;
		Cmd_InvUse(argv(1));
	}
	else if (cmd == "invdrop")
	{
		if (self.deadflag || self.current_slot==0)
			return;
		Cmd_InvDrop(argv(1));
	}
	else if (cmd == "invsell")
	{
		if (self.deadflag || self.current_slot==0)
			return;
		Cmd_InvSell(argv(1));
	}
	else if (cmd == "invbuy")
	{
		if (self.deadflag || self.current_slot==0)
			return;
		Cmd_InvBuy(argv(1));
	}
	else if (cmd == "invmix")
	{
		if (self.deadflag)
			return;
		Cmd_InvMix(argv(1));
	}
	else if (cmd == "create")
	{
		if (self.deadflag)
			return;
		Cmd_InvCreate(argv(1));
	}
	else if (cmd == "invswap")
	{
		if (self.deadflag || self.current_slot==0)
			return;
		Cmd_InvSwap(argv(1), argv(2));
	}
	else if (cmd == "invgive")
	{
		if (self.deadflag || self.current_slot==0)
			return;
		Cmd_InvGive(argv(1), stof(argv(2)), stof(argv(3)));
	}
	else if (cmd == "god")
	{
		sprint(self, PRINT_HIGH, "sorry, but I'm an athiest\n");
	}
	else
		clientcommand(self, line);
};
