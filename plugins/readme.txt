Plugins are a low priority.

There are a few plugins at the moment:
emailnot: pop3/imap email notification.
emailsv: pop3/smtp email server functionality (a bit pointless - won't even compile)
ezscript: command remapping to make fte compatable with ezqauke/fuhquake/mqwcl configs. 
hud: kinda cool. But use csqc instead.
irc: an irc client: use irc nick, irc open, that'll spawn a new console in quake which can accept irc commands naturally. ctrl+tab to switch consoles. Currently native only.
serverb: server browser. The one built in is better. This one was for the sake of it. Native only, could be rewritten for qvm.
winamp: greater winamp control, and general media menu. Native only.
xsv: An X windowing system server. Absolutly bug ridden. About the only thing it runs properly is Quake. Native only, for efficiency reasons.



To compile a qvm version of a plugin, go to the root plugin directory. edit paths.bat to change the Q3SrcDir variable to point to where you have the q3 sourcecode installed (qvms require q3 tools). You should almost definatly be using the gpled sourcecode release, from which you will need to compile q3asm yourself.
If you also set the QuakeDir variable, the install.bat files will copy the newly compiled qvms to fte/plugins, which is rather convienient.

Note that when building a qvm using id's version of lcc, you will be spammed by messages regarding compiler-dependant conversions.
There is no easy way to disable these messages without killing off all warnings (otherwise it would already be off).
The only way is to recompile the compiler. This requires msvc or a lot of C knoledge!
The actual sourcecode change is around line 621 or lcc/src/expr.c
Make it so it reads like this instead.
	case POINTER:
		if (src->op != dst->op)
			p = simplify(CVP, dst, p, NULL);
		else {
>			if (Aflag >= 1)
			if (isfunc(src->type) && !isfunc(dst->type)
			|| !isfunc(src->type) &&  isfunc(dst->type))
				warning("conversion from `%t' to `%t' is compiler dependent\n", p->type, type);
The > characture denotes the line to add. This will make the warning only appear when ansi compatability warnings are enabled.
You will then need to recompile.
With msvc installed, load up a command prompt, run vcvars32.bat (eg, with the quotes, type "c:\program files\microsoft visual studio\v98\bin\vcvars32.bat"), then go to the lcc directory (using the cd command) and type 'buildnt' (without quotes). You should now have a new lcc compiler without this pesky warning (by default).
This will then cause most of the qvm-compatable plugins to compile warning free.

If you ask, I will send a copy of the required qvm-compiling files to you. They will be either copied directly from the q3 source release, or self built with no more changes than in this file.