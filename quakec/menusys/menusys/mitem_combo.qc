/***************************************************************************
combo item.
No longer using pointers, now using a tokenized string. Less efficient, but saves creating lots of different arrays, just pass a string.
The string list is doubled, first is the actual value, second is the friendly text.
Will show actual value when focused, and will show readable value when not.
The possible values is a separate popup.
*/

//FIXME: should probably set up a grabs to intercept right-click / escape outside of the item

class mitem_combo;
class mitem_combo_popup;

class mitem_combo : mitem
{
	virtual void(vector pos)								item_draw;
	virtual float(vector pos, float scan, float char, float down)	item_keypress;
	virtual void(mitem newfocus, float changedflag)				item_focuschange;

	mitem_combo_popup		cfriend;
	string	mstrlist;
	float		firstrow;
	float		visrows;

	virtual void() item_remove;
};

class mitem_combo_popup : mitem
{
	virtual void(vector pos)								item_draw;
	virtual float(vector pos, float scan, float char, float down)	item_keypress;
	virtual void(mitem newfocus, float changedflag)				item_focuschange;

	mitem_combo 		pfriend;
	
	virtual void() item_remove =
	{
		if (pfriend)
			pfriend.cfriend = 0;
		super::item_remove();
	};
};

void() mitem_combo::item_remove =
{
	mitem_combo_popup p = cfriend;
	if (p)
		p.item_remove();
	strunzone(mstrlist);
	super::item_remove();
};

void(vector pos) mitem_combo::item_draw =
{
	local float i, v;
	local string curval = get(item_command);
	vector rgb = item_rgb;
	if (!(item_flags & IF_SELECTABLE))
		rgb *= 0.2;
	
	if (cfriend)
		cfriend.item_position = pos + [item_size_x / 2, item_size_y];

	super::item_draw(pos);

	v = tokenize(mstrlist);

	//display the friendly string if the current value matches
//	if (!(item_flags & IF_KFOCUSED) && (!cfriend || !(cfriend.item_flags & IF_KFOCUSED)))
	{
		for (i = 0; i < v; i+=2)
		{
			if (argv(i) == curval)
			{
				curval = argv(i+1);
				break;
			}
		}
	}

	pos_x += item_size_x / 2;


/*	//border
	ui.drawfill(pos, [item_size_x/2, 1], TD_BOT, item_alpha, 0);
	ui.drawfill(pos, [1, item_size_y - 1], TD_RGT, item_alpha, 0);
	ui.drawfill(pos + [item_size_x/2-1, 1], [1, item_size_y - 1], TD_LFT, item_alpha, 0);
	ui.drawfill(pos + [0, item_size_y-1], [item_size_x/2, 1], TD_TOP, item_alpha, 0);
*/
	//silly strings need to get cut off properly.
	ui.setcliparea(pos[0], pos[1], item_size_x/2, item_size_y);
	pos_y += (item_size_y - item_scale)*0.5;
	pos_x += 1;
	ui.drawstring(pos, curval, '1 1 0' * item_scale, rgb, item_alpha, 0);
	ui.setcliparea(ui.drawrectmin[0], ui.drawrectmin[1], ui.drawrectmax[0] - ui.drawrectmin[0], ui.drawrectmax[1] - ui.drawrectmin[1]);
};
void(mitem newfocus, float flag) mitem_combo::item_focuschange =
{
	if (!cfriend || !(flag & IF_KFOCUSED))
		return;	//don't care
	if (newfocus != (mitem)this && newfocus != (mitem)cfriend)
	{
		cfriend.item_size = cfriend.maxs = '0 0';
		cfriend.item_flags &~= IF_SELECTABLE;
	}
};
void(mitem newfocus, float flag) mitem_combo_popup::item_focuschange =
{
	pfriend.item_focuschange(newfocus, flag);
};
void(vector pos) mitem_combo_popup::item_draw =
{
	vector col;
	if (item_size_y < 1)
		return;

	local mitem_combo f = pfriend;
	item_command = f.item_command;
	local string curval = f.get(f.item_command);
	local float i, m, c, v;

	if (!((f.item_flags | item_flags) & IF_KFOCUSED))
	{
		item_size = maxs = '0 0';
		item_flags &~= IF_SELECTABLE;
		return;
	}

	ui.drawfill(pos, item_size, item_rgb, item_alpha, 0);

/*	//border
	ui.drawfill(pos, [item_size_x, 1], TD_BOT, item_alpha, 0);
	ui.drawfill(pos, [1, item_size_y - 1], TD_RGT, item_alpha, 0);
	ui.drawfill(pos + [item_size_x-1, 1], [1, item_size_y - 1], TD_LFT, item_alpha, 0);
	ui.drawfill(pos + [0, item_size_y-1], [item_size_x, 1], TD_TOP, item_alpha, 0);
*/	pos_x += 1;

	v = tokenize(f.mstrlist);
	for (c = 0; c < v; c += 2)
		if (argv(c) == curval)
			break;
	if (c >= v)
		c = 0;

	i = f.firstrow;
	i = i*2;
	if (!f.visrows)
		i = 0;
	else
	{
		//bound the displayed position 
		if (c < i)
			i = c;
		if (i < c - (f.visrows-1)*2)
			i = c - (f.visrows-1)*2;
	}
	m = i + f.visrows*2;
	f.firstrow = floor(i*0.5);

	//constrain the drawing so it doesn't overflow the combo
	ui.setcliparea(pos[0], pos[1], item_size[0], item_size[1]);

	for (; i < m && i < v ; i+=2)
	{
		col = f.item_rgb;
		if (item_flags & IF_MFOCUSED)
			if (mouseinbox(pos, [item_size_x, item_scale]))
				col_z = 0;
		if (c == i)
			col_x = 0;

		ui.drawstring(pos, argv(i+1), '1 1 0' * item_scale, col, f.item_alpha, 0);
		pos_y += item_scale;
	}

	//reset the clip area
	ui.setcliparea(ui.drawrectmin[0], ui.drawrectmin[1], ui.drawrectmax[0] - ui.drawrectmin[0], ui.drawrectmax[1] - ui.drawrectmin[1]);
};
float(vector pos, float scan, float char, float down) mitem_combo_popup::item_keypress =
{
	return pfriend.item_keypress(pos - [0, pfriend.item_size_y], scan, char, down);
};
float(vector pos, float scan, float char, float down) mitem_combo::item_keypress =
{
	if (!down)
		return FALSE;

	local string curval = get(item_command);
	local float i, c;
	local float f;
	c = tokenize(mstrlist);

	//find which one is active
	for (i = 0; i < c; i+=2)
	{
		if (argv(i) == curval)
		{
			break;
		}
	}

	if (scan == K_ESCAPE || scan == K_MOUSE2)
	{
		if (cfriend)
		{
			cfriend.item_remove();
			return TRUE;
		}
		return FALSE;
	}
	else if (scan == K_MWHEELUP || (scan == K_UPARROW && cfriend))
	{
		i -= 2;
		if (i < 0)
			i = c - 2;
		curval = argv(i);
	}
	else if (scan == K_MWHEELDOWN || (scan == K_DOWNARROW && cfriend))
	{
		i += 2;
		if (i >= c)
			i = 0;
		curval = argv(i);
	}
	else if (scan == K_MOUSE1)
	{
		visrows = ((c>18)?18/2:c/2);
		if (!cfriend)
		{
			cfriend = spawn(mitem_combo_popup);
			cfriend.pfriend = this;
			cfriend.item_scale = 8;
			cfriend.item_rgb = MENUBACK_RGB;
			cfriend.item_alpha = MENUBACK_ALPHA;
			pos = item_position;
			mitem_frame fr = item_parent;
			while (fr.item_parent)
			{	//try to inject the combo thingie into the desktop item. this is to avoid scissoring.
				pos += fr.item_position;
				fr = fr.item_parent;
			}
			fr.addr(cfriend, RS_X_MIN_PARENT_MIN | RS_X_MAX_OWN_MIN | RS_Y_MIN_PARENT_MIN | RS_Y_MAX_OWN_MIN, pos + [self.item_size_x / 2, self.item_size_y], [item_size_x*0.5, item_size_y*visrows]);
		}
		cfriend.item_size = cfriend.maxs = [item_size_x*0.5, item_size_y*visrows];
		cfriend.item_flags |= IF_SELECTABLE;
		cfriend.totop();

		if (cfriend.item_flags & IF_MFOCUSED)
		{
			//if they clicked inside the popup, change the selected item.
			f = ui.mousepos[1] - (pos_y + item_size_y);
			f /= cfriend.item_scale;
			f += firstrow;
			i = floor(f) * 2;
			if (i < c)
			{
				curval = argv(i);
				cfriend.item_flags &~= IF_SELECTABLE;
				cfriend.item_size = cfriend.maxs = '0 0';
				item_parent.item_focuschange(this, IF_MFOCUSED|IF_KFOCUSED);
			}
		}
	}
	else if (scan == K_BACKSPACE || scan == K_DEL)
		curval = substring(curval, 0, -2);
	else if (char >= ' ')
		curval = strcat(curval, chr2str(char));
	else
		return FALSE;

	set(item_command, curval);
	return TRUE;
};
mitem_combo(string text, string command, vector sz, string valuelist) menuitemcombo_spawn =
{
	mitem_combo n = spawn(mitem_combo);
	n.item_scale = sz_y;
	n.item_text = text;
	n.item_size = sz;
	n.mstrlist = strzone(valuelist);

	n.item_command = command;
	if (n.isvalid(command))
		n.item_flags |= IF_SELECTABLE;
	return n;
};
