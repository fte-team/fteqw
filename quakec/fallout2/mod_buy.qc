
float () weightx;

void (float wt, float cost, string item) BuyWeapon =
{
	local float lbs;
	local string qq;
	local float curr;
	local float c3;
	local string c1;
	local string c2;

	lbs = weightx ();
	if (((self.team == SPAWNFLAG_LASER) && ((random () * SECRET_NO_SHOOT) < AS_MELEE)))
	{
		sprint (self, SPAWNFLAG_LASER, "markdown: from ");
		c1 = ftos (cost);
		sprint (self, SPAWNFLAG_LASER, c1);
		sprint (self, SPAWNFLAG_LASER, " to ");
		c3 = ceil ((cost * 0.66));
		c2 = ftos (c3);
		sprint (self, SPAWNFLAG_LASER, c2);
		sprint (self, SPAWNFLAG_LASER, "\n");
		cost = (cost * 0.66);
	}
	if ((self.current_slot == SPAWNFLAG_SUPERSPIKE))
	{
		curr = self.slot1_weight;
		if ((self.slot2 == "none"))
		{
			self.slot2 = self.slot1;
		}
		/*else
		{
                        if ((self.slot1 != "none"))
			{
				DropWeapon (self.slot1, SPAWNFLAG_SUPERSPIKE, MULTICAST_ALL);
			}
		}*/
		//GetWeaponModel ();
	}
	if ((self.current_slot == SPAWNFLAG_LASER))
	{
		curr = self.slot2_weight;
		if ((self.slot1 == "none"))
		{
			self.slot1 = self.slot2;
		}
		/*else
		{
			if ((self.slot2 != MULTICAST_ALL))
			{
				DropWeapon (self.slot2, SPAWNFLAG_SUPERSPIKE, MULTICAST_ALL);
			}
		}*/
		//GetWeaponModel ();
	}
	if ((((lbs + wt) - curr) > 30))
	{
		//Weight ();
		sprint (self, PRINT_HIGH, "you can't carry that much.\n");
		self.currentmenu = "none";
		//GetWeaponModel ();
		return;
	}
	if ((cost > self.ammo_shells))
	{
		sprint (self, PRINT_HIGH, "not enough money.\n");
		self.currentmenu = "none";
		//GetWeaponModel ();
		return;
	}
	self.ammo_shells = (self.ammo_shells - cost);
	if ((self.current_slot == SPAWNFLAG_SUPERSPIKE))
	{
		GetWeaponWeight (self, self.current_slot);
		stuffcmd (self, "impulse 1\n");
		self.slot1 = item;
		self.items = (self.items | IT_SUPER_NAILGUN);
		//GetWeaponModel ();
	}
	else
	{
		if ((self.current_slot == SPAWNFLAG_LASER))
		{
			GetWeaponWeight (self, self.current_slot);
			stuffcmd (self, "impulse 2\n");
			self.slot2 = item;
			self.items = (self.items | IT_GRENADE_LAUNCHER);
			//GetWeaponModel ();
		}
	}
	//qq = GetWeaponName (self, self.current_slot);
	sprint (self, PRINT_HIGH, qq);
	sprint (self, PRINT_HIGH, " purchased.\n");
	self.currentmenu = "none";
	//WeaponAmmo (SPAWNFLAG_SUPERSPIKE);
	//WeaponAmmo (SPAWNFLAG_LASER);
	GetWeaponWeight (self, SPAWNFLAG_SUPERSPIKE);
	GetWeaponWeight (self, SPAWNFLAG_LASER);
	//GetWeaponModel ();
	return;
};

void (float cost, string item) BuyPerk =
{
	if ((self.frags < cost))
	{
		sprint (self, PRINT_HIGH, "not enough kills\n");
		self.currentmenu = "none";
		return;
	}
	sprint (self, PRINT_HIGH, "ok, ability gained.\n");
	self.perk = item;
	self.currentmenu = "none";
	return;
};

void (float wt, float cost, string item) BuyArmor =
{
	local float curr;
	local float lbs;

	lbs = weightx ();
  
	if ((cost > self.ammo_shells))
	{
		sprint (self, PRINT_HIGH, "not enough money.\n");
		self.currentmenu = "none";
		return;
	}
	if ((((lbs + wt) - self.armor_weight) > 30))
	{
		//Weight ();
		sprint (self, PRINT_HIGH, "you can't carry that much.\n");
		self.currentmenu = "none";
		return;
	}
	sprint (self, PRINT_HIGH, "armor purchased.\n");
	self.ammo_shells = (self.ammo_shells - cost);
	self.armor_weight = wt;
	self.armor = item;
	//Equip_Armor ();
	return;
};

void (float cost, string item) BuyGadget =
{
	local float lbs;

	lbs = weightx ();
	if ((cost > self.ammo_shells))
	{
		sprint (self, PRINT_HIGH, "not enough money.\n");
		self.currentmenu = "none";
		return;
	}
	sprint (self, PRINT_HIGH, "gadget purchased.\n");
	self.ammo_shells = (self.ammo_shells - cost);
	self.equipment = item;
	return;
};

void (float cost, string item) Buyprotect =
{
	if ((self.armor == "brotherhood combat armor"))
	{
		sprint (self, PRINT_HIGH, "too many electronics in\nyour currently worn armor!");
		self.currentmenu = "none";
		return;
	}
	if ((cost > self.ammo_shells))
	{
		sprint (self, PRINT_HIGH, "not enough money.\n");
		self.currentmenu = "none";
		return;
	}
	sprint (self, PRINT_HIGH, "protect purchased.\n");
	self.ammo_shells = (self.ammo_shells - cost);
	self.protect = item;
	return;
};

float (float input) overweight =
{
	local float var;
	local float max;

	var = ((((input) + self.armor_weight) + self.slot1_weight) + self.slot2_weight);
	max = 30;
	if (self.class == 6)
		max = 40;

        if (self.perk == "")
		max = max + 10;

	if (var > max)
		return (TRUE);
	else
		return (FALSE);
};

float () weightx =
{
	local float var;

	var = 0;
	var = (self.slot1_weight + self.slot2_weight + self.armor_weight);
	return (var);
};

void () W_GetClass =
{
	if ((self.currentmenu == "select_class"))
	{
		sound (self, CHAN_WEAPON, "buttons/switch02.wav", TRUE, ATTN_NORM);
		if ((self.impulse == SPAWNFLAG_SUPERSPIKE))
		{
			if ((self.class == SPAWNFLAG_LASER))
			{
				return;
			}
			self.currentmenu = "none";
			self.max_health = 80;
			self.class = SPAWNFLAG_LASER;
			self.override = SPAWNFLAG_SUPERSPIKE;
			stuffcmd (self, "kill\n");
			self.ghost = MULTICAST_ALL;
			return;
		}
		if ((self.impulse == SPAWNFLAG_LASER))
		{
			if ((self.class == AS_MELEE))
			{
				return;
			}
			self.currentmenu = "none";
			self.max_health = 70;
			self.class = AS_MELEE;
			self.override = SPAWNFLAG_SUPERSPIKE;
			stuffcmd (self, "kill\n");
			self.ghost = MULTICAST_ALL;
			return;
		}
		if ((self.impulse == AS_MELEE))
		{
			if ((self.class == SECRET_1ST_DOWN))
			{
				return;
			}
			self.currentmenu = "none";
			self.max_health = 100;
			self.class = SECRET_1ST_DOWN;
			self.override = SPAWNFLAG_SUPERSPIKE;
			stuffcmd (self, "kill\n");
			self.ghost = MULTICAST_ALL;
			return;
		}
		if ((self.impulse == SECRET_1ST_DOWN))
		{
			if ((self.class == MULTICAST_PVS_R))
			{
				return;
			}
			self.max_health = 80;
			self.currentmenu = "none";
			self.class = TE_LIGHTNING2;
			self.override = SPAWNFLAG_SUPERSPIKE;
			stuffcmd (self, "kill\n");
			self.ghost = MULTICAST_ALL;
			return;
		}
	}
	if ((self.impulse > SECRET_1ST_DOWN))
	{
		return;
	}
};

/*
void () W_PlayerMenu =
{
	local float type;
	local float value;
	local float bit;
	local float r;
	local entity spot;
	local entity te;
	local string ac;
	local string menu;
	local float ratio;
	local float r1;
	local float r2;
	local float var;
	local float soldout;
	local float lbs;
	local float b1;

	if ((self.currentmenu == "none"))
	{
		return;
	}
	else
	{
		if ((self.currentmenu == "help"))
		{
			if ((self.impulse == SPAWNFLAG_SUPERSPIKE))
			{
				self.currentmenu = "help1";
			}
			if ((self.impulse == SPAWNFLAG_LASER))
			{
				self.currentmenu = "help2";
			}
			if ((self.impulse == AS_MELEE))
			{
				self.currentmenu = "help3";
			}
			if ((self.impulse == SECRET_1ST_DOWN))
			{
				self.currentmenu = "help4";
			}
			if ((self.impulse == MULTICAST_PVS_R))
			{
				self.currentmenu = "help5";
			}
			if ((self.impulse == TE_LIGHTNING2))
			{
				self.currentmenu = "help6";
			}
			if ((self.impulse == TE_WIZSPIKE))
			{
				self.currentmenu = "help7";
			}
			return;
		}
		else
		{
			if ((self.currentmenu == "menu_votehs"))
			{
				spot = find (spot, classname, "voteboy");
				if ((self.impulse == SPAWNFLAG_SUPERSPIKE))
				{
					spot.vote1 = (spot.vote1 + SPAWNFLAG_SUPERSPIKE);
					self.currentmenu = "none";
				}
				if ((self.impulse == SPAWNFLAG_LASER))
				{
					spot.vote2 = (spot.vote2 + SPAWNFLAG_SUPERSPIKE);
					self.currentmenu = "none";
				}
				sprint (self, PRINT_HIGH, "Thank you for voting!\n");
				return;
			}
			else
			{
				if ((self.currentmenu == "menu_voters"))
				{
					spot = find (spot, classname, "voteboy");
					if ((self.impulse == SPAWNFLAG_SUPERSPIKE))
					{
						spot.vote1 = (spot.vote1 + SPAWNFLAG_SUPERSPIKE);
						self.currentmenu = "none";
					}
					if ((self.impulse == SPAWNFLAG_LASER))
					{
						spot.vote2 = (spot.vote2 + SPAWNFLAG_SUPERSPIKE);
						self.currentmenu = "none";
					}
					sprint (self, PRINT_HIGH, "Thank you for voting!\n");
					return;
				}
				else
				{
					if ((self.currentmenu == "menu_votemap"))
					{
						spot = find (spot, classname, "voteboy");
						if ((self.impulse == SPAWNFLAG_SUPERSPIKE))
						{
							spot.vote1 = (spot.vote1 + SPAWNFLAG_SUPERSPIKE);
							self.currentmenu = "none";
						}
						if ((self.impulse == SPAWNFLAG_LASER))
						{
							spot.vote2 = (spot.vote2 + SPAWNFLAG_SUPERSPIKE);
							self.currentmenu = "none";
						}
						if ((self.impulse == AS_MELEE))
						{
							spot.vote3 = (spot.vote3 + SPAWNFLAG_SUPERSPIKE);
							self.currentmenu = "none";
						}
						if ((self.impulse == SECRET_1ST_DOWN))
						{
							spot.vote4 = (spot.vote4 + SPAWNFLAG_SUPERSPIKE);
							self.currentmenu = "none";
						}
						if ((self.impulse == MULTICAST_PVS_R))
						{
							spot.vote5 = (spot.vote5 + SPAWNFLAG_SUPERSPIKE);
							self.currentmenu = "none";
						}
						if ((self.impulse == TE_LIGHTNING2))
						{
							spot.vote6 = (spot.vote6 + SPAWNFLAG_SUPERSPIKE);
							self.currentmenu = "none";
						}
						if ((self.impulse == TE_WIZSPIKE))
						{
							spot.vote7 = (spot.vote7 + SPAWNFLAG_SUPERSPIKE);
							self.currentmenu = "none";
						}
						if ((self.impulse <= TE_WIZSPIKE))
						{
							sprint (self, PRINT_HIGH, "Thank you for voting!\n");
						}
						return;
					}
					else
					{
						if ((self.currentmenu == "menu_voteff"))
						{
							spot = find (spot, classname, "voteboy");
							if ((self.impulse == SPAWNFLAG_SUPERSPIKE))
							{
								spot.vote1 = (spot.vote1 + SPAWNFLAG_SUPERSPIKE);
								self.currentmenu = "none";
							}
							if ((self.impulse == SPAWNFLAG_LASER))
							{
								spot.vote2 = (spot.vote2 + SPAWNFLAG_SUPERSPIKE);
								self.currentmenu = "none";
							}
							sprint (self, PRINT_HIGH, "Thank you for voting!\n");
							return;
						}
						else
						{
							if ((self.currentmenu == "menu_votez"))
							{
								spot = find (spot, classname, "voteboy");
								if ((self.impulse == SPAWNFLAG_SUPERSPIKE))
								{
									spot.vote1 = (spot.vote1 + SPAWNFLAG_SUPERSPIKE);
									self.currentmenu = "none";
								}
								if ((self.impulse == SPAWNFLAG_LASER))
								{
									spot.vote2 = (spot.vote2 + SPAWNFLAG_SUPERSPIKE);
									self.currentmenu = "none";
								}
								if ((self.impulse == AS_MELEE))
								{
									spot.vote3 = (spot.vote3 + SPAWNFLAG_SUPERSPIKE);
									self.currentmenu = "none";
								}
								if ((self.impulse == SECRET_1ST_DOWN))
								{
									spot.vote4 = (spot.vote4 + SPAWNFLAG_SUPERSPIKE);
									self.currentmenu = "none";
								}
								sprint (self, PRINT_HIGH, "Thank you for voting!\n");
								return;
							}
							else
							{
								if ((self.currentmenu == "menu_buy"))
								{
									if ((self.impulse == SPAWNFLAG_SUPERSPIKE))
									{
										menu = TraitString ();
										centerprint (self, menu);
										self.currentmenu = "menu_trait";
										return;
									}
									if ((self.impulse == SPAWNFLAG_LASER))
									{
										menu = PlusString ();
										centerprint (self, menu);
										self.currentmenu = "menu_plus";
										return;
									}
									if ((self.impulse == AS_MELEE))
									{
										menu = ArmorString1 ();
										centerprint (self, menu);
										self.currentmenu = "menu_armor";
										return;
									}
									if ((self.impulse == SECRET_1ST_DOWN))
									{
										if (((self.team == SPAWNFLAG_SUPERSPIKE) && (blue_gadget == MULTICAST_ALL)))
										{
											self.currentmenu = "none";
											sprint (self, SPAWNFLAG_LASER, "your team needs a proto-lab.\n");
											return;
										}
										if (((self.team == SPAWNFLAG_LASER) && (red_gadget == MULTICAST_ALL)))
										{
											self.currentmenu = "none";
											sprint (self, SPAWNFLAG_LASER, "your team needs a proto-lab.\n");
											return;
										}
										menu = HardwareString ();
										centerprint (self, menu);
										self.currentmenu = "menu_protect";
										return;
									}
									if ((self.impulse == MULTICAST_PVS_R))
									{
										menu = WeaponString ();
										centerprint (self, menu);
										self.currentmenu = "menu_weapon";
										return;
									}
									if ((self.impulse == TE_LIGHTNING2))
									{
										if (((self.team == SPAWNFLAG_SUPERSPIKE) && (blue_gadget == MULTICAST_ALL)))
										{
											self.currentmenu = "none";
											sprint (self, SPAWNFLAG_LASER, "your team needs a proto-lab.\n");
											return;
										}
										if (((self.team == SPAWNFLAG_LASER) && (red_gadget == MULTICAST_ALL)))
										{
											self.currentmenu = "none";
											sprint (self, SPAWNFLAG_LASER, "your team needs a proto-lab.\n");
											return;
										}
										menu = GadgetString ();
										centerprint (self, menu);
										self.currentmenu = "menu_gadget";
										return;
									}
									if ((self.impulse == TE_WIZSPIKE))
									{
										if ((self.team == SPAWNFLAG_SUPERSPIKE))
										{
											menu = DrugString1 ();
										}
										if ((self.team == SPAWNFLAG_LASER))
										{
											menu = DrugString2 ();
										}
										centerprint (self, menu);
										if ((self.team == SPAWNFLAG_SUPERSPIKE))
										{
											self.currentmenu = "menu_drug1";
										}
										if ((self.team == SPAWNFLAG_LASER))
										{
											self.currentmenu = "menu_drug2";
										}
										return;
									}
								}
								else
								{
									if ((self.currentmenu == "menu_weapon"))
									{
										if ((self.impulse == SPAWNFLAG_SUPERSPIKE))
										{
											menu = MeleeString ();
											centerprint (self, menu);
											self.currentmenu = "menu_melee";
											return;
										}
										if ((self.impulse == SPAWNFLAG_LASER))
										{
											menu = MiscString ();
											centerprint (self, menu);
											self.currentmenu = "menu_misc";
											return;
										}
										if ((self.impulse == AS_MELEE))
										{
											menu = SmallArmString1 ();
											centerprint (self, menu);
											self.currentmenu = "menu_smallarm1";
											return;
										}
										if ((self.impulse == SECRET_1ST_DOWN))
										{
											menu = SmallArmString2 ();
											centerprint (self, menu);
											self.currentmenu = "menu_smallarm2";
											return;
										}
										if ((self.impulse == MULTICAST_PVS_R))
										{
											menu = SmallArmString3 ();
											centerprint (self, menu);
											self.currentmenu = "menu_smallarm3";
											return;
										}

										if ((self.impulse == TE_LIGHTNING2))
										{
											if (((self.team == SPAWNFLAG_SUPERSPIKE) && (blue_weapon == MULTICAST_ALL)))
											{
												self.currentmenu = "none";
												sprint (self, SPAWNFLAG_LASER, "your team needs an arms lab.\n");
												return;
											}
											if (((self.team == SPAWNFLAG_LASER) && (red_weapon == MULTICAST_ALL)))
											{
												self.currentmenu = "none";
												sprint (self, SPAWNFLAG_LASER, "your team needs an arms lab.\n");
												return;
											}
											menu = HeavyGunsString ();
											centerprint (self, menu);
											self.currentmenu = "menu_heavy";
											return;
										}
										if ((self.impulse == TE_WIZSPIKE))
										{
											if (((self.team == SPAWNFLAG_SUPERSPIKE) && (blue_weapon == MULTICAST_ALL)))
											{
												self.currentmenu = "none";
												sprint (self, SPAWNFLAG_LASER, "your team needs an arms lab.\n");
												return;
											}
											if (((self.team == SPAWNFLAG_LASER) && (red_weapon == MULTICAST_ALL)))
											{
												self.currentmenu = "none";
												sprint (self, SPAWNFLAG_LASER, "your team needs an arms lab.\n");
												return;
											}
											menu = EnergyWeaponsString ();
											centerprint (self, menu);
											self.currentmenu = "menu_energy";
											return;
										}

									}
									else
									{
										if ((self.currentmenu == "menu_build"))
										{
											if ((self.impulse == SPAWNFLAG_SUPERSPIKE))
											{
												spawn_station (self.origin, self, MULTICAST_ALL, SECRET_1ST_DOWN);
												return;
											}
											if ((self.impulse == SPAWNFLAG_LASER))
											{
												spawn_station (self.origin, self, SPAWNFLAG_SUPERSPIKE, TE_LIGHTNING2);
												return;
											}
											if ((self.impulse == AS_MELEE))
											{
												spawn_station (self.origin, self, SPAWNFLAG_LASER, TE_LAVASPLASH);
												return;
											}
											if ((self.impulse == SECRET_1ST_DOWN))
											{
												spawn_station (self.origin, self, 3, 15);
												return;
											}
										}
										else
										{
											if ((self.currentmenu == "menu_misc"))
											{
												if ((self.impulse == SPAWNFLAG_SUPERSPIKE))
												{
													if ((self.ammo_shells >= TE_TELEPORT))
													{
														if ((self.total_handgren >= SPAWNFLAG_SUPERSPIKE))
														{
															sprint (self, SPAWNFLAG_LASER, "max of one grenade.\n");
															return;
														}
														self.ammo_shells = (self.ammo_shells - TE_TELEPORT);
														self.handgren_plasma = (self.handgren_plasma + SPAWNFLAG_SUPERSPIKE);
														self.total_handgren = (self.total_handgren + SPAWNFLAG_SUPERSPIKE);
														sprint (self, SPAWNFLAG_LASER, "plasma grenade purchased.\n");
														return;
													}
												}
												if ((self.impulse == SPAWNFLAG_LASER))
												{
													if ((self.ammo_shells >= MULTICAST_PVS_R))
													{
														if ((self.total_handgren >= SPAWNFLAG_SUPERSPIKE))
														{
															sprint (self, SPAWNFLAG_LASER, "max of one grenade.\n");
															return;
														}
														self.ammo_shells = (self.ammo_shells - MULTICAST_PVS_R);
														self.handgren_frag = (self.handgren_frag + SPAWNFLAG_SUPERSPIKE);
														self.total_handgren = (self.total_handgren + SPAWNFLAG_SUPERSPIKE);
														sprint (self, SPAWNFLAG_LASER, "frag grenade purchased.\n");
														return;
													}
												}
												if ((self.impulse == AS_MELEE))
												{
													if ((self.ammo_shells >= AS_MELEE))
													{
														if ((self.total_handgren >= SPAWNFLAG_SUPERSPIKE))
														{
															sprint (self, SPAWNFLAG_LASER, "max of one grenade.\n");
															return;
														}
														self.ammo_shells = (self.ammo_shells - AS_MELEE);
														self.handgren_emp = (self.handgren_emp + SPAWNFLAG_SUPERSPIKE);
														self.total_handgren = (self.total_handgren + SPAWNFLAG_SUPERSPIKE);
														sprint (self, SPAWNFLAG_LASER, "emp grenade purchased.\n");
														return;
													}
												}
												if ((self.impulse == SECRET_1ST_DOWN))
												{
													if ((self.ammo_shells >= SPAWNFLAG_LASER))
													{
														if ((self.total_handgren >= SPAWNFLAG_SUPERSPIKE))
														{
															sprint (self, SPAWNFLAG_LASER, "max of one grenade.\n");
															return;
														}
														self.ammo_shells = (self.ammo_shells - SPAWNFLAG_LASER);
														self.handgren_smoke = (self.handgren_smoke + SPAWNFLAG_SUPERSPIKE);
														self.total_handgren = (self.total_handgren + SPAWNFLAG_SUPERSPIKE);
														centerprint (self, "Smoke Grenade purchased.");
														return;
													}
												}
												if ((self.impulse == MULTICAST_PVS_R))
												{
													if ((self.ammo_shells >= SPAWNFLAG_LASER))
													{
														if ((self.total_handgren >= SPAWNFLAG_LASER))
														{
															sprint (self, SPAWNFLAG_LASER, "max of two flashbangs.\n");
															return;
														}
														self.ammo_shells = (self.ammo_shells - SPAWNFLAG_LASER);
														self.handgren_flash = (self.handgren_flash + SPAWNFLAG_SUPERSPIKE);
														self.total_handgren = (self.total_handgren + SPAWNFLAG_SUPERSPIKE);
														centerprint (self, "Flash Bang purchased.");
														return;
													}
												}
											}
											else
											{

													if ((self.currentmenu == "menu_armor"))
													{
											
														if ((((self.impulse == MULTICAST_PVS_R) || (self.impulse == TE_LIGHTNING2)) || (self.impulse == SECRET_NO_SHOOT)))
														{
															if (((self.team == SPAWNFLAG_SUPERSPIKE) && (blue_armor == MULTICAST_ALL)))
															{
																self.currentmenu = "none";
																sprint (self, SPAWNFLAG_LASER, "your team needs an armory.\n");
																return;
															}
															if (((self.team == SPAWNFLAG_LASER) && (red_armor == MULTICAST_ALL)))
															{
																self.currentmenu = "none";
																sprint (self, SPAWNFLAG_LASER, "your team needs an armory.\n");
																return;
															}
														}
														if ((self.impulse == SPAWNFLAG_SUPERSPIKE))
														{
															BuyArmor (AS_MELEE, AS_MELEE, "vault suit");
														}
														else
														{
															if ((self.impulse == 2))
															{
																BuyArmor (TE_WIZSPIKE, MULTICAST_PVS_R, "leather armor");
															}
															else
															{
																if ((self.impulse == 3))
																{
																	BuyArmor (TE_LAVASPLASH, SECRET_NO_SHOOT, "kevlar body armor");
																}
																else
																{
																	if ((self.impulse == 4))
																	{
																		BuyArmor (TE_LIGHTNINGBLOOD, TE_TELEPORT, "combat armor");
																	}
																	else
																	{
																		if ((self.impulse == 5))
																		{
																			BuyArmor (17, 15, "brotherhood combat armor");
																		}
																														

		

																						}
																					}
																				}
																			}
																		}
																	}
																}
															}
														}
													}
													else
													{
														if ((self.currentmenu == "menu_drop"))
														{
															if ((self.impulse == SPAWNFLAG_SUPERSPIKE))
															{
																if ((self.current_slot == SPAWNFLAG_SUPERSPIKE))
																{
																	DropWeapon (self.slot1, SPAWNFLAG_SUPERSPIKE, MULTICAST_ALL);
																	self.slot1 = "none";
																	self.slot1_weight = MULTICAST_ALL;
																}
																if ((self.current_slot == SPAWNFLAG_LASER))
																{
																	DropWeapon (self.slot2, SPAWNFLAG_SUPERSPIKE, MULTICAST_ALL);
																	self.slot2 = "none";
																	self.slot2_weight = MULTICAST_ALL;
																}
																centerprint (self, "WEAPON REMOVED!");
															}
															if ((self.impulse == SPAWNFLAG_LASER))
															{
																self.armor = "none";
																self.armor_weight = MULTICAST_ALL;
																centerprint (self, "ARMOR REMOVED!");
															}
															if ((self.impulse == AS_MELEE))
															{
																		centerprint (self, "HELMET REMOVED!");
															}
															if ((self.impulse == SECRET_1ST_DOWN))
															{
																self.protect = MULTICAST_ALL;
																centerprint (self, "protect REMOVED!");
															}
															if ((self.impulse == 5))
															{
																self.equipment = "none";
																self.runevar = MULTICAST_ALL;
																centerprint (self, "GADGET REMOVED!");
															}
															if ((self.impulse == TE_LIGHTNING2))
															{
																self.perk = MULTICAST_ALL;
																centerprint (self, "AUGMENT REMOVED!");
															}
														}
														else
														{
															if ((self.currentmenu == "menu_armor2"))
															{
																lbs = weightx ();
																															if ((self.impulse == SPAWNFLAG_SUPERSPIKE))
																{
																	BuyArmor (SECRET_NO_SHOOT, 50, TE_LAVASPLASH);
																}
																else
																{
																	if ((self.impulse == SPAWNFLAG_LASER))
																	{
																		BuyArmor (TE_LIGHTNING3, 60, TE_TELEPORT);
																	}
																	else
																	{
																		if ((self.impulse == AS_MELEE))
																		{
																			BuyArmor (TE_LIGHTNING3, 60, TE_BLOOD);
																		}
																		else
																		{
																			if ((self.impulse == SECRET_1ST_DOWN))
																			{
																				BuyArmor (TE_LIGHTNING3, 70, TE_LIGHTNINGBLOOD);
																			}
																			else
																			{
																				if ((self.impulse == MULTICAST_PVS_R))
																				{
																					BuyArmor (TE_WIZSPIKE, 70, IDLE2A);
																				}
																				else
																				{
																					if ((self.impulse == TE_LIGHTNING2))
																					{
																						BuyArmor (TE_TELEPORT, 80, IDLE3A);
																					}
																					else
																					{
																						if ((self.impulse == TE_WIZSPIKE))
																						{
																							BuyArmor (TE_BLOOD, 80, SECRET_YES_SHOOT);
																						}
																						else
																						{
																							if ((self.impulse == SECRET_NO_SHOOT))
																							{
																								BuyArmor (IDLE2A, 90, IDLE5A);
																							}
																							else
																							{
																								if ((self.impulse == TE_LIGHTNING3))
																								{
																									BuyArmor (SECRET_YES_SHOOT, 95, IDLE6A);
																								}
																								else
																								{
																									if ((self.impulse == TE_LAVASPLASH))
																									{
																										if (((self.frags / self.dead) < 3.5))
																										{
																											menu = ArmorString1 ();
																											centerprint (self, menu);
																											self.currentmenu = "menu_armor";
																											return;
																										}
																										if (((self.frags / self.dead) >= 3.5))
																										{
																											menu = ArmorString3 ();
																											centerprint (self, menu);
																											self.currentmenu = "menu_armor3";
																											return;
																										}
																									}
																								}
																							}
																						}
																					}
																				}
																			}
																		}
																	}
																}
															}
															else
															{
																if ((self.currentmenu == "menu_armor3"))
																{
													
																	if ((self.impulse == SPAWNFLAG_SUPERSPIKE))
																	{
																		BuyArmor (TE_WIZSPIKE, 95, IDLE7A);
																	}
																	else
																	{
																		if ((self.impulse == SPAWNFLAG_LASER))
																		{
																			BuyArmor (SECRET_NO_SHOOT, 95, IDLE8A);
																		}
																		else
																		{
																			if ((self.impulse == AS_MELEE))
																			{
																				BuyArmor (IDLE2A, 95, IDLE9A);
																			}
																			else
																			{
																				if ((self.impulse == TE_LAVASPLASH))
																				{
																					menu = ArmorString1 ();
																					centerprint (self, menu);
																					self.currentmenu = "menu_armor";
																					return;
																				}
																			}
																		}
																	}
																}
																else
																{
																	if ((self.currentmenu == "menu_plus"))
																	{
																		if ((self.impulse == SPAWNFLAG_SUPERSPIKE))
																		{
																			BuyPerk (SECRET_NO_SHOOT, SPAWNFLAG_SUPERSPIKE);
																		}
																		if ((self.impulse == SPAWNFLAG_LASER))
																		{
																			BuyPerk (TE_BLOOD, SPAWNFLAG_LASER);
																		}
																		if ((self.impulse == AS_MELEE))
																		{
																			BuyPerk (IDLE2A, AS_MELEE);
																		}
																		if ((self.impulse == SECRET_1ST_DOWN))
																		{
																			BuyPerk (IDLE3A, SECRET_1ST_DOWN);
																		}
																		if ((self.impulse == MULTICAST_PVS_R))
																		{
																			BuyPerk (IDLE5A, MULTICAST_PVS_R);
																		}
																		if ((self.impulse == TE_LIGHTNING2))
																		{
																			BuyPerk (SVC_TEMPENTITY, TE_LIGHTNING2);
																		}
																		if ((self.impulse == TE_WIZSPIKE))
																		{
																			BuyPerk (DRAW4, TE_WIZSPIKE);
																		}
																		if ((self.impulse == SECRET_NO_SHOOT))
																		{
																			BuyPerk (DOOR_TOGGLE, SECRET_NO_SHOOT);
																		}
																		if ((self.impulse == TE_LIGHTNING3))
																		{
																			BuyPerk (RN_SHUB, TE_LIGHTNING3);
																		}
																		if ((self.impulse == TE_LAVASPLASH))
																		{
																			BuyPerk (44, TE_LAVASPLASH);
																		}
																	}
																	else
																	{
																		if ((self.currentmenu == "menu_protect"))
																		{
																			if ((self.armor == "brotherhood combat armor"))
																			{
																				sprint (self, PRINT_HIGH, "too many electronics in\nyour currently worn 				armor!");
																				self.currentmenu = "none";
																				return;
																			}
																			if (((self.impulse == SPAWNFLAG_SUPERSPIKE) && (self.ammo_shells >= IDLE3A)))
																			{
																				self.protect = SPAWNFLAG_SUPERSPIKE;
																				self.ammo_shells = (self.ammo_shells - IDLE3A);
																				sprint (self, PRINT_HIGH, "pro cloak\n");
																				self.currentmenu = "none";
																				return;
																			}
																			if (((self.impulse == SPAWNFLAG_LASER) && (self.ammo_shells >= DRAW3)))
																			{
																				self.protect = SPAWNFLAG_LASER;
																				self.ammo_shells = (self.ammo_shells - DRAW3);
																				sprint (self, PRINT_HIGH, "emp shield\n");
																				self.currentmenu = "none";
																				return;
																			}
																			if ((self.armor == "brotherhood combat armor"))
																			{
																				sprint (self, PRINT_HIGH, "too many electronics in\nyour currently worn 				armor!");
																				return;
																			}
																			if (((self.impulse == AS_MELEE) && (self.ammo_shells >= IDLE8A)))
																			{
																				self.protect = AS_MELEE;
																				self.ammo_shells = (self.ammo_shells - IDLE8A);
																				sprint (self, PRINT_HIGH, "force shield\n");
																				self.currentmenu = "none";
																				return;
																			}
																			if (((self.impulse == SECRET_1ST_DOWN) && (self.ammo_shells >= DRAW3)))
																			{
																				self.protect = SECRET_1ST_DOWN;
																				self.ammo_shells = (self.ammo_shells - DRAW3);
																				sprint (self, PRINT_HIGH, "force field\n");
																				self.currentmenu = "none";
																				return;
																			}
																			if (((self.impulse == MULTICAST_PVS_R) && (self.ammo_shells >= DRAW3)))
																			{
																				self.protect = MULTICAST_PVS_R;
																				self.ammo_shells = (self.ammo_shells - DRAW3);
																				sprint (self, PRINT_HIGH, "energy shield\nonly protects against energy\n");
																				self.currentmenu = "none";
																				return;
																			}
																			if (((self.impulse == TE_LIGHTNING2) && (self.ammo_shells >= 65)))
																			{
																				self.protect = TE_LIGHTNING2;
																				self.ammo_shells = (self.ammo_shells - 65);
																				sprint (self, PRINT_HIGH, "pro ring\n");
																				self.currentmenu = "none";
																				return;
																			}
																			if (((self.impulse == TE_WIZSPIKE) && (self.ammo_shells >= 75)))
																			{
																				self.protect = TE_WIZSPIKE;
																				self.ammo_shells = (self.ammo_shells - 75);
																				sprint (self, PRINT_HIGH, "dark force\n");
																				self.currentmenu = "none";
																				return;
																			}
																			if (((self.impulse == SECRET_NO_SHOOT) && (self.ammo_shells >= 85)))
																			{
																				self.protect = SECRET_NO_SHOOT;
																				self.ammo_shells = (self.ammo_shells - 85);
																				sprint (self, PRINT_HIGH, "efreet-9088 module\nprotects against fire\n");
																				self.currentmenu = "none";
																				return;
																			}
																			if (((self.impulse == TE_LIGHTNING3) && (self.ammo_shells >= 95)))
																			{
																				self.protect = TE_LIGHTNING3;
																				self.ammo_shells = (self.ammo_shells - 95);
																				sprint (self, PRINT_HIGH, "sentient cube\n");
																				self.currentmenu = "none";
																				return;
																			}
																			if (((self.impulse == TE_LAVASPLASH) && (self.ammo_shells >= 95)))
																			{
																				self.currentmenu = "none";
																				self.protect = TE_LAVASPLASH;
																				self.ammo_shells = (self.ammo_shells - 95);
																				sprint (self, PRINT_HIGH, "xe+ vampire\n");
																				self.currentmenu = "none";
																				return;
																			}
																		}
																		else
																		{
																			if ((self.currentmenu == "menu_melee"))
																			{
																				if ((self.impulse == SPAWNFLAG_SUPERSPIKE))
																				{
																					BuyWeapon (SPAWNFLAG_SUPERSPIKE, SPAWNFLAG_SUPERSPIKE, SPAWNFLAG_SUPERSPIKE);
																				}
																				else
																				{
																					if ((self.impulse == SPAWNFLAG_LASER))
																					{
																						BuyWeapon (SPAWNFLAG_LASER, AS_MELEE, SPAWNFLAG_LASER);
																					}
																					else
																					{
																						if ((self.impulse == AS_MELEE))
																						{
																							BuyWeapon (SPAWNFLAG_LASER, TE_WIZSPIKE, AS_MELEE);
																						}
																					}
																				}
																			}
																			else
																			{
																				if ((self.currentmenu == "menu_trait"))
																				{
																					if ((self.impulse == SPAWNFLAG_SUPERSPIKE))
																					{
																						self.trait = SPAWNFLAG_SUPERSPIKE;
																						sprint (self, PRINT_HIGH, "trait acquired\n");
																					}
																					else
																					{
																						if ((self.impulse == SPAWNFLAG_LASER))
																						{
																							self.trait = SPAWNFLAG_LASER;
																							sprint (self, PRINT_HIGH, "trait acquired\n");
																						}
																						else
																						{
																							if ((self.impulse == AS_MELEE))
																							{
																								self.trait = AS_MELEE;
																								sprint (self, PRINT_HIGH, "trait acquired\n");
																							}
																							else
																							{
																								if ((self.impulse == SECRET_1ST_DOWN))
																								{
																									self.trait = SECRET_1ST_DOWN;
																									sprint (self, PRINT_HIGH, "trait acquired\n");
																								}
																								else
																								{
																									if ((self.impulse == MULTICAST_PVS_R))
																									{
																										self.trait = MULTICAST_PVS_R;
																										sprint (self, PRINT_HIGH, "trait acquired\n");
																									}
																									else
																									{
																										if ((self.impulse == TE_LIGHTNING2))
																										{
																											self.trait = TE_LIGHTNING2;
																											sprint (self, PRINT_HIGH, "trait acquired\n");
																										}
																									}
																								}
																							}
																						}
																					}
																				}
																				else
																				{
																					if ((self.currentmenu == "menu_smallarm1"))
						{								
							if (self.impulse == 1)
								BuyWeapon (1, 5, TE_LAVASPLASH);
							if (self.impulse == 2)
								BuyWeapon (2, 7, TE_TELEPORT);
							if (self.impulse == 3)
								BuyWeapon (2, 9, TE_WIZSPIKE);
							if (self.impulse == 4)
								BuyWeapon (3, 14, TE_BLOOD);
							if (self.impulse == 5)
								BuyWeapon (3, 17, TE_LIGHTNINGBLOOD);
							if (self.impulse == 6)
								BuyWeapon (2, 21, 42);
						}
						else
																					{
																						if ((self.currentmenu == "menu_smallarm2"))
							{
																						if (self.impulse == 1)
								BuyWeapon (2, 3, IDLE8A);
							if (self.impulse == 2)
								BuyWeapon (3, 3, DRAW3);
							if (self.impulse == 3)
								BuyWeapon (4, IDLE2A, DRAW4);
							if (self.impulse == 4)
								BuyWeapon (5, 35, RN_HOLO);
							}
																						else
																						{
																							if ((self.currentmenu == "menu_drug1"))
																							{
																								if ((self.regen > MULTICAST_ALL))
																								{
																									return;
																								}
																								if ((self.impulse == SPAWNFLAG_SUPERSPIKE))
																								{
																									if ((self.ammo_shells < AS_MELEE))
																									{
																										centerprint (self, "not enough money.");
																										self.currentmenu = "none";
																										return;
																									}
																									self.drug = SPAWNFLAG_SUPERSPIKE;
																									if ((self.class == SPAWNFLAG_LASER))
																									{
																										self.supplies = MULTICAST_PVS_R;
																									}
																									else
																									{
																										self.supplies = SPAWNFLAG_LASER;
																									}
																									self.ammo_shells = (self.ammo_shells - AS_MELEE);
																									centerprint (self, "stimpacks purchased.");
																								}
																								if ((self.impulse == SPAWNFLAG_LASER))
																								{
																									if ((self.ammo_shells < MULTICAST_PVS_R))
																									{
																										centerprint (self, "not enough money.");
																										self.currentmenu = "none";
																										return;
																									}
																									if ((self.class != SPAWNFLAG_LASER))
																									{
																										centerprint (self, "not a medic.");
																										self.currentmenu = "none";
																										return;
																									}
																									self.drug = SPAWNFLAG_LASER;
																									if ((self.class == SPAWNFLAG_LASER))
																									{
																										self.supplies = MULTICAST_PVS_R;
																									}
																									else
																									{
																										self.supplies = SPAWNFLAG_LASER;
																									}
																									self.ammo_shells = (self.ammo_shells - MULTICAST_PVS_R);
																									centerprint (self, "medkit purchased.");
																								}
																								if ((self.impulse == AS_MELEE))
																								{
																									if ((self.ammo_shells < TE_BLOOD))
																									{
																										centerprint (self, "not enough money.");
																										self.currentmenu = "none";
																										return;
																									}
																									if ((self.class != SPAWNFLAG_LASER))
																									{
																										centerprint (self, "not a medic.");
																										self.currentmenu = "none";
																										return;
																									}
																									if ((self.frags < TE_LAVASPLASH))
																									{
																										centerprint (self, "not yet ready.");
																										self.currentmenu = "none";
																										return;
																									}
																									self.drug = AS_MELEE;
																									if ((self.class == SPAWNFLAG_LASER))
																									{
																										self.supplies = MULTICAST_PVS_R;
																									}
																									else
																									{
																										self.supplies = SPAWNFLAG_LASER;
																									}
																									self.ammo_shells = (self.ammo_shells - TE_BLOOD);
																									centerprint (self, "superstims purchased.");
																								}
																							}
																							else
																							{
																								if ((self.currentmenu == "menu_drug2"))
																								{
																									if ((self.regen > MULTICAST_ALL))
																									{
																										return;
																									}
																									if ((self.impulse == SPAWNFLAG_SUPERSPIKE))
																									{
																										if ((self.ammo_shells < AS_MELEE))
																										{
																											centerprint (self, "not enough money.");
																											self.currentmenu = "none";
																											return;
																										}
																										self.drug = SPAWNFLAG_SUPERSPIKE;
																										if ((self.class == SPAWNFLAG_LASER))
																										{
																											self.supplies = MULTICAST_PVS_R;
																										}
																										else
																										{
																											self.supplies = SPAWNFLAG_LASER;
																										}
																										self.ammo_shells = (self.ammo_shells - AS_MELEE);
																										centerprint (self, "stimpacks purchased.");
																									}
																									if ((self.impulse == SPAWNFLAG_LASER))
																									{
																										if ((self.ammo_shells < TE_WIZSPIKE))
																										{
																											centerprint (self, "not enough money.");
																											self.currentmenu = "none";
																											return;
																										}
																										self.drug = SECRET_1ST_DOWN;
																										if ((self.class == SPAWNFLAG_LASER))
																										{
																											self.supplies = MULTICAST_PVS_R;
																										}
																										else
																										{
																											self.supplies = SPAWNFLAG_LASER;
																										}
																										self.ammo_shells = (self.ammo_shells - TE_WIZSPIKE);
																										centerprint (self, "adrenaline purchased.");
																									}
																									if ((self.impulse == AS_MELEE))
																									{
																										if ((self.ammo_shells < IDLE3A))
																										{
																											centerprint (self, "not enough money.");
																											self.currentmenu = "none";
																											return;
																										}
																										if ((self.class != SPAWNFLAG_LASER))
																										{
																											centerprint (self, "not a medic.");
																											self.currentmenu = "none";
																											return;
																										}
																										self.drug = MULTICAST_PVS_R;
																										if ((self.class == SPAWNFLAG_LASER))
																										{
																											self.supplies = MULTICAST_PVS_R;
																										}
																										else
																										{
																											self.supplies = SPAWNFLAG_LASER;
																										}
																										self.ammo_shells = (self.ammo_shells - IDLE3A);
																										centerprint (self, "psycho purchased.");
																									}
																									if ((self.impulse == SECRET_1ST_DOWN))
																									{
																										if ((self.ammo_shells < TE_BLOOD))
																										{
																											centerprint (self, "not enough money.");
																											self.currentmenu = "none";
																											return;
																										}
																										if ((self.class != SPAWNFLAG_LASER))
																										{
																											centerprint (self, "not a medic.");
																											self.currentmenu = "none";
																											return;
																										}
																										if ((self.frags < TE_LAVASPLASH))
																										{
																											centerprint (self, "not yet ready.");
																											self.currentmenu = "none";
																											return;
																										}
																										self.drug = SPAWNFLAG_LASER;
																										if ((self.class == SPAWNFLAG_LASER))
																										{
																											self.supplies = MULTICAST_PVS_R;
																										}
																										else
																										{
																											self.supplies = SPAWNFLAG_LASER;
																										}
																										self.ammo_shells = (self.ammo_shells - TE_BLOOD);
																										centerprint (self, "medkit purchased.");
																									}
																									if ((self.impulse == MULTICAST_PVS_R))
																									{
																										if ((self.ammo_shells < IDLE9A))
																										{
																											centerprint (self, "not enough money.");
																											self.currentmenu = "none";
																											return;
																										}
																										if ((self.class != SPAWNFLAG_LASER))
																										{
																											centerprint (self, "not a medic.");
																											self.currentmenu = "none";
																											return;
																										}
																										if ((self.frags < TE_LAVASPLASH))
																										{
																											centerprint (self, "not yet ready.");
																											self.currentmenu = "none";
																											return;
																										}
																										self.drug = TE_LIGHTNING2;
																										if ((self.class == SPAWNFLAG_LASER))
																										{
																											self.supplies = MULTICAST_PVS_R;
																										}
																										else
																										{
																											self.supplies = SPAWNFLAG_LASER;
																										}
																										self.ammo_shells = (self.ammo_shells - IDLE9A);
																										centerprint (self, "berserk purchased.");
																									}
																								}
																								else
																								{
																								if (self.currentmenu == "menu_smallarm3")
																									{
									if (self.impulse == 1)
										BuyWeapon (3, 11, EF_FLAG1);
									if (self.impulse == 2)
										BuyWeapon (4, 21, IDLE9A);
									if (self.impulse == 3)
										BuyWeapon (6, 24, IDLE5A);
									if (self.impulse == 4)
										BuyWeapon (5, 27, IDLE7A);
									if (self.impulse == 5)
										BuyWeapon (6, 36, IDLE10A);
									if (self.impulse == 6)
										BuyWeapon (5, 23, SVC_SELLSCREEN);
									if (self.impulse == 7)
										BuyWeapon (7, 41, RN_ATTRACT);
									if (self.impulse == 8)
										BuyWeapon (8, 51, SVC_FINALE);

										}
																									else
																									{
																										if ((self.currentmenu == "menu_energy"))
																										{
																											if ((self.impulse == SPAWNFLAG_SUPERSPIKE))
																											{
																												BuyWeapon (AS_MELEE, IDLE9A, RN_FIELD);
																											}
																											else
																											{
																												if ((self.impulse == SPAWNFLAG_LASER))
																												{
																													BuyWeapon (TE_WIZSPIKE, SVC_SMALLKICK, 43);
																												}
																												else
																												{
																													if ((self.impulse == AS_MELEE))
																													{
																														BuyWeapon (TE_TELEPORT, RN_SHAMB, RN_CLOUD);
																													}
																													else
																													{
																														if ((self.impulse == SECRET_1ST_DOWN))
																														{
																															BuyWeapon (TE_LIGHTNING2, 57, RN_ATTRACT);
																														}
																														else
																														{
																															if ((self.impulse == MULTICAST_PVS_R))
																															{
																																BuyWeapon (SPAWNFLAG_LASER, 72, 42);
																															}
																															else
																															{
																																if ((self.impulse == TE_LIGHTNING2))
																																{
																																	BuyWeapon (TE_LIGHTNING2, 89, SVC_BIGKICK);
																																}
																																else
																																{
																																	if ((self.impulse == TE_WIZSPIKE))
																																	{
																																		BuyWeapon (TE_WIZSPIKE, 96, DOOR_TOGGLE);
																																	}
																																}
																															}
																														}
																													}
																												}
																											}
																										}
																										else
																										{
																											if ((self.currentmenu == "menu_heavy"))
																											{
																												if ((self.impulse == SPAWNFLAG_SUPERSPIKE))
																												{
																													BuyWeapon (SECRET_YES_SHOOT, 55, 47);
																												}
																												else
																												{
																													if ((self.impulse == SPAWNFLAG_LASER))
																													{
																														BuyWeapon (TE_TELEPORT, 75, 50);
																													}
																													else
																													{
																														if ((self.impulse == AS_MELEE))
																														{
																															BuyWeapon (SECRET_YES_SHOOT, SVC_BIGKICK, RN_SHAMB);
																														}
																														else
																														{
																															if ((self.impulse == SECRET_1ST_DOWN))
																															{
																																BuyWeapon (IDLE5A, 72, 49);
																															}
																															else
																															{
																																if ((self.impulse == MULTICAST_PVS_R))
																																{
																																	BuyWeapon (TE_BLOOD, 81, SVC_FINALE);
																																}
																																else
																																{
																																	if ((self.impulse == TE_LIGHTNING2))
																																	{
																																		BuyWeapon (IDLE2A, 81, 55);
																																	}
																																	else
																																	{
																																		if ((self.impulse == TE_WIZSPIKE))
																																		{
																																			BuyWeapon (TE_LIGHTNINGBLOOD, 56, 54);
																																		}
																																	}
																																}
																															}
																														}
																													}
																												}
																											}
																											else
																											{
																												if ((self.currentmenu == "menu_gadget"))
																												{
																													if ((self.impulse == SPAWNFLAG_SUPERSPIKE))
																													{
																														BuyGadget (IDLE8A, SPAWNFLAG_SUPERSPIKE);
																													}
																													else
																													{
																														if ((self.impulse == SPAWNFLAG_LASER))
																														{
																															BuyGadget (IDLE8A, SPAWNFLAG_LASER);
																														}
																														else
																														{
																															if ((self.impulse == AS_MELEE))
																															{
																																BuyGadget (IDLE8A, AS_MELEE);
																															}
																															else
																															{
																																if ((self.impulse == SECRET_1ST_DOWN))
																																{
																																	BuyGadget (IDLE8A, SECRET_1ST_DOWN);
																																}
																																else
																																{
																																	if ((self.impulse == MULTICAST_PVS_R))
																																	{
																																		BuyGadget (IDLE8A, MULTICAST_PVS_R);
																																		//WeaponAmmo (SPAWNFLAG_SUPERSPIKE);
																																		//WeaponAmmo (SPAWNFLAG_LASER);
																																	}
																																	else
																																	{
																																		if ((self.impulse == TE_LIGHTNING2))
																																		{
																																			BuyGadget (IDLE8A, TE_LIGHTNING2);
																																		}
																																		else
																																		{
																																			if ((self.impulse == TE_WIZSPIKE))
																																			{
																																				BuyGadget (IDLE8A, TE_WIZSPIKE);
																																			}
																																			else
																																			{
																																				if ((self.impulse == SECRET_NO_SHOOT))
																																				{
																																					BuyGadget (IDLE8A, SECRET_NO_SHOOT);
																																				}
																																				else
																																				{
																																					if ((self.impulse == TE_LIGHTNING3))
																																					{
																																						BuyGadget (IDLE8A, TE_LIGHTNING3);
																																					}
																																					else
																																					{
																																						if ((self.impulse == TE_LAVASPLASH))
																																						{
																																							BuyGadget (IDLE8A, TE_LAVASPLASH);
																																						}
																																					}
																																				}
																																			}
																																		}
																																	}
																																}
																															}
																														}
																													}
																												}
																												else
																												{
																													if ((self.currentmenu == "13"))
																													{
																														return;
																													}
																													else
																													{
																														if ((self.currentmenu == "menu_svote"))
																														{
																															if ((self.impulse == SPAWNFLAG_SUPERSPIKE))
																															{
																																stuffcmd (self, "votehs\n");
																																self.currentmenu = "none";
																																return;
																															}
																															if ((self.impulse == SPAWNFLAG_LASER))
																															{
																																stuffcmd (self, "voters\n");
																																self.currentmenu = "none";
																																return;
																															}
																															if ((self.impulse == AS_MELEE))
																															{
																																stuffcmd (self, "votemap\n");
																																self.currentmenu = "none";
																																return;
																															}
																															if ((self.impulse == SECRET_1ST_DOWN))
																															{
																																stuffcmd (self, "voteff\n");
																																self.currentmenu = "none";
																																return;
																															}
																															if ((self.impulse == MULTICAST_PVS_R))
																															{
																																stuffcmd (self, "zombie\n");
																																self.currentmenu = "none";
																																return;
																															}
																														}
																														else
																														{
																															if ((self.currentmenu == "menu_menu"))
																															{
																																if ((self.impulse == SPAWNFLAG_SUPERSPIKE))
																																{
																																	Garage ();
																																	self.currentmenu = "none";
																																	return;
																																}
																																if ((self.impulse == SPAWNFLAG_LASER))
																																{
																																	self.currentmenu = "menu_drop";
																																	return;
																																}
																																if ((self.impulse == AS_MELEE))
																																{
																																	Score ();
																																	return;
																																}
																																if ((self.impulse == SECRET_1ST_DOWN))
																																{
																																	self.currentmenu = "menu_svote";
																																	return;
																																}
																																if ((self.impulse == MULTICAST_PVS_R))
																																{
																																	self.currentmenu = "menu_website";
																																	return;
																																}
																																if ((self.impulse == TE_LIGHTNING2))
																																{
																																	self.currentmenu = "menu_contact";
																																	return;
																																}
																																if ((self.impulse == TE_WIZSPIKE))
																																{
																																	stuffcmd (self, "clear\n");
																																	stuffcmd (self, "cmd topten\n");
																																	stuffcmd (self, "toggleconsole\n");
																																	self.currentmenu = "none";
																																	return;
																																}
																																if ((self.impulse == SECRET_NO_SHOOT))
																																{
																																	stuffcmd (self, "clear\n");
																																	stuffcmd (self, "server\n");
																																	stuffcmd (self, "toggleconsole\n");
																																	self.currentmenu = "none";
																																	return;
																																}
																																if ((self.impulse == TE_LIGHTNING3))
																																{
																																	centerprint (self, "stats and equipment reset");
																																	BasicConfig ();
																																	self.currentmenu = "none";
																																	return;
																																}
																															}
																															else
																															{
																																if ((self.currentmenu == "menu_special"))
																																{
																																	if ((self.impulse == SPAWNFLAG_SUPERSPIKE))
																																	{
																																		Hack ();
																																		self.currentmenu = "none";
																																		return;
																																	}
																																	if ((self.impulse == SPAWNFLAG_LASER))
																																	{
																																		Shield ();
																																		self.currentmenu = "none";
																																		return;
																																	}
																																	if ((self.impulse == AS_MELEE))
																																	{
																																		LocalScan ();
																																		self.currentmenu = "none";
																																		return;
																																	}
																																}
																																else
																																{
																																	if ((self.currentmenu == "775"))
																																	{
																																		return;
																																	}
																																	else
																																	{
																																		if ((self.currentmenu == "774"))
																																		{
																																			return;
																																		}
																																		else
																																		{
																																			if ((self.currentmenu == "motd"))
																																			{
																																				MOTD ();
																																			}
																																			else
																																			{
																																				if ((self.currentmenu == "select_team"))
																																				{
																																					sound (self, CHAN_WEAPON, "buttons/switch02.wav", TRUE, ATTN_NORM);
																																					if ((self.impulse == SPAWNFLAG_SUPERSPIKE))
																																					{
																																						self.ghost = MULTICAST_ALL;
																																						stuffcmd (self, "team Good\n");
																																						self.team = SPAWNFLAG_SUPERSPIKE;
																																						centerprint (self, "You now work for Vault 18.");
																																						bprint (PRINT_HIGH, self.netname);
																																						bprint (PRINT_HIGH, " has joined Vault 18\n");
																																						if ((self.class == MULTICAST_ALL))
																																						{
																																							self.currentmenu = "select_class";
																																						}
																																						if ((self.class != MULTICAST_ALL))
																																						{
																																							self.currentmenu = "none";
																																							self.override = SPAWNFLAG_SUPERSPIKE;
																																							stuffcmd (self, "kill\n");
																																							self.motd_count = MULTICAST_ALL;
																																							if ((self.max_health == SPAWNFLAG_SUPERSPIKE))
																																							{
																																								self.max_health = 90;
																																							}
																																							self.ghost = MULTICAST_ALL;
																																							return;
																																						}
																																						self.motd_count = MULTICAST_ALL;
																																						return;
																																					}
																																					if ((self.impulse == SPAWNFLAG_LASER))
																																					{
																																						self.ghost = MULTICAST_ALL;
																																						stuffcmd (self, "team Evil\n");
																																						self.team = SPAWNFLAG_LASER;
																																						centerprint (self, "You now work for the Enclave.");
																																						bprint (PRINT_HIGH, self.netname);
																																						bprint (PRINT_HIGH, " has joined the Enclave\n");
																																						if ((self.class == MULTICAST_ALL))
																																						{
																																							self.currentmenu = "select_class";
																																						}
																																						if ((self.class != MULTICAST_ALL))
																																						{
																																							self.currentmenu = "none";
																																							self.override = SPAWNFLAG_SUPERSPIKE;
																																							stuffcmd (self, "kill\n");
																																							self.motd_count = MULTICAST_ALL;
																																							if ((self.max_health == SPAWNFLAG_SUPERSPIKE))
																																							{
																																								self.max_health = 90;
																																							}
																																							self.ghost = MULTICAST_ALL;
																																							return;
																																						}
																																						self.motd_count = MULTICAST_ALL;
																																						return;
																																					}
																																					if ((self.impulse == AS_MELEE))
																																					{
																																						other = find (world, classname, "player");
																																						while ((other != world))
																																						{
																																							if ((other.team == SPAWNFLAG_SUPERSPIKE))
																																							{
																																								b1 = (b1 + other.frags);
																																							}
																																							if ((other.team == SPAWNFLAG_LASER))
																																							{
																																								r1 = (r1 + other.frags);
																																							}
																																							other = find (other, classname, "player");
																																						}
																																						if ((r1 > b1))
																																						{
																																							self.impulse = SPAWNFLAG_SUPERSPIKE;
																																							W_PlayerMenu ();
																																							return;
																																						}
																																						if ((b1 > r1))
																																						{
																																							self.impulse = SPAWNFLAG_LASER;
																																							W_PlayerMenu ();
																																							return;
																																						}
																																						if ((r1 == b1))
																																						{
																																							if (((random () * SECRET_NO_SHOOT) <= SECRET_1ST_DOWN))
																																							{
																																								self.impulse = SPAWNFLAG_SUPERSPIKE;
																																							}
																																							else
																																							{
																																								self.impulse = SPAWNFLAG_LASER;
																																							}
																																							W_PlayerMenu ();
																																							return;
																																						}
																																					}
																																				}
																																			}
																																		}
																																	}
																																}
																															}
																														}
																													}
																												}
																											}
																										}
																									}
																								}
																							}
																						}
																					}
																				}
																			}
																		}
																	}
																}
															}
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	if ((self.impulse > AS_MELEE))
	{
		return;
	}
	self.currentmenu = "none";
	return;
};*/
