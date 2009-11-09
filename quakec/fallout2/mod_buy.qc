
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

void() BuyBandages =
{
	local string y;

	if (self.ammo_shells < 2)
	{
		self.currentmenu = "none";
		sprint(self, PRINT_HIGH, "not enough money.\n");
		return;
	}
	if (self.class != 2)
	{
		self.currentmenu = "none";
		sprint(self, PRINT_HIGH, "not a medic.\n");
		return;
	}
	if (self.bandages >= 50)
	{
		self.currentmenu = "none";
		sprint(self, PRINT_HIGH, "too many bandages.\n");
		return;
	}

	sound (self, CHAN_BODY, "misc/item1.wav", 1, ATTN_NORM);
	self.bandages = self.bandages + 25;
	self.ammo_shells = self.ammo_shells - 2;
	if (self.bandages > 50)
		self.bandages = 50;
	y = ftos(self.bandages);
	sprint(self, PRINT_HIGH, "you now have ");
	sprint(self, PRINT_HIGH, y);
	sprint(self, PRINT_HIGH, "/50 bandages.\n");	
};

void(float cost, float type, float count) BuyChem =
{
	local string chemname;
	local float max;
	local float alreadygot;
	local float typex;

	if (self.ammo_shells < cost)
	{
		self.currentmenu = "none";
		sprint(self, PRINT_HIGH, "not enough money.\n");
		return;
	}

	//only medics and engineers can buy superstims
	if (type == IID_CHEM_SUPERSTIM && self.class != 1 && self.class != 4)
	{
		self.currentmenu = "none";
		sprint(self, PRINT_HIGH, "you'll need a higher first aid skill\n");
		sprint(self, PRINT_HIGH, "for authorization to purchase those.\n");
		return;
	}

	//only medics and engineers can buy adrenaline
	if (type == IID_CHEM_ADRENALINE && self.class != 1 && self.class != 4)
	{
		self.currentmenu = "none";
		sprint(self, PRINT_HIGH, "you'll need a higher first aid skill\n");
		sprint(self, PRINT_HIGH, "for authorization to purchase those.\n");
		return;
	}

	//only medics can buy psycho
	if (type == IID_CHEM_PSYCHO && self.class != 1)
	{
		self.currentmenu = "none";
		sprint(self, PRINT_HIGH, "you'll need a higher first aid skill\n");
		sprint(self, PRINT_HIGH, "for authorization to purchase those.\n");
		return;
	}

	//only medics can buy berserk
	if (type == IID_CHEM_BESERK && self.class != 1)
	{
		self.currentmenu = "none";
		sprint(self, PRINT_HIGH, "you'll need a higher first aid skill\n");
		sprint(self, PRINT_HIGH, "for authorization to purchase those.\n");
		return;
	}

	//only medics can buy the doctor bag
	if (type == IID_CHEM_MEDICALBAG && self.class != 1)
	{
		self.currentmenu = "none";
		sprint(self, PRINT_HIGH, "you will need a higher doctor skill\n");
		sprint(self, PRINT_HIGH, "for authorization to purchase those.\n");
		return;
	}

	if (type == IID_CHEM_STIMPACK)
		max = 4;
	if (type == IID_CHEM_SUPERSTIM)
		max = 1;
	if (type == IID_CHEM_MEDICALBAG)
		max = 50;
	if (type == IID_CHEM_ADRENALINE)
		max = 25;
	if (type == IID_CHEM_PSYCHO)
		max = 25;
	if (type == IID_CHEM_BESERK)
		max = 25;
	if (type == IID_CHEM_RADX)
		max = 5;

	if (self.class == 1)
		max = max + 1;

	if (ToIID(self.islot4) == IID_EQUIP_MEDIC_BAG)
		max = max * 2;

	alreadygot = TotalQuantity(self, type);
	if (alreadygot >= max)
	{
		self.currentmenu = "none";
		sprint(self, PRINT_HIGH, "you can't hold any more of those chems.\n");
		return;
	}

	//can only carry two types of chem
	typex = 0;
	if (SlotOfItem(self, IID_CHEM_STIMPACK) > 0)
		typex = typex + 1;
	if (SlotOfItem(self, IID_CHEM_SUPERSTIM) > 0)
		typex = typex + 1;
	if (SlotOfItem(self, IID_CHEM_MEDICALBAG) > 0)
		typex = typex + 1;
	if (SlotOfItem(self, IID_CHEM_ADRENALINE) > 0)
		typex = typex + 1;
	if (SlotOfItem(self, IID_CHEM_PSYCHO) > 0)
		typex = typex + 1;
	if (SlotOfItem(self, IID_CHEM_BESERK) > 0)
		typex = typex + 1;
	if (SlotOfItem(self, IID_CHEM_RADX) > 0)
		typex = typex + 1;

	max = 2;

	if (self.class == 1)
		max = 4;
	if (ToIID(self.islot4) == IID_EQUIP_MEDIC_BAG)
		max = 4;

	if (typex >= max)
	{
		self.currentmenu = "none";
		sprint(self, PRINT_HIGH, "only authorized to carry ");
		if (max == 2)
			sprint(self, PRINT_HIGH, "two types of chems (non-medic)\n");
		else
			sprint(self, PRINT_HIGH, "four types of chems (medic)\n");

		return;
	}

	if (!TryGiveStackable(self, type, count))
	{
		sprint(self, 2, "full inventory.\n");
		return;
	}

	sound (self, CHAN_BODY, "misc/item1.wav", 1, ATTN_NORM);
	self.ammo_shells = self.ammo_shells - cost;
	chemname = GetItemName(type);
	sprint(self, PRINT_HIGH, chemname);
	sprint(self, PRINT_HIGH, " purchased.\n");
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
	else if (self.currentmenu == "shop_list")
	{
		if (self.impulse == 1)
			self.currentmenu = "shop_perk1";
		if (self.impulse == 2)
			self.currentmenu = "shop_perk2";
		if (self.impulse == 3)
			self.currentmenu = "shop_armor";
		if (self.impulse == 4)
			self.currentmenu = "shop_ammo";
		if (self.impulse == 5)
			self.currentmenu = "shop_weapons";
		if (self.impulse == 6)
			self.currentmenu = "shop_equipment";
		if (self.impulse == 7)
			self.currentmenu = "shop_chems";
		if (self.impulse == 8)
			self.currentmenu = "shop_other";

		DisplayMenu();
		return;
	}
	else if (self.currentmenu == "shop_perk1")
	{
		if (self.impulse == 1)
                  BuyPerk(1, 1, 1);
		if (self.impulse == 2)
                  BuyPerk(1, 2, 1);
		if (self.impulse == 3)
                  BuyPerk(1, 3, 1);
		if (self.impulse == 4)
                  BuyPerk(2, 4, 1);
		if (self.impulse == 5)
                  BuyPerk(2, 5, 1);
		if (self.impulse == 6)
                  BuyPerk(2, 6, 1);
		if (self.impulse == 7)
                  BuyPerk(2, 7, 1);
		if (self.impulse == 8)
                  BuyPerk(3, 8, 1);
		if (self.impulse == 9)
                  BuyPerk(4, 9, 1);
		if (self.impulse == 10)
                  BuyPerk(4, 10, 1);
		if (self.impulse == 212)
                  BuyPerk(4, 11, 1);
		if (self.impulse == 213)
                  BuyPerk(4, 12, 1);
		if (self.impulse == 214)
                  BuyPerk(4, 13, 1);
		if (self.impulse == 215)
                  BuyPerk(4, 14, 1);
		if (self.impulse == 216)
                  BuyPerk(4, 15, 1);
		if (self.impulse == 217)
                  BuyPerk(4, 16, 1);
		if (self.impulse == 218)
                  BuyPerk(4, 17, 1);
		if (self.impulse == 219)
                  BuyPerk(4, 18, 1);
		if (self.impulse == 220)
                  BuyPerk(4, 19, 1);


			return;
	}
	if (self.currentmenu == "shop_perk2")
	{
		if (self.impulse == 1)
                  BuyPerk(1, 1, 2);
		if (self.impulse == 2)
                  BuyPerk(1, 2, 2);
		if (self.impulse == 3)
                  BuyPerk(1, 3, 2);
		if (self.impulse == 4)
                  BuyPerk(2, 4, 2);
		if (self.impulse == 5)
                  BuyPerk(2, 5, 2);
		if (self.impulse == 6)
                  BuyPerk(2, 6, 2);
		if (self.impulse == 7)
                  BuyPerk(2, 7, 2);
		if (self.impulse == 8)
                  BuyPerk(3, 8, 2);
		if (self.impulse == 9)
                  BuyPerk(4, 9, 2);
		if (self.impulse == 10)
                  BuyPerk(4, 10, 2);
		if (self.impulse == 212)
                  BuyPerk(4, 11, 2);
		if (self.impulse == 213)
                  BuyPerk(4, 12, 2);
		if (self.impulse == 214)
                  BuyPerk(4, 13, 2);
		if (self.impulse == 215)
                  BuyPerk(4, 14, 2);
		if (self.impulse == 216)
                  BuyPerk(4, 15, 2);
		if (self.impulse == 217)
                  BuyPerk(4, 16, 2);
		if (self.impulse == 218)
                  BuyPerk(4, 17, 2);
		if (self.impulse == 219)
                  BuyPerk(4, 18, 2);
		if (self.impulse == 220)
                  BuyPerk(4, 19, 2);

			return;
	}

	else if (self.currentmenu == "shop_ammo" && world.map_obj != 4)
	{
		if (self.impulse == 1)
			BuyStackableMulti(15,IID_AM_10MM,50);
		if (self.impulse == 2)
			BuyStackableMulti(25,IID_AM_12GAUGESHELLS,50);
		if (self.impulse == 3)
			BuyStackableMulti(30,IID_AM_44MAGNUM,50);
		if (self.impulse == 4)
			BuyStackableMulti(20,IID_AM_45ACP,50);
		if (self.impulse == 5)
			BuyStackableMulti(30,IID_AM_556MM,50);
		if (self.impulse == 6)
			BuyStackableMulti(25,IID_AM_5MMHIGHVEL,50);
		if (self.impulse == 7)
			BuyStackableMulti(40,IID_AM_762MM,50);
		if (self.impulse == 8)
			BuyStackableMulti(35,IID_AM_NEEDLER,50);
		if (self.impulse == 9)
			BuyStackableMulti(40,IID_AM_CASELESS, 50);
		if (self.impulse == 10)
			BuyStackableMulti(55,IID_AM_ENERGYCELL,50);

		if (random() < 0.5)
			sound (self, CHAN_BODY, "misc/item1.wav", 1, ATTN_NORM);
		else
			sound (self, CHAN_BODY, "misc/item2.wav", 1, ATTN_NORM);

			return;
	}
	else if (self.currentmenu == "shop_ammo" && world.map_obj == 4)
	{
		if (self.impulse == 1)
			BuyStackableMulti(5,IID_AM_10MM,50);
		if (self.impulse == 2)
			BuyStackableMulti(8,IID_AM_12GAUGESHELLS,50);
		if (self.impulse == 3)
			BuyStackableMulti(10,IID_AM_44MAGNUM,50);
		if (self.impulse == 4)
			BuyStackableMulti(7,IID_AM_45ACP,50);
		if (self.impulse == 5)
			BuyStackableMulti(10,IID_AM_556MM,50);
		if (self.impulse == 6)
			BuyStackableMulti(8,IID_AM_5MMHIGHVEL,50);
		if (self.impulse == 7)
			BuyStackableMulti(15,IID_AM_762MM,50);
		if (self.impulse == 8)
			BuyStackableMulti(10,IID_AM_NEEDLER,50);
		if (self.impulse == 9)
			BuyStackableMulti(20,IID_AM_CASELESS, 50);
		if (self.impulse == 10)
			BuyStackableMulti(15,IID_AM_ENERGYCELL,50);

		if (random() < 0.5)
			sound (self, CHAN_BODY, "misc/item1.wav", 1, ATTN_NORM);
		else
			sound (self, CHAN_BODY, "misc/item2.wav", 1, ATTN_NORM);

			return;
	}
	else if (self.currentmenu == "shop_weapons")
	{
		if (self.impulse == 1)
			self.currentmenu = "shop_melee";
		if (self.impulse == 2)
			self.currentmenu = "shop_thrown";
		if (self.impulse == 3)
			self.currentmenu = "shop_pistols";
		if (self.impulse == 4)
			self.currentmenu = "shop_shotguns";
		if (self.impulse == 5)
			self.currentmenu = "shop_rifles";
		if (self.impulse == 6)
			self.currentmenu = "shop_heavy";

		DisplayMenu();
		return;
	}
	else if (self.currentmenu == "shop_melee")
	{
		if (self.impulse == 1)
                  BuyWeapon(1, 6, IID_WP_KNIFE);         //weight, cost, item
		if (self.impulse == 2)
                  BuyWeapon(3, 8, IID_WP_WRENCH);         //weight, cost, item
		if (self.impulse == 3)
                  BuyWeapon(8, 9, IID_WP_SLEDGE);         //weight, cost, item
		if (self.impulse == 4)
                  BuyWeapon(4, 12, IID_WP_SPEAR);         //weight, cost, item

			return;
	}
	else if (self.currentmenu == "shop_thrown")
	{
		if (self.impulse == 1)
                  BuyStackableMulti(8, IID_GREN_FLASH, 1);         //weight, cost, item
		if (self.impulse == 2)
                  BuyStackableMulti(12, IID_GREN_FRAG, 1);         //weight, cost, item
		if (self.impulse == 3)
                  BuyStackableMulti(1, IID_GREN_FLARE, 1);         //weight, cost, item
		if (self.impulse == 4)
                  BuyStackableMulti(2, IID_MISC_HMXCOMPOUND, 1);   //weight, cost, item
		if (self.impulse == 5)
                  BuyStackableMulti(2, IID_MISC_RDXCRYSTAL, 1);    //weight, cost, item
		if (self.impulse == 6)
                  BuyStackableMulti(10, IID_AM_ROCKET, 1);         //weight, cost, item

			return;
	}
	else if (self.currentmenu == "shop_pistols")
	{
		if (self.impulse == 1)
                  BuyWeapon(1, 50, IID_WP_USP);         //weight, cost, item
		if (self.impulse == 2)
                  BuyWeapon(2, 70, IID_WP_DEAGLE);         //weight, cost, item
		if (self.impulse == 3)
                  BuyWeapon(1, 40, IID_WP_GLOCK);         //weight, cost, item
		if (self.impulse == 4)
                  BuyWeapon(2, 120, IID_WP_NEEDLER);         //weight, cost, item
		if (self.impulse == 5)
                  BuyWeapon(3, 170, IID_WP_MP9);         //weight, cost, item
		if (self.impulse == 6)
                  BuyWeapon(3, 150, IID_WP_GREASEGUN);         //weight, cost, item

			return;
	}
	else if (self.currentmenu == "shop_shotguns")
	{
		if (self.impulse == 1)
                  BuyWeapon(3, 15, IID_WP_PIPERIFLE);         //weight, cost, item
		if (self.impulse == 2)
                  BuyWeapon(4, 60, IID_WP_WINCHESTER);         //weight, cost, item
		if (self.impulse == 3)
                  BuyWeapon(5, 240, IID_WP_MOSSBERG);         //weight, cost, item
		if (self.impulse == 4)
                  BuyWeapon(7, 350, IID_WP_JACKHAMMER);         //weight, cost, item

			return;
	}
	else if (self.currentmenu == "shop_rifles")
	{
		if (self.impulse == 1)
                  BuyWeapon(3, 110, IID_WP_RANGEMASTER);    //weight, cost, item
		if (self.impulse == 2)
                  BuyWeapon(4, 180, IID_WP_AK112);          //weight, cost, item
		if (self.impulse == 3)
                  BuyWeapon(8, 190, IID_WP_FNFAL);          //weight, cost, item
		if (self.impulse == 4)
                  BuyWeapon(9, 270, IID_WP_DKS1);           //weight, cost, item
		if (self.impulse == 5)
                  BuyWeapon(5, 280, IID_WP_MOONLIGHT);      //weight, cost, item
		if (self.impulse == 6)
                  BuyWeapon(6, 220, IID_WP_G11);           //weight, cost, item

			return;
	}
	else if (self.currentmenu == "shop_heavy")
	{
		if (self.impulse == 1)
                  BuyWeapon(10, 650, IID_WP_ROCKETLAUNCHER);   //weight, cost, item
		if (self.impulse == 2)
                  BuyWeapon(7, 550, IID_WP_GAUSERIFLE);        //weight, cost, item
		if (self.impulse == 3)
                  BuyWeapon(2, 250, IID_WP_ALIENBLASTER);      //weight, cost, item
		if (self.impulse == 4)
                  BuyWeapon(11, 450, IID_WP_PULSERIFLE);       //weight, cost, item
		if (self.impulse == 5)
                  BuyWeapon(8, 550, IID_WP_PLASMACARBINE);    //weight, cost, item
		if (self.impulse == 6)
                  BuyWeapon(25, 650, IID_WP_LASERGATLING);    //weight, cost, item
	}
	else if (self.currentmenu == "shop_equipment")
	{
		if (self.impulse == 1)
                  BuyEquipment(5, IID_EQUIP_MEDIC_BAG);         //cost, item
		if (self.impulse == 2)
                  BuyEquipment(5, IID_EQUIP_GOGGLES);         //cost, item
		if (self.impulse == 3)
                  BuyEquipment(5, IID_EQUIP_STEALTHBOY);         //cost, item
		if (self.impulse == 4)
                  BuyEquipment(5, IID_EQUIP_BELTPOUCH);         //cost, item
		if (self.impulse == 5)
                  BuyEquipment(5, IID_EQUIP_BACKPACK);         //cost, item
		if (self.impulse == 6)
                  BuyEquipment(5, IID_EQUIP_TOOLKIT);         //cost, item
		if (self.impulse == 7)
                  BuyEquipment(5, IID_EQUIP_CLIMBINGGEAR);         //cost, item
		if (self.impulse == 8)
                  BuyEquipment(5, IID_EQUIP_BATTERY);         //cost, item

			return;
	}
	else if (self.currentmenu == "shop_chems")
	{
		if (self.impulse == 1)
                  BuyChem(5, IID_CHEM_STIMPACK, 1);         //cost, item
		if (self.impulse == 2)
                  BuyChem(15, IID_CHEM_RADX, 5);            //cost, item
		if (self.impulse == 3)
                  BuyChem(15, IID_CHEM_ADRENALINE, 25);     //cost, item
		if (self.impulse == 4)
                  BuyChem(25, IID_CHEM_MEDICALBAG, 100);    //cost, item
		if (self.impulse == 5)
                  BuyChem(25, IID_CHEM_SUPERSTIM, 1);       //cost, item
		if (self.impulse == 6)
                  BuyChem(25, IID_CHEM_PSYCHO, 25);         //cost, item
		if (self.impulse == 7)
                  BuyChem(25, IID_CHEM_BESERK, 25);         //cost, item

		return;
	}
	else if (self.currentmenu == "shop_other")
	{
		if (self.impulse == 1)
                  BuyStackableMulti(1, IID_MISC_JUNK, 1);         //weight, cost, item
		if (self.impulse == 2)
                  BuyStackableMulti(1, IID_MISC_NUKACOLA, 1);         //weight, cost, item
		if (self.impulse == 3)
                  BuyStackableMulti(2, IID_MISC_CHEMICALS, 1);         //weight, cost, item
		if (self.impulse == 4)
                  BuyStackableMulti(2, IID_MISC_AEROSOL, 1);         //weight, cost, item
		if (self.impulse == 5)
                  BuyStackableMulti(5, IID_MISC_CIRCUITBOARD, 1);         //weight, cost, item
		if (self.impulse == 6)
                  BuyStackableMulti(3, IID_MISC_STEELPIPE, 1);         //weight, cost, item
		if (self.impulse == 7)
                  BuyStackableMulti(1, IID_MISC_DUCKTAPE, 1);         //weight, cost, item
		if (self.impulse == 8)
                  BuyStackableMulti(1, IID_MISC_GUM, 1);         //weight, cost, item
		if (self.impulse == 9)
                  BuyStackableMulti(1, IID_MISC_COPPERWIRE, 1);         //weight, cost, item
		if (self.impulse == 10)
                  BuyStackableMulti(7, IID_MISC_XRAYTUBE, 1);         //weight, cost, item

		return;
	}
	else if (self.currentmenu == "select_team")
	{
		if (self.impulse == 1)
		{
			sound (self, CHAN_WEAPON, "player/yourturn.wav", TRUE, ATTN_NORM);
			bprint(2, self.netname);
			bprint(2, " has joined the rangers.\n");
			self.currentmenu = "confirm_team";
			DisplayMenu();
			self.team = 1;
			return;
		}
		if (self.impulse == 2)
		{
			sound (self, CHAN_WEAPON, "player/yourturn.wav", TRUE, ATTN_NORM);
			bprint(2, self.netname);
			bprint(2, " has joined the raiders.\n");
			self.currentmenu = "confirm_team";
			DisplayMenu();
			self.team = 2;
			return;
		}

	}
	else if (self.currentmenu == "gain_skill")
	{
		if (self.impulse == 1)
		{
			self.missionbrief = 2;
			sound (self, CHAN_WEAPON, "player/yourturn.wav", TRUE, ATTN_NORM);
			self.currentmenu = "none";
			self.tclass = 1;
			self.currentmenu = "confirm_skill";
			self.ghost = 0;
			return;
		}
		if ((self.impulse == 2))
		{
			self.missionbrief = 2;
			sound (self, CHAN_WEAPON, "player/yourturn.wav", TRUE, ATTN_NORM);
			self.currentmenu = "none";
			self.tclass = 2;
			self.currentmenu = "confirm_skill";
			self.ghost = 0;
			return;
		}
		if (self.impulse == 3)
		{
			self.missionbrief = 2;
			sound (self, CHAN_WEAPON, "player/yourturn.wav", TRUE, ATTN_NORM);
			self.currentmenu = "none";
			self.tclass = 3;
			self.currentmenu = "confirm_skill";
			self.ghost = 0;
			return;
		}
		if (self.impulse == 4)
		{
			self.missionbrief = 2;
			sound (self, CHAN_WEAPON, "player/yourturn.wav", TRUE, ATTN_NORM);
			self.currentmenu = "none";
			self.tclass = 4;
			self.currentmenu = "confirm_skill";
			self.ghost = 0;
			return;
		}
	}
		else if (self.currentmenu == "confirm_team")
		{
			if (self.impulse == 1)
			{
				sound (self, CHAN_WEAPON, "player/yourturn.wav", TRUE, ATTN_NORM);
				self.currentmenu = "select_skill";
				DisplayMenu();
				self.impulse = 0;
				return;
			}
			if (self.impulse == 2)
			{
				sound (self, CHAN_WEAPON, "player/yourturn.wav", TRUE, ATTN_NORM);
				self.currentmenu = "select_team";
				DisplayMenu();
				self.impulse = 0;
				self.team = 0;
				return;
			}
		}
		else if (self.currentmenu == "confirm_skill")
		{
			if (self.impulse == 1)
			{
				self.missionbrief = 0;
				self.class = self.tclass;
				if (self.class == 1)
					self.skill_doctor = self.skill_doctor + 1;
				if (self.class == 2)
					self.skill_sneak = self.skill_sneak + 1;
				if (self.class == 3)
					self.skill_combat = self.skill_combat + 1;
				if (self.class == 4)
					self.skill_science = self.skill_science + 1;

				self.score = 0;
				sound (self, CHAN_WEAPON, "player/yourturn.wav", TRUE, ATTN_NORM);
				self.currentmenu = "none";
				centerprint(self, "");
				bprint(2, self.netname);
				bprint(2, " has gained a level.\n");
				self.class = self.tclass;
				self.impulse = 0;
				return;
			}
			if (self.impulse == 2)
			{
				sound (self, CHAN_WEAPON, "player/yourturn.wav", TRUE, ATTN_NORM);
				self.currentmenu = "gain_skill";
				DisplayMenu();
				self.impulse = 0;
				self.class = 0;
				return;
			}
		}
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
				sound (self, CHAN_WEAPON, "player/yourturn.wav", TRUE, ATTN_NORM);
				self.connected = 1;
				self.currentmenu = "select_team";
				DisplayMenu();
				self.impulse = 0;
				return;
			}
		}
		else if (self.currentmenu == "display_brief")
		{
			if (self.impulse == 1)
			{
				if (self.skill_combat == 0 && self.skill_sneak == 0 && self.skill_doctor == 0 && self.skill_science == 0)
				{
					self.team = 1;
					self.missionbrief = 2;
					sound (self, CHAN_WEAPON, "player/yourturn.wav", TRUE, ATTN_NORM);
					self.currentmenu = "select_skill";
					DisplayMenu();
					self.impulse = 0;
					return;
				}
				else
				{

					self.missionbrief = 0;
					sound (self, CHAN_WEAPON, "player/yourturn.wav", TRUE, ATTN_NORM);
					self.currentmenu = "none";
					centerprint(self, "");
					PutClientInServer();
					bprint(2, self.netname);
					bprint(2, " has entered the wasteland.\n");
					total_players = total_players + 1;
					return;
				}

			}
		}
		else if (self.currentmenu == "select_mission")
		{
			if (self.impulse >= 1 && self.impulse <= 6)
			{
				if (infokey(world, "objective") == "return" || infokey(world, "objective") == "start")
					get_new_mission(1);
				else
				{
					sprint(self, 2, "you're already on a mission!\n");
					self.currentmenu = "none";
					return;
				}
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