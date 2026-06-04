#include "../plugin.h"

qboolean ORE_Init(void);
qboolean CMF_Init(void);

qboolean Plug_Init(void)
{
	char plugname[] = "narbdrop";

	// set plugin name
	plugfuncs->GetPluginName(0, plugname, sizeof(plugname));

	// initialize components
	if (!ORE_Init()) return false;
	if (!CMF_Init()) return false;

	return true;
}
