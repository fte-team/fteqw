
void (entity chest, float iid, float num) DropFromChest;

void(entity to, float iid, float amount) AddNonStackable =
{
	local float slot;

	slot = SlotOfItem(to, iid);
	if (!slot)
		slot = FindSuitableEmptySlot(to, iid);
	if (slot)
		SetItemSlot(to, slot, SlotVal(iid, amount));
};

void(entity to, float iid, float amount) AddStackable =
{
	local float slot;

	slot = FindSuitableEmptySlot(to, iid);
	if (slot)
		SetItemSlot(to, slot, SlotVal(iid, amount + ToStatus(ItemInSlot(to, slot))));
}

void(float cost, float iid) BuyStackable =
{
	local string z;

	if (self.ammo_shells < cost)
	{
		self.currentmenu = "none";
		sprint(self, PRINT_HIGH, "not enough money.\n");
		return;
	}

	if (!TryGiveStackable(self, iid))
	{
		sprint(self, 2, "full inventory.\n");
		return;
	}

	sound (self, CHAN_BODY, "misc/item1.wav", 1, ATTN_NORM);
	self.ammo_shells = self.ammo_shells - cost;

	sprint(self, PRINT_HIGH, "you bought a ");

	z = GetItemName(iid);
	sprint(self, PRINT_HIGH, z);
	sprint(self, PRINT_HIGH, "\n");
	self.impulse = 0;
};

void(float cost, float iid, float amount) BuyStackableMulti =
{
	local string z;

	if (self.ammo_shells < cost)
	{
		self.currentmenu = "none";
		sprint(self, PRINT_HIGH, "not enough money.\n");
		return;
	}

	if (!TryGiveStackable(self, iid))
	{
		sprint(self, 2, "full inventory.\n");
		return;
	}

	sound (self, CHAN_BODY, "misc/item1.wav", 1, ATTN_NORM);
	self.ammo_shells = self.ammo_shells - cost;

	sprint(self, PRINT_HIGH, "you bought a ");

	z = GetItemName(iid);
	sprint(self, PRINT_HIGH, z);
	sprint(self, PRINT_HIGH, "\n");

	//put grenade in inventory
	//finds existing grenades, otherwise, empty slot
};




void(string type) ChangeAmmo =
{

	if (self.ammo_shells < 1)
	{
		self.currentmenu = "none";
		sprint(self, PRINT_HIGH, "not enough money.\n");
		return;
	}

	
	sound (self, CHAN_BODY, "misc/item1.wav", 1, ATTN_NORM);

	if (self.current_slot == 1)
		self.ammotype1 = type;
	if (self.current_slot == 2)
		self.ammotype2 = type;
};

void() SetWeaponModel;

void (float wt, float cost, float wid) BuyWeapon =
{
	local string itname;
	local float ammotype, ammocount;
	local float slotnum;
	local float curweap;

	if (self.ammo_shells < cost)
	{
		self.currentmenu = "none";
		sound (self, CHAN_BODY, "misc/menu3.wav", 1, ATTN_IDLE);
		sprint(self, PRINT_HIGH, "not enough money.\n");
		return;
	}

	sound (self, CHAN_BODY, "misc/item1.wav", 1, ATTN_NORM);
	self.ammo_shells = self.ammo_shells - cost;

	sprint(self, PRINT_HIGH, "you bought ");

	itname = GetItemName(wid);
	sprint(self, PRINT_HIGH, itname);
	sprint(self, PRINT_HIGH, ".\n");

	//put new weapon in current slot
	//put old weapon in empty slot (but not armor slot!)
      //otherwise, drop it.

	curweap = ItemInSlot(self, self.current_slot);
	slotnum = FindSuitableEmptySlot(self, ToIID(curweap));

	if (slotnum == 0)//no more room
		DropFromSlot (self.current_slot, 1, 0);
	else       //found a place to stick old weapon
		SetItemSlot(self, slotnum, curweap);

	ammocount = WeaponMagQuant(wid);//load weapon with full ammo (may be changed)
	SetItemSlot(self, self.current_slot, SlotVal(wid, ammocount));


	ammotype = WeaponAmmoType(wid);//load weapon with full ammo (may be changed)
	TryGiveStackable(self, ammotype, ammocount*3);

	SetWeaponModel();
};


void (float cost, float item, float slot) BuyPerk =
{

	if (slot == 1 && self.perk2 == item)
	{
		sprint (self, PRINT_HIGH, "you already know that ability!\n");
		return;
	}
	if (slot == 2 && self.perk1 == item)
	{
		sprint (self, PRINT_HIGH, "you already know that ability!\n");
		return;
	}

	sprint (self, PRINT_HIGH, "you have learned a new ability in the wastes.\n");

	if (slot == 1)
		self.perk1 = item;
	else if (slot == 2)
		self.perk2 = item;
	self.currentmenu = "none";
	return;
};

void (float wt, float cost, float item) BuyArmor =
{
	local string z;
	local float x;

	if (self.ammo_shells < cost)
	{
		self.currentmenu = "none";
		sound (self, CHAN_BODY, "misc/menu3.wav", 1, ATTN_IDLE);
		sprint(self, PRINT_HIGH, "not enough money.\n");
		return;
	}

	sound (self, CHAN_BODY, "misc/item1.wav", 1, ATTN_NORM);
	self.ammo_shells = self.ammo_shells - cost;

	sprint(self, PRINT_HIGH, "you bought ");

	z = GetItemName(item);
	sprint(self, PRINT_HIGH, z);
	sprint(self, PRINT_HIGH, ".\n");

	//put new armor in armor slot
	//put old armor in inventory

	x = FindEmptySlot(self);

//	if (x == 0)
//		sprint(self, 2, "no more room. giving armor to merchant.\n");

//	if (x != 0) //found a place to stick old armor
//		SetItemSlot(self, x, SlotVal(self.islot3, 1));

		SetItemSlot(self, 3, SlotVal(item, 1));
};

void (float cost, float item) BuyEquipment =
{
	local float lbs;
	local string x;

	lbs = weightx ();
	if ((cost > self.ammo_shells))
	{
		sprint (self, PRINT_HIGH, "not enough money.\n");
		self.currentmenu = "none";
		return;
	}
	sound (self, CHAN_BODY, "items/item1.wav", 1, ATTN_NORM);

	x = GetItemName(item);
	SetItemSlot(self, 4, SlotVal(item, 1));

	sprint (self, PRINT_HIGH, x);
	sprint (self, PRINT_HIGH, " purchased.\n");
	self.ammo_shells = (self.ammo_shells - cost);
	return;
};

void (float cost, float item) BuyProtect =
{
	local string x;

/*
	if ((self.armor >= TE_LIGHTNING2))
	{
		sprint (self, PRINT_HIGH, "too many electronics in\nyour currently worn armor!");
		self.currentmenu = "none";
		return;
	}
*/
	if ((cost > self.ammo_shells))
	{
		sprint (self, PRINT_HIGH, "not enough money.\n");
		self.currentmenu = "none";
		return;
	}
	sound (self, CHAN_BODY, "items/item1.wav", 1, ATTN_NORM);
	self.ammo_shells = (self.ammo_shells - cost);
	self.protect = item;
	x = GetProtectName();
	sprint (self, PRINT_HIGH, x);
	sprint (self, PRINT_HIGH, " purchased.\n");

	return;
};


float (float input) overweight =
{
	local float tmp;
	local float max;

	tmp = input + self.weight;
	max = self.max_weight;

	if (tmp > max)
		return (TRUE);
	else
		return (FALSE);
};


void() W_PlayerMenu =
{
	local float xx;
	local string yy, q, x;

	if (self.currentmenu == "none")
		return;


		else if (self.currentmenu == "display_enter_screen")
		{
			if (self.impulse == 1 && coop == 1)
			{
				self.missionbrief = 0;
				self.team = 1;
				self.connected = 1;
				sound (self, CHAN_WEAPON, "player/yourturn.wav", TRUE, ATTN_NORM);
				centerprint(self, "\n");
				self.currentmenu = "none";
				PutClientInServer();
				self.impulse = 0;
				self.current_slot = 1;
				return;
			}
			else if (self.impulse == 1 && coop == 0)
			{
				self.missionbrief = 0;
				if (teamblue == teamred)
				{
					if (random()*100 <= 50)
					{
						self.team = 1;
						teamblue = teamblue + 1;
					}
					else
					{
						self.team = 2;
						teamred = teamred + 1;
					}
				}
				else if (teamblue > teamred)
				{
					self.team = 2;
					teamred = teamred + 1;
				}
				else if (teamblue < teamred)
				{
					self.team = 1;
					teamblue = teamblue + 1;
				}

				self.connected = 1;
				sound (self, CHAN_WEAPON, "player/yourturn.wav", TRUE, ATTN_NORM);
				centerprint(self, "\n");
				self.currentmenu = "none";
				PutClientInServer();
				self.impulse = 0;
				self.current_slot = 1;
				return;
			}
		}
		else if (self.currentmenu == "select_mission")
		{
			if (self.impulse >= 1 && self.impulse <= 6)
			{

					if (self.impulse == 1)
						get_new_mission(1);
					if (self.impulse == 2)
						get_new_mission(2);
					if (self.impulse == 3)
						get_new_mission(3);
					if (self.impulse == 4)
						get_new_mission(4);
					if (self.impulse == 5)
						get_new_mission(5);
					if (self.impulse == 6)
						get_new_mission(6);


				self.team = 1;
				sound (self, CHAN_WEAPON, "effects/radio2.wav", TRUE, ATTN_IDLE);
				centerprint(self, "mission obtained\n");
				self.currentmenu = "none";
				self.impulse = 0;
				return;
			}
	
			else
			{
				centerprint(self, "\n");
				self.currentmenu = "none";
				self.impulse = 0;
				return;
			}
		}
		else if (self.currentmenu == "confirm_depart")
		{
			if (self.impulse == 1)
			{
				if (infokey(world, "objective") == "return")
				{
					sprint(self, 2, "you're not on a mission!\n");
					self.currentmenu = "none";
					return;
				}
				else
				{
					changelevel(m_map);
					return;
				}
			}
			else if (self.impulse == 2)
			{
				centerprint(self, "\n");
				self.currentmenu = "none";
				self.impulse = 0;
				return;
			}
		}
		else if (self.currentmenu == "menu_lockpick")
		{
			if (self.impulse == 1)
			{
				if (self.chest.inplace == 1 && self.chest.tumbler1.inplace == 1 && self.chest.tumbler2.inplace == 1)
				{
					local entity te;
					local float greed;

					te = findradius (self.origin, 600);
					while (te)
					{
						if (te.classname == "player" && te != self)
							greed = 0;
						else
							greed = 1;

						te = te.chain;
					}
				
					if (greed == 1)
						self.score = self.score - 50;
					else
						self.score = self.score - 25;


					sound (self, CHAN_WEAPON, "effects/openlock.wav", TRUE, ATTN_NORM);
					sprint(self, 2, "you manage to pick the lock. you find:\n");
					if (random()<1)
					{
						sprint(self, 2, "‘ money (");
						xx = 10 + ceil(random()*30);
						yy = ftos(xx);
						sprint(self, 2, yy);
						sprint(self, 2, ")\n");
						self.ammo_shells = self.ammo_shells + xx;
					}
					if (random()<0.5)
					{
						xx = ceil(random()*2);
						DropFromChest(self.chest.owner, IID_CHEM_STIMPACK, xx);
						sprint(self, 2, "‘ stimpacks (");
						yy = ftos(xx);
						sprint(self, 2, yy);
						sprint(self, 2, ")\n");
					}
					if (random()<0.75)
					{
						xx = ceil(309+random()*12);
						yy = GetItemName(xx);
						sprint(self, 2, "‘ ");
						sprint(self, 2, yy);
						sprint(self, 2, "\n");
						DropFromChest(self.chest.owner, xx, 1);
						//TryGiveStackable(self, xx, 1);
					}
					if (random()<0.75)
					{
						xx = ceil(309+random()*12);
						yy = GetItemName(xx);
						sprint(self, 2, "‘ ");
						sprint(self, 2, yy);
						sprint(self, 2, "\n");
						DropFromChest(self.chest.owner, xx, 1);
						//TryGiveStackable(self, xx, 1);
					}
					if (random()<0.75)
					{
						xx = ceil(309+random()*12);
						yy = GetItemName(xx);
						sprint(self, 2, "‘ ");
						sprint(self, 2, yy);
						sprint(self, 2, "\n");
						DropFromChest(self.chest.owner, xx, 1);
						//TryGiveStackable(self, xx, 1);
					}
					if (random()<0.75)
					{
						xx = ceil(309+random()*12);
						yy = GetItemName(xx);
						sprint(self, 2, "‘ ");
						sprint(self, 2, yy);
						sprint(self, 2, "\n");
						DropFromChest(self.chest.owner, xx, 1);
						//TryGiveStackable(self, xx, 1);
					}
					if (random()<0.05)
					{
						yy = GetItemName(IID_WP_MP9);
						sprint(self, 2, "‘ ");
						sprint(self, 2, yy);
						sprint(self, 2, "\n");
						DropFromChest(self.chest.owner, IID_WP_MP9, 30);
						//AddNonStackable(self, IID_WP_MP9, 30);
					}
					if (random()<0.05)
					{
						yy = GetItemName(IID_WP_NEEDLER);
						sprint(self, 2, "‘ ");
						sprint(self, 2, yy);
						sprint(self, 2, "\n");
						DropFromChest(self.chest.owner, IID_WP_NEEDLER, 10);
						//AddNonStackable(self, IID_WP_NEEDLER, 10);
					}
					if (random()<0.50)
					{
						yy = GetItemName(IID_ARM_LEATHER);
						sprint(self, 2, "‘ ");
						sprint(self, 2, yy);
						sprint(self, 2, "\n");
						DropFromChest(self.chest.owner, IID_ARM_LEATHER, 1);
						//AddNonStackable(self, IID_ARM_LEATHER, 1);
					}
					if (random()<0.05)
					{
						yy = GetItemName(IID_WP_PIPERIFLE_SCOPE_S);
						sprint(self, 2, "‘ ");
						sprint(self, 2, yy);
						sprint(self, 2, "\n");
						DropFromChest(self.chest.owner, IID_WP_PIPERIFLE_SCOPE_S, 1);
						//AddNonStackable(self, IID_WP_PIPERIFLE_SCOPE_S, 1);
						//AddNonStackable(self, IID_AM_44MAGNUM, 12);
					}
					if (random()<0.05)
					{
						yy = GetItemName(IID_WP_USP);
						sprint(self, 2, "‘ ");
						sprint(self, 2, yy);
						sprint(self, 2, "\n");
						DropFromChest(self.chest.owner, IID_WP_USP, 12);
						//AddNonStackable(self, IID_WP_USP, 10);
						//AddNonStackable(self, IID_AM_45ACP, 12);
					}
					if (random()<0.05)
					{
						yy = GetItemName(IID_WP_WINCHESTER);
						sprint(self, 2, "‘ ");
						sprint(self, 2, yy);
						sprint(self, 2, "\n");
						DropFromChest(self.chest.owner, IID_WP_WINCHESTER, 2);
						//AddNonStackable(self, IID_WP_WINCHESTER, 2);
						//AddNonStackable(self, IID_AM_12GAUGESHELLS, 18);
					}
					if (random()<0.75)
					{
						xx = ceil(309+random()*12);
						yy = GetItemName(xx);
						sprint(self, 2, "‘ ");
						sprint(self, 2, yy);
						sprint(self, 2, "\n");
						DropFromChest(self.chest.owner, xx, 1);
						//TryGiveStackable(self, xx, 1);
					}
					if (random()<0.25)
					{
						yy = GetItemName(IID_GREN_FRAG);
						sprint(self, 2, "‘ ");
						sprint(self, 2, yy);
						sprint(self, 2, "\n");
						DropFromChest(self.chest.owner, IID_GREN_FRAG, 1);
						//TryGiveStackable(self, IID_GREN_FRAG, 1);
					}
					self.picking = 0;
					self.chest.owner.picking = 2;
					self.chest.owner.frame = 1;
					self.currentmenu = "none";
					remove(self.chest.tumbler1);
					remove(self.chest.tumbler2);
					remove(self.chest);
					return;
				}
				else
				{
					if (random()<0.90)
					{
						sprint(self, 2, "failed to pick the lock.\n");
						self.chest.owner.picking = 0;
					}
					else if (ToIID(self.islot4) == IID_EQUIP_TOOLKIT)
					{
						sprint(self, 2, "failed, almost jammed but super toolkit prevented.\n");
						self.chest.owner.picking = 0;
					}
					else
					{
						sprint(self, 2, "failed. lock is jammed.\n");
						self.chest.owner.picking = 3;
					}
					self.picking = 0;
					self.currentmenu = "none";
					remove(self.chest.tumbler1);
					remove(self.chest.tumbler2);
					remove(self.chest);
					return;
				}
			}
			else if (self.impulse == 2)
			{
				self.picking = 0;
				self.chest.owner.picking = 0;
				remove(self.chest.tumbler1);
				remove(self.chest.tumbler2);
				remove(self.chest);
				self.currentmenu = "none";
				self.impulse = 0;
				return;
			}
		}
		else if (self.currentmenu == "menu_defuse")
		{
			if (self.impulse == 1)
			{
				if (self.chest.inplace == 1 && self.chest.tumbler1.inplace == 1 && self.chest.tumbler2.inplace == 1)
				{

					if (self.chest.owner.rtime <= 0)
					{
						sound (self, CHAN_WEAPON, "effects/beep1.wav", TRUE, ATTN_NORM);
						sprint(self, 2, "you manage to disarm the bomb\n");

						self.score = self.score + 75;
						spawn_excla(self.chest.owner, 3000);
						remove(self.chest.owner);
						self.picking = 0;
						self.chest.owner.picking = 2;
						self.chest.owner.frame = 1;
						self.currentmenu = "none";
						remove(self.chest.tumbler1);
						remove(self.chest.tumbler2);
						remove(self.chest);
						return;
					}
					else
					{
						sprint(self, 2, "almost got it... ");
						self.chest.owner.rtime = self.chest.owner.rtime - 1;
						x = ftos(self.chest.owner.rtime);
						sprint(self, 2, x);
						sprint(self, 2, " circuits left\n");

						return;
					}
				}
				else
				{
					if (random()<0.75)
					{
						sprint(self, 2, "oops! slipped...\n");
						self.chest.owner.picking = 0;
					}
					else if (ToIID(self.islot4) == IID_EQUIP_TOOLKIT)
					{
						sprint(self, 2, "failed, almost jammed but super toolkit prevented.\n");
						self.chest.owner.picking = 0;
					}
					else
					{
						sprint(self, 2, "oops! circuits have been shorted. ");
						self.chest.owner.rtime = self.chest.owner.rtime + 1;
						self.chest.owner.picking = 0;
						x = ftos(self.chest.owner.rtime);
						sprint(self, 2, x);
						sprint(self, 2, " circuits left\n");
					}
					self.picking = 0;
					self.currentmenu = "none";
					remove(self.chest.tumbler1);
					remove(self.chest.tumbler2);
					remove(self.chest);
					return;
				}
			}
			else if (self.impulse == 2)
			{
				self.picking = 0;
				self.chest.owner.picking = 0;
				remove(self.chest.tumbler1);
				remove(self.chest.tumbler2);
				remove(self.chest);
				self.currentmenu = "none";
				self.impulse = 0;
				return;
			}
		}
};