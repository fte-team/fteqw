#include "qcc.h"
#include "gui.h"

void GoToDefinition(char *name)
{
	QCC_def_t *def;
	QCC_dfunction_t *fnc;

	char *strip;	//trim whitespace (for convieniance).
	while (*name <= ' ' && *name)
		name++;
	for (strip = name + strlen(name)-1; *strip; strip++)
	{
		if (*strip <= ' ')
			*strip = '\0';
		else	//got some part of a word
			break;
	}

	if (!globalstable.numbuckets)
	{
		MessageBox(NULL, "You need to compile first.", "Not found", 0);
		return;
	}


	def = QCC_PR_GetDef(NULL, name, NULL, false, 0);

	if (def)
	{
		if (def->type->type == ev_function && def->constant)
		{
			fnc = &functions[((int *)qcc_pr_globals)[def->ofs]];
			if (fnc->first_statement>=0 && fnc->s_file)
			{
				EditFile(fnc->s_file+strings, statement_linenums[fnc->first_statement]);
				return;
			}
		}
		EditFile(def->s_file+strings, def->s_line-1);
	}
	else
		MessageBox(NULL, "Global instance of var was not found", "Not found", 0);
}
