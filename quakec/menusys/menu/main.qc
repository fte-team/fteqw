/*
The main / root menu.
Just a load of text with console commands attached.
Choice of buttons available is somewhat dynamic.

There's also some generic kludge crap in here, like menu background tints
*/

/*
Adds a background tint to a (typically) exmenu parent.
In FTE, we use built-in stuff to give a sepia effect.
In DP, we just tint it black.
*/
nonstatic void(mitem_frame m) addmenuback =
{
	if (iscachedpic("menutint"))	//fte internal hacks! meh, admit it. its cool.
		m.add(spawn (mitem_pic, item_text:"menutint", item_alpha:0.5),
												RS_X_MIN_PARENT_MIN|RS_Y_MIN_PARENT_MIN | RS_X_MAX_PARENT_MAX|RS_Y_MAX_PARENT_MAX, [0, 0], [0, 0]);
	else
		m.add(spawn (mitem_fill, item_rgb:'0 0 0.01', item_alpha:0.5),
												RS_X_MIN_PARENT_MIN|RS_Y_MIN_PARENT_MIN | RS_X_MAX_PARENT_MAX|RS_Y_MAX_PARENT_MAX, [0, 0], [0, 0]);
};

/*
helper functions to avoid blowing up in older clients.

*/
#define dp(dpc,qwc) (cvar_type(dpc)?dpc:qwc)
float(string cmd) assumetruecheckcommand =
{
	if (!checkextension("FTE_QC_CHECKCOMMAND"))
		return TRUE;
	return checkcommand(cmd);
};
float(string cmd) assumefalsecheckcommand =
{
	if (!checkextension("FTE_QC_CHECKCOMMAND"))
		return FALSE;
	return checkcommand(cmd);
};



nonstatic void(mitem_desktop desktop) M_Main =
{
	local float y;
	local mitem_exmenu m;

	//no dupes please.
	m = (mitem_exmenu)desktop.findchildtext(_("Main Menu"));
	if (m)
	{
		m.totop();
		return;
	}

	//create a fullscreen frame
	m = spawn(mitem_exmenu, item_text:_("Main Menu"), item_flags:IF_SELECTABLE);
	desktop.add(m, RS_X_MIN_PARENT_MIN|RS_Y_MIN_PARENT_MIN | RS_X_MAX_PARENT_MAX|RS_Y_MAX_PARENT_MAX, '0 0', '0 0');
	desktop.item_focuschange(m, IF_KFOCUSED);
	m.totop();

//	m.item_flags |= IF_NOKILL;
//	m.adda(menuitempic_spawn ("gfx/qplaque.lmp", 	'32 144'), 	'16 4');

	y = 7*-16/2;

	//draw title art above the options
	mitem_pic banner = spawn(mitem_pic, item_text:"gfx/ttl_main.lmp", item_size_y:24, item_flags:IF_CENTERALIGN);
	m.add(banner, RS_X_MIN_PARENT_MID|RS_Y_MIN_PARENT_MID | RS_X_MAX_PARENT_MID|RS_Y_MAX_PARENT_MID, [(160-banner.item_size_x)*0.5, y-32], [(160+banner.item_size_x)*0.5, y-8]);


//a macro, in a desperate attempt at readability
#define menuitemtext_cladd16(m,t,c,y) m.addm(spawn(mitem_text, item_text:t, item_command:c, item_scale:16, item_flags:IF_CENTERALIGN), [0, y], [160, y+16])

#ifdef CSQC
	if (serverkey("constate") != "disconnected")				{menuitemtext_cladd16(m, _("Return To Game"), 			"m_pop",			y); y += 16;}
	if (serverkey("constate") != "disconnected")				{menuitemtext_cladd16(m, isserver?_("End Game"):_("Disconnect"),"m_pop;disconnect",	y); y += 16;} else
#endif
#ifdef CSQC
	if (checkextension("FTE_CSQC_SERVERBROWSER"))
#endif
 										{menuitemtext_cladd16(m, _("Join Server"),			"m_pop;m_servers",	y); y += 16;}
	if (assumetruecheckcommand("map"))					{menuitemtext_cladd16(m, _("New Game"),				"m_pop;m_newgame",	y); y += 16;}
	if (assumefalsecheckcommand("menu_demo"))				{menuitemtext_cladd16(m, _("Demos"),				"m_pop;menu_demo",	y); y += 16;}
	if (assumetruecheckcommand("save") && (isserver()||dp_workarounds))	{menuitemtext_cladd16(m, _("Save"),				"m_pop;m_save",		y); y += 16;}
	if (assumetruecheckcommand("load"))					{menuitemtext_cladd16(m, _("Load"),				"m_pop;m_load",		y); y += 16;}
	if (assumefalsecheckcommand("cef"))					{menuitemtext_cladd16(m, _("Browser"),				"m_pop;cef google.com",	y); y += 16;}
	if (assumefalsecheckcommand("xmpp"))					{menuitemtext_cladd16(m, _("Social"),				"m_pop;xmpp",		y); y += 16;}
	if (assumefalsecheckcommand("irc"))					{menuitemtext_cladd16(m, _("IRC"),				"m_pop;irc /info",	y); y += 16;}
	if (assumefalsecheckcommand("menu_download"))				{menuitemtext_cladd16(m, _("Updates+Packages"),			"m_pop;menu_download",	y); y += 16;}
	if (assumefalsecheckcommand("qi"))					{menuitemtext_cladd16(m, _("Quake Injector"),			"m_pop;qi",		y); y += 16;}
										{menuitemtext_cladd16(m, _("Options"),				"m_pop;m_options",	y); y += 16;}
										{menuitemtext_cladd16(m, _("Quit"), 				"m_pop;m_quit",		y); y += 16;}

#if 1//def CSQC
	//spinny quad/pent, for the luls
	local string it = (random()<0.9)?"progs/quaddama.mdl":"progs/invulner.mdl";
	m.add(spawn (mitem_spinnymodel, item_text: it), RS_X_MIN_PARENT_MID|RS_Y_MIN_PARENT_MID | RS_X_MAX_PARENT_MID|RS_Y_MAX_PARENT_MID, [-160, 12*-16/2], [0, 12*16/2]);
#else
	//menuqc doesn't support entities. shove some random crappy static image there instead.
	local mitem_pic plaque = spawn (mitem_pic, item_text:"gfx/qplaque.lmp", item_alpha:1);
	m.addm(plaque, [(-160-plaque.item_size_x)*0.5, 12*-16/2], [(-160+plaque.item_size_x)*0.5, 12*16/2]);
#endif

	addmenuback(m);
};
