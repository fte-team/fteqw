string osgkname;

void(float event, float button, float mousex, float mousey) OSGKMenuEvent =
{
	float f;
	string str;
	if (event == ME_DRAW)
	{
		//nothing.
		return;
	}

	if (event == ME_MOUSEMOVE)
	{
		gecko_mousemove(osgkname, mousex/640, mousey/480);
		return;
	}
	if (event == ME_KEYDOWN)
	{
		if (button == K_ESCAPE)
		{
			Menu_Main();
			return;
		}
		gecko_keyevent(osgkname, button, 2);
	}
/*	if (event == ME_KEYUP)
	{
		gecko_keyevent(osgkname, button, 1);
	}
*/};


void(string posgkname) Menu_OSGK
{
	if (osgkname)
		strunzone(osgkname);
	osgkname = strzone(posgkname);
	Menu_Activate(OSGKMenuEvent);
};
