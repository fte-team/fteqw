#include <xtl.h>
#include "quakedef.h"       

void main( int argc, char **argv) {
	float time, lasttime;
	quakeparms_t parms;

	memset(&parms, 0, sizeof(parms));

	//fill in parms
	COM_InitArgv(parms.argc, parms.argv);
	TL_InitLanguages(parms.basedir);
	Host_Init(&parms);

	//main loop
	lasttime = Sys_DoubleTime();

	while (1)
	{
		time = Sys_DoubleTime();
		Host_Frame(time - lasttime);
		lasttime = time;
	}

}