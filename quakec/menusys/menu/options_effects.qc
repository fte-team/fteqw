nonstatic void(mitem_desktop desktop) M_Options_Effects =
{
	mitem_exmenu m;
	m = spawn(mitem_exmenu, item_text:_("Effects Options"), item_flags:IF_SELECTABLE, item_command:"m_options");
	desktop.add(m, RS_X_MIN_PARENT_MIN|RS_Y_MIN_PARENT_MIN | RS_X_MAX_PARENT_MAX|RS_Y_MAX_PARENT_MAX, '0 0', '0 0');
	desktop.item_focuschange(m, IF_KFOCUSED);
	m.totop();

	float h = 200 * 0.5;
	//draw title art above the options
	mitem_pic banner = spawn(mitem_pic, item_text:"gfx/p_option.lmp", item_size_y:24, item_flags:IF_CENTERALIGN);
	m.add(banner, RS_X_MIN_PARENT_MID|RS_Y_MIN_PARENT_MID | RS_X_MAX_OWN_MIN|RS_Y_MAX_PARENT_MID, [(160-160-banner.item_size_x)*0.5, -h-32], [banner.item_size_x, -h-8]);
	//spawn a container frame for the actual options. this provides a scrollbar if we have too many items.
	mitem_frame fr = spawn(mitem_frame, item_flags: IF_SELECTABLE, frame_hasscroll:TRUE);
	m.add(fr, RS_X_MIN_PARENT_MID|RS_Y_MIN_PARENT_MID | RS_X_MAX_PARENT_MID|RS_Y_MAX_OWN_MIN, [-160, -h], [160, h*2]);
	float fl = RS_X_MIN_PARENT_MIN|RS_Y_MIN_PARENT_MIN | RS_X_MAX_PARENT_MAX|RS_Y_MAX_OWN_MIN;
	float pos = 0;

	fr.add(menuitemcheck_spawn(_("Show Framerate"),			dp("showfps", "show_fps"), 		'280 8'),	fl, [0, pos], [0, 8]); pos += 8;
	fr.add(menuitemcheck_spawn(_("Bloom"),				"r_bloom", 				'280 8'),	fl, [0, pos], [0, 8]); pos += 8;
	if (assumefalsecheckcommand("r_drawflat"))
		fr.add(menuitemcheck_spawn(_("Simple Textures"),	"r_drawflat", 				'280 8'),	fl, [0, pos], [0, 8]); pos += 8;
	if (assumefalsecheckcommand("r_softwarebanding"))
		fr.add(menuitemcheck_spawn(_("Colourmapped Rendering"),	"r_softwarebanding", 			'280 8'),	fl, [0, pos], [0, 8]); pos += 8;
	fr.add(menuitemcheck_spawn(_("HDR"),				"r_hdr_irisadaptation", 		'280 8'),	fl, [0, pos], [0, 8]); pos += 8;
	fr.add(menuitemcheck_spawn(_("Coronas"),			"r_coronas", 				'280 8'),	fl, [0, pos], [0, 8]); pos += 8;
	fr.add(menuitemcheck_spawn(_("High Res Textures"),		"gl_load24bit", 			'280 8'),	fl, [0, pos], [0, 8]); pos += 8;
	fr.add(menuitemcheck_spawn(_("Relief Mapping"),			"r_glsl_offsetmapping", 		'280 8'),	fl, [0, pos], [0, 8]); pos += 8;
	fr.add(menuitemcheck_spawn(_("Realtime Dynamic Lights"),	"r_shadow_realtime_dlight", 		'280 8'),	fl, [0, pos], [0, 8]); pos += 8;
	fr.add(menuitemcheck_spawn(_("Realtime World Lighting"),	"r_shadow_realtime_world", 		'280 8'),	fl, [0, pos], [0, 8]); pos += 8;


	fr.add(menuitemslider_spawn(_("Particle Density"),		"r_part_density",	'0.25 4 0.25',	'280 8'),	fl, [0, pos], [0, 8]); pos += 8;

	fr.add(menuitemcombo_spawn(_("Water Effects"),			"r_waterstyle",				'280 8', _(
																		"1	\"Classic\" "
																		"2	\"Ripples\" "
																		"3	\"Reflections\" "
															)),	fl, [0, pos], [0, 8]); pos += 8;
	if (assumefalsecheckcommand("r_projection"))
		fr.add(menuitemcombo_spawn(_("View Projection"),		"r_projection",				'280 8', _(
																		"0	\"Standard\" "
																		"1	\"Stereographic / Pannini\" "
																		"2	\"Fish-Eye\" "
																		"3	\"Panoramic\" "
																		"4	\"Lambert Azimuthal Equal-Area\" "
																		"5	\"Equirectangular\" "
															)),	fl, [0, pos], [0, 8]); pos += 8;
	if (assumefalsecheckcommand("ffov"))
		fr.add(menuitemcombo_spawn(_("View Projection Angle"),		"ffov",					'280 8', _(
																		"90	\"Normal\" "
																		"180	\"180 degrees\" "
																		"270	\"270 degrees\" "
																		"360	\"360 degrees\" "
															)),	fl, [0, pos], [0, 8]); pos += 8;

	fr.add(spawn(mitem_text, item_text:_("Apply"), 	item_command:"vid_restart", item_scale:8, item_flags:IF_RIGHTALIGN),	RS_X_MIN_PARENT_MIN|RS_Y_MIN_PARENT_MIN | RS_X_MAX_PARENT_MID|RS_Y_MAX_OWN_MIN, [0, pos], [-8, 8]); pos += 8;

	addmenuback(m);
};

