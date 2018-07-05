#include "quakedef.h"
#include <ctype.h>

#ifdef _WIN32
#include "winquake.h"
#endif

typedef struct f_modified_s {
	char name[MAX_QPATH];
	qboolean ismodified;
	struct f_modified_s *next;
} f_modified_t;

static f_modified_t *f_modified_list;
qboolean care_f_modified;
qboolean f_modified_particles;

static void QDECL rulesetcallback(cvar_t *var, char *oldval)
{
	Validation_Apply_Ruleset();
}

cvar_t allow_f_version		= CVAR("allow_f_version", "1");
cvar_t allow_f_server		= CVAR("allow_f_server", "1");
cvar_t allow_f_modified		= CVAR("allow_f_modified", "1");
cvar_t allow_f_skins		= CVAR("allow_f_skins", "1");
cvar_t allow_f_ruleset		= CVAR("allow_f_ruleset", "1");
cvar_t allow_f_scripts		= CVAR("allow_f_scripts", "1");
cvar_t allow_f_fakeshaft	= CVAR("allow_f_fakeshaft", "1");
cvar_t allow_f_system		= CVAR("allow_f_system", "0");
cvar_t allow_f_cmdline		= CVAR("allow_f_cmdline", "0");
cvar_t auth_validateclients	= CVAR("auth_validateclients", "1");
cvar_t ruleset			= CVARC("ruleset", "none", rulesetcallback);


#define SECURITY_INIT_BAD_CHECKSUM	1
#define SECURITY_INIT_BAD_VERSION	2
#define SECURITY_INIT_ERROR			3
#define SECURITY_INIT_NOPROC		4

typedef struct signed_buffer_s {
	qbyte *buf;
	unsigned long size;
} signed_buffer_t;

typedef signed_buffer_t *(*Security_Verify_Response_t) (int playernum, unsigned char *, char *userinfo, char *serverinfo);
typedef int (*Security_Init_t) (char *);
typedef signed_buffer_t *(*Security_Generate_Crc_t) (int playernum, char *userinfo, char *serverinfo);
typedef signed_buffer_t *(*Security_IsModelModified_t) (char *, int, qbyte *, int);
typedef void (*Security_Supported_Binaries_t) (void *);
typedef void (*Security_Shutdown_t) (void);


static Security_Verify_Response_t Security_Verify_Response;
static Security_Init_t Security_Init;
static Security_Generate_Crc_t Security_Generate_Crc;
static Security_IsModelModified_t Security_IsModelModified;
static Security_Supported_Binaries_t Security_Supported_Binaries;
static Security_Shutdown_t Security_Shutdown;


#if 0//def _WIN32
static void *secmodule;
#endif

static void Validation_Version(void)
{
	char sr[256];
	char *s = sr;
	char authbuf[256];
	char *auth = authbuf;

	extern cvar_t r_drawflat;

	//print certain allowed 'cheat' options.
	//realtime lighting (shadows can show around corners)
	//drawflat is just lame
	//24bits can be considered eeeevil, by some.
#ifdef RTLIGHTS
	if (r_shadow_realtime_world.ival)
		*s++ = 'W';
	else if (r_shadow_realtime_dlight.ival)
		*s++ = 'S';
#endif
	if (r_drawflat.ival || r_lightmap.ival)
		*s++ = 'F';
	if (gl_load24bit.ival)
		*s++ = 'H';

	*s = '\0';

	if (!allow_f_version.ival)
		return;	//suppress it

	if (Security_Generate_Crc)
	{
		signed_buffer_t *resp;

		resp = NULL;//Security_Generate_Crc(cl.playerview[0].playernum, cl.players[cl.playerview[0].playernum].userinfo, cl.serverinfo);
		if (!resp || !resp->buf)
			auth = "";
		else
			Q_snprintfz(auth, sizeof(authbuf), " crc: %s", resp->buf);
	}
	else
		auth = "";

	if (*sr)
		Cbuf_AddText (va("say %s "PLATFORM"/%s/%s%s\n", version_string(), q_renderername, sr, auth), RESTRICT_RCON);
	else
		Cbuf_AddText (va("say %s "PLATFORM"/%s%s\n", version_string(), q_renderername, auth), RESTRICT_RCON);
}
void Validation_CheckIfResponse(char *text)
{
	//client name, version type(os-renderer where it matters, os/renderer where renderer doesn't), 12 char hex crc
	int f_query_client;
	int i;
	char *crc;
	char *versionstring;

	if (!Security_Verify_Response)
		return;	//valid or not, we can't check it.

	if (!auth_validateclients.ival)
		return;

	//do the parsing.
	{
		char *comp;
		int namelen;

		for (crc = text + strlen(text) - 1; crc > text; crc--)
			if ((unsigned)*crc > ' ')
				break;

		//find the crc.
		for (i = 0; i < 29; i++)
		{
			if (crc <= text)
				return;	//not enough chars.
			if ((unsigned)crc[-1] <= ' ')
				break;
			crc--;
		}

		//we now want 3 string seperated tokens, so the first starts at the fourth found ' ' + 1
		i = 7;
		for (comp = crc-1; ; comp--)
		{
			if (comp < text)
				return;
			if (*comp == ' ')
			{
				i--;
				if (!i)
					break;
			}

		}

		versionstring = comp+1;
		if (comp <= text)
			return;	//not enough space for the 'name:'
		if (*(comp-1) != ':')
			return;	//whoops. not a say.

		namelen = comp - text-1;

		for (f_query_client = 0; f_query_client < cl.allocated_client_slots; f_query_client++)
		{
			if (strlen(cl.players[f_query_client].name) == namelen)
				if (!strncmp(cl.players[f_query_client].name, text, namelen))
					break;
		}
		if (f_query_client == cl.allocated_client_slots)
			return; //looks like a validation, but it's not from a known client.
	}

	{
		char *match = DISTRIBUTION" v";
		if (strncmp(versionstring, match, strlen(match)))
			return;	//this is not us
	}

	//now do the validation
	{
		signed_buffer_t *resp;

		resp = NULL;//Security_Verify_Response(f_query_client, crc, cl.players[f_query_client].userinfo, cl.serverinfo);

		if (resp && resp->size && *resp->buf)
			Con_Printf(CON_NOTICE "Authentication Successful.\n");
		else// if (!resp)
			Con_Printf(CON_ERROR "AUTHENTICATION FAILED.\n");
	}
}

void InitValidation(void)
{
	Cvar_Register(&allow_f_version,	"Authentication");
	Cvar_Register(&allow_f_server,	"Authentication");
	Cvar_Register(&allow_f_modified,	"Authentication");
	Cvar_Register(&allow_f_skins,	"Authentication");
	Cvar_Register(&allow_f_ruleset,	"Authentication");
	Cvar_Register(&allow_f_fakeshaft,	"Authentication");
	Cvar_Register(&allow_f_scripts,	"Authentication");
	Cvar_Register(&allow_f_system,	"Authentication");
	Cvar_Register(&allow_f_cmdline,	"Authentication");
	Cvar_Register(&ruleset,		"Authentication");

#if 0//def _WIN32
	secmodule = LoadLibrary("fteqw-security.dll");
	if (secmodule)
	{
		Security_Verify_Response	= (void*)GetProcAddress(secmodule, "Security_Verify_Response");
		Security_Init				= (void*)GetProcAddress(secmodule, "Security_Init");
		Security_Generate_Crc		= (void*)GetProcAddress(secmodule, "Security_Generate_Crc");
		Security_IsModelModified	= (void*)GetProcAddress(secmodule, "Security_IsModelModified");
		Security_Supported_Binaries	= (void*)GetProcAddress(secmodule, "Security_Supported_Binaries");
		Security_Shutdown			= (void*)GetProcAddress(secmodule, "Security_Shutdown");
	}
#endif

	if (Security_Init)
	{
		switch(Security_Init(va("%s %.2f %i", DISTRIBUTION, 2.57, version_number())))
		{
		case SECURITY_INIT_BAD_CHECKSUM:
			Con_Printf("Checksum failed. Security module does not support this build. Go upgrade it.\n");
			break;
		case SECURITY_INIT_BAD_VERSION:
			Con_Printf("Version failed. Security module does not support this version. Go upgrade.\n");
			break;
		case SECURITY_INIT_ERROR:
			Con_Printf("'Generic' security error. Stop hacking.\n");
			break;
		case SECURITY_INIT_NOPROC:
			Con_Printf("/proc/* does not exist. You will need to upgrade/reconfigure your kernel.\n");
			break;
		case 0:
			Cvar_Register(&auth_validateclients,	"Authentication");
			return;
		}
#if 0//def _WIN32
		FreeLibrary(secmodule);
#endif
	}
	Security_Verify_Response	= NULL;
	Security_Init				= NULL;
	Security_Generate_Crc		= NULL;
	Security_IsModelModified	= NULL;
	Security_Supported_Binaries	= NULL;
	Security_Shutdown			= NULL;
}

//////////////////////
//f_modified

void Validation_IncludeFile(char *filename, char *file, int filelen)
{
}

static void Validation_FilesModified (void)
{
	Con_Printf ("f_modified not implemented\n");
}

void Validation_FlushFileList(void)
{
	f_modified_t *fm;
	while(f_modified_list)
	{
		fm = f_modified_list->next;

		Z_Free(f_modified_list);
		f_modified_list = fm;
	}
}

/////////////////////////
//minor (codewise) responses

static void Validation_Server(void)
{
	char adr[MAX_ADR_SIZE];

#ifdef warningmsg
#pragma warningmsg("is allowing the user to turn this off practical?..")
#endif
	if (!allow_f_server.ival)
		return;
	Cbuf_AddText(va("say server is %s\n", NET_AdrToString(adr, sizeof(adr), &cls.netchan.remote_address)), RESTRICT_LOCAL);
}

static void Validation_Skins(void)
{
	extern cvar_t r_fullbrightSkins, r_fb_models, ruleset_allow_fbmodels;
	int percent = r_fullbrightSkins.value*100;

	if (!allow_f_skins.ival)
		return;

	RulesetLatch(&r_fb_models);
	RulesetLatch(&r_fullbrightSkins);

	if (percent < 0)
		percent = 0;
	if (percent > cls.allow_fbskins*100)
		percent = cls.allow_fbskins*100;
	if (percent)
		Cbuf_AddText(va("say all player skins %i%% fullbright%s\n", percent, (r_fb_models.ival == 1 && ruleset_allow_fbmodels.ival)?" (non-player 100%%)":(r_fb_models.value?" (plus luma)":"")), RESTRICT_LOCAL);
	else if (r_fb_models.ival == 1 && ruleset_allow_fbmodels.ival)
		Cbuf_AddText("say non-player entities glow in the dark like a bright big cheat\n", RESTRICT_LOCAL);
	else if (r_fb_models.ival)
		Cbuf_AddText("say luma textures only\n", RESTRICT_LOCAL);
	else
		Cbuf_AddText("say Only cheaters use full bright skins\n", RESTRICT_LOCAL);
}

static void Validation_Scripts(void)
{	//subset of ruleset
	if (!allow_f_scripts.ival)
		return;
	if (ruleset_allow_frj.ival)
		Cbuf_AddText("say scripts are allowed\n", RESTRICT_LOCAL);
	else
		Cbuf_AddText("say scripts are capped\n", RESTRICT_LOCAL);
}

static void Validation_FakeShaft(void)
{
	extern cvar_t cl_truelightning;
	if (!allow_f_fakeshaft.ival)
		return;
	if (cl_truelightning.value > 0.999)
		Cbuf_AddText("say fakeshaft on\n", RESTRICT_LOCAL);
	else if (cl_truelightning.value > 0)
		Cbuf_AddText(va("say fakeshaft %.1f%%\n", cl_truelightning.value), RESTRICT_LOCAL);
	else
		Cbuf_AddText("say fakeshaft off\n", RESTRICT_LOCAL);
}

static void Validation_System(void)
{	//subset of ruleset
	if (!allow_f_system.ival)
		return;
	Cbuf_AddText("say f_system not supported\n", RESTRICT_LOCAL);
}

static void Validation_CmdLine(void)
{
	if (!allow_f_cmdline.ival)
		return;
	Cbuf_AddText("say f_cmdline not supported\n", RESTRICT_LOCAL);
}

//////////////////////
//rulesets

typedef struct {
	char *rulename;
	char *rulevalue;
} rulesetrule_t;
typedef struct {
	char *rulesetname;

	rulesetrule_t *rule;

	qboolean flagged;
} ruleset_t;

rulesetrule_t rulesetrules_strict[] = {
	{"ruleset_allow_shaders", "0"},	/*users can potentially create all sorts of wallhacks or spiked models with this*/
	{"ruleset_allow_watervis", "0"}, /*oh noes! users might be able to see underwater if they're already in said water. oh wait. what? why do we care, dude*/
	{"r_vertexlight", "0"},
	{"ruleset_allow_playercount", "0"},
	{"ruleset_allow_frj", "0"},
	{"ruleset_allow_packet", "0"},
	{"ruleset_allow_particle_lightning", "0"},
	{"ruleset_allow_overlong_sounds", "0"},
	{"ruleset_allow_larger_models", "0"},
	{"ruleset_allow_modified_eyes", "0"},
	{"ruleset_allow_sensitive_texture_replacements", "0"},
	{"ruleset_allow_localvolume", "0"},
	{"ruleset_allow_fbmodels", "0"},
	{"scr_autoid_team", "0"},	/*sort of a wallhack*/
	{"tp_disputablemacros", "0"},
	{"cl_instantrotate", "0"},
	{"v_projectionmode", "0"},	/*no extended fovs*/
	{"r_shadow_realtime_world", "0"}, /*static lighting can be used to cast shadows around corners*/
	{"ruleset_allow_in", "0"},
	{"r_projection", "0"},
	{"gl_shadeq1_name", "*"},
	{"cl_iDrive", "0"},
	{NULL}
};

rulesetrule_t rulesetrules_nqr[] = {
	{"ruleset_allow_larger_models", "0"},
	{"ruleset_allow_watervis", "0"}, /*block seeing through turbs, as well as all our cool graphics stuff. apparently we're not allowed.*/
	{"ruleset_allow_overlong_sounds", "0"},
	{"ruleset_allow_particle_lightning", "0"},
	{"ruleset_allow_packet", "0"},
	{"ruleset_allow_frj", "0"},
	{"ruleset_allow_modified_eyes", "0"},
	{"ruleset_allow_sensitive_texture_replacements", "0"},
	{"ruleset_allow_localvolume", "0"},
	{"ruleset_allow_shaders", "0"},
	{"ruleset_allow_fbmodels", "0"},
	{"r_vertexlight", "0"},
	{"v_projectionmode", "0"},
	{"sbar_teamstatus", "0"},
	{"ruleset_allow_in", "0"},
	{"r_projection", "0"},
	{"gl_shadeq1_name", "*"},
	{"cl_iDrive", "0"},
	{NULL}
};

static ruleset_t rulesets[] =
{
	{"strict", rulesetrules_strict},
	{"nqr", rulesetrules_nqr},
	//{"eql", rulesetrules_nqr},
	{NULL}
};

static qboolean ruleset_locked;
void RulesetLatch(cvar_t *cvar)
{
	cvar->flags |= CVAR_RULESETLATCH;
}

void Validation_DelatchRulesets(void)
{	//game has come to an end, allow the ruleset to be changed
	ruleset_locked = false;
	if (Cvar_ApplyLatches(CVAR_RULESETLATCH))
		Con_DPrintf("Ruleset deactivated\n");
}

qboolean Validation_GetCurrentRulesetName(char *rsnames, int resultbuflen, qboolean enforcechosenrulesets)
{	//this code is more complex than it needs to be
	//this allows for the ruleset code to print a ruleset name that is applied via the cvars, but not directly named by the user
	cvar_t *var;
	ruleset_t *rs;
	int i;

	if (enforcechosenrulesets)
		ruleset_locked = true;

	rs = rulesets;
	*rsnames = '\0';

	for (rs = rulesets; rs->rulesetname; rs++)
	{
		rs->flagged = false;

		for (i = 0; rs->rule[i].rulename; i++)
		{
			var = Cvar_FindVar(rs->rule[i].rulename);
			if (!var)	//sw rendering?
				continue;

			if (strcmp(var->string, rs->rule[i].rulevalue))
			{
				Con_DPrintf("ruleset \"%s\" requires \"%s\" to be \"%s\"\n", rs->rulesetname, rs->rule[i].rulename, rs->rule[i].rulevalue);
				break;	//current settings don't match
			}
		}
		if (!rs->rule[i].rulename)
		{
			if (*rsnames)
			{
				Q_strncatz(rsnames, ", ", resultbuflen);
			}
			Q_strncatz(rsnames, rs->rulesetname, resultbuflen);
			rs->flagged = true;
		}
	}
	if (*rsnames)
	{
		//as we'll be telling the other players what rules we're playing by, we'd best stick to them
		if (enforcechosenrulesets)
		{
			for (rs = rulesets; rs->rulesetname; rs++)
			{
				if (!rs->flagged)
					continue;
				for (i = 0; rs->rule[i].rulename; i++)
				{
					var = Cvar_FindVar(rs->rule[i].rulename);
					if (!var)
						continue;
					RulesetLatch(var);	//set the latched flag
				}
			}
		}
		return true;
	}
	else
		return false;
}

void Validation_OldRuleset(void)
{
	char rsnames[1024];

	if (Validation_GetCurrentRulesetName(rsnames, sizeof(rsnames), true))
		Cbuf_AddText(va("say Ruleset: %s\n", rsnames), RESTRICT_LOCAL);
	else
		Cbuf_AddText("say No specific ruleset\n", RESTRICT_LOCAL);
}

void Validation_AllChecks(void)
{
	char servername[22];
	char playername[16];
	char *enginebuild = version_string();
	char localpnamelen = strlen(cl.players[cl.playerview[0].playernum].name);
	char ruleset[1024];

	//figure out the padding for the player's name.
	if (localpnamelen >= 15)
		playername[0] = 0;
	else
	{
		//pad the left side to compensate for the player name prefix the server will add in the final svc_print
		memset(playername, ' ', 15-localpnamelen);
		playername[15-localpnamelen] = 0;
	}

	//get the current server address
	NET_AdrToString(servername, sizeof(servername), &cls.netchan.remote_address);

	//get the ruleset names
	if (!Validation_GetCurrentRulesetName(ruleset, sizeof(ruleset), true))
		Q_strncpyz(ruleset, "no ruleset", sizeof(ruleset));

	//now send it
	CL_SendClientCommand(true, "say \"%s%21s " "%16s %s\"", playername, servername, enginebuild, ruleset);

}

void Validation_Apply_Ruleset(void)
{	//rulesets are applied when the client first gets a connection to the server
	ruleset_t *rs;
	rulesetrule_t *rule;
	cvar_t *var;
	int i;
	char *rulesetname = ruleset.string;

	if (ruleset_locked)
	{
		if (ruleset.modified)
		{
			Con_Printf("Cannot change rulesets after the current ruleset has been announced\n");
			ruleset.modified = false;
		}
		return;
	}
	ruleset.modified = false;

	if  (!strcmp(rulesetname, "smackdown"))	//officially, smackdown cannot authorise this, thus we do not use that name. however, imported configs tend to piss people off.
		rulesetname = "strict";

	if (!*rulesetname || !strcmp(rulesetname, "none") || !strcmp(rulesetname, "default"))
	{
		if (Cvar_ApplyLatches(CVAR_RULESETLATCH))
			Con_DPrintf("Ruleset deactivated\n");
		return;	//no ruleset is set
	}

	for (rs = rulesets; rs->rulesetname; rs++)
	{
		if (!stricmp(rs->rulesetname, rulesetname))
			break;
	}
	if (!rs->rulesetname)
	{
		Con_Printf("Cannot apply ruleset %s - not recognised\n", rulesetname);
		if (Cvar_ApplyLatches(CVAR_RULESETLATCH))
			Con_DPrintf("Ruleset deactivated\n");
		return;
	}
	
	for (rule = rs->rule; rule->rulename; rule++)
	{
		for (i = 0; rs->rule[i].rulename; i++)
		{
			var = Cvar_FindVar(rs->rule[i].rulename);
			if (!var)
				continue;

			if (!Cvar_ApplyLatchFlag(var, rs->rule[i].rulevalue, CVAR_RULESETLATCH))
			{
				Con_Printf("Failed to apply ruleset %s due to cvar %s\n", rs->rulesetname, var->name);
				break;
			}
		}
	}

	Con_DPrintf("Ruleset set to %s\n", rs->rulesetname);
}

//////////////////////

void Validation_Auto_Response(int playernum, char *s)
{
	static float versionresponsetime;
	static float modifiedresponsetime;
	static float skinsresponsetime;
	static float serverresponsetime;
	static float rulesetresponsetime;
	static float systemresponsetime;
	static float fakeshaftresponsetime;
	static float cmdlineresponsetime;
	static float scriptsresponsetime;

	//quakeworld tends to use f_*
	//netquake uses the slightly more guessable q_* form
	if (!strncmp(s, "f_", 2))
		s+=2;
	else if (!strncmp(s, "q_", 2))
		s+=2;
	else
		return;

	if (!strncmp(s, "version", 7) && versionresponsetime < Sys_DoubleTime())	//respond to it.
	{
		Validation_Version();
		versionresponsetime = Sys_DoubleTime() + 5;
	}
	else if (cl.playerview[0].spectator)
		return;
	else if (!strncmp(s, "server", 6) && serverresponsetime < Sys_DoubleTime())	//respond to it.
	{
		Validation_Server();
		serverresponsetime = Sys_DoubleTime() + 5;
	}
	else if (!strncmp(s, "system", 6) && systemresponsetime < Sys_DoubleTime())
	{
		Validation_System();
		systemresponsetime = Sys_DoubleTime() + 5;
	}
	else if (!strncmp(s, "cmdline", 7) && cmdlineresponsetime < Sys_DoubleTime())
	{
		Validation_CmdLine();
		cmdlineresponsetime = Sys_DoubleTime() + 5;
	}
	else if (!strncmp(s, "fakeshaft", 9) && fakeshaftresponsetime < Sys_DoubleTime())
	{
		Validation_FakeShaft();
		fakeshaftresponsetime = Sys_DoubleTime() + 5;
	}
	else if (!strncmp(s, "modified", 8) && modifiedresponsetime < Sys_DoubleTime())	//respond to it.
	{
		Validation_FilesModified();
		modifiedresponsetime = Sys_DoubleTime() + 5;
	}
	else if (!strncmp(s, "scripts", 7) && scriptsresponsetime < Sys_DoubleTime())
	{
		Validation_Scripts();
		scriptsresponsetime = Sys_DoubleTime() + 5;
	}
	else if (!strncmp(s, "skins", 5) && skinsresponsetime < Sys_DoubleTime())	//respond to it.
	{
		Validation_Skins();
		skinsresponsetime = Sys_DoubleTime() + 5;
	}
	else if (!strncmp(s, "ruleset", 7) && rulesetresponsetime < Sys_DoubleTime())
	{
		if (1)
			Validation_AllChecks();
		else
			Validation_OldRuleset();
		rulesetresponsetime = Sys_DoubleTime() + 5;
	}
}


