#include "../plugin.h"
qboolean VPK_Init(void);
qboolean VTF_Init(void);
qboolean MDL_Init(void);

qboolean Plug_Init(void)
{
	if (!VPK_Init())
		return false;
	VTF_Init();
	MDL_Init();
	return true;
}

