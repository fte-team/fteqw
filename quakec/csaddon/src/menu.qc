typedef struct
{
	float *current;
	float key;
	vector *mousepos;
	vector mousediff;
} menu_t;

void(menu_t menu, string fieldname, vector pos, float *value, float minv, float maxv) sliderf_widgit =
{
	local vector rpos, spos, col;
	local float f;
	local int render_only;
	local vector mousepos = *menu->mousepos;
	local vector mp = *menu->mousepos;
	local vector mousediff = menu->mousediff;
	local int draw_value = 1;	// 8 parameter limit?
	local float key = menu->key;

	render_only = 0;

	rpos = pos;
	spos = rpos;
	spos_x = pos_x + strlen(fieldname) * 8 + 4;
	/*if they're dragging us, update*/
	if ((int)menu->current == (int)value)
	{
		if (mousedown == 1)
		{
			*value = *value + mousediff_x/10;
			*value = bound(minv, *value, maxv);
			f = *value;
			mp_y = spos_y + 4;
			mp_x = spos_x + 9 * 8 * (f - minv) / (maxv - minv) + 4;
			*menu->mousepos = mp;
		}
		else
		{
			menu->current = (int*)__NULL__;
		}
	}
	else
		if (mousedown == 1)
			render_only = 1;

	if (key == -1 || render_only == 1 )
	{
		if (mousepos_y >= pos_y && mousepos_y < pos_y + 8 && render_only == 0)
		{
			col_x = (sin(time*3.14)+1) / 2;
			col_y = col_x;
			col_z = 1;
		}
		else
			col = '1 1 1';

		f = *value;
		if (f < minv)
			f = minv;
		if (f > maxv)
			f = maxv;
		drawstring(pos, fieldname, '8 8 0', col, 1, 0);
		drawstring(spos, "^Ue080^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue082", '8 8 0', '1 1 1', 1, 0);
		if (draw_value == 1)
		{
			drawstring(spos + '88 0', ftos(f), '8 8 0', col, 1, 0);
		}
		spos_x += 9*8*(f - minv) / (maxv - minv);
		drawstring(spos, "^Ue083", '8 8 0', '1 1 1', 1, 0);
	}
	else 	if (mousepos_y >= pos_y && mousepos_y < pos_y + 8)
	{
		if (key == 512)
		{
			f = *value;
			menu->current = value;
			mp_x = spos_x + 9 * 8 * (f - minv) / (maxv - minv) + 4;
			*menu->mousepos = mp;
		}
		else if (key == 515)
			*value = bound(minv, *value + (maxv - minv) / 10, maxv);
		else if (key == 516)
			*value = bound(minv, *value - (maxv - minv) / 10, maxv);
	}
};

#pragma wrasm 1
void(menu_t *menu, string fieldname, vector pos, float *value) sliderf_fixed_widgit =
{
	local vector rpos, spos, col;
	local float f;
	local int render_only;
	local vector mousepos = *menu->mousepos;
	local vector mp = *menu->mousepos;
	local vector mousediff = menu->mousediff;
	local vector *mpp = menu->mousepos;
	local int draw_value = 1;	// 8 parameter limit?
	local float key = menu->key;

	render_only = 0;

	rpos = pos;
	spos = rpos;
	spos_x = pos_x + strlen(fieldname) * 8 + 4;
	/*if they're dragging us, update*/
	//print(sprintf("%i %i\n", (int) menu->current, (int) value));
	if ((int)menu->current == (int)value)
	{
		print(sprintf("mousedown: %i\n", mousedown));
		if (mousedown == 1)
		{
			*value = *value + mousediff_x/10;
			f = *value;
			mp_y = spos_y + 4;
			mp_x = spos_x + 4;
			*menu->mousepos = mp;
		}
		else
		{
			menu->current = (int*)__NULL__;
		}
	}
	else
		if (mousedown == 1)
			render_only = 1;

	if (key == -1 || render_only == 1 )
	{
		if (mousepos_y >= pos_y && mousepos_y < pos_y + 8 && render_only == 0)
		{
			col_x = (sin(time*3.14)+1) / 2;
			col_y = col_x;
			col_z = 1;
		}
		else
			col = '1 1 1';

		f = *value;
		drawstring(pos, fieldname, '8 8 0', col, 1, 0);
		
		drawstring(spos, "^Ue080^Ue082", '8 8 0', '1 1 1', 1, 0);
		if (draw_value)
			drawstring(spos + '20 0', ftos(f), '8 8 0', col, 1, 0);
		drawstring(spos + '4 0', "^Ue083", '8 8 0', '1 1 1', 1, 0);
	}
	else 	if (mousepos_y >= pos_y && mousepos_y < pos_y + 8)
	{
		if (key == 512)
		{
			f = *value;
			int*tmp=(int*)menu;tmp[0] = (int)value; //fteqcc bug - really menu->current = value;
			print(sprintf("test thing: %i %i\n", menu->current, value));
			mp_x = spos_x + 4;
			*mpp = mp;
		}
		else if (key == 515)
			*value = *value + 0.1;
		else if (key == 516)
			*value = *value - 0.1;
	}
};
#pragma wrasm 0

void(menu_t menu, string fieldname, vector pos, int *value, int minv, int maxv) slideri_widgit =
{
	local vector rpos, spos, col;
	local int f;
	local int render_only;
	local vector mousepos = *menu->mousepos;
	local vector mp = *menu->mousepos;
	local vector mousediff = menu->mousediff;
	local int draw_value = 1;	// 8 parameter limit?
	local float key;

	key = menu->key;

	render_only = 0;

	rpos = pos;
	spos = rpos;
	spos_x = pos_x + strlen(fieldname) * 8 + 4;
	/*if they're dragging us, update*/
	if ((int)menu->current == (int)value)
	{
		if (mousedown == 1)
		{
			*value = *value + mousediff_x/10;
			*value = bound(minv, *value, maxv);
			f = *value;
			mp_y = spos_y + 4;
			mp_x = spos_x + 9 * 8 * (f - minv) / (maxv - minv) + 4;
			*menu->mousepos = mp;
		}
		else
		{
			menu->current = (int*)__NULL__;
		}
	}
	else
		if (mousedown == 1)
			render_only = 1;

	if (key == -1 || render_only == 1 )
	{
		if (mousepos_y >= pos_y && mousepos_y < pos_y + 8 && render_only == 0)
		{
			col_x = (sin(time*3.14)+1) / 2;
			col_y = col_x;
			col_z = 1;
		}
		else
			col = '1 1 1';

		f = *value;
		if (f < minv)
			f = minv;
		if (f > maxv)
			f = maxv;
		drawstring(pos, fieldname, '8 8 0', col, 1, 0);
		drawstring(spos, "^Ue080^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue082", '8 8 0', '1 1 1', 1, 0);
		if (draw_value == 1)
		{
			drawstring(spos + '88 0', ftos(f), '8 8 0', col, 1, 0);
		}
		spos_x += 9*8*(f - minv) / (maxv - minv);
		drawstring(spos, "^Ue083", '8 8 0', '1 1 1', 1, 0);
	}
	else 	if (mousepos_y >= pos_y && mousepos_y < pos_y + 8)
	{
		if (key == 512)
		{
			f = *value;
			menu->current = value;
			mp_x = spos_x + 9 * 8 * (f - minv) / (maxv - minv) + 4;
			*menu->mousepos = mp;
		}
		else if (key == 515)
			*value = bound(minv, *value + (maxv - minv) / 10, maxv);
		else if (key == 516)
			*value = bound(minv, *value - (maxv - minv) / 10, maxv);
	}
};


void(string fieldname, vector pos, vector *mousepos_in, float *value, float key) checkboxf_widgit =
{
	local vector rpos, col;
	local int render_only;
	local vector mousepos = *mousepos_in;
	rpos = pos + '132 0 0';

	render_only = 0;
	// fix this
	if (mousedown == 1)
		render_only = 1;

	if (key == -1 || render_only == 1)
	{
		if (mousepos_y >= pos_y && mousepos_y < pos_y + 8 && render_only == 0)
		{
			col_x = (sin(time*3.14)+1) / 2;
			col_y = col_x;
			col_z = 1;
		}
		else
			col = '1 1 1';

		drawstring(pos, fieldname, '8 8 0', col, 1, 0);
		rpos_x -= 4;
		drawstring(rpos, "^Ue080^Ue082", '8 8 0', '1 1 1', 1, 0);
		drawstring(rpos + '88 0', ((*value) ?"TRUE":"FALSE"), '8 8 0', col, 1, 0);
		rpos_x += 4;
		if (*value)
			drawstring(rpos, "^Ue083", '8 8 0', '1 1 1', 1, 0);
	}
	else 	if (mousepos_y >= pos_y && mousepos_y < pos_y + 8)
	{
		if (key == 512)
			*value = !*value;
		else if (key == 515)
			*value = !*value;
		else if (key == 516)
			*value = !*value;
	}
};

