/***************************************************************************
editable text, directly linked to a cvar.
FIXME: This can only edit the end of the string.
*/
class mitem_edit : mitem
{
	virtual void(vector pos) item_draw;
	virtual float(vector pos, float scan, float char, float down) item_keypress;
	virtual void() item_remove;
	float spos;
	
	virtual void() item_resized =
	{
		if (isvalid(item_command))
			item_flags |= IF_SELECTABLE;		
		else
			item_flags &= ~IF_SELECTABLE;
		super::item_resized();
	};
};

void() mitem_edit::item_remove =
{
	strunzone(item_text);
	strunzone(item_command);
	super::item_remove();
};

void(vector pos) mitem_edit::item_draw =
{
	local string curval = get(item_command);

	super::item_draw(pos);

	pos_x += item_size_x / 2;
/*	ui.drawfill(pos, [item_size_x/2, 1], TD_BOT, item_alpha, 0);
	ui.drawfill(pos, [1, self.item_size_y - 1], TD_RGT, item_alpha, 0);
	ui.drawfill(pos + [item_size_x/2-1, 1], [1, item_size_y - 1], TD_LFT, item_alpha, 0);
	ui.drawfill(pos + [0, item_size_y-1], [item_size_x/2, 1], TD_TOP, item_alpha, 0);
*/	pos_y += (item_size_y - item_scale)*0.5;
	pos_x += 1;

	spos = min(spos, strlen(curval));
	if (((cltime*4)&1) && (item_flags & IF_KFOCUSED))
		curval = strcat(substring(curval, 0, spos), chr2str(0xe00b), substring(curval, spos+1, -1));	//replace the char with a box... ugly, whatever
	ui.drawstring(pos, curval, '1 1 0' * item_scale, item_rgb, item_alpha, 0);
};
float(vector pos, float scan, float char, float down) mitem_edit::item_keypress =
{
	if (!down)
		return FALSE;

	local string curval = get(item_command);
	spos = min(spos, strlen(curval));

	if (scan == K_ESCAPE)
		return FALSE;
	else if (scan == K_LEFTARROW)
		spos = max(spos-1, 0);
	else if (scan == K_RIGHTARROW)
		spos+=1;
/*	else if (scan == K_MOUSE1)
	{
		//FIXME: figure out the spos for the cursor
		return TRUE;
	}*/
	else if (scan == K_BACKSPACE || scan == K_DEL)
	{
		if (spos)
		{
			curval = strcat(substring(curval, 0, spos-1), substring(curval, spos, -1));
			spos -= 1;
		}
	}
	else if (char >= ' ')
	{
		curval = strcat(substring(curval, 0, spos), chr2str(char), substring(curval, spos, -1));
		spos += strlen(chr2str(char));
	}
	else
		return FALSE;

	set(item_command, curval);
	return TRUE;
};
mitem_edit(string text, string command, vector sz) menuitemeditt_spawn =
{
	mitem_edit n = spawn(mitem_edit);
	n.item_scale = sz_y;
	n.item_text = strzone(text);
	n.item_size = sz;
	n.spos = 100000;	//will be clipped so meh

	n.item_command = strzone(command);
	n.item_flags |= IF_SELECTABLE;
	return n;
};

