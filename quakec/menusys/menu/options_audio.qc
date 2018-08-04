nonstatic void(mitem_desktop desktop) M_Options_Audio =
{
	local float pos;
	mitem_exmenu m;
	m = spawn(mitem_exmenu, item_text:_("Audio Options"), item_flags:IF_SELECTABLE, item_command:"m_options");
	desktop.add(m, RS_X_MIN_PARENT_MIN|RS_Y_MIN_PARENT_MIN | RS_X_MAX_PARENT_MAX|RS_Y_MAX_PARENT_MAX, '0 0', '0 0');
	desktop.item_focuschange(m, IF_KFOCUSED);
	m.totop();

	float h = 200 * 0.5;
	//draw title art above the options
	mitem_pic banner = spawn(mitem_pic, item_text:"gfx/p_option.lmp", item_size_y:24, item_flags:IF_CENTERALIGN);
	m.add(banner, RS_X_MIN_PARENT_MID|RS_Y_MIN_PARENT_MID | RS_X_MAX_PARENT_MID|RS_Y_MAX_PARENT_MID, [banner.item_size_x*-0.5, -h-32], [banner.item_size_x*0.5, -h-8]);
	
	//spawn a container frame for the actual options. this provides a scrollbar if we have too many items.
	mitem_frame fr = spawn(mitem_frame, item_flags: IF_SELECTABLE, frame_hasscroll:TRUE);
	m.add(fr, RS_X_MIN_PARENT_MID|RS_Y_MIN_PARENT_MID | RS_X_MAX_PARENT_MID|RS_Y_MAX_OWN_MIN, [-160, -h], [160, h*2]);
	
	float fl = RS_X_MIN_PARENT_MIN|RS_Y_MIN_PARENT_MIN | RS_X_MAX_PARENT_MAX|RS_Y_MAX_OWN_MIN;
	pos = 0;

	//add the options
	fr.add(spawn(mitem_text, item_text:_("Restart Sound"), 	item_command:"snd_restart", item_scale:8, item_flags:IF_RIGHTALIGN),		RS_X_MIN_PARENT_MIN|RS_Y_MIN_PARENT_MIN | RS_X_MAX_PARENT_MID|RS_Y_MAX_OWN_MIN, [0, pos], [-8, 8]); pos += 8;
	pos += 8;
	fr.add(menuitemcombo_spawn(_("Sound Device"),	"s_device",			'280 8', cvar_string("_s_device_opts")),	fl, [0, pos], [0, 8]); pos += 8;
	fr.add(menuitemslider_spawn(_("Master Volume"),	"volume",			'0.0 1 0.1', 		'280 8'),		fl, [0, pos], [0, 8]); pos += 8;
	fr.add(menuitemslider_spawn(_("Ambient Volume"),"ambient_level",		'0 0.5 0.05', 		'280 8'),		fl, [0, pos], [0, 8]); pos += 8;
	fr.add(menuitemslider_spawn(_("Self Volume"),	"s_localvolume",		'0 1 0.1', 		'280 8'),		fl, [0, pos], [0, 8]); pos += 8;
	fr.add(menuitemslider_spawn(_("Music Volume"),	"bgmvolume",			'0 0.5 0.05', 		'280 8'),		fl, [0, pos], [0, 8]); pos += 8;
	fr.add(menuitemslider_spawn(_("Channels"),	"s_numspeakers",		'1 6 1', 		'280 8'),		fl, [0, pos], [0, 8]); pos += 8;
	fr.add(menuitemcombo_spawn(_("Audio Quality"),	"s_khz",						'280 8', _(
												"11025	\"11025hz (vanilla quake)\" "
												"22050	\"22050hz\" "
												"44100	\"44100hz (cd quality)\" "
												"48000	\"48000hz (dvd quality)\" "
												"96000	\"96000hz\" "
												"192000	\"192000hz\" "
																)),	fl, [0, pos], [0, 8]); pos += 8;
	fr.add(menuitemcheck_spawn(_("Doppler"),	"s_doppler", 						'280 8'),		fl, [0, pos], [0, 8]); pos += 8;
	fr.add(menuitemcheck_spawn(_("8bit audio"),	"s_loadas8bit", 					'280 8'),		fl, [0, pos], [0, 8]); pos += 8;
	fr.add(menuitemcheck_spawn(_("Swap Speakers"),	"s_swapstereo", 					'280 8'),		fl, [0, pos], [0, 8]); pos += 8;
	fr.add(menuitemslider_spawn(_("Latency"),	"s_mixahead",			'0.1 0.3 0.01',		'280 8'),		fl, [0, pos], [0, 8]); pos += 8;
	fr.add(menuitemcheck_spawn(_("Disable Sound"),	"nosound", 						'280 8'),		fl, [0, pos], [0, 8]); pos += 8;
	//ambient fade
	fr.add(menuitemcheck_spawn(_("Static Sounds"),	"cl_staticsounds", 					'280 8'),		fl, [0, pos], [0, 8]); pos += 8;
	fr.add(menuitemcheck_spawn(_("Mix in Background"),"s_inactive", 					'280 8'),		fl, [0, pos], [0, 8]); pos += 8;

	pos += 8;
	fr.add(menuitemcombo_spawn(_("Microphone Device"),	"cl_voip_capturedevice",		'280 8', cvar_string("_cl_voip_capturedevice_opts")),
																fl, [0, pos], [0, 8]); pos += 8;
	fr.add(menuitemslider_spawn(_("VOIP Playback Vol"),"cl_voip_play",		'0 2 0.1', 	'280 8'),		fl, [0, pos], [0, 8]); pos += 8;
	fr.add(menuitemcheck_spawn(_("VOIP Test"),		"cl_voip_test", 			'280 8'),		fl, [0, pos], [0, 8]); pos += 8;
	fr.add(menuitemslider_spawn(_("VOIP Record Vol"),	"cl_voip_micamp",	'0 4 0.1', 	'280 8'),		fl, [0, pos], [0, 8]); pos += 8;
	fr.add(menuitemcombo_spawn(_("VOIP Mode"),		"cl_voip_send",				'280 8', _(
											"0 \"Push-To-Talk\" 1 "
											"\"Voice Activation\" "
											"2 \"Continuous\""
														)),		fl, [0, pos], [0, 8]); pos += 8;
	//VAD threshhold
	//ducking
	//noise cancelation
	fr.add(menuitemcombo_spawn(_("VOIP Codec"),		"cl_voip_codec",			'280 8',_(
											"0	\"speex (narrow 11khz)\" "
											//"1	\"raw (wasteful)\" "
											"2	\"opus\" "
											"3	\"speex (narrow 8khz)\" "
											"4	\"speex (wide 16khz)\" "
											"5	\"speex (ultrawide 32khz)\" "
														)),		fl, [0, pos], [0, 8]); pos += 8;
														
	fr.add(menuitemslider_spawn(_("Opus bitrate"),		"cl_voip_bitrate",	'0.5 128 0.5','280 8'),		fl, [0, pos], [0, 8]); pos += 8;

	addmenuback(m);
};