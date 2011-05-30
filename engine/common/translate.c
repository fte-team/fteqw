#include "quakedef.h"
#include "errno.h"

#undef malloc
#undef free

static char *defaultlanguagetext =
"STL_LANGUAGENAME \"English\"\n"
"TL_NL \"\\n\"\n"
"TL_STNL \"%s\\n\"\n"
"STL_CLIENTCONNECTED \"client %s connected\\n\"\n"
"STL_SPECTATORCONNECTED \"spectator %s connected\\n\"\n"
"STL_RECORDEDCLIENTCONNECTED \"recorded client %s connected\\n\"\n"
"STL_RECORDEDSPECTATORCONNECTED \"recorded spectator %s connected\\n\"\n"
"STL_CLIENTWASBANNED \"%s was banned\\n\"\n"
"STL_YOUWEREBANNED \"You were banned\\n\"\n"
"STL_YOUAREBANNED \"You are still banned\\n\"\n"
"STL_CLIENTTIMEDOUT \"Client %s timed out\\n\"\n"
"STL_LOADZOMIBETIMEDOUT \"LoadZombie %s timed out\\n\"\n"
"STL_CLIENTWASKICKED \"%s was kicked\\n\"\n"
"STL_YOUWEREKICKED \"You were kicked\\n\"\n"
"STL_YOUWEREKICKEDNAMESPAM \"You were kicked for name spamming\\n\"\n"
"STL_CLIENTKICKEDNAMESPAM \"%s was kicked for name spamming\\n\"\n"
"STL_GODON \"godmode ON\\n\"\n"
"STL_GODOFF \"godmode OFF\\n\"\n"
"STL_NOCLIPON \"noclip ON\\n\"\n"
"STL_NOCLIPOFF \"noclip OFF\"\n"
"STL_CLIENTISCUFFEDPERMANENTLY \"%s is still cuffed\\n\"\n"
"STL_CLIENTISCUFFED \"%s is cuffed\\n\"\n"
"STL_CLIENTISSTILLCUFFED \"%s is now cuffed permanently\\n\"\n"
"STL_YOUWERECUFFED \"You were cuffed\\n\"\n"
"STL_YOUARNTCUFFED \"You are no longer cuffed\\n\"\n"
"STL_CLIENTISCRIPPLEDPERMANENTLY \"%s is now crippled permanently\\n\"\n"
"STL_CLIENTISCRIPPLED \"%s is crippled\\n\"\n"
"STL_CLIENTISSTILLCRIPPLED \"%s is still crippled\\n\"\n"
"STL_YOUWERECLIPPLED \"You have been crippled\\n\"\n"
"STL_YOUARNTCRIPPLED \"You are no longer crippled\\n\"\n"
"STL_CLIENTISMUTEDPERMANENTLY \"%s was muted permanently\\n\"\n"
"STL_CLIENTISMUTED \"%s was muted\\n\"\n"
"STL_CLIENTISSTILLMUTED \"%s is muted (still)\\n\"\n"
"STL_YOUAREMUTED \"%s is muted\\n\"\n"
"STL_YOUARNTMUTED \"You are no longer muted\\n\"\n"
"STL_NONAMEASMUTE \"Muted players may not change their names\\n\"\n"
"STL_MUTEDVOTE \"Sorry, you cannot vote when muted as it may allow you to send a message.\\n\"\n"
"STL_MUTEDCHAT \"You cannot chat while muted\\n\"\n"
"STL_FLOODPROTACTIVE \"floodprot: You can't talk for %i seconds\\n\"\n"
"STL_FLOODPROTTIME \"You can't talk for %i more seconds\\n\"\n"
"STL_BUFFERPROTECTION \"buffer overflow protection: failiure\\n\"\n"
"STL_FIRSTGREETING \"Welcome %s. Your time on this server is being logged and ranked\\n\"\n"
"STL_SHORTGREETING \"Welcome back %s. You have previously spent %i mins connected\\n\"\n"
"STL_BIGGREETING \"Welcome back %s. You have previously spent %i:%i hours connected\\n\"\n"
"STL_POSSIBLEMODELCHEAT \"warning: %s eyes or player model does not match\\n\"\n"
"STL_MAPCHEAT \"Map model file does not match (%s), %i != %i/%i.\\nYou may need a new version of the map, or the proper install files.\\n\"\n"
"STL_INVALIDTRACKCLIENT \"invalid player to track\\n\"\n"
"STL_BADNAME \"Can't change name - new is invalid\\n\"\n"
"STL_CLIENTNAMECHANGE \"%s changed their name to %s\\n\"\n"
"STL_SERVERPAUSED \"server is paused\\n\"\n"
"STL_UPLOADDENIED \"Upload denied\\n\"\n"
"STL_NAMEDCLIENTDOESNTEXIST \"client does not exist\\n\"\n"
"STL_NOSUICIDEWHENDEAD \"Can't suicide -- Already dead\\n\"\n"
"STL_CANTPAUSE \"Can't pause. Not allowed\\n\"\n"
"STL_CANTPAUSESPEC \"Spectators may not pause the game\\n\"\n"
"STL_CLIENTPAUSED \"%s paused the game\\n\"\n"
"STL_CLIENTUNPAUSED \"%s unpaused the game\\n\"\n"
"STL_CLIENTLESSUNPAUSE \"pause released due to empty server\\n\"\n"
"STL_CURRENTRATE \"current rate is %i\\n\"\n"
"STL_RATESETTO \"rate is changed to %i\\n\"\n"
"STL_CURRENTMSGLEVEL \"current msg level is %i\\n\"\n"
"STL_MSGLEVELSET \"new msg level set to %i\\n\"\n"
"STL_GAMESAVED \"Server has saved the game\\n\"\n"
"STL_CLIENTDROPPED \"%s dropped\\n\"\n"
"STL_SNAPREFUSED \"%s refused remote screenshot\\n\"\n"
"STL_FINALVOTE \"%s casts final vote for '%s'\\n\"\n"
"STL_VOTE \"%s casts a vote for '%s'\\n\"\n"
"STL_SPEEDCHEATKICKED \"%s was kicked for speedcheating (%s)\\n\"\n"
"STL_SPEEDCHEATPOSSIBLE \"Speed cheat possibility, analyzing:\\n  %d %.1f %d for: %s\\n\"\n"
"STL_INITED \"======== FTE QuakeWorld Initialized ========\\n\"\n"
"STL_BACKBUFSET \"WARNING %s: [SV_New] Back buffered (%d0, clearing)\\n\"\n"
"STL_MESSAGEOVERFLOW \"WARNING: backbuf [%d] reliable overflow for %s\\n\"\n"
"STL_BUILDINGPHS \"Building PHS...\\n\"\n"
"STL_PHSINFO \"Average leafs visible / hearable / total: %i / %i / %i\\n\"\n"
"STL_BREAKSTATEMENT \"Break Statement\\n\"\n"
"STL_BADSPRINT \"tried to sprint to a non-client\\n\"\n"
"STL_NOPRECACHE \"no precache: %s\\n\"\n"
"STL_CANTFREEWORLD \"cannot free world entity\\n\"\n"
"STL_CANTFREEPLAYERS \"cannot free player entities\\n\"\n"
"STL_COMPILEROVER \"Compile took %f secs\\n\"\n"
"STL_EDICTWASFREE \"%s edict was free\\n\"\n"
"STL_NOFREEEDICTS \"WARNING: no free edicts\\n\"\n"
"STL_NEEDCHEATPARM \"You must run the server with -cheats to enable this command.\\n\"\n"
"STL_USERDOESNTEXIST \"Couldn't find user number %s\\n\"\n"
"STL_MAPCOMMANDUSAGE \"map <levelname> : continue game on a new level\\n\"\n"
"STL_NOVOTING \"Voting was dissallowed\\n\"\n"
"STL_BADVOTE \"You arn't allowed to vote for that\\n\"\n"
"STL_VOTESREMOVED \"All votes removed.\\n\"\n"
"STL_OLDVOTEREMOVED \"Old vote removed.\\n\"\n"
"TL_EXECING \"execing %s\\n\"\n"
"TL_EXECCOMMANDUSAGE \"exec <filename> : execute a script file\\n\"\n"
"TL_EXECFAILED \"couldn't exec %s\\n\"\n"
"TL_FUNCOVERFLOW \"%s: overflow\\n\"\n"
"TL_CURRENTALIASCOMMANDS \"Current alias commands:\\n\"\n"
"TL_ALIASNAMETOOLONG \"Alias name is too long\\n\"\n"
"TL_ALIASRESTRICTIONLEVELERROR \"Alias is already bound with a higher restriction\\n\"\n"
"TL_ALIASLEVELCOMMANDUSAGE \"aliaslevel <var> [execlevel]\\n\"\n"
"TL_ALIASNOTFOUND \"Alias not found\\n\"\n"
"TL_ALIASRAISELEVELERROR \"You arn't allowed to raise a command above your own level\\n\"\n"
"TL_ALIASRESTRICTIONLEVELWARN \"WARNING: %s is available to all clients, any client will be able to use it at the new level.\\n\"\n"
"TL_ALIASRESTRICTLEVEL \"alias %s is set to run at the user level of %i\\n\"\n"
"TL_ALIASLIST \"Alias list:\\n\"\n"
"TL_COMMANDLISTHEADER \"Command list:\\n\"\n"
"TL_CVARLISTHEADER \"CVar list:\\n\"\n"
"TL_RESTRICTCOMMANDRAISE \"You arn't allowed to raise a command above your own level\\n\"\n"
"TL_RESTRICTCOMMANDTOOHIGH \"You arn't allowed to alter a level above your own\\n\"\n"
"TL_RESTRICTCURRENTLEVEL \"%s is restricted to %i\\n\"\n"
"TL_RESTRICTCURRENTLEVELDEFAULT \"%s is restricted to rcon_level (%i)\\n\"\n"
"TL_RESTRICTNOTDEFINED \"restrict: %s not defined\\n\"\n"
"TL_WASRESTIRCTED \"%s was restricted.\\n\"\n"
"TL_COMMANDNOTDEFINED \"Unknown command \\\"%s\\\"\\n\"\n"
"TL_IFSYNTAX \"if <condition> <statement> [elseif <condition> <statement>] [...] [else <statement>]\\n\"\n"
"TL_IFSYNTAXERROR \"Not terminated\\n\"\n"
"TL_SETSYNTAX \"set <var> <equation>\\n\"\n"
"TL_CANTXNOTCONNECTED \"Can't \\\"%s\\\", not connected\\n\"\n"
"TL_SHAREWAREVERSION \"Playing shareware version.\\n\"\n"
"TL_REGISTEREDVERSION \"Playing registered version.\\n\"\n"
"TL_CURRENTSEARCHPATH \"Current search path:\\n\"\n"
"TL_SERACHPATHISPACK \"%s (%i files)\\n\"\n"
"TL_SERACHPATHISZIP \"%s (%i files)\\n\"\n"
"TL_COMPRESSEDFILEOPENFAILED \"Tried opening a handle to a compressed stream - %s\\n\"\n"
"TL_ADDEDPACKFILE \"Added packfile %s (%i files)\\n\"\n"
"TL_COULDNTOPENZIP \"Failed opening zipfile \\\"%s\\\" corrupt?\\n\"\n"
"TL_ADDEDZIPFILE \"Added zipfile %s (%i files)\\n\"\n"
"TL_GAMEDIRAINTPATH \"Gamedir should be a single filename, not a path\\n\"\n"
"TL_KEYHASSLASH \"Can't use a key with a \\\\\\n\"\n"
"TL_KEYHASQUOTE \"Can't use a key with a \\\"\\n\"\n"
"TL_KEYTOOLONG \"Keys and values must be < 64 characters.\\n\"\n"
"TL_INFOSTRINGTOOLONG \"Info string length exceeded\\n\"\n"
"TL_STARKEYPROTECTED \"Can't set * keys\\n\"\n"
"TL_KEYHASNOVALUE \"MISSING VALUE\\n\"\n"
"TL_OVERSIZEPACKETFROM \"Warning:  Oversize packet from %s\\n\"\n"
"TL_CONNECTIONLOSTORABORTED \"Connection lost or aborted\\n\"\n"
"TL_NETGETPACKETERROR \"NET_GetPacket: %s\\n\"\n"
"TL_NETSENDERROR \"NET_SendPacket ERROR: %i\\n\"\n"
"TL_NETBINDINTERFACE \"Binding to IP Interface Address of %s\\n\"\n"
"TL_IPADDRESSIS \"IP address %s\\n\"\n"
"TL_UDPINITED \"UDP Initialized\\n\"\n"
"TL_SERVERPORTINITED \"Server port Initialized\\n\"\n"
"TL_CLIENTPORTINITED \"Client port Initialized\\n\"\n"
"TL_OUTMESSAGEOVERFLOW \"%s:Outgoing message overflow\\n\"\n"
"TL_OUTOFORDERPACKET \"%s:Out of order packet %i at %i\\n\"\n"
"TL_DROPPEDPACKETCOUNT \"%s:Dropped %i packets at %i\\n\"\n"
"STL_SERVERUNSPAWNED \"Server ended\\n\"\n"
"STL_SERVERSPAWNED \"Server spawned.\\n\"\n"
"TL_EXEDATETIME \"Exe: %s %s\\n\"\n"
"TL_HEAPSIZE \"%4.1f megs RAM available.\\n\"\n"
"TL_VERSION \"\\n%s Build %i\\n\\n\"\n"
"STL_SAVESYNTAX \"save <savename> : save a game\\n\"\n"
"STL_NORELATIVEPATHS \"Relative pathnames are not allowed.\\n\"\n"
"STL_SAVEGAMETO \"Saving game to %s...\\n\"\n"
"STL_ERRORCOULDNTOPEN \"ERROR: couldn't open.\\n\"\n"
"STL_SAVEDONE \"done.\\n\"\n"
"STL_LOADSYNTAX \"load <savename> : load a game\\n\"\n"
"STL_LOADGAMEFROM \"Loading game from %s...\\n\"\n"
"STL_BADSAVEVERSION \"Savegame is version %i, not %i\\n\"\n"
"STL_LOADFAILED \"Couldn't load map\\n\"\n"
"STL_NOMASTERMODE \"Setting nomaster mode.\\n\"\n"
"STL_MASTERAT \"Master server at %s\\n\"\n"
"STL_SENDINGPING \"Sending a ping.\\n\"\n"
"STL_SHUTTINGDOWN \"Shutting down.\\n\"\n"
"STL_LOGGINGOFF \"File logging off.\\n\"\n"
"STL_LOGGINGTO \"Logging text to %s.\\n\"\n"
"STL_FLOGGINGOFF \"Frag file logging off.\\n\"\n"
"STL_FLOGGINGFAILED \"Can't open any logfiles.\\n\"\n"
"STL_FLOGGINGTO \"Logging frags to %s.\\n\"\n"
"STL_USERIDNOTONSERVER \"Userid %i is not on the server\\n\"\n"
"STL_CANTFINDMAP \"Can't find %s\\n\"\n"
"STL_SERVERINFOSETTINGS \"Server info settings:\\n\"\n"
"STL_SERVERINFOSYNTAX \"usage: serverinfo [ <key> <value> ]\\n\"\n"
"STL_LOCALINFOSETTINGS \"Local info settings:\\n\"\n"
"STL_LOCALINFOSYNTAX \"usage: localinfo [ <key> <value> ]\\n\"\n"
"STL_USERINFOSYNTAX \"Usage: info <userid>\\n\"\n"
"STL_NONEGATIVEVALUES \"All values must be positive numbers\\n\"\n"
"STL_CURRENTGAMEDIR \"Current gamedir: %s\\n\"\n"
"STL_SVGAMEDIRUSAGE \"Usage: sv_gamedir <newgamedir>\\n\"\n"
"STL_GAMEDIRCANTBEPATH \"*Gamedir should be a single filename, not a path\\n\"\n"
"STL_GAMEDIRUSAGE \"Usage: gamedir <newgamedir>\\n\"\n"
"STL_SNAPTOOMANYFILES \"Snap: Couldn't create a file, clean some out.\\n\"\n"
"STL_SNAPREQUEST \"Requesting snap from user %d...\\n\"\n"
"STL_SNAPUSAGE \"Usage:  snap <userid>\\n\"\n"
"TLC_BADSERVERADDRESS \"Bad server address\\n\"\n"
"TLC_ILLEGALSERVERADDRESS \"Illegal server address\\n\"\n"
"TLC_CONNECTINGTO \"Connecting to %s...\\n\"\n"
"TLC_SYNTAX_CONNECT \"usage: connect <server>\\n\"\n"
"TLC_NORCONPASSWORD \"'rcon_password' is not set.\\n\"\n"
"TLC_NORCONDEST \"You must either be connected,\\nor set the 'rcon_address' cvar\\nto issue rcon commands\\n\"\n"
"TLC_SYNTAX_USER \"Usage: user <username / userid>\\n\"\n"
"TLC_USER_NOUSER \"User not in server.\\n\"\n"
"TLC_USERBANNER \"userid frags name\\n\"\n"
"TLC_USERBANNER2 \"------ ----- ----\\n\"\n"
"TLC_USERLINE \"%6i %4i %s\\n\"\n"
"TLC_USERTOTAL \"%i total users\\n\"\n"
"TLC_COLOURCURRENT \"\\\"color\\\" is \\\"%s %s\\\"\\n\"\n"
"TLC_SYNTAX_COLOUR \"color <0-13> [0-13]\\n\"\n"
"TLC_SYNTAX_FULLSERVERINFO \"usage: fullserverinfo <complete info string>\\n\"\n"
"TLC_SERVER_VERSION \"Version %1.2f Server\\n\"\n"
"TLC_SYNTAX_FULLINFO \"fullinfo <complete info string>\\n\"\n"
"TLC_SYNTAX_SETINFO \"usage: setinfo [ <key> <value> ]\\n\"\n"
"TLC_PACKET_SYNTAX \"packet <destination> <contents>\\n\"\n"
"TLC_BADADDRESS \"Bad address\\n\"\n"
"TLC_CHANGINGMAP \"\\nChanging map...\\n\"\n"
"TLC_RECONNECTING \"reconnecting...\\n\"\n"
"TLC_RECONNECT_NOSERVER \"No server to reconnect to...\\n\"\n"
"TLC_VERSIONST \"%s Build %i\n\"\n"
"TL_ST_COLON \"%s: \"\n"
"TLC_GOTCONNECTION \"connection\\n\"\n"
"TLC_DUPCONNECTION \"Dup connect received.  Ignored.\\n\"\n"
"TLC_CONNECTED \"Connected.\\n\"\n"
"TLC_CONLESS_CONCMD \"client command\\n\"\n"
"TLC_CMDFROMREMOTE \"Command packet from remote host.  Ignored.\\n\"\n"
"TLC_LOCALID_NOTSET \""CON_ERROR"Command packet received from local host, but no localid has been set.  You may need to upgrade your server browser.\\n\"\n"
"TLC_LOCALID_BAD \""CON_ERROR"Invalid localid on command packet received from local host. \\n|%s| != |%s|\\nYou may need to reload your server browser and QuakeWorld.\\n\"\n"
"TLC_A2C_PRINT \"print\\n\"\n"
"TLC_A2A_PING \"ping\\n\"\n"
"TLC_S2C_CHALLENGE \"challenge\\n\"\n"
"TLC_CONLESSPACKET_UNKNOWN \"unknown connectionless packet:  %c\\n\"\n"
"TL_RUNTPACKET \"%s: Runt packet\\n\"\n"
"TLC_SERVERTIMEOUT \"\\nServer connection timed out.\\n\"\n"
"TLC_CONNECTFIRST \"Must be connected.\\n\"\n"
"TLC_SYNTAX_DOWNLOAD \"Usage: download <datafile>\\n\"\n"
"TLC_REQUIRESSERVERMOD \"%s is only available with server support\\n\"\n"
"TLC_CLIENTCON_ERROR_ENDGAME \""CON_ERROR"Host_EndGame: %s\\n\"\n"
"TLC_HOSTFATALERROR \"Host_Error: %s\\n\"\n"
"TLC_CONFIGCFG_WRITEFAILED \"Couldn't write config.cfg.\\n\"\n"
"TLC_HOSTSPEEDSOUTPUT \"%3i tot %3i server %3i gfx %3i snd\\n\"\n"
"TLC_QUAKEWORLD_INITED \"^Ue080^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081 QuakeWorld Initialized ^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue082\\n\"\n"
"TLC_DEDICATEDCANNOTCONNECT \"Connect ignored - dedicated. set a renderer first\\n\"\n"
"TLC_Q2CONLESSPACKET_UNKNOWN \"unknown connectionless packet for q2:  %s\\n\"\n"
"TL_NORELATIVEPATHS \"Refusing to download a path with ..\\n\"\n"
"TL_NODOWNLOADINDEMO \"Unable to download %s in record mode.\\n\"\n"
"TL_DOWNLOADINGFILE \"Downloading %s...\\n\"\n"
"TLC_CHECKINGMODELS \"Checking models...\\n\"\n"
"TLC_CHECKINGSOUNDS \"Checking sounds...\\n\"\n"
"TL_FILENOTFOUND \"File not found.\\n\"\n"
"TL_CLS_DOWNLOAD_ISSET \"cls.download shouldn't have been set\\n\"\n"
"TL_FAILEDTOOPEN \"Failed to open %s\\n\"\n"
"TL_RENAMEFAILED \"failed to rename.\\n\"\n"
"TL_UPLOADCOMPLEATE \"Upload completed\\n\"\n"
"TL_FTEEXTENSIONS \"Using FTE extensions 0x%x%x\\n\"\n"
"TLC_LINEBREAK_NEWLEVEL \"\\n\\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\\n\\n\"\n"
"TLC_PC_PS_NL \"%c%s\\n\"\n"
"TLC_NOQ2CINEMATICSSUPPORT \"Cinematics on q2 levels is not yet supported\\nType 'cmd nextserver %i' to proceed.\"\n"
"TLC_GOTSVDATAPACKET \"Serverdata packet received.\\n\"\n"
"TLC_BAD_MAXCLIENTS \"Bad maxclients from server\\n\"\n"
"TLC_TOOMANYMODELPRECACHES \"Server sent too many model precaches\\n\"\n"
"TLC_TOOMANYSOUNDPRECACHES \"Server sent too many sound precaches\\n\"\n"
"TLC_PARSESTATICWITHNOMAP \"Warning: Parsestatic and no map loaded yet\\n\"\n"
"TL_FILE_X_MISSING \"\\nThe required model file '%s' could not be found or downloaded.\\n\\n\"\n"
"TL_GETACLIENTPACK \"You may need to download or purchase a %s client or map pack in order to play on this server.\\n\\n\"\n"
"TLC_LINEBREAK_MINUS \"------------------\\n\"\n"
"TL_INT_SPACE \"%i \"\n"
;

//client may remap messages from the server to a regional bit of text.
//server may remap progs messages

//basic language is english (cos that's what (my version of) Quake uses).
//translate is english->lang
//untranslate is lang->english for console commands.



cvar_t language = SCVAR("language", "uk");
char lastlang[9];

typedef struct trans_s {
	char *english;
	char *foreign;
	struct trans_s *next;
} trans_t;

trans_t *firsttranslation;

void TranslateReset(void)
{
	trans_t *trans;
	char *f, *eng, *fore;
//	FILE *F;
	char *s, *s1, *s2;
	/*
	if (*lastlang)
	{
		//write
		F = fopen(va("%s/%s.lng", com_gamedir, lastlang), "wb");
		if (F)
		{
			for (trans = firsttranslation; trans; trans=trans->next)
			{
				if (strchr(trans->english, '\n') || strchr(trans->english, '\"') || strchr(trans->english, '\\') || strchr(trans->english, '\t'))
				{
					s = trans->english;
					fputc('"', F);
					while(*s)
					{
						if (*s == '\n')
							fprintf(F, "\\n");
						else if (*s == '\\')
							fprintf(F, "\\\\");
						else if (*s == '\"')
							fprintf(F, "\"");
						else if (*s == '\t')
							fprintf(F, "\\t");
						else
							fputc(*s, F);

						s++;
					}
					fputc('"', F);
				}
				else
					fprintf(F, "\"%s\"", trans->english);
				fputc(' ', F);
				if (strchr(trans->foreign, '\n') || strchr(trans->foreign, '\"') || strchr(trans->foreign, '\\') || strchr(trans->foreign, '\t'))
				{
					s = trans->foreign;
					fputc('"', F);
					while(*s)
					{
						if (*s == '\n')
							fprintf(F, "\\n");
						else if (*s == '\\')
							fprintf(F, "\\\\");
						else if (*s == '\"')
							fprintf(F, "\"");
						else if (*s == '\t')
							fprintf(F, "\\t");
						else
							fputc(*s, F);

						s++;
					}
					fputc('"', F);
					fputc('\n', F);
				}
				else
					fprintf(F, "\"%s\"\n", trans->foreign);
			}
			fclose(F);
		}
	}*/
	Q_strncpyz(lastlang, language.string, 8);
	if (*language.string)
	{
		firsttranslation = NULL;
		//read in
		f = COM_LoadTempFile(va("%s.lng", lastlang));
		s = f;
		next:
		while (s && *s)
		{
			if (*s == '\"')
			{
				s++;
				eng = s;

				while(*s)
				{
					if (*s == '\"')
					{
						*s = '\0';	//end of from
						s++;
						while (*s)
						{
							if (*s == '\"')
							{
								s++;
								fore = s;

								while(*s)
								{
									if (*s == '\"')
									{
										*s = '\0';	//end of to
										s++;
										if (!firsttranslation)
											trans = firsttranslation = Z_Malloc(sizeof(trans_t)+strlen(eng)+strlen(fore)+2);
										else
										{
											for (trans = firsttranslation; trans->next; trans=trans->next) ;
											trans = (trans->next = Z_Malloc(sizeof(trans_t)+strlen(eng)+strlen(fore)+2));
										}

										trans->english = (char *)(trans+1);
										trans->foreign = trans->english + strlen(eng) + 1;

										s1 = trans->english;
										s2 = eng;
										while(*s2)
										{
											if (*s2 == '\\')
											{
												s2++;
												switch(*s2)
												{
												case 'n':
													*s1 = '\n';
													break;
												case '\"':
													*s1 = '\"';
													break;
												case 't':
													*s1 = '\t';
													break;
												case '\\':
													*s1 = '\\';
													break;
												default:
													*s1 = '?';
													break;
												}
											}
											else
												*s1 = *s2;
											s1++;
											s2++;
										}
										//strcpy(trans->english, eng);
										s1 = trans->foreign;
										s2 = fore;
										while(*s2)
										{
											if (*s2 == '\\')
											{
												s2++;
												switch(*s2)
												{
												case 'n':
													*s1 = '\n';
													break;
												case '\"':
													*s1 = '\"';
													break;
												case 't':
													*s1 = '\t';
													break;
												case '\\':
													*s1 = '\\';
													break;
												default:
													*s1 = '?';
													break;
												}
											}
											else
												*s1 = *s2;
											s1++;
											s2++;
										}
//										strcpy(trans->foreign, fore);
										goto next;
									}
									else if (*s == '\\')	//skip
										s++;
									s++;
								}
							}
							else if (*s == '\\')	//skip
								s++;
							s++;
						}
					}
					else if (*s == '\\')	//skip
						s++;
					s++;
				}
			}
			else if (*s == '\\')	//skip
				s++;
			s++;
		}
	}
}

char *Translate(char *message)
{
	return message;

	//this is pointless.

/*	trans_t *trans;
	if (!*message)
		return message;
	if (Q_strncmp(language.string, lastlang, 8))
	{
		TranslateReset();
	}

	for (trans = firsttranslation; trans; trans=trans->next)
	{
		if (*trans->english == *message)	//it's a little faster
		if (!Q_strcmp(trans->english+1, message+1))
			return trans->foreign;
	}

//add translation info to data

	if (!firsttranslation)
		trans = firsttranslation = Z_Malloc(sizeof(trans_t) + strlen(message)+1);
	else
	{
		for (trans = firsttranslation; trans->next; trans=trans->next) ;
		trans = (trans->next = Z_Malloc(sizeof(trans_t) + strlen(message)+1));
	}
	trans->english = (char *)(trans+1);
	trans->foreign = (char *)(trans+1);
	strcpy(trans->english, message);
	//strcpy(trans->foreign, message);

	return message;
*/
}

char *untranslate(char *message)
{
	return message;
}

void TranslateInit(void)
{
	Cvar_Register(&language, "International variables");
}










char *languagetext[STL_MAXSTL][MAX_LANGUAGES];

void TL_ParseLanguage (char *name, char *data, int num)	//this is one of the first functions to be called. so it mustn't use any quake subsystem routines
{
	int i;
	char *s;

	s = data;
	while(s)
	{
		s = COM_Parse(s);
		if (!s)
			return;

		for (i = 0; i < STL_MAXSTL; i++)
		{
			if (!strcmp(com_token, langtext(i, 0)))	//lang 0 is actually the string names.
				break;
		}

		s = COM_ParseCString(s);
		if (i == STL_MAXSTL)	//silently ignore - allow other servers or clients to add stuff
			continue;

		langtext(i, num) = malloc(strlen(com_token)+1);
		strcpy(langtext(i, num), com_token);
//		langtext(i, num) = "";
	}
}

void TL_LoadLanguage (char *name, char *shortname, int num)	//this is one of the first functions to be called.
{
	FILE *f;
	int size;
	char *buffer;
	size_t result;

	f = fopen(va("%s.trl", shortname), "rb");
	if (!f)
		return;
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	buffer = malloc(size+1);
	buffer[size] = '\0';
	result = fread(buffer, 1, size, f); // do something with result

	if (result != size)
		Con_Printf("TL_LoadLanguage() fread: Filename: %s, expected %i, result was %i (%i)\n",va("%s.trl", shortname),size,result,errno);

	fclose(f);

	TL_ParseLanguage(name, buffer, num);
	free(buffer);
}
#ifdef _DEBUG
#define CONVERTDEFAULT
#endif
#ifdef CONVERTDEFAULT

char *TL_ExpandToCString(char *in)
{
	static char buffer[2048];
	char *out = buffer;

	while(*in)
	{
		if (*in == '\"' || *in == '\\')
		{
			*out++ = '\\';
			*out = *in;
		}
		else if (*in == '\n')
		{
			*out++ = '\\';
			*out++ = 'n';
			*out++ = '\"';
			*out++ = '\n';
			*out = '\"';
		}
/*		else if (*in == '\t')
		{
			*out++ = '\\';
			*out = '\t';
		}*/
		else if (*in == '\r')
		{
			in++;
			continue;
		}
		else
		{
			*out = *in;
		}

		out++;
		in++;
	}
	*out = '\0';

	return buffer;
}
char *TL_ExpandToDoubleCString(char *in) //TL_ExpandToCString twice
{
	static char buffer[2048];
	char *out = buffer;

	while(*in)
	{
		if (*in == '\"' || *in == '\\')
		{
			*out++ = '\\';
			*out = *in;
		}
		else if (*in == '\n')
		{
			*out++ = '\\';
			*out = 'n';
		}
		else
		{
			*out = *in;
		}

		out++;
		in++;
	}
	*out = '\0';

	return TL_ExpandToCString(buffer);
}
void TL_WriteTLHeader(void)
{
	/*
	int i;
	FILE *f;
	f = fopen("tlout.h", "wb");
	if (!f)
		return;

	for (i = 0; i < STL_MAXSTL; i++)
	{
		if (langtext(i, 1))
		{
			fprintf(f, "\"%s \\\"%s\\\"\\n\"\n", langtext(i, 0), TL_ExpandToDoubleCString(langtext(i, 1)));
		}
	}
	fclose(f);*/
}
#endif

void TL_InitLanguages(void)
{
	int i, j;
	int lang;

	#define NAME(i) (languagetext[i][0] = #i);
	#include "translate.h"
	#undef NAME
/*
	#define ENGLISH(i, s) (languagetext[i][1] = s)
	#undef ENGLISH
*/
	TL_ParseLanguage("English", defaultlanguagetext, 1);
	TL_LoadLanguage("English", "english", 1);
	TL_LoadLanguage("Spanish", "spanish", 2);
	TL_LoadLanguage("Portuguese", "portu", 3);
	TL_LoadLanguage("French", "french", 4);

	if ((i = COM_CheckParm("-lang")))
		lang = atoi(com_argv[i+1]);
	else
		lang = 1;

	if (lang < 1)
		lang = 1;
	if (lang >= MAX_LANGUAGES)
		lang = MAX_LANGUAGES-1;
#ifndef CLIENTONLY
	svs.language = lang;
#endif
#ifndef SERVERONLY
	cls.language = lang;
#endif

#ifdef CONVERTDEFAULT
	TL_WriteTLHeader();
#endif

//	Sys_Printf("-lang %i\n", lang);

	for (i = 0; i < STL_MAXSTL; i++)
	{
		if (!langtext(i, 1))
		{
			Sys_Printf("warning: default translation for %s isn't set\n", langtext(i, 0));
			langtext(i, 1) = "";
		}
	}
	if (COM_CheckParm("-langugly"))	//so non-translated strings show more clearly (and you know what they are called).
	{
		for (j = 2; j < MAX_LANGUAGES; j++)
		for (i = 0; i < STL_MAXSTL; i++)
		{
			if (!langtext(i, j))
			{
				langtext(i, j) = langtext(i, 0);
			}
		}
	}
	else
	{
		for (j = 2; j < MAX_LANGUAGES; j++)
		for (i = 0; i < STL_MAXSTL; i++)
		{
			if (!langtext(i, j))
			{
				langtext(i, j) = langtext(i, 1);
			}
		}
	}
}




#ifndef CLIENTONLY
//this stuff is for hexen2 translation strings.
//(hexen2 is uuuuggllyyyy...)
static char *strings_list;
static char **strings_table;
static int strings_count;
static qboolean strings_loaded;
void T_FreeStrings(void)
{	//on map change, following gamedir change
	if (strings_loaded)
	{
		BZ_Free(strings_list);
		BZ_Free(strings_table);
		strings_count = 0;
		strings_loaded = false;
	}
}
void T_LoadString(void)
{
	int i;
	char *s, *s2;
	//count new lines
	strings_loaded = true;
	strings_count = 0;
	strings_list = FS_LoadMallocFile("strings.txt");
	if (!strings_list)
		return;

	for (s = strings_list; *s; s++)
	{
		if (*s == '\n')
			strings_count++;
	}
	strings_table = BZ_Malloc(sizeof(char*)*strings_count);

	s = strings_list;
	for (i = 0; i < strings_count; i++)
	{
		strings_table[i] = s;
		s2 = strchr(s, '\n');
		if (!s2)
			break;

		while (s < s2)
		{
			if (*s == '\r')
				*s = '\0';
			else if (*s == '^' || *s == '@')	//becomes new line
				*s = '\n';
			s++;
		}
		s = s2+1;
		*s2 = '\0';
	}
}
char *T_GetString(int num)
{
	if (!strings_loaded)
	{
		T_LoadString();
	}
	if (num<0 || num >= strings_count)
		return "BAD STRING";

	return strings_table[num];
}
#endif

#ifndef SERVERONLY
static char *info_strings_list;
static char **info_strings_table;
static int info_strings_count;
static qboolean info_strings_loaded;
void T_FreeInfoStrings(void)
{	//on map change, following gamedir change
	if (info_strings_loaded)
	{
		BZ_Free(info_strings_list);
		BZ_Free(info_strings_table);
		info_strings_count = 0;
		info_strings_loaded = false;
	}
}
void T_LoadInfoString(void)
{
	int i;
	char *s, *s2;
	//count new lines
	info_strings_loaded = true;
	info_strings_count = 0;
	info_strings_list = FS_LoadMallocFile("infolist.txt");
	if (!info_strings_list)
		return;

	for (s = info_strings_list; *s; s++)
	{
		if (*s == '\n')
			info_strings_count++;
	}
	info_strings_table = BZ_Malloc(sizeof(char*)*info_strings_count);

	s = info_strings_list;
	for (i = 0; i < info_strings_count; i++)
	{
		info_strings_table[i] = s;
		s2 = strchr(s, '\n');
		if (!s2)
			break;

		while (s < s2)
		{
			if (*s == '\r')
				*s = '\0';
			else if (*s == '^' || *s == '@')	//becomes new line
				*s = '\n';
			s++;
		}
		s = s2+1;
		*s2 = '\0';
	}
}
char *T_GetInfoString(int num)
{
	if (!info_strings_loaded)
	{
		T_LoadInfoString();
	}
	if (num<0 || num >= info_strings_count)
		return "BAD STRING";

	return info_strings_table[num];
}
#endif