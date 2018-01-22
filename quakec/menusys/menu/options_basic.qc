
class mitem_playerpreview : mitem_spinnymodel
{
#if defined(FTE_QC_CUSTOMSKINS)
	virtual void(vector pos) item_draw =
	{
		//if you wanted to get more advanced, you could use q3 skins here.
		if (cvar("noskins"))
			setcustomskin(self, "", sprintf("q1upper \"%s\"\nq1lower \"%s\"\n\n", cvar_string("topcolor"), cvar_string("bottomcolor")));
		else if (cvar_string("cl_teamskin") != "")
			setcustomskin(self, "", sprintf("q1upper \"%s\"\nq1lower \"%s\"\nqwskin \"%s\"\n", cvar_string("topcolor"), cvar_string("bottomcolor"), cvar_string("cl_teamskin")));
		else
			setcustomskin(self, "", sprintf("q1upper \"%s\"\nq1lower \"%s\"\nqwskin \"%s\"\n", cvar_string("topcolor"), cvar_string("bottomcolor"), cvar_string("skin")));
	
		super::item_draw(pos);
	};
#endif
};

static string() skinopts =
{
	string opts = "";
	float s = search_begin("skins/*.pcx", TRUE, TRUE);
	if (s < 0)
		return opts;
	float n = search_getsize(s);
	for (float i = 0; i < n; i++)
	{
		string f = substring(search_getfilename(s, i), 6, -5);
		opts = strcat(opts, "\"", f, "\" \"", f, "\" ");
	}
	return opts;
};

var float autocvar_m_pitch = 0.022;
class options_basic : mitem_exmenu
{
	virtual float(string key) isvalid =
	{
		if (key == "m_pitchsign")
			return TRUE;
		return super::isvalid(key);
	};
	virtual string(string key) get =
	{
		if (key == "m_pitchsign")
			return (autocvar_m_pitch<0)?"1":"0";
		return super::get(key);
	};
	virtual void(string key, string newval) set =
	{
		if (key == "m_pitchsign")
		{
			float invert;
			if (stof(newval))
				invert = autocvar_m_pitch > 0;
			else
				invert = autocvar_m_pitch < 0;
			if (invert)
				cvar_set("m_pitch", ftos(-autocvar_m_pitch));
		}
		else
			super::set(key, newval);
	};
};
nonstatic void(mitem_desktop desktop) M_Options_Basic =
{
	mitem_exmenu m;
	m = spawn(options_basic, item_text:_("Basic Options"), item_flags:IF_SELECTABLE, item_command:"m_options");
	desktop.add(m, RS_X_MIN_PARENT_MIN|RS_Y_MIN_PARENT_MIN | RS_X_MAX_PARENT_MAX|RS_Y_MAX_PARENT_MAX, '0 0', '0 0');
	desktop.item_focuschange(m, IF_KFOCUSED);
	m.totop();

	float h = 200 * 0.5;
	//draw title art above the options
	mitem_pic banner = spawn(mitem_pic, item_text:"gfx/ttl_cstm.lmp", item_size_y:24, item_flags:IF_CENTERALIGN);
	m.add(banner, RS_X_MIN_PARENT_MID|RS_Y_MIN_PARENT_MID | RS_X_MAX_OWN_MIN|RS_Y_MAX_PARENT_MID, [(160-160-banner.item_size_x)*0.5, -h-32], [banner.item_size_x, -h-8]);
	//spawn a container frame for the actual options. this provides a scrollbar if we have too many items.
	mitem_frame fr = spawn(mitem_frame, item_flags: IF_SELECTABLE, frame_hasscroll:TRUE);
	m.add(fr, RS_X_MIN_PARENT_MID|RS_Y_MIN_PARENT_MID | RS_X_MAX_PARENT_MID|RS_Y_MAX_OWN_MIN, [-160, -h], [160, h*2]);
	float fl = RS_X_MIN_PARENT_MIN|RS_Y_MIN_PARENT_MIN | RS_X_MAX_PARENT_MAX|RS_Y_MAX_OWN_MIN;
	float pos = 0;
	

	fr.add(menuitemeditt_spawn(_("Player Name"), 		dp("_cl_name", "name"), 			'280 8'), 	fl, [0, pos], [0, 8]); pos += 8;
	fr.add(menuitemeditt_spawn(_("Player Team"), 		"team",						'280 8'), 	fl, [0, pos], [0, 8]); pos += 8;
	fr.add(menuitemcombo_spawn(_("Player Skin"), 		"skin",						'280 8', 	skinopts()), fl, [0, pos], [0, 8]); pos += 8;
																	
																	
	if (assumefalsecheckcommand("topcolor"))
		fr.add(menuitemcolour_spawn(_("Upper Colour"), 	"topcolor", 					'280 8'),	fl, [0, pos], [0, 8]); pos += 8;
	if (assumefalsecheckcommand("bottomcolor"))
		fr.add(menuitemcolour_spawn(_("Lower Colour"), 	"bottomcolor", 					'280 8'),	fl, [0, pos], [0, 8]); pos += 8;	/*aka: arse colour*/
	pos += 8;
	fr.add(menuitemcheck_spawn (_("Invert Mouse"),		"m_pitchsign", 					'280 8'),	fl, [0, pos], [0, 8]); pos += 8;
	fr.add(menuitemslider_spawn(_("Sensitivity"),		"sensitivity", 			'3 20 1',	'280 8'),	fl, [0, pos], [0, 8]); pos += 8;
	fr.add(menuitemslider_spawn(_("Fov"),			"fov", 				'80 130 5',	'280 8'),	fl, [0, pos], [0, 8]); pos += 8;
	fr.add(menuitemslider_spawn(_("Gamma"),			dp("v_gamma", "gamma"), 	'0.4 1.3 0.1',	'280 8'),	fl, [0, pos], [0, 8]); pos += 8;
	fr.add(menuitemslider_spawn(_("Contrast"),		dp("v_contrast", "contrast"), 	'0.8 1.8 0.1', 	'280 8'),	fl, [0, pos], [0, 8]); pos += 8;
	fr.add(menuitemslider_spawn(_("Brightness"),		dp("v_brightness", "brightness"),'0.0 0.5 0.1', '280 8'),	fl, [0, pos], [0, 8]); pos += 8;
	fr.add(menuitemslider_spawn(_("Crosshair"),		"crosshair", 			'0.0 19 1', 	'280 8'),	fl, [0, pos], [0, 8]); pos += 8;
	
	m.add(spawn (mitem_playerpreview, item_text: "progs/player.mdl", firstframe:0, framecount:6, shootframe:119, shootframes:6), RS_X_MIN_PARENT_MID|RS_Y_MIN_PARENT_MID | RS_X_MAX_PARENT_MID|RS_Y_MAX_PARENT_MID, [-200, 12*-16/2], [-40, 12*16/2]);

	addmenuback(m);
};
