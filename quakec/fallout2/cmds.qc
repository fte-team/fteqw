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

	if (iid == IID_CHEM_RADX)
	{
		UseRadX();
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
	    iid == IID_CHEM_BESERK)
	{
		if (UseBoostingChem(iid))
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

void(string arg1) Cmd_GainLevel =
{
	if (self.score < self.tnl)
	{
		sprint(self, 2, "not enough experience points.\n");
		sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
		return;
	}
	if (self.score >= self.tnl)
	{
		sound (self, CHAN_BODY, "buttons/switch02.wav", 1, ATTN_NORM);
		self.score = self.score - self.tnl;
	}	

	if (arg1 == "combat")
		self.skill_combat = self.skill_combat + 1;
	else if (arg1 == "doctor")
		self.skill_doctor = self.skill_doctor + 1;
	else if (arg1 == "sneak")
		self.skill_sneak = self.skill_sneak + 1;
	else if (arg1 == "science")
		self.skill_science = self.skill_science + 1;
	else if (arg1 == "speech")
		self.skill_speech = self.skill_speech + 1;

	self.max_health = 80 + (self.skill_combat*4) + (self.skill_doctor*2) + (self.skill_sneak*2) + (self.skill_speech*2) + (self.skill_science);
};

void(string arg1, string arg2) Cmd_GainPerk =
{

	sound (self, CHAN_BODY, "buttons/switch02.wav", 1, ATTN_NORM);
	

	if (arg2 == "1")
	{
		if (arg1 == "1")
			self.perk1 = 1;
		if (arg1 == "2")
			self.perk1 = 2;
		if (arg1 == "3")
			self.perk1 = 3;
		if (arg1 == "4")
			self.perk1 = 4;
		if (arg1 == "5")
			self.perk1 = 5;
		if (arg1 == "6")
			self.perk1 = 6;
		if (arg1 == "7")
			self.perk1 = 7;
		if (arg1 == "8")
			self.perk1 = 8;
		if (arg1 == "9")
			self.perk1 = 9;
		if (arg1 == "10")
			self.perk1 = 10;
		if (arg1 == "11")
			self.perk1 = 11;
		if (arg1 == "12")
			self.perk1 = 12;
		if (arg1 == "14")
			self.perk1 = 14;
		if (arg1 == "15")
			self.perk1 = 15;

	}
	if (arg2 == "2")
	{
		if (arg1 == "1")
			self.perk2 = 1;
		if (arg1 == "2")
			self.perk2 = 2;
		if (arg1 == "3")
			self.perk2 = 3;
		if (arg1 == "4")
			self.perk2 = 4;
		if (arg1 == "5")
			self.perk2 = 5;
		if (arg1 == "6")
			self.perk2 = 6;
		if (arg1 == "7")
			self.perk2 = 7;
		if (arg1 == "8")
			self.perk2 = 8;
		if (arg1 == "9")
			self.perk2 = 9;
		if (arg1 == "10")
			self.perk2 = 10;
		if (arg1 == "11")
			self.perk2 = 11;
		if (arg1 == "12")
			self.perk2 = 12;
		if (arg1 == "14")
			self.perk2 = 14;
		if (arg1 == "15")
			self.perk2 = 15;
	}
};


void(string arg1) Cmd_InvBuy =
{
	local float x,y, iid, amount, money, item, m, n, p;


	x = FindEmptySlot(self);

	if (x == 0)
	{
		sprint(self, 2, "no more room in inventory!\n");
		sound (self, CHAN_BODY, "misc/beep2.wav", 1, ATTN_NORM);
		return;
	}

	if (arg1 == "1")
		item = self.xslot1;
	if (arg1 == "2")
		item = self.xslot2;
	if (arg1 == "3")
		item = self.xslot3;
	if (arg1 == "4")
		item = self.xslot4;
	if (arg1 == "5")
		item = self.xslot5;
	if (arg1 == "6")
		item = self.xslot6;
	if (arg1 == "7")
		item = self.xslot7;
	if (arg1 == "8")
		item = self.xslot8;
	if (arg1 == "9")
		item = self.xslot9;
	if (arg1 == "10")
		item = self.xslot10;
	if (arg1 == "11")
		item = self.xslot11;
	if (arg1 == "12")
		item = self.xslot12;
	if (arg1 == "13")
		item = self.xslot13;
	if (arg1 == "14")
		item = self.xslot14;
	if (arg1 == "15")
		item = self.xslot15;
	if (arg1 == "16")
		item = self.xslot16;
	if (arg1 == "17")
		item = self.xslot17;
	if (arg1 == "18")
		item = self.xslot18;
	if (arg1 == "19")
		item = self.xslot19;
	if (arg1 == "20")
		item = self.xslot20;


		iid = ToIID(item);
		amount = ToStatus(item);
		money = GetBaseValue(iid);
		if (self.supertime <= 0)
			money = ceil(money * (1 - (self.skill_speech/30)));
		else
		{
			sprint(self, 2, "We don't serve your kind here!\n");
			sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
			return;
		}

		m = SlotOfItem(self, IID_MONEY);
		p = ItemInSlot(self, m);
		n = ToStatus(p);

		if (m == 0)
		{
			sprint(self, 2, "not enough money.\n");
			sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
			return;
		}
		if (n < money)
		{
			sprint(self, 2, "not enough money\n");
			sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
			return;
		}
		else
		{
			SetItemSlot(self, m, 0);
			TryGiveStackable(self, IID_MONEY, n-money);
		}

		if (NotStackable(iid))
			SetItemSlot(self, x, item);
		else if (IsEquip(iid))
			SetItemSlot(self, x, item);
		else
			if (!TryGiveStackable(self, iid, amount))
			{
				sprint(self, 2, "full inventory.\n");
				sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
				m = SlotOfItem(self, IID_MONEY);
				SetItemSlot(self, m, 0);
				TryGiveStackable(self, IID_MONEY, n);
				return;
			}
		sound (self, CHAN_BODY, "misc/item2.wav", 1, ATTN_NORM);


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


	x = GetBaseValue(iid);
	x = ceil(x * (1 + (self.skill_speech/30)));
	x = ceil(x / 5);
	y = ftos(x);
	z = GetItemName(iid);

	if (!TryGiveStackable(self, IID_MONEY, x))
	{
		sprint(self, 2, "not enough room.\n");
		return;
	}

	SetItemSlot(self, slotno, 0);



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

		if (random()*100 > (self.skill_science*20))
		{
			sprint(self, PRINT_MEDIUM, "your concoction exploded!\n");
			Explosion();
			return;
		}

		if (random()*100 < ((self.skill_science*20) - 100))
		{
			sprint(self, PRINT_MEDIUM, "3 nuka bombs created!\n");
			sound (self, CHAN_BODY, "misc/yourturn.wav", 1, ATTN_NORM);
			TryGiveStackable(self, IID_GREN_FRAG, 3);
			return;
		}
		TryGiveStackable(self, IID_GREN_FRAG, 1);
		sprint(self, PRINT_MEDIUM, "1 nuka bomb created\n");
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
	else if ((iid1 == IID_MISC_DUCKTAPE && iid2 == IID_MISC_AEROSOL) ||
            (iid2 == IID_MISC_DUCKTAPE && iid1 == IID_MISC_AEROSOL))
	{
		x = FindEmptySlot(self);

		if (x == 0)
		{
			sprint(self, 2, "no more room in inventory!\n");
			sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
			return;
		}

		DecreaseDestroySlot(23);
		DecreaseDestroySlot(24);

		if (random()*100 > (self.skill_science*20))
		{
			sprint(self, PRINT_MEDIUM, "your concoction failed!\n");
			sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
			return;
		}
	



		SetItemSlot(self, x, SlotVal(IID_EQUIP_SILENCER, 1));

		sprint(self, PRINT_MEDIUM, "silencer created\n");
		sound (self, CHAN_BODY, "misc/item1.wav", 1, ATTN_NORM);
	}
	else if ((iid1 == IID_EQUIP_SILENCER && iid2 == IID_WP_USP) ||
            (iid2 == IID_EQUIP_SILENCER && iid1 == IID_WP_USP))
	{
		if (random()*100 > (self.skill_science*20))
		{
			sprint(self, PRINT_MEDIUM, "your concoction failed!\n");
			sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
			return;
		}
		x = FindEmptySlot(self);

		if (x == 0)
		{
			sprint(self, 2, "no more room in inventory!\n");
			sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
			return;
		}

		DecreaseDestroySlot(23);
		DecreaseDestroySlot(24);

		SetItemSlot(self, x, SlotVal(IID_WP_USP_S, 1));

		sprint(self, PRINT_MEDIUM, "silencer created\n");
		sound (self, CHAN_BODY, "misc/item1.wav", 1, ATTN_NORM);
	}
	else if ((iid1 == IID_EQUIP_SILENCER && iid2 == IID_WP_GLOCK) ||
            (iid2 == IID_EQUIP_SILENCER && iid1 == IID_WP_GLOCK))
	{
		if (random()*100 > (self.skill_science*20))
		{
			sprint(self, PRINT_MEDIUM, "your concoction failed!\n");
			sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
			return;
		}
		x = FindEmptySlot(self);

		if (x == 0)
		{
			sprint(self, 2, "no more room in inventory!\n");
			sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
			return;
		}

		DecreaseDestroySlot(23);
		DecreaseDestroySlot(24);

		SetItemSlot(self, x, SlotVal(IID_WP_GLOCK_S, 1));

		sprint(self, PRINT_MEDIUM, "silencer created\n");
		sound (self, CHAN_BODY, "misc/item1.wav", 1, ATTN_NORM);
	}
	else if ((iid1 == IID_ARM_VANDAL && iid2 == IID_MISC_JUNK) ||
            (iid2 == IID_ARM_VANDAL && iid1 == IID_MISC_JUNK))
	{



		if (random()*100 > (self.skill_science*20))
		{
			sprint(self, PRINT_MEDIUM, "your concoction failed.\n");
			return;
		}
		
		x = FindEmptySlot(self);

		if (x == 0)
		{
			sprint(self, 2, "no more room in inventory!\n");
			sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
			return;
		}

		DecreaseDestroySlot(23);
		DecreaseDestroySlot(24);

		SetItemSlot(self, x, SlotVal(IID_ARM_WASTELAND, 1));
		sprint(self, PRINT_MEDIUM, "Wasteland Jacket created\n");
		sound (self, CHAN_BODY, "misc/item1.wav", 1, ATTN_NORM);
	}
	else
	{
		sprint(self, PRINT_MEDIUM, "cannot merge items!\n");
		sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
		return;
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
	else if (cmd == "gainlevel")
	{
		if (self.deadflag || self.current_slot==0)
			return;
		Cmd_GainLevel(argv(1));
	}
	else if (cmd == "invmix")
	{
		if (self.deadflag)
			return;
		Cmd_InvMix(argv(1));
	}
	else if (cmd == "invswap")
	{
		if (self.deadflag || self.current_slot==0)
			return;
		Cmd_InvSwap(argv(1), argv(2));
	}
	else if (cmd == "gainperk")
	{
		if (self.deadflag || self.current_slot==0)
			return;
		Cmd_GainPerk(argv(1), argv(2));
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
