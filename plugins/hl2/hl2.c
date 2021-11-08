#include "../plugin.h"
qboolean VPK_Init(void);
qboolean VTF_Init(void);

qboolean Plug_Init(void)
{
	if (!VPK_Init())
		return false;
	if (!VTF_Init())
		return false;
	return true;
}

