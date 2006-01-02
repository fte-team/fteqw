void bprint(float plev, string st, string s2){}
float PRINT_MEDIUM = 1;
#include "/../inventory.qc"


void() Invent_Draw =
{
	local float i;
	local float it;
	local string itname;

	for (i = 0; i < 16; i++)
	{
		it = getstati(32+i);
		itname = GetItemName(ToIID(it));
		drawstring('0 32 0' + (i * '0 8 0'), strcat(itname, " (", ftos(ToStatus(it)), ")"), '8 8 0', '1 1 1', 1);
	}
};
