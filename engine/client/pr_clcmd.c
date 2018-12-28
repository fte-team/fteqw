//file for builtin implementations relevent to only clientside VMs (menu+csqc).
#include "quakedef.h"

#include "pr_common.h"
#include "shader.h"

#if defined(CSQC_DAT) || defined(MENU_DAT)

//these two global qcinput variables are the current scan code being passed to qc, if valid. this protects against protected apis where the qc just passes stuff through.
int qcinput_scan;
int qcinput_unicode;

//QC key codes are based upon DP's keycode constants. This is on account of menu.dat coming first.
int MP_TranslateFTEtoQCCodes(int code)
{
	switch(code)
	{
	case K_TAB:				return 9;
	case K_ENTER:			return 13;
	case K_ESCAPE:			return 27;
	case K_SPACE:			return 32;
	case K_BACKSPACE:		return 127;
	case K_UPARROW:			return 128;
	case K_DOWNARROW:		return 129;
	case K_LEFTARROW:		return 130;
	case K_RIGHTARROW:		return 131;
	case K_LALT:			return 132;
	case K_RALT:			return -K_RALT;
	case K_LCTRL:			return 133;
	case K_RCTRL:			return -K_RCTRL;
	case K_LSHIFT:			return 134;
	case K_RSHIFT:			return -K_RSHIFT;
	case K_F1:				return 135;
	case K_F2:				return 136;
	case K_F3:				return 137;
	case K_F4:				return 138;
	case K_F5:				return 139;
	case K_F6:				return 140;
	case K_F7:				return 141;
	case K_F8:				return 142;
	case K_F9:				return 143;
	case K_F10:				return 144;
	case K_F11:				return 145;
	case K_F12:				return 146;
	case K_INS:				return 147;
	case K_DEL:				return 148;
	case K_PGDN:			return 149;
	case K_PGUP:			return 150;
	case K_HOME:			return 151;
	case K_END:				return 152;
	case K_PAUSE:			return 153;
	case K_KP_NUMLOCK:		return 154;
	case K_CAPSLOCK:		return 155;
	case K_SCRLCK:			return 156;
	case K_KP_INS:			return 157;
	case K_KP_END:			return 158;
	case K_KP_DOWNARROW:	return 159;
	case K_KP_PGDN:			return 160;
	case K_KP_LEFTARROW:	return 161;
	case K_KP_5:			return 162;
	case K_KP_RIGHTARROW:	return 163;
	case K_KP_HOME:			return 164;
	case K_KP_UPARROW:		return 165;
	case K_KP_PGUP:			return 166;
	case K_KP_DEL:			return 167;
	case K_KP_SLASH:		return 168;
	case K_KP_STAR:			return 169;
	case K_KP_MINUS:		return 170;
	case K_KP_PLUS:			return 171;
	case K_KP_ENTER:		return 172;
	case K_KP_EQUALS:		return 173;
	case K_PRINTSCREEN:		return 174;

	case K_MOUSE1:			return 512;
	case K_MOUSE2:			return 513;
	case K_MOUSE3:			return 514;
	case K_MWHEELUP:		return 515;
	case K_MWHEELDOWN:		return 516;
	case K_MOUSE4:			return 517;
	case K_MOUSE5:			return 518;
	case K_MOUSE6:			return 519;
	case K_MOUSE7:			return 520;
	case K_MOUSE8:			return 521;
	case K_MOUSE9:			return 522;
	case K_MOUSE10:			return 523;
//	case K_MOUSE11:			return 524;
//	case K_MOUSE12:			return 525;
//	case K_MOUSE13:			return 526;
//	case K_MOUSE14:			return 527;
//	case K_MOUSE15:			return 528;
//	case K_MOUSE16:			return 529;

	case K_JOY1:			return 768;
	case K_JOY2:			return 769;
	case K_JOY3:			return 770;
	case K_JOY4:			return 771;
//	case K_JOY5:			return 772;
//	case K_JOY6:			return 773;
//	case K_JOY7:			return 774;
//	case K_JOY8:			return 775;
//	case K_JOY9:			return 776;
//	case K_JOY10:			return 777;
//	case K_JOY11:			return 778;
//	case K_JOY12:			return 779;
//	case K_JOY13:			return 780;
//	case K_JOY14:			return 781;
//	case K_JOY15:			return 782;
//	case K_JOY16:			return 783;

	case K_AUX1:			return 784;
	case K_AUX2:			return 785;
	case K_AUX3:			return 786;
	case K_AUX4:			return 787;
	case K_AUX5:			return 788;
	case K_AUX6:			return 789;
	case K_AUX7:			return 790;
	case K_AUX8:			return 791;
	case K_AUX9:			return 792;
	case K_AUX10:			return 793;
	case K_AUX11:			return 794;
	case K_AUX12:			return 795;
	case K_AUX13:			return 796;
	case K_AUX14:			return 797;
	case K_AUX15:			return 798;
	case K_AUX16:			return 799;
	case K_AUX17:			return 800;
	case K_AUX18:			return 801;
	case K_AUX19:			return 802;
	case K_AUX20:			return 803;
	case K_AUX21:			return 804;
	case K_AUX22:			return 805;
	case K_AUX23:			return 806;
	case K_AUX24:			return 807;
	case K_AUX25:			return 808;
	case K_AUX26:			return 809;
	case K_AUX27:			return 810;
	case K_AUX28:			return 811;
	case K_AUX29:			return 812;
	case K_AUX30:			return 813;
	case K_AUX31:			return 814;
	case K_AUX32:			return 815;

	case K_GP_DPAD_UP:			return 816;
	case K_GP_DPAD_DOWN:		return 817;
	case K_GP_DPAD_LEFT:		return 818;
	case K_GP_DPAD_RIGHT:		return 819;
	case K_GP_START:			return 820;
	case K_GP_BACK:				return 821;
	case K_GP_LEFT_THUMB:		return 822;
	case K_GP_RIGHT_THUMB:		return 823;
	case K_GP_LEFT_SHOULDER:	return 824;
	case K_GP_RIGHT_SHOULDER:	return 825;
	case K_GP_A:				return 826;
	case K_GP_B:				return 827;
	case K_GP_X:				return 828;
	case K_GP_Y:				return 829;
	case K_GP_LEFT_TRIGGER:		return 830;
	case K_GP_RIGHT_TRIGGER:	return 831;
	case K_GP_LEFT_THUMB_UP:	return 832;
	case K_GP_LEFT_THUMB_DOWN:	return 833;
	case K_GP_LEFT_THUMB_LEFT:	return 834;
	case K_GP_LEFT_THUMB_RIGHT:	return 835;
	case K_GP_RIGHT_THUMB_UP:	return 836;
	case K_GP_RIGHT_THUMB_DOWN:	return 837;
	case K_GP_RIGHT_THUMB_LEFT:	return 838;
	case K_GP_RIGHT_THUMB_RIGHT:return 839;
	case K_JOY_UP:				return 840;
	case K_JOY_DOWN:			return 841;
	case K_JOY_LEFT:			return 842;
	case K_JOY_RIGHT:			return 843;


	case K_F13:
	case K_F14:
	case K_F15:
	case K_POWER:
	case K_LWIN:
	case K_RWIN:
	case K_VOLUP:
	case K_VOLDOWN:
	case K_APP:
	case K_SEARCH:			return -code;

	default:
		if (code < 0)	//negative values are 'qc-native' keys, for stuff that the api lacks.
			return -code;
		if (code >= 0 && code < 128)	//ascii codes identical
			return code;
		return -code;	//unknown key.
	}
}

int MP_TranslateQCtoFTECodes(int code)
{
	switch(code)
	{
	case 9:			return K_TAB;
	case 13:		return K_ENTER;
	case 27:		return K_ESCAPE;
	case 32:		return K_SPACE;
	case 127:		return K_BACKSPACE;
	case 128:		return K_UPARROW;
	case 129:		return K_DOWNARROW;
	case 130:		return K_LEFTARROW;
	case 131:		return K_RIGHTARROW;
	case 132:		return K_LALT;
	case 133:		return K_LCTRL;
	case 134:		return K_LSHIFT;
	case 135:		return K_F1;
	case 136:		return K_F2;
	case 137:		return K_F3;
	case 138:		return K_F4;
	case 139:		return K_F5;
	case 140:		return K_F6;
	case 141:		return K_F7;
	case 142:		return K_F8;
	case 143:		return K_F9;
	case 144:		return K_F10;
	case 145:		return K_F11;
	case 146:		return K_F12;
	case 147:		return K_INS;
	case 148:		return K_DEL;
	case 149:		return K_PGDN;
	case 150:		return K_PGUP;
	case 151:		return K_HOME;
	case 152:		return K_END;
	case 153:		return K_PAUSE;
	case 154:		return K_KP_NUMLOCK;
	case 155:		return K_CAPSLOCK;
	case 156:		return K_SCRLCK;
	case 157:		return K_KP_INS;
	case 158:		return K_KP_END;
	case 159:		return K_KP_DOWNARROW;
	case 160:		return K_KP_PGDN;
	case 161:		return K_KP_LEFTARROW;
	case 162:		return K_KP_5;
	case 163:		return K_KP_RIGHTARROW;
	case 164:		return K_KP_HOME;
	case 165:		return K_KP_UPARROW;
	case 166:		return K_KP_PGUP;
	case 167:		return K_KP_DEL;
	case 168:		return K_KP_SLASH;
	case 169:		return K_KP_STAR;
	case 170:		return K_KP_MINUS;
	case 171:		return K_KP_PLUS;
	case 172:		return K_KP_ENTER;
	case 173:		return K_KP_EQUALS;
	case 174:		return K_PRINTSCREEN;

	case 512:		return K_MOUSE1;
	case 513:		return K_MOUSE2;
	case 514:		return K_MOUSE3;
	case 515:		return K_MWHEELUP;
	case 516:		return K_MWHEELDOWN;
	case 517:		return K_MOUSE4;
	case 518:		return K_MOUSE5;
	case 519:		return K_MOUSE6;
	case 520:		return K_MOUSE7;
	case 521:		return K_MOUSE8;
	case 522:		return K_MOUSE9;
	case 523:		return K_MOUSE10;
//	case 524:		return K_MOUSE11;
//	case 525:		return K_MOUSE12;
//	case 526:		return K_MOUSE13;
//	case 527:		return K_MOUSE14;
//	case 528:		return K_MOUSE15;
//	case 529:		return K_MOUSE16;

	case 768:		return K_JOY1;
	case 769:		return K_JOY2;
	case 770:		return K_JOY3;
	case 771:		return K_JOY4;
//	case 772:		return K_JOY5;
//	case 773:		return K_JOY6;
//	case 774:		return K_JOY7;
//	case 775:		return K_JOY8;
//	case 776:		return K_JOY9;
//	case 777:		return K_JOY10;
//	case 778:		return K_JOY11;
//	case 779:		return K_JOY12;
//	case 780:		return K_JOY13;
//	case 781:		return K_JOY14;
//	case 782:		return K_JOY15;
//	case 783:		return K_JOY16;

	case 784:		return K_AUX1;
	case 785:		return K_AUX2;
	case 786:		return K_AUX3;
	case 787:		return K_AUX4;
	case 788:		return K_AUX5;
	case 789:		return K_AUX6;
	case 790:		return K_AUX7;
	case 791:		return K_AUX8;
	case 792:		return K_AUX9;
	case 793:		return K_AUX10;
	case 794:		return K_AUX11;
	case 795:		return K_AUX12;
	case 796:		return K_AUX13;
	case 797:		return K_AUX14;
	case 798:		return K_AUX15;
	case 799:		return K_AUX16;
	case 800:		return K_AUX17;
	case 801:		return K_AUX18;
	case 802:		return K_AUX19;
	case 803:		return K_AUX20;
	case 804:		return K_AUX21;
	case 805:		return K_AUX22;
	case 806:		return K_AUX23;
	case 807:		return K_AUX24;
	case 808:		return K_AUX25;
	case 809:		return K_AUX26;
	case 810:		return K_AUX27;
	case 811:		return K_AUX28;
	case 812:		return K_AUX29;
	case 813:		return K_AUX30;
	case 814:		return K_AUX31;
	case 815:		return K_AUX32;

	case 816:		return K_GP_DPAD_UP;
	case 817:		return K_GP_DPAD_DOWN;
	case 818:		return K_GP_DPAD_LEFT;
	case 819:		return K_GP_DPAD_RIGHT;
	case 820:		return K_GP_START;
	case 821:		return K_GP_BACK;
	case 822:		return K_GP_LEFT_THUMB;
	case 823:		return K_GP_RIGHT_THUMB;
	case 824:		return K_GP_LEFT_SHOULDER;
	case 825:		return K_GP_RIGHT_SHOULDER;
	case 826:		return K_GP_A;
	case 827:		return K_GP_B;
	case 828:		return K_GP_X;
	case 829:		return K_GP_Y;
	case 830:		return K_GP_LEFT_TRIGGER;
	case 831:		return K_GP_RIGHT_TRIGGER;
	case 832:		return K_GP_LEFT_THUMB_UP;
	case 833:		return K_GP_LEFT_THUMB_DOWN;
	case 834:		return K_GP_LEFT_THUMB_LEFT;
	case 835:		return K_GP_LEFT_THUMB_RIGHT;
	case 836:		return K_GP_RIGHT_THUMB_UP;
	case 837:		return K_GP_RIGHT_THUMB_DOWN;
	case 838:		return K_GP_RIGHT_THUMB_LEFT;
	case 839:		return K_GP_RIGHT_THUMB_RIGHT;
	case 840:		return K_JOY_UP;
	case 841:		return K_JOY_DOWN;
	case 842:		return K_JOY_LEFT;
	case 843:		return K_JOY_RIGHT;
	default:		
		if (code < 0)	//negative values are 'fte-native' keys, for stuff that the api lacks.
			return -code;
		if (code >= 0 && code < 128)
			return code;
		return -code;	//these keys are not supported in fte. use negatives so that they can be correctly mapped back to qc codes if the need arises. no part of the engine will recognise them.
	}
}

//string	findkeysforcommand(string command) = #610;
void QCBUILTIN PF_cl_findkeysforcommand (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char *cmdname = PR_GetStringOfs(prinst, OFS_PARM0);
	int bindmap = (prinst->callargc > 1)?G_FLOAT(OFS_PARM1):0;
	int keynums[16];
	char keyname[512];
	size_t u;

	M_FindKeysForCommand(bindmap, 0, cmdname, keynums, NULL, countof(keynums));

	keyname[0] = '\0';
	for (u = 0; u < countof(keynums); u++)
	{
		if (keynums[u] >= 0)
			keynums[u] = MP_TranslateFTEtoQCCodes(keynums[u]);
		else if (u >= 2)	//would ideally be 0, but nexuiz would bug out then.
			break;
		Q_strncatz (keyname, va(" \'%i\'", keynums[u]), sizeof(keyname));
	}

	RETURN_TSTRING(keyname);
}
void QCBUILTIN PF_cl_findkeysforcommandex (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char *cmdname = PR_GetStringOfs(prinst, OFS_PARM0);
	int bindmap = (prinst->callargc > 1)?G_FLOAT(OFS_PARM1):0;
	int keynums[256];
	int keymods[countof(keynums)];
	char keyname[512];
	int i, count = M_FindKeysForBind(bindmap, cmdname, keynums, keymods, countof(keynums));

	keyname[0] = '\0';

	for (i = 0; i < count; i++)
	{
		if (i)
			Q_strncatz (keyname, " ", sizeof(keyname));
		Q_strncatz (keyname, Key_KeynumToString(keynums[i], keymods[i]), sizeof(keyname));
	}

	RETURN_TSTRING(keyname);
}

void QCBUILTIN PF_cl_getkeybind (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int bindmap = (prinst->callargc > 1)?G_FLOAT(OFS_PARM1):0;
	int modifier = (prinst->callargc > 2)?G_FLOAT(OFS_PARM2):0;
	char *binding = Key_GetBinding(MP_TranslateQCtoFTECodes(G_FLOAT(OFS_PARM0)), bindmap, modifier);
	RETURN_TSTRING(binding);
}
void QCBUILTIN PF_cl_setkeybind (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int keynum = MP_TranslateQCtoFTECodes(G_FLOAT(OFS_PARM0));
	const char *binding = PR_GetStringOfs(prinst, OFS_PARM1);
	int bindmap = (prinst->callargc > 2)?G_FLOAT(OFS_PARM2):0;
	int modifier = (prinst->callargc > 3)?G_FLOAT(OFS_PARM3):~0;

	if (bindmap > 0 && bindmap <= KEY_MODIFIER_ALTBINDMAP)
		modifier = (bindmap-1) | KEY_MODIFIER_ALTBINDMAP;	//ignore the modifier if we're setting into a bindmap...

	Key_SetBinding(keynum, modifier, binding, RESTRICT_INSECURE);
}

void QCBUILTIN PF_cl_stringtokeynum(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int i;
	int modifier;
	const char *s;

	s = PR_GetStringOfs(prinst, OFS_PARM0);
	i = Key_StringToKeynum(s, &modifier);
	if (i < 0 || modifier != ~0)
	{
		G_FLOAT(OFS_RETURN) = -1;
		return;
	}
	i = MP_TranslateFTEtoQCCodes(i);
	G_FLOAT(OFS_RETURN) = i;
}

//string	keynumtostring(float keynum) = #609;
void QCBUILTIN PF_cl_keynumtostring (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int code = G_FLOAT(OFS_PARM0);

	code = MP_TranslateQCtoFTECodes (code);

	RETURN_TSTRING(Key_KeynumToString(code, 0));
}

void QCBUILTIN PF_cl_setwindowcaption(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char *newcaption = PR_GetStringOfs(prinst, OFS_PARM0);
	if (!cl.windowtitle || strcmp(cl.windowtitle, newcaption))
	{
		Z_Free(cl.windowtitle);
		cl.windowtitle = NULL;
		if (*newcaption)
			cl.windowtitle = Z_StrDup(newcaption);
		CL_UpdateWindowTitle();
	}
}

//#343
void QCBUILTIN PF_cl_setcursormode (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	world_t *world = prinst->parms->user;
	if (G_FLOAT(OFS_PARM0))
		key_dest_absolutemouse |= world->keydestmask;
	else
		key_dest_absolutemouse &= ~world->keydestmask;

	if (prinst->callargc>1)
	{
		struct key_cursor_s *m = &key_customcursor[(world->keydestmask==kdm_game)?kc_game:kc_menu];
		Q_strncpyz(m->name, PR_GetStringOfs(prinst, OFS_PARM1), sizeof(m->name));
		m->hotspot[0] = (prinst->callargc>2)?G_FLOAT(OFS_PARM2+0):0;
		m->hotspot[1] = (prinst->callargc>2)?G_FLOAT(OFS_PARM2+1):0;
		m->scale = (prinst->callargc>2)?G_FLOAT(OFS_PARM2+2):0;
		if (m->scale <= 0)
			m->scale = 1;
		m->dirty = true;
	}
}

void QCBUILTIN PF_cl_getcursormode (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	world_t *world = prinst->parms->user;
	if (G_FLOAT(OFS_PARM0))
		G_FLOAT(OFS_RETURN) = Key_MouseShouldBeFree();
	else if (key_dest_absolutemouse & world->keydestmask)
		G_FLOAT(OFS_RETURN) = true;
	else
		G_FLOAT(OFS_RETURN) = false;
}

void QCBUILTIN PF_cl_playingdemo (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	switch(cls.demoplayback)
	{
	case DPB_NONE:
		G_FLOAT(OFS_RETURN) = 0;
		break;
	case DPB_MVD:
	case DPB_EZTV:
		G_FLOAT(OFS_RETURN) = 2;
		break;
	default:
		G_FLOAT(OFS_RETURN) = 1;
		break;
	}
}

void QCBUILTIN PF_cl_runningserver (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
#ifdef CLIENTONLY
	G_FLOAT(OFS_RETURN) = false;
#else
	if (sv.state != ss_dead)
	{
		if (sv.allocated_client_slots > 1)
			G_FLOAT(OFS_RETURN) = true;
		else
			G_FLOAT(OFS_RETURN) = 0.5;	//give some half-way value if we're singleplayer. NOTE: DP returns 0 in this case, which is kinda useless for things like deciding whether a 'save' menu option can be used.
	}
	else
		G_FLOAT(OFS_RETURN) = false;
#endif
}



#ifdef HAVE_MEDIA_DECODER

// #487 float(string name, string video="http:") gecko_create
void QCBUILTIN PF_cs_media_create (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char *shadername = PR_GetStringOfs(prinst, OFS_PARM0);
	const char *videoname = (prinst->callargc > 1)?PR_GetStringOfs(prinst, OFS_PARM1):"http:";
	cin_t *cin;
	cin = R_ShaderGetCinematic(R_RegisterShader(shadername, SUF_2D, va(
				"{\n"
					"program default2d\n"
					"{\n"
						"videomap %s\n"
						"rgbgen vertex\n"
						"alphagen vertex\n"
						"blendfunc gl_one gl_one_minus_src_alpha\n"
						"nodepth\n"
					"}\n"
				"}\n",		
			videoname)));

	if (cin)
	{
		G_FLOAT(OFS_RETURN) = 1;
		Media_Send_Reset(cin);
	}
	else
		G_FLOAT(OFS_RETURN) = 0;
}
// #488 void(string name) gecko_destroy
void QCBUILTIN PF_cs_media_destroy (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char *shadername = PR_GetStringOfs(prinst, OFS_PARM0);
	shader_t *shader = R_ShaderFind(shadername);
	cin_t *cin;
	if (!shader)
		return;
	cin = R_ShaderGetCinematic(shader);
	if (cin && shader->uses > 1)
	{
		if (shader->uses > 1)
			Media_Send_Reset(cin);	//will still be active afterwards.
	}
	R_UnloadShader(shader);
}
// #489 void(string name, string URI) gecko_navigate
void QCBUILTIN PF_cs_media_command (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char *shader = PR_GetStringOfs(prinst, OFS_PARM0);
	const char *command = PR_GetStringOfs(prinst, OFS_PARM1);
	cin_t *cin;
	cin = R_ShaderFindCinematic(shader);
	if (!cin)
		return;
	Media_Send_Command(cin, command);
}
// #490 float(string name, float key, float eventtype, optional float charcode) gecko_keyevent
void QCBUILTIN PF_cs_media_keyevent (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char *shader = PR_GetStringOfs(prinst, OFS_PARM0);
	int key = G_FLOAT(OFS_PARM1);
	int eventtype = G_FLOAT(OFS_PARM2);
	int charcode = (prinst->callargc>3)?G_FLOAT(OFS_PARM3):((key>127)?0:key);
	cin_t *cin;
	cin = R_ShaderFindCinematic(shader);
	G_FLOAT(OFS_RETURN) = 0;
	if (!cin)
		return;
	Media_Send_KeyEvent(cin, MP_TranslateQCtoFTECodes(key), charcode, eventtype);
	G_FLOAT(OFS_RETURN) = 1;
}
// #491 void(string name, float x, float y) gecko_mousemove
void QCBUILTIN PF_cs_media_mousemove (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char *shader = PR_GetStringOfs(prinst, OFS_PARM0);
	float posx = G_FLOAT(OFS_PARM1);
	float posy = G_FLOAT(OFS_PARM2);
	cin_t *cin;
	cin = R_ShaderFindCinematic(shader);
	if (!cin)
		return;
	Media_Send_MouseMove(cin, posx, posy);
}
// #492 void(string name, float w, float h) gecko_resize
void QCBUILTIN PF_cs_media_resize (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char *shader = PR_GetStringOfs(prinst, OFS_PARM0);
	float sizex = G_FLOAT(OFS_PARM1);
	float sizey = G_FLOAT(OFS_PARM2);
	cin_t *cin;
	cin = R_ShaderFindCinematic(shader);
	if (!cin)
		return;
	Media_Send_Resize(cin, sizex, sizey);
}
// #493 vector(string name) gecko_get_texture_extent
void QCBUILTIN PF_cs_media_get_texture_extent (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char *shader = PR_GetStringOfs(prinst, OFS_PARM0);
	float *ret = G_VECTOR(OFS_RETURN);
	int sx = 0, sy = 0;
	float aspect = 0;
	cin_t *cin;
	cin = R_ShaderFindCinematic(shader);
	if (cin)
		Media_Send_GetSize(cin, &sx, &sy, &aspect);
	ret[0] = sx;
	ret[1] = sy;
	ret[2] = aspect;
}
void QCBUILTIN PF_cs_media_getproperty (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char *shader = PR_GetStringOfs(prinst, OFS_PARM0);
	const char *propname = PR_GetStringOfs(prinst, OFS_PARM1);
	const char *ret = NULL;
	cin_t *cin;
	cin = R_ShaderFindCinematic(shader);
	if (cin)
		ret = Media_Send_GetProperty(cin, propname);

	G_INT(OFS_RETURN) = ret?PR_TempString(prinst, ret):0;
}
void QCBUILTIN PF_cs_media_getstate (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char *shader = PR_GetStringOfs(prinst, OFS_PARM0);
	cinstates_t ret = CINSTATE_INVALID;
	cin_t *cin;
	cin = R_ShaderFindCinematic(shader);
	if (cin)
		ret = Media_GetState(cin);

	G_FLOAT(OFS_RETURN) = ret;
}
void QCBUILTIN PF_cs_media_setstate (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char *shader = PR_GetStringOfs(prinst, OFS_PARM0);
	cinstates_t state = G_FLOAT(OFS_PARM1);
	cin_t *cin;
	cin = R_ShaderFindCinematic(shader);
	if (cin)
		Media_SetState(cin, state);
}

void QCBUILTIN PF_cs_media_restart (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char *shader = PR_GetStringOfs(prinst, OFS_PARM0);
	cin_t *cin;
	cin = R_ShaderFindCinematic(shader);
	if (cin)
		Media_Send_Reset(cin);
}
#endif

void QCBUILTIN PF_soundlength (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char *sample = PR_GetStringOfs(prinst, OFS_PARM0);

	sfx_t *sfx = S_PrecacheSound(sample);
	if (sfx && sfx->loadstate == SLS_LOADING)
		COM_WorkerPartialSync(sfx, &sfx->loadstate, SLS_LOADING);
	if (!sfx || sfx->loadstate != SLS_LOADED)
		G_FLOAT(OFS_RETURN) = 0;
	else
	{
		sfxcache_t cachebuf, *cache;
		if (sfx->decoder.querydata)
		{
			G_FLOAT(OFS_RETURN) = sfx->decoder.querydata(sfx, NULL, NULL, 0);
			return;
		}
		else if (sfx->decoder.decodedata)
			cache = sfx->decoder.decodedata(sfx, &cachebuf, 0x7ffffffe, 0);
		else
			cache = sfx->decoder.buf;
		if (!cache)
			G_FLOAT(OFS_RETURN) = 0;
		else
			G_FLOAT(OFS_RETURN) = (cache->soundoffset+cache->length) / (float)snd_speed;
	}
}

qboolean M_Vid_GetMode(qboolean forfullscreen, int num, int *w, int *h);
//a bit pointless really
void QCBUILTIN PF_cl_getresolution (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float mode = G_FLOAT(OFS_PARM0);
	qboolean forfullscreen = (prinst->callargc >= 2)?G_FLOAT(OFS_PARM1):true; //if true, we should return queried video modes... or the mod could make up its own, but whatever.
	float *ret = G_VECTOR(OFS_RETURN);
	int w, h;
	float pixelheight = 0;

	w=h=0;
	if (mode == -1)
	{
		int bpp, rate;
		Sys_GetDesktopParameters(&w, &h, &bpp, &rate);
	}
	else
		M_Vid_GetMode(forfullscreen, mode, &w, &h);

	ret[0] = w;
	ret[1] = h;
	ret[2] = pixelheight?pixelheight:((w&&h)?1:0);	//pixelheight
}

#ifdef CL_MASTER
#include "cl_master.h"

void QCBUILTIN PF_cl_gethostcachevalue (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	hostcacheglobal_t hcg = G_FLOAT(OFS_PARM0);
	G_FLOAT(OFS_RETURN) = 0;
	switch(hcg)
	{
	case SLIST_HOSTCACHEVIEWCOUNT:
		CL_QueryServers();
		Master_CheckPollSockets();
		G_FLOAT(OFS_RETURN) = Master_NumSorted();
		return;
	case SLIST_HOSTCACHETOTALCOUNT:
		CL_QueryServers();
		Master_CheckPollSockets();
		G_FLOAT(OFS_RETURN) = Master_TotalCount();
		return;

	case SLIST_MASTERQUERYCOUNT:
	case SLIST_MASTERREPLYCOUNT:
	case SLIST_SERVERQUERYCOUNT:
	case SLIST_SERVERREPLYCOUNT:
		G_FLOAT(OFS_RETURN) = 0;
		return;

	case SLIST_SORTFIELD:
		G_FLOAT(OFS_RETURN) = Master_GetSortField();
		return;
	case SLIST_SORTDESCENDING:
		G_FLOAT(OFS_RETURN) = Master_GetSortDescending();
		return;
	default:
		return;
	}
}

//void 	resethostcachemasks(void) = #615;
void QCBUILTIN PF_cl_resethostcachemasks(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	Master_ClearMasks();
}
//void 	sethostcachemaskstring(float mask, float fld, string str, float op) = #616;
void QCBUILTIN PF_cl_sethostcachemaskstring(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int mask = G_FLOAT(OFS_PARM0);
	int field = G_FLOAT(OFS_PARM1);
	const char *str = PR_GetStringOfs(prinst, OFS_PARM2);
	int op = G_FLOAT(OFS_PARM3);

	Master_SetMaskString((mask&512)?true:false, field, str, op);
}
//void	sethostcachemasknumber(float mask, float fld, float num, float op) = #617;
void QCBUILTIN PF_cl_sethostcachemasknumber(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int mask = G_FLOAT(OFS_PARM0);
	int field = G_FLOAT(OFS_PARM1);
	int str = G_FLOAT(OFS_PARM2);
	int op = G_FLOAT(OFS_PARM3);

	Master_SetMaskInteger((mask&512)?true:false, field, str, op);
}
//void 	resorthostcache(void) = #618;
void QCBUILTIN PF_cl_resorthostcache(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	Master_SortServers();
}
//void	sethostcachesort(float fld, float descending) = #619;
void QCBUILTIN PF_cl_sethostcachesort(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	Master_SetSortField(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1));
}
//void	refreshhostcache(void) = #620;
void QCBUILTIN PF_cl_refreshhostcache(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	qboolean doreset = (prinst->callargc>=1)?G_FLOAT(OFS_PARM0):false;
	MasterInfo_Refresh(doreset);
}
//float	gethostcachenumber(float fld, float hostnr) = #621;
void QCBUILTIN PF_cl_gethostcachenumber(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float ret = 0;
	int keynum = G_FLOAT(OFS_PARM0);
	int svnum = G_FLOAT(OFS_PARM1);
	serverinfo_t *sv;
	sv = Master_SortedServer(svnum);

	ret = Master_ReadKeyFloat(sv, keynum);

	G_FLOAT(OFS_RETURN) = ret;
}
void QCBUILTIN PF_cl_gethostcachestring (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *ret;
	int keynum = G_FLOAT(OFS_PARM0);
	int svnum = G_FLOAT(OFS_PARM1);
	serverinfo_t *sv;

	sv = Master_SortedServer(svnum);
	ret = Master_ReadKeyString(sv, keynum);

	RETURN_TSTRING(ret);
}

//float	gethostcacheindexforkey(string key) = #622;
void QCBUILTIN PF_cl_gethostcacheindexforkey(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char *keyname = PR_GetStringOfs(prinst, OFS_PARM0);

	G_FLOAT(OFS_RETURN) = Master_KeyForName(keyname);
}
//void	addwantedhostcachekey(string key) = #623;
void QCBUILTIN PF_cl_addwantedhostcachekey(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	PF_cl_gethostcacheindexforkey(prinst, pr_globals);
}

void QCBUILTIN PF_cl_getextresponse(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	//this does something weird
	G_INT(OFS_RETURN) = 0;
}
#else
void QCBUILTIN PF_cl_gethostcachevalue (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals){G_FLOAT(OFS_RETURN) = 0;}
void QCBUILTIN PF_cl_gethostcachestring (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals) {G_INT(OFS_RETURN) = 0;}
//void 	resethostcachemasks(void) = #615;
void QCBUILTIN PF_cl_resethostcachemasks(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals){}
//void 	sethostcachemaskstring(float mask, float fld, string str, float op) = #616;
void QCBUILTIN PF_cl_sethostcachemaskstring(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals){}
//void	sethostcachemasknumber(float mask, float fld, float num, float op) = #617;
void QCBUILTIN PF_cl_sethostcachemasknumber(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals){}
//void 	resorthostcache(void) = #618;
void QCBUILTIN PF_cl_resorthostcache(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals){}
//void	sethostcachesort(float fld, float descending) = #619;
void QCBUILTIN PF_cl_sethostcachesort(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals){}
//void	refreshhostcache(void) = #620;
void QCBUILTIN PF_cl_refreshhostcache(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals) {}
//float	gethostcachenumber(float fld, float hostnr) = #621;
void QCBUILTIN PF_cl_gethostcachenumber(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals){G_FLOAT(OFS_RETURN) = 0;}
//float	gethostcacheindexforkey(string key) = #622;
void QCBUILTIN PF_cl_gethostcacheindexforkey(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals){G_FLOAT(OFS_RETURN) = 0;}
//void	addwantedhostcachekey(string key) = #623;
void QCBUILTIN PF_cl_addwantedhostcachekey(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals){}
#endif


void QCBUILTIN PF_shaderforname (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char *str = PR_GetStringOfs(prinst, OFS_PARM0);
	const char *defaultbody = PF_VarString(prinst, 1, pr_globals);

	shader_t *shad;

	if (*defaultbody)
		shad = R_RegisterShader(str, SUF_NONE, defaultbody);
	else
		shad = R_RegisterSkin(str, NULL);
	if (shad)
		G_FLOAT(OFS_RETURN) = shad->id+1;
	else
		G_FLOAT(OFS_RETURN) = 0;
}

void QCBUILTIN PF_cl_GetBindMap (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int bm[2];
	Key_GetBindMap(bm);
	G_VECTOR(OFS_RETURN)[0] = bm[0];
	G_VECTOR(OFS_RETURN)[1] = bm[1];
	G_VECTOR(OFS_RETURN)[2] = 0;
}
void QCBUILTIN PF_cl_SetBindMap (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int bm[2] =
	{
		G_FLOAT(OFS_PARM0+0),
		G_FLOAT(OFS_PARM0+1)
	};
	Key_SetBindMap(bm);
	G_FLOAT(OFS_RETURN) = 1;
}

//void	setmousetarget(float trg) = #603;
void QCBUILTIN PF_cl_setmousetarget (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	world_t *world = prinst->parms->user;
	unsigned int target = world->keydestmask;
	switch ((int)G_FLOAT(OFS_PARM0))
	{
	case 1:	//1 is delta-based (mt_menu).
		key_dest_absolutemouse &= ~target;
		break;
	case 2:	//2 is absolute (mt_client).
		key_dest_absolutemouse |= target;
		break;
	default:
		PR_BIError(prinst, "PF_setmousetarget: not a valid destination\n");
	}
}

//float	getmousetarget(void)	  = #604;
void QCBUILTIN PF_cl_getmousetarget (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	world_t *world = prinst->parms->user;
	unsigned int target = world->keydestmask;
	G_FLOAT(OFS_RETURN) = (key_dest_absolutemouse&target)?2:1;
}

//evil builtins to pretend to be a server.
void QCBUILTIN PF_cl_sprint (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	//this is a bit pointless for menus as it doesn't know player names or anything.
#ifndef CLIENTONLY
	int clientnum = G_FLOAT(OFS_PARM0);
	const char *str = PF_VarString(prinst, 1, pr_globals);
	if (sv.active && clientnum < sv.allocated_client_slots && svs.clients[clientnum].state >= cs_connected)
		SV_PrintToClient(&svs.clients[clientnum], PRINT_HIGH, str);
#endif
}
void QCBUILTIN PF_cl_bprint (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
#ifndef CLIENTONLY
	const char *str = PF_VarString(prinst, 0, pr_globals);
	if (sv.active)
		SV_BroadcastPrintf(PRINT_HIGH, "%s", str);
#endif
}
void QCBUILTIN PF_cl_clientcount (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
#ifndef CLIENTONLY
	if (sv.active)
		G_FLOAT(OFS_RETURN) = sv.allocated_client_slots;
	else
		G_FLOAT(OFS_RETURN) = 0;
#endif
}

void QCBUILTIN PF_cl_localsound(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char * s = PR_GetStringOfs(prinst, OFS_PARM0);
	float chan = (prinst->callargc>=1)?G_FLOAT(OFS_PARM1):0;
	float vol = (prinst->callargc>=2)?G_FLOAT(OFS_PARM2):1;

	S_LocalSound2(s, chan, vol);
}


#include "fs.h"
static struct modlist_s
{
	ftemanifest_t *manifest;
	char *gamedir;
	char *description;
} *modlist;
static size_t nummods;
static qboolean modsinited;

/*static qboolean Mods_AddManifest(void *usr, ftemanifest_t *man)
{
	int i = nummods;
	modlist = BZ_Realloc(modlist, (i+1) * sizeof(*modlist));
	modlist[i].manifest = man;
	modlist[i].gamedir = man->updatefile;
	modlist[i].description = man->formalname;
	nummods = i+1;
	return true;
}*/
static int QDECL Mods_AddGamedir(const char *fname, qofs_t fsize, time_t mtime, void *usr, searchpathfuncs_t *spath)
{
	char *f;
	size_t l = strlen(fname);
	int i, p;
	char gamedir[MAX_QPATH];
	if (l && fname[l-1] == '/' && l < countof(gamedir))
	{
		l--;
		memcpy(gamedir, fname, l);
		gamedir[l] = 0;
		for (i = 0; i < nummods; i++)
		{
			//don't add dupes (can happen from basedir+homedir)
			//if the gamedir was already included in one of the manifests, don't bother including it again.
			//this generally removes id1.
			if (modlist[i].manifest)
			{
				for (p = 0; p < countof(fs_manifest->gamepath); p++)
					if (modlist[i].manifest->gamepath[p].path)
						if (!Q_strcasecmp(modlist[i].manifest->gamepath[p].path, gamedir))
							return true;
			}
			else if (modlist[i].gamedir)
			{
				if (!Q_strcasecmp(modlist[i].gamedir, gamedir))
					return true;
			}
		}
		f = FS_MallocFile(va("%s%s/modinfo.txt", (const char*)usr, gamedir), FS_SYSTEM, NULL);
		if (f)
		{
			modlist = BZ_Realloc(modlist, (i+1) * sizeof(*modlist));
			modlist[i].manifest = NULL;
			modlist[i].gamedir = Z_StrDup(gamedir);
			modlist[i].description = f;
			nummods = i+1;
		}
	}
	return true;
}
static void Mods_InitModList (void)
{
	extern qboolean com_homepathenabled;

	//FS_EnumerateKnownGames(Mods_AddManifest, NULL);

	if (com_homepathenabled)
		Sys_EnumerateFiles(com_homepath, "*", Mods_AddGamedir, com_homepath, NULL);
	Sys_EnumerateFiles(com_gamepath, "*", Mods_AddGamedir, com_gamepath, NULL);
}

void QCBUILTIN PF_cl_getgamedirinfo(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	size_t diridx = G_FLOAT(OFS_PARM0);
	int propidx = G_FLOAT(OFS_PARM1);

	if (!modsinited)
	{
		modsinited = true;
		Mods_InitModList();
	}

	G_INT(OFS_RETURN) = 0;
	if (diridx < nummods)
	{
		switch(propidx)
		{
		case 1:	//description (contents of modinfo.txt)
			if (modlist[diridx].description)
				RETURN_TSTRING(modlist[diridx].description);
			break;
		case 0:	//name
			RETURN_TSTRING(modlist[diridx].gamedir);
			break;
		}
	}
}

//This is consistent with vanilla quakeworld's 'packet' console command.
void QCBUILTIN PF_cl_SendPacket(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	netadr_t to;
	const char *address = PR_GetStringOfs(prinst, OFS_PARM0);
	const char *contents = PF_VarString(prinst, 1, pr_globals);

	G_FLOAT(OFS_RETURN) = NETERR_NOROUTE;
	if (NET_StringToAdr(address, 0, &to))
	{
		char *send = Z_Malloc(4+strlen(contents));
		send[0] = send[1] = send[2] = send[3] = 0xff;
		memcpy(send+4, contents, strlen(contents));
		//FIXME: this is likely to change its port randomly...
		G_FLOAT(OFS_RETURN) = NET_SendPacket(cls.sockets, 4+strlen(contents), send, &to);
		Z_Free(send);
	}
}


#endif
