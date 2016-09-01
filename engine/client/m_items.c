//read menu.h

#include "quakedef.h"
#include "shader.h"

void Draw_TextBox (int x, int y, int width, int lines)
{
	mpic_t	*p;
	int		cx, cy;
	int		n;

	// draw left side
	cx = x;
	cy = y;
	p = R2D_SafeCachePic ("gfx/box_tl.lmp");

	if (R_GetShaderSizes(p, NULL, NULL, false) != true)	//assume none exist
	{
		R2D_ImageColours(0.0, 0.0, 0.0, 0.5);
		R2D_FillBlock(x, y, width*8 + 16, 8 * (2 + lines));
		R2D_ImageColours(1.0, 1.0, 1.0, 1.0);
		return;
	}

	if (p)
		R2D_ScalePic (cx, cy, 8, 8, p);
	p = R2D_SafeCachePic ("gfx/box_ml.lmp");
	for (n = 0; n < lines; n++)
	{
		cy += 8;
		if (p)
			R2D_ScalePic (cx, cy, 8, 8, p);
	}
	p = R2D_SafeCachePic ("gfx/box_bl.lmp");
	if (p)
		R2D_ScalePic (cx, cy+8, 8, 8, p);

	// draw middle
	cx += 8;
	while (width > 0)
	{
		cy = y;
		p = R2D_SafeCachePic ("gfx/box_tm.lmp");
		if (p)
			R2D_ScalePic (cx, cy, 16, 8, p);
		p = R2D_SafeCachePic ("gfx/box_mm.lmp");
		for (n = 0; n < lines; n++)
		{
			cy += 8;
			if (n == 1)
				p = R2D_SafeCachePic ("gfx/box_mm2.lmp");
			if (p)
				R2D_ScalePic (cx, cy, 16, 8, p);
		}
		p = R2D_SafeCachePic ("gfx/box_bm.lmp");
		if (p)
			R2D_ScalePic (cx, cy+8, 16, 8, p);
		width -= 2;
		cx += 16;
	}

	// draw right side
	cy = y;
	p = R2D_SafeCachePic ("gfx/box_tr.lmp");
	if (p)
		R2D_ScalePic (cx, cy, 8, 8, p);
	p = R2D_SafeCachePic ("gfx/box_mr.lmp");
	for (n = 0; n < lines; n++)
	{
		cy += 8;
		if (p)
			R2D_ScalePic (cx, cy, 8, 8, p);
	}
	p = R2D_SafeCachePic ("gfx/box_br.lmp");
	if (p)
		R2D_ScalePic (cx, cy+8, 8, 8, p);
}

#ifndef NOBUILTINMENUS

int omousex;
int omousey;
qboolean mousemoved;
qboolean bindingactive;
extern cvar_t cl_cursor;
extern cvar_t cl_cursorsize;
extern cvar_t cl_cursorbias;
extern cvar_t m_preset_chosen;
menu_t *currentmenu;
menu_t *firstmenu;
menuoption_t *M_NextSelectableItem(menu_t *m, menuoption_t *old);

#ifdef HEXEN2
//this function is so fucked up.
//firstly, the source image uses 0 for transparent instead of 255. this means we need special handling. *sigh*.
//secondly we have to avoid sampling too much of the image, because i chars seem to have stray white pixels in them
//thirdly, we hard-code (by eye) the space between chars, which should be different for any character pair.
//but we're lazy so we don't consider the next char. italic fonts are annoying like that. feel free to refudge it.
void Draw_Hexen2BigFontString(int x, int y, const char *text)
{
	int c;
	int sx, sy;
	mpic_t *p;
	p = R_RegisterShader ("gfx/menu/bigfont.lmp", SUF_2D,
		"{\n"
			"if $nofixed\n"
				"program default2d\n"
			"endif\n"
			"affine\n"
			"nomipmaps\n"
			"{\n"
				"clampmap $diffuse\n"
				"rgbgen vertex\n"
				"alphagen vertex\n"
				"blendfunc gl_one gl_one_minus_src_alpha\n"
			"}\n"
			"sort additive\n"
		"}\n");
	if (!p->defaulttextures->base)
	{
		void *file;
		qofs_t fsize = FS_LoadFile("gfx/menu/bigfont.lmp", &file);
		if (file)
		{
			unsigned int w = ((unsigned int*)file)[0];
			unsigned int h = ((unsigned int*)file)[1];
			if (8+w*h==fsize)
				p->defaulttextures->base = R_LoadReplacementTexture("gfx/menu/bigfont.lmp", NULL, IF_NOPCX|IF_PREMULTIPLYALPHA|IF_UIPIC|IF_NOPICMIP|IF_NOMIPMAP|IF_CLAMP, (qbyte*)file+8, w, h, TF_H2_TRANS8_0);
			FS_FreeFile(file);	//got image data
		}
		if (!p->defaulttextures->base)
			p->defaulttextures->base = R_LoadHiResTexture("gfx/menu/bigfont.lmp", NULL, IF_PREMULTIPLYALPHA|IF_UIPIC|IF_NOPICMIP|IF_NOMIPMAP|IF_CLAMP);
	}

	while(*text)
	{
		c = *text++;
		if (c >= 'a' && c <= 'z')
		{
			sx = ((c-'a')%8)*20;
			sy = ((c-'a')/8)*20;
		}
		else if (c >= 'A' && c <= 'Z')
		{
			c = c - 'A' + 'a';
			sx = ((c-'a')%8)*20;
			sy = ((c-'a')/8)*20;
		}
		else// if (*text <= ' ')
		{
			sx=-1;
			sy=-1;
		}
		if(sx>=0)
			R2D_SubPic(x, y, 18, 20, p, sx, sy, 20*8, 20*4);

		switch(c)
		{
		case 'a':	x+=15; break;
		case 'b':	x+=15; break;
		case 'c':	x+=15; break;
		case 'd':	x+=15; break;
		case 'e':	x+=15; break;
		case 'f':	x+=15; break;
		case 'g':	x+=15; break;
		case 'h':	x+=15; break;
		case 'i':	x+=10; break;
		case 'j':	x+=15; break;
		case 'k':	x+=18; break;
		case 'l':	x+=15; break;
		case 'm':	x+=18; break;
		case 'n':	x+=15; break;
		case 'o':	x+=15; break;
		case 'p':	x+=15; break;
		case 'q':	x+=18; break;
		case 'r':	x+=18; break;
		case 's':	x+=13; break;
		case 't':	x+=15; break;
		case 'u':	x+=15; break;
		case 'v':	x+=12; break;
		case 'w':	x+=15; break;
		case 'x':	x+=18; break;
		case 'y':	x+=15; break;
		case 'z':	x+=18; break;
		default:	x+=20; break;
		}
	}
}
#endif

mpic_t *QBigFontWorks(void)
{
	mpic_t *p;
	int i;
	char *names[] = {
		"gfx/mcharset.lmp",
		"mcharset.lmp",
		"textures/gfx/mcharset.lmp",
		"textures/mcharset.lmp",
		NULL
	};
	for (i = 0; names[i]; i++)
	{
		p = R2D_SafeCachePic (names[i]);
		if (p && R_GetShaderSizes(p, NULL, NULL, true))
			return p;
	}
	return NULL;
}
void Draw_BigFontString(int x, int y, const char *text)
{
	int sx, sy;
	mpic_t *p;
	p = QBigFontWorks();
	if (!p)
	{
		Draw_FunString(x, y, text);
		return;
	}

	{	//a hack for scaling
		p->width = 20*8;
		p->height = 20*8;
	}

	while(*text)
	{
		if (*text >= 'A' && *text <= 'Z')
		{
			sx = ((*text-'A')%8)*(p->width>>3);
			sy = ((*text-'A')/8)*(p->height>>3);
		}
		else if (*text >= 'a' && *text <= 'z')
		{
			sx = ((*text-'a'+26)%8)*(p->width>>3);
			sy = ((*text-'a'+26)/8)*(p->height>>3);
		}
		else if (*text >= '0' && *text <= '1')
		{
			sx = ((*text-'0'+26*2)%8)*(p->width>>3);
			sy = ((*text-'0'+26*2)/8)*(p->height>>3);
		}
		else if (*text == ':')
		{
			sx = ((*text-'0'+26*2+10)%8)*(p->width>>3);
			sy = ((*text-'0'+26*2+10)/8)*(p->height>>3);
		}
		else if (*text == '/')
		{
			sx = ((*text-'0'+26*2+11)%8)*(p->width>>3);
			sy = ((*text-'0'+26*2+11)/8)*(p->height>>3);
		}
		else// if (*text <= ' ')
		{
			sx=-1;
			sy=-1;
		}
		if(sx>=0)
			R2D_SubPic(x, y, 20, 20, p, sx, sy, 20*8, 20*8);
		x+=(p->width>>3);
		text++;
	}
}

char *menudotstyle;
int maxdots;
int mindot;
int dotofs;

static void MenuTooltipChange(menu_t *menu, const char *text)
{
	unsigned int MAX_CHARS=1024;
	menutooltip_t *mtt;
	if (menu->tooltip)
	{
		Z_Free(menu->tooltip);
		menu->tooltip = NULL;
	}

	if (!text || !text[0] || vid.width < 320 || vid.height < 200)
		return;

	// allocate new tooltip structure, copy text to structure
	mtt = (menutooltip_t *)Z_Malloc(sizeof(menutooltip_t) + sizeof(conchar_t)*MAX_CHARS);
	mtt->end = COM_ParseFunString(CON_WHITEMASK, text, mtt->text, sizeof(conchar_t)*MAX_CHARS, false);

	menu->tooltip = mtt;
}

static qboolean MI_Selectable(menuoption_t *op)
{
	switch(op->common.type)
	{
	case mt_text:
		return false;
	case mt_button:
		return true;
#ifdef HEXEN2
	case mt_hexen2buttonbigfont:
		return true;
#endif
	case mt_qbuttonbigfont:
		return true;
	case mt_menudot:
		return false;
	case mt_picturesel:
		return true;
	case mt_picture:
		return false;
	case mt_childwindow:
		return true;
	case mt_box:
		return false;
	case mt_slider:
		return true;
	case mt_checkbox:
		return true;
	case mt_edit:
		return true;
	case mt_bind:
		return true;
	case mt_combo:
		return true;
	case mt_custom:
		return true;
	default:
		return false;
	}
}

static void M_CheckMouseMove(void)
{
	qboolean foundexclusive = false;
	menu_t *menu;
	menuoption_t *option;

	if (omousex != (int)mousecursor_x || omousey != (int)mousecursor_y)
		mousemoved = true;
	else
		mousemoved = false;
	omousex = mousecursor_x;
	omousey = mousecursor_y;

	if (mousemoved)
	{
		for (menu = firstmenu; menu; menu = menu->parent)
		{
			if (menu->exclusive)
			{
				if (foundexclusive)
					continue;
				foundexclusive=true;
			}

			for(option = menu->options; option; option = option->common.next)
			{
				if (mousemoved && !bindingactive && !option->common.ishidden)
				{
					if (mousecursor_x > menu->xpos+option->common.posx-option->common.extracollide && mousecursor_x < menu->xpos+option->common.posx+option->common.width)
					{
						if (mousecursor_y > menu->ypos+option->common.posy && mousecursor_y < menu->ypos+option->common.posy+option->common.height)
						{
							if (MI_Selectable(option))
							{
								if (menu->selecteditem != option)
								{
									if (!option->common.noselectionsound && vid.activeapp)
									{
#ifdef HEXEN2
										if (M_GameType() == MGT_HEXEN2)
											S_LocalSound ("raven/menu1.wav");
										else
#endif
											S_LocalSound ("misc/menu1.wav");
									}

									menu->selecteditem = option;
									menu->tooltiptime = realtime + 1;
									MenuTooltipChange(menu, menu->selecteditem->common.tooltip);
								}
								if (menu->cursoritem)
									menu->cursoritem->common.posy = menu->selecteditem->common.posy;
							}
						}
					}
				}
			}
		}
	}
}

static void MenuDrawItems(int xpos, int ypos, menuoption_t *option, menu_t *menu)
{
	int i;
	mpic_t *p;
	int pw,ph;

	while (option)
	{
		if (!option->common.ishidden)
		switch(option->common.type)
		{
		case mt_menucursor:
			if ((int)(realtime*4)&1)
				Draw_FunString(xpos+option->common.posx, ypos+option->common.posy, "^Ue00d");
			break;
		case mt_text:
			if (!option->text.text)
			{	//blinking cursor image hack (FIXME)
				if ((int)(realtime*4)&1)
					Draw_FunString(xpos+option->common.posx, ypos+option->common.posy, "^Ue00d");
			}
			else if (option->common.width)
				Draw_FunStringWidth(xpos + option->common.posx, ypos+option->common.posy, option->text.text, option->common.width, true, option->text.isred);
			else if (option->text.isred)
				Draw_AltFunString(xpos+option->common.posx, ypos+option->common.posy, option->text.text);
			else
				Draw_FunString(xpos+option->common.posx, ypos+option->common.posy, option->text.text);
			break;
		case mt_button:
			Draw_FunStringWidth(xpos + option->common.posx, ypos+option->common.posy, option->button.text, option->common.width, option->button.rightalign, !menu->cursoritem && menu->selecteditem == option);
			break;
#ifdef HEXEN2
		case mt_hexen2buttonbigfont:
			Draw_Hexen2BigFontString(xpos+option->common.posx, ypos+option->common.posy, option->button.text);
			break;
#endif
		case mt_qbuttonbigfont:
			Draw_BigFontString(xpos+option->common.posx, ypos+option->common.posy, option->button.text);
			break;
		case mt_menudot:
			i = (int)(realtime * 10)%maxdots;
			p = R2D_SafeCachePic(va(menudotstyle, i+mindot ));
			R2D_ScalePic(xpos+option->common.posx, ypos+option->common.posy+dotofs, option->common.width, option->common.height, p);
			break;
		case mt_picturesel:
			p = NULL;
			if (menu->selecteditem && menu->selecteditem->common.posx == option->common.posx && menu->selecteditem->common.posy == option->common.posy)
			{
				char selname[MAX_QPATH];
				Q_strncpyz(selname, option->picture.picturename, sizeof(selname));
				COM_StripExtension(selname, selname, sizeof(selname));
				Q_strncatz(selname, "_sel", sizeof(selname));
				p = R2D_SafeCachePic(selname);
			}
			if (!R_GetShaderSizes(p, &pw, &ph, false))
				p = R2D_SafeCachePic(option->picture.picturename);

			if (R_GetShaderSizes(p, &pw, &ph, false)>0)
				R2D_ScalePic(xpos+option->common.posx, ypos+option->common.posy, option->common.width?option->common.width:pw, option->common.height?option->common.height:ph, p);
			break;
		case mt_picture:
			p = R2D_SafeCachePic(option->picture.picturename);
			if (R_GetShaderSizes(p, NULL, NULL, false)>0) R2D_ScalePic(xpos+option->common.posx, ypos+option->common.posy, option->common.width, option->common.height, p);
			break;
		case mt_childwindow:
			MenuDrawItems(xpos+option->common.posx, ypos+option->common.posy, ((menu_t *)option->custom.dptr)->options, (menu_t *)option->custom.dptr);
			break;
		case mt_box:
			Draw_TextBox(xpos+option->common.posx, ypos+option->common.posy, option->box.width, option->box.height);
			break;
		case mt_slider:
			if (option->slider.var)
			{
#define SLIDER_RANGE 10
				float range;
				int	i;
				int x = xpos+option->common.posx;
				int y = ypos+option->common.posy;
				int s;

				range = (option->slider.current - option->slider.min)/(option->slider.max-option->slider.min);

				if (option->slider.text)
				{
					Draw_FunStringWidth(x, y, option->slider.text, option->slider.textwidth, true, !menu->cursoritem && menu->selecteditem == option);
					x += option->slider.textwidth + 3*8;
				}

				if (range < 0)
					range = 0;
				if (range > 1)
					range = 1;
				option->slider.vx = x;
				x -= 8;
				Font_BeginString(font_default, x, y, &x, &y);
				x = Font_DrawChar(x, y, CON_WHITEMASK, 0xe080);
				s = x;
				for (i=0 ; i<SLIDER_RANGE ; i++)
					x = Font_DrawChar(x, y, CON_WHITEMASK, 0xe081);
				Font_DrawChar(x, y, CON_WHITEMASK, 0xe082);
				Font_DrawChar(s + (x-s) * range - Font_CharWidth(CON_WHITEMASK, 0xe083)/2, y, CON_WHITEMASK, 0xe083);
				Font_EndString(font_default);
			}
			break;
		case mt_checkbox:
			{
				int x = xpos+option->common.posx;
				int y = ypos+option->common.posy;
				qboolean on;
				if (option->check.func)
					on = option->check.func(&option->check, menu, CHK_CHECKED);
				else if (!option->check.var)
						on = option->check.value;
				else if (option->check.bits)	//bits is a bitmask for use with cvars (users can be clumsy, so bittage of 0 uses non-zero as true, but sets only bit 1)
				{
					if (option->check.var->latched_string)
						on = atoi(option->check.var->latched_string)&option->check.bits;
					else
						on = (int)(option->check.var->value)&option->check.bits;
				}
				else
				{
					if (option->check.var->latched_string)
						on = !!atof(option->check.var->latched_string);
					else
						on = !!option->check.var->value;
				}

				if (option->check.text)
				{
					Draw_FunStringWidth(x, y, option->check.text, option->check.textwidth, true, !menu->cursoritem && menu->selecteditem == option);
					x += option->check.textwidth + 3*8;
				}
#if 0
				if (on)
					Draw_Character (x, y, 131);
				else
					Draw_Character (x, y, 129);
#endif
				if (!menu->cursoritem && menu->selecteditem == option)
					Draw_AltFunString (x, y, on ? "on" : "off");
				else
					Draw_FunString (x, y, on ? "on" : "off");
			}
			break;
		case mt_edit:
			{
				int x = xpos+option->common.posx;
				int y = ypos+option->common.posy;

				Draw_FunStringWidth(x, y, option->edit.caption, option->edit.captionwidth, true, !menu->cursoritem && menu->selecteditem == option);
				x += option->edit.captionwidth + 3*8;
				if (option->edit.slim)
					x += 8; // more space for cursor
				else
					Draw_TextBox(x-8, y-8, 16, 1);
				Draw_FunString(x, y, option->edit.text);

				if (menu->selecteditem == option && (int)(realtime*4) & 1)
				{
					x += strlen(option->edit.text)*8;
					Draw_FunString(x, y, "^Ue00b");
				}
			}
			break;
		case mt_bind:
			{
				int x = xpos+option->common.posx;
				int y = ypos+option->common.posy;
				int		keys[8], keymods[countof(keys)];
				int keycount;
				char *keyname;
				int j;

				Draw_FunStringWidth(x, y, option->bind.caption, option->bind.captionwidth, true, !menu->cursoritem && menu->selecteditem == option);
				x += option->bind.captionwidth + 3*8;

				keycount = M_FindKeysForCommand (0, cl_forceseat.ival, option->bind.command, keys, keymods, countof(keys));

				if (bindingactive && menu->selecteditem == option)
					Draw_FunString (x, y, "Press key");
				else if (!keycount)
					Draw_FunString (x, y, "???");
				else
				{
					for (j = 0; j < keycount; j++)
					{	/*these offsets are wrong*/
						if (j)
						{
							Draw_FunString (x + 8, y, "or");
							x += 32;
						}
						keyname = Key_KeynumToString (keys[j], keymods[j]);
						Draw_FunString (x, y, keyname);
						x += strlen(keyname) * 8;
					}
				}
			}
			break;

		case mt_combo:
			{
				int x = xpos+option->common.posx;
				int y = ypos+option->common.posy;

				Draw_FunStringWidth(x, y, option->combo.caption, option->combo.captionwidth, true, !menu->cursoritem && menu->selecteditem == option);
				x += option->combo.captionwidth + 3*8;

				if (option->combo.numoptions)
				{
					if (!menu->cursoritem && menu->selecteditem == option)
						Draw_AltFunString(x, y, option->combo.options[option->combo.selectedoption]);
					else
						Draw_FunString(x, y, option->combo.options[option->combo.selectedoption]);
				}
			}
			break;
		case mt_custom:
			option->custom.draw(xpos+option->common.posx, ypos+option->common.posy, &option->custom, menu);
			break;
		default:
			Sys_Error("Bad item type\n");
			break;
		}
		option = option->common.next;
	}
}

static void MenuDraw(menu_t *menu)
{
	if (!menu->dontexpand)
		menu->xpos = ((vid.width - 320)>>1);
	if (menu->predraw)
		menu->predraw(menu);
	MenuDrawItems(menu->xpos, menu->ypos, menu->options, menu);
	// draw tooltip
	if (menu->selecteditem && menu->tooltip && realtime > menu->tooltiptime)
	{
		menuoption_t *option = menu->selecteditem;
		if (omousex > menu->xpos+option->common.posx && omousex < menu->xpos+option->common.posx+option->common.width)
			if (omousey > menu->ypos+option->common.posy && omousey < menu->ypos+option->common.posy+option->common.height)
			{
				int x = omousex+8;
				int y = omousey;
				int w;
				int h;
				int l, lines;
				conchar_t *line_start[16];
				conchar_t *line_end[countof(line_start)];

				//figure out the line breaks
				Font_BeginString(font_default, 0, 0, &l, &l);
				lines = Font_LineBreaks(menu->tooltip->text, menu->tooltip->end, min(vid.pixelwidth/2, 30*8*vid.pixelwidth/vid.width), countof(line_start), line_start, line_end);
				Font_EndString(font_default);

				//figure out how wide that makes the tip
				w = 16;
				h = (lines+2)*8;
				for (l = 0; l < lines; l++)
				{
					int lw = 16+Font_LineWidth(line_start[l], line_end[l])*vid.width/vid.pixelwidth;
					if (w < lw)
						w = lw;
				}

				// keep the tooltip within view
				if (x + w >= vid.width)
					x = vid.width - w - 1;
				if (y + h >= vid.height)
					y -= h;

				// draw the background
				Draw_TextBox(x, y, (w-16)/8, lines);
				x += 8;
				y += 8;

				//draw the text
				Font_BeginString(font_default, x, y, &x, &y);
				for (l = 0; l < lines; l++)
				{
					Font_LineDraw(x, y, line_start[l], line_end[l]);
					y += Font_CharHeight();
				}
				Font_EndString(font_default);
			}
	}

	if (menu->postdraw)
		menu->postdraw(menu);
}


menutext_t *MC_AddWhiteText(menu_t *menu, int lhs, int rhs, int y, const char *text, qboolean rightalign)
{
	menutext_t *n = Z_Malloc(sizeof(menutext_t) + (text?strlen(text):0)+1);
	n->common.type = mt_text;
	n->common.iszone = true;
	n->common.posx = lhs;
	n->common.posy = y;
	n->common.width = (rhs && rightalign)?rhs-lhs:0;
	if (text)
	{
		n->text = (char*)(n+1);
		strcpy((char*)(n+1), (text));
	}

	n->common.next = menu->options;
	menu->options = (menuoption_t *)n;
	return n;
}

menutext_t *MC_AddBufferedText(menu_t *menu, int lhs, int rhs, int y, const char *text, qboolean rightalign, qboolean red)
{
	menutext_t *n = Z_Malloc(sizeof(menutext_t) + strlen(text)+1);
	n->common.type = mt_text;
	n->common.iszone = true;
	n->common.posx = lhs;
	n->common.posy = y;
	n->common.width = rhs?rhs-lhs:0;
	n->text = (char *)(n+1);
	strcpy((char *)(n+1), text);
	n->isred = red;

	if (rightalign && text)
		n->common.posx -= strlen(text)*8;

	n->common.next = menu->options;
	menu->options = (menuoption_t *)n;
	return n;
}

menutext_t *MC_AddRedText(menu_t *menu, int lhs, int rhs, int y, const char *text, qboolean rightalign)
{
	menutext_t *n;
	n = MC_AddWhiteText(menu, lhs, rhs, y, text, rightalign);
	n->isred = true;
	return n;
}

menubind_t *MC_AddBind(menu_t *menu, int cx, int bx, int y, const char *caption, char *command, char *tooltip)
{
	menubind_t *n = Z_Malloc(sizeof(*n) + strlen(caption)+1 + strlen(command)+1 + (tooltip?strlen(tooltip)+1:0));
	n->common.type = mt_bind;
	n->common.iszone = true;
	n->common.posx = cx;
	n->common.posy = y;
	n->captionwidth = bx-cx;
	n->caption = (char *)(n+1);
	strcpy(n->caption, caption);
	n->command = n->caption+strlen(n->caption)+1;
	strcpy(n->command, command);
	if (tooltip)
	{
		char *tip = n->command+strlen(n->command)+1;
		n->common.tooltip = tip;
		strcpy(tip, tooltip);
	}
	n->common.width = n->captionwidth + 64;
	n->common.height = 8;

	n->common.next = menu->options;
	menu->options = (menuoption_t *)n;
	return n;
}

menupicture_t *MC_AddSelectablePicture(menu_t *menu, int x, int y, char *picname)
{
	char selname[MAX_QPATH];
	menupicture_t *n;

	if (qrenderer == QR_NONE)
		return NULL;

	Q_strncpyz(selname, picname, sizeof(selname));
	COM_StripExtension(selname, selname, sizeof(selname));
	Q_strncatz(selname, "_sel", sizeof(selname));

	R2D_SafeCachePic(picname);
	R2D_SafeCachePic(selname);

	n = Z_Malloc(sizeof(menupicture_t) + strlen(picname)+1);
	n->common.type = mt_picturesel;
	n->common.iszone = true;
	n->common.posx = x;
	n->common.posy = y;
	n->picturename = (char *)(n+1);
	strcpy(n->picturename, picname);

	n->common.next = menu->options;
	menu->options = (menuoption_t *)n;
	return n;
}

menupicture_t *MC_AddPicture(menu_t *menu, int x, int y, int width, int height, char *picname)
{
	menupicture_t *n;
	if (qrenderer == QR_NONE)
		return NULL;

	R2D_SafeCachePic(picname);

	n = Z_Malloc(sizeof(menupicture_t) + strlen(picname)+1);
	n->common.type = mt_picture;
	n->common.iszone = true;
	n->common.posx = x;
	n->common.posy = y;
	n->common.width = width;
	n->common.height = height;
	n->picturename = (char *)(n+1);
	strcpy(n->picturename, picname);

	n->common.next = menu->options;
	menu->options = (menuoption_t *)n;
	return n;
}

menupicture_t *MC_AddCenterPicture(menu_t *menu, int y, int height, char *picname)
{
	int x;
	int width;
	mpic_t *p;

	if (qrenderer == QR_NONE)
		return NULL;
	p = R2D_SafeCachePic(picname);
	if (!p)
	{
		x = 320/2;
		width = 64;
	}
	else
	{
		int pwidth, pheight;
		if (R_GetShaderSizes(p, &pwidth, &pheight, true))
			width = (pwidth * (float)height) / pheight;
		else
			width = 64;
		x = (320-(int)width)/2;
	}

	return MC_AddPicture(menu, x, y, width, height, picname);
}

menuoption_t *MC_AddCursorSmall(menu_t *menu, menuresel_t *reselection, int x, int y)
{
	menuoption_t *n = Z_Malloc(sizeof(menucommon_t));
	if (reselection)
		menu->reselection = reselection;
	n->common.type = mt_menucursor;
	n->common.iszone = true;
	n->common.posx = x;
	n->common.posy = y;

	n->common.next = menu->options;
	menu->options = (menuoption_t *)n;


	if (menu->reselection)
	{
		menuoption_t *sel, *firstsel = M_NextSelectableItem(menu, NULL);
		for (sel = firstsel; sel; )
		{
			if (sel->common.posx == menu->reselection->x && sel->common.posy == menu->reselection->y)
			{
				menu->selecteditem = sel;
				n->common.posy = sel->common.posy;
				break;
			}
			sel = M_NextSelectableItem(menu, sel);
			if (sel == firstsel)
				break;
		}
	}
	return n;
}

menupicture_t *MC_AddCursor(menu_t *menu, menuresel_t *reselection, int x, int y)
{
	int i;
	menupicture_t *n = Z_Malloc(sizeof(menupicture_t));
	if (reselection)
		menu->reselection = reselection;
	n->common.type = mt_menudot;
	n->common.iszone = true;
	n->common.posx = x;
	n->common.posy = y;
	n->common.width = 20;
	n->common.height = 20;

	n->common.next = menu->options;
	menu->options = (menuoption_t *)n;

	switch(M_GameType())
	{
#ifdef Q2CLIENT
	case MGT_QUAKE2:
		//AND QUAKE 2 WINS!!!
		menudotstyle = "pics/m_cursor%i.pcx";
		mindot = 0;
		maxdots = 15;
		dotofs=0;

		//this is *obviously* the correct size... not.
		n->common.width = 22;
		n->common.height = 29;
		break;
#endif
#ifdef HEXEN2
	case MGT_HEXEN2:
		//AND THE WINNER IS HEXEN 2!!!
		menudotstyle = "gfx/menu/menudot%i.lmp";
		mindot = 1;
		maxdots = 8;
		dotofs=-2;
		break;
#endif
	default:
		//QUAKE 1 WINS BY DEFAULT!
		menudotstyle = "gfx/menudot%i.lmp";
		mindot = 1;
		maxdots = 6;
		dotofs=0;
		break;
	}

	//cache them all. this avoids weird flickering as things get dynamically loaded
	for (i = 0; i < maxdots; i++)
		R2D_SafeCachePic(va(menudotstyle, i+mindot));

	if (menu->reselection)
	{
		menuoption_t *sel, *firstsel = M_NextSelectableItem(menu, NULL);
		for (sel = firstsel; sel; )
		{
			if (sel->common.posx == menu->reselection->x && sel->common.posy == menu->reselection->y)
			{
				menu->selecteditem = sel;
				n->common.posy = sel->common.posy;
				break;
			}
			sel = M_NextSelectableItem(menu, sel);
			if (sel == firstsel)
				break;
		}
	}
	return n;
}

menuedit_t *MC_AddEdit(menu_t *menu, int cx, int ex, int y, char *text, char *def)
{
	menuedit_t *n = Z_Malloc(sizeof(menuedit_t)+strlen(text)+1);
	n->slim = false;
	n->common.type = mt_edit;
	n->common.iszone = true;
	n->common.posx = cx;
	n->common.posy = y;
	n->common.width = ex-cx+(17)*8;
	n->common.height = n->slim?8:16;
	n->modified = true;
	n->captionwidth = ex-cx;
	n->caption = (char *)(n+1);
	strcpy((char *)(n+1), text);
	Q_strncpyz(n->text, def, sizeof(n->text));

	n->common.next = menu->options;
	menu->options = (menuoption_t *)n;
	return n;
}

menuedit_t *MC_AddEditCvar(menu_t *menu, int cx, int ex, int y, char *text, char *name, qboolean isslim)
{
	menuedit_t *n = Z_Malloc(sizeof(menuedit_t)+strlen(text)+1);
	cvar_t *cvar;
	cvar = Cvar_Get(name, "", CVAR_USERCREATED|CVAR_ARCHIVE, NULL);	//well, this is a menu/
	n->slim = isslim;
	n->common.type = mt_edit;
	n->common.iszone = true;
	n->common.posx = cx;
	n->common.posy = y;
	n->common.width = ex-cx+(17)*8;
	n->common.height = n->slim?8:16;
	n->common.tooltip = cvar->description;
	n->modified = true;
	n->captionwidth = ex-cx;
	n->caption = (char *)(n+1);
	strcpy((char *)(n+1), text);
	n->cvar = cvar;
#ifdef _DEBUG
	if (!(cvar->flags & CVAR_ARCHIVE))
		Con_Printf("Warning: %s is not set for archiving\n", cvar->name);
#endif
	Q_strncpyz(n->text, cvar->string, sizeof(n->text));

	n->common.next = menu->options;
	menu->options = (menuoption_t *)n;
	return n;
}

menubox_t *MC_AddBox(menu_t *menu, int x, int y, int width, int height)
{
	menubox_t *n = Z_Malloc(sizeof(menubox_t));
	n->common.type = mt_box;
	n->common.iszone = true;
	n->common.posx = x;
	n->common.posy = y;
	n->width = width;
	n->height = height;

	n->common.next = menu->options;
	menu->options = (menuoption_t *)n;
	return n;
}

menucustom_t *MC_AddCustom(menu_t *menu, int x, int y, void *dptr, int dint)
{
	menucustom_t *n = Z_Malloc(sizeof(menucustom_t));
	n->common.type = mt_custom;
	n->common.iszone = true;
	n->common.posx = x;
	n->common.posy = y;
	n->dptr = dptr;
	n->dint = dint;

	n->common.next = menu->options;
	menu->options = (menuoption_t *)n;
	return n;
}

menucheck_t *MC_AddCheckBox(menu_t *menu, int tx, int cx, int y, const char *text, cvar_t *var, int bits)
{
	menucheck_t *n = Z_Malloc(sizeof(menucheck_t)+strlen(text)+1);
	n->common.type = mt_checkbox;
	n->common.iszone = true;
	n->common.posx = tx;
	n->common.posy = y;
	n->common.height = 8;
	n->textwidth = cx - tx;
	n->common.width = cx-tx + 7*8;
	n->common.tooltip = var?var->description:NULL;
	n->text = (char *)(n+1);
	strcpy((char *)(n+1), text);
	n->var = var;
	n->bits = bits;

#ifdef _DEBUG
	if (var)
		if (!(var->flags & CVAR_ARCHIVE))
			Con_Printf("Warning: %s is not set for archiving\n", var->name);
		else if (var->flags & CVAR_RENDERERLATCH)
			Con_Printf("Warning: %s requires a vid_restart\n", var->name);
#endif

	n->common.next = menu->options;
	menu->options = (menuoption_t *)n;
	return n;
}
menucheck_t *MC_AddCheckBoxFunc(menu_t *menu, int tx, int cx, int y, const char *text, qboolean (*func) (menucheck_t *option, menu_t *menu, chk_set_t set), int bits)
{
	menucheck_t *n = Z_Malloc(sizeof(menucheck_t)+strlen(text)+1);
	n->common.type = mt_checkbox;
	n->common.iszone = true;
	n->common.posx = tx;
	n->common.posy = y;
	n->common.height = 8;
	n->textwidth = cx - tx;
	n->common.width = cx-tx + 7*8;
	n->text = (char *)(n+1);
	strcpy((char *)(n+1), text);
	n->func = func;
	n->bits = bits;

	n->common.next = menu->options;
	menu->options = (menuoption_t *)n;
	return n;
}

//delta may be 0
menuslider_t *MC_AddSlider(menu_t *menu, int tx, int sx, int y, const char *text, cvar_t *var, float min, float max, float delta)
{
	menuslider_t *n = Z_Malloc(sizeof(menuslider_t)+strlen(text)+1);
	n->common.type = mt_slider;
	n->common.iszone = true;
	n->common.posx = tx;
	n->common.posy = y;
	n->common.height = 8;
	n->common.width = sx-tx + (SLIDER_RANGE+5)*8;
	n->common.tooltip = var->description;
	n->var = var;
	n->textwidth = sx-tx;
	n->text = (char *)(n+1);
	strcpy((char *)(n+1), text);

	if (var)
	{
		n->current = var->value;

#ifdef _DEBUG
		if (!(var->flags & CVAR_ARCHIVE))
			Con_Printf("Warning: %s is not set for archiving\n", var->name);
#endif
	}

	n->min = min;
	n->max = max;
	n->smallchange = delta;
	n->largechange = delta*5;

	n->common.next = menu->options;
	menu->options = (menuoption_t *)n;

	return n;
}

menucombo_t *MC_AddCombo(menu_t *menu, int tx, int cx, int y, const char *caption, const char **ops, int initialvalue)
{
	int numopts;
	int optlen;
	int maxoptlen;
	int optbufsize;
	menucombo_t *n;
	char **newops;
	char *optbuf;
	int i;

	maxoptlen = 0;
	optbufsize = sizeof(char*);
	numopts = 0;
	optlen = 0;
	while(ops[numopts])
	{
		optlen = strlen(ops[numopts]);
		if (maxoptlen < optlen)
			maxoptlen = optlen;
		optbufsize += optlen+1+sizeof(char*);
		numopts++;
	}


	n = Z_Malloc(sizeof(*n) + optbufsize);
	newops = (char **)(n+1);
	optbuf = (char*)(newops + numopts+1);
	n->common.type = mt_combo;
	n->common.iszone = true;
	n->common.posx = tx;
	n->common.posy = y;
	n->common.height = 8;
	n->common.width = cx-tx + maxoptlen*8;
	n->captionwidth = cx-tx;
	n->caption = caption;
	n->options = (const char **)newops;

	n->common.next = menu->options;
	menu->options = (menuoption_t *)n;

	n->numoptions = numopts;
	for (i = 0; i < numopts; i++)
	{
		strcpy(optbuf, ops[i]);
		newops[i] = optbuf;
		optbuf += strlen(optbuf)+1;
	}
	newops[i] = NULL;

	if (initialvalue >= n->numoptions)
	{
		Con_Printf("WARNING: Fixed initialvalue for %s\n", caption);
		initialvalue = n->numoptions-1;
	}
	n->selectedoption = initialvalue;

	return n;
}
menucombo_t *MC_AddCvarCombo(menu_t *menu, int tx, int cx, int y, const char *caption, cvar_t *cvar, const char **ops, const char **values)
{
	int numopts;
	int optlen;
	int maxoptlen;
	int optbufsize;
	menucombo_t *n;
	char **newops;
	char **newvalues;
	char *optbuf;
	int i;

	maxoptlen = 0;
	optbufsize = sizeof(char*)*2 + strlen(caption)+1;
	numopts = 0;
	optlen = 0;
	while(ops[numopts])
	{
		optlen = strlen(ops[numopts]);
		if (maxoptlen < optlen)
			maxoptlen = optlen;
		optbufsize += optlen+1+sizeof(char*);
		optbufsize += strlen(values[numopts])+1+sizeof(char*);
		numopts++;
	}



	n = Z_Malloc(sizeof(*n) + optbufsize);
	newops = (char **)(n+1);
	newvalues = (char**)(newops + numopts+1);
	optbuf = (char*)(newvalues + numopts+1);
	n->common.type = mt_combo;
	n->common.iszone = true;
	n->common.posx = tx;
	n->common.posy = y;
	n->common.height = 8;
	n->common.width = cx-tx + maxoptlen*8;
	n->common.tooltip = cvar->description;
	n->captionwidth = cx-tx;

	strcpy(optbuf, caption);
	n->caption = optbuf;
	optbuf += strlen(optbuf)+1;

	n->options = (const char **)newops;
	n->values = (const char **)newvalues;
	n->cvar = cvar;

//	if (!(cvar->flags & CVAR_ARCHIVE))
//		Con_Printf("Warning: %s is not set for archiving\n", cvar->name);

	n->selectedoption = 0;

	n->common.next = menu->options;
	menu->options = (menuoption_t *)n;

	n->numoptions = numopts;
	for (i = 0; i < numopts; i++)
	{
		if (!strcmp(values[i], cvar->string))
			n->selectedoption = i;

		strcpy(optbuf, ops[i]);
		newops[i] = optbuf;
		optbuf += strlen(optbuf)+1;

		strcpy(optbuf, values[i]);
		newvalues[i] = optbuf;
		optbuf += strlen(optbuf)+1;
	}
	newops[i] = NULL;
	newvalues[i] = NULL;

	return n;
}

menubutton_t *MC_AddConsoleCommand(menu_t *menu, int lhs, int rhs, int y, const char *text, const char *command)
{
	menubutton_t *n = Z_Malloc(sizeof(menubutton_t)+strlen(text)+1+strlen(command)+1);
	n->common.type = mt_button;
	n->common.iszone = true;
	n->common.posx = lhs;
	n->common.posy = y;
	n->common.height = 8;
	n->common.width = rhs?rhs - lhs:strlen(text)*8;
	n->rightalign = true;
	n->text = (char *)(n+1);
	strcpy((char *)(n+1), text);
	n->command = n->text + strlen(n->text)+1;
	strcpy((char *)n->command, command);

	n->common.next = menu->options;
	menu->options = (menuoption_t *)n;
	return n;
}

menubutton_t *MC_AddConsoleCommandQBigFont(menu_t *menu, int x, int y, const char *text, const char *command)
{
	menubutton_t *n = Z_Malloc(sizeof(menubutton_t)+strlen(text)+1+strlen(command)+1);
	n->common.type = mt_qbuttonbigfont;
	n->common.iszone = true;
	n->common.posx = x;
	n->common.posy = y;
	n->common.height = 20;
	n->common.width = strlen(text)*20;
	n->text = (char *)(n+1);
	strcpy((char *)(n+1), text);
	n->command = n->text + strlen(n->text)+1;
	strcpy((char *)n->command, command);

	n->common.next = menu->options;
	menu->options = (menuoption_t *)n;
	return n;
}
#ifdef HEXEN2
menubutton_t *MC_AddConsoleCommandHexen2BigFont(menu_t *menu, int x, int y, const char *text, const char *command)
{
	menubutton_t *n = Z_Malloc(sizeof(menubutton_t)+strlen(text)+1+strlen(command)+1);
	n->common.type = mt_hexen2buttonbigfont;
	n->common.iszone = true;
	n->common.posx = x;
	n->common.posy = y;
	n->common.height = 20;
	n->common.width = strlen(text)*20;
	n->text = (char *)(n+1);
	strcpy((char *)(n+1), text);
	n->command = n->text + strlen(n->text)+1;
	strcpy((char *)n->command, command);

	n->common.next = menu->options;
	menu->options = (menuoption_t *)n;
	return n;
}
#endif
menubutton_t *MC_AddCommand(menu_t *menu, int lhs, int rhs, int y, char *text, qboolean (*command) (union menuoption_s *,struct menu_s *,int))
{
	menubutton_t *n = Z_Malloc(sizeof(menubutton_t));
	n->common.type = mt_button;
	n->common.iszone = true;
	n->common.posx = lhs;
	n->common.posy = y;
	n->rightalign = true;
	n->text = text;
	n->command = NULL;
	n->key = command;
	n->common.height = 8;
	n->common.width = rhs?rhs-lhs:strlen(text)*8;

	n->common.next = menu->options;
	menu->options = (menuoption_t *)n;
	return n;
}

menubutton_t *VARGS MC_AddConsoleCommandf(menu_t *menu, int lhs, int rhs, int y, qboolean rightalign, const char *text, char *command, ...)
{
	va_list		argptr;
	static char		string[1024];
	menubutton_t *n;

	va_start (argptr, command);
	vsnprintf (string,sizeof(string)-1, command,argptr);
	va_end (argptr);

	n = Z_Malloc(sizeof(menubutton_t) + strlen(string)+1);
	n->common.type = mt_button;
	n->common.iszone = true;
	n->common.posx = lhs;
	n->common.posy = y;
	n->common.width = rhs-lhs;
	n->rightalign = rightalign;
	n->text = text;
	n->command = (char *)(n+1);
	strcpy((char *)(n+1), string);

	n->common.next = menu->options;
	menu->options = (menuoption_t *)n;
	return n;
}

void MC_Slider_Key(menuslider_t *option, int key)
{
	float range = option->current;
	float delta;

	float ix = option->vx;
	float ex = ix + 10*8;

	if (option->smallchange)
		delta = option->smallchange;
	else
		delta = 0.1;

	if (key == K_LEFTARROW || key == K_MWHEELDOWN)
	{
		range -= delta;
		if (option->min > option->max)
			range = bound(option->max, range, option->min);
		else
			range = bound(option->min, range, option->max);
		option->current = range;
	}
	else if (key == K_RIGHTARROW || key == K_MWHEELUP)
	{
		range += delta;
		if (option->min > option->max)
			range = bound(option->max, range, option->min);
		else
			range = bound(option->min, range, option->max);
		option->current = range;
	}
	else if (key == K_MOUSE1 && mousecursor_x >= ix-8 && mousecursor_x < ex+8)
	{
		range = (mousecursor_x - ix) / (ex - ix);
		range = option->min + range*(option->max-option->min);
		if (option->min > option->max)
			range = bound(option->max, range, option->min);
		else
			range = bound(option->min, range, option->max);
		option->current = range;
	}
	else if (key == K_ENTER || key == K_KP_ENTER || key == K_MOUSE1)
	{
		if (range == option->max)
			range = option->min;
		else
		{
			range += delta;
			if (option->min > option->max)
			{
				if (range < option->max-delta/2)
					range = option->max;
			}
			else
				if (range > option->max-delta/2)
					range = option->max;
		}
		option->current = range;
	}
	else
		return;

	S_LocalSound ("misc/menu2.wav");
	if (option->var)
		Cvar_SetValue(option->var, option->current);
}

void MC_CheckBox_Key(menucheck_t *option, menu_t *menu, int key)
{
	if (key != K_ENTER && key != K_KP_ENTER && key != K_LEFTARROW && key != K_RIGHTARROW && key != K_MOUSE1)
		return;
	if (option->func)
		option->func(option, menu, CHK_TOGGLE);
	else if (!option->var)
		option->value = !option->value;
	else
	{
		if (option->bits)
		{
			int old;
			if (option->var->latched_string)
				old = atoi(option->var->latched_string);
			else
				old = option->var->value;

			if (old & option->bits)
				Cvar_SetValue(option->var, old&~option->bits);
			else
				Cvar_SetValue(option->var, old|option->bits);
		}
		else
		{
			if (option->var->latched_string)
				Cvar_SetValue(option->var, !atof(option->var->latched_string));
			else
				Cvar_SetValue(option->var, !option->var->value);
		}
		S_LocalSound ("misc/menu2.wav");
	}
}

void MC_EditBox_Key(menuedit_t *edit, int key, unsigned int unicode)
{
	int len = strlen(edit->text);
	if (key == K_DEL || key == K_BACKSPACE)
	{
		if (!len)
			return;
		edit->text[len-1] = '\0';
	}
	else if (!unicode)
		return;
	else
	{
		if (unicode < 128)
		{
			if (len < sizeof(edit->text))
			{
				edit->text[len] = unicode;
				edit->text[len+1] = '\0';
			}
		}
	}

	edit->modified = true;

	if (edit->cvar)
	{
		Cvar_Set(edit->cvar, edit->text);
		S_LocalSound ("misc/menu2.wav");
	}
}

void MC_Combo_Key(menucombo_t *combo, int key)
{
	if (key == K_ENTER || key == K_KP_ENTER || key == K_RIGHTARROW || key == K_MOUSE1)
	{
		combo->selectedoption++;
		if (combo->selectedoption >= combo->numoptions)
			combo->selectedoption = 0;

changed:
		if (combo->cvar && combo->numoptions)
			Cvar_Set(combo->cvar, (char *)combo->values[combo->selectedoption]);
		S_LocalSound ("misc/menu2.wav");
	}
	else if (key == K_LEFTARROW)
	{
		combo->selectedoption--;
		if (combo->selectedoption < 0)
			combo->selectedoption = combo->numoptions-1;
		goto changed;
	}
}

void M_AddMenuFront (menu_t *menu)
{
	menu_t *pmenu;
	m_state = m_complex;
	if (!firstmenu)
	{
		M_AddMenu(menu);
		return;
	}
	pmenu = firstmenu;
	while(pmenu->parent)
		pmenu = pmenu->parent;
	pmenu->parent = menu;
	menu->child = pmenu;
	menu->parent = NULL;

	menu->exclusive = true;

	menu->xpos = ((vid.width - 320)>>1);

	currentmenu = menu;
}

void M_AddMenu (menu_t *menu)
{
	m_state = m_complex;
	menu->parent = firstmenu;
	if (firstmenu)
		firstmenu->child = menu;
	menu->child = NULL;
	firstmenu = menu;

	menu->exclusive = true;

	currentmenu = menu;
}
menu_t *M_CreateMenu (int extrasize)
{
	menu_t *menu;
	menu = Z_Malloc(sizeof(menu_t)+extrasize);
	menu->iszone=true;
	menu->data = menu+1;

	M_AddMenu(menu);

	return menu;
}
menu_t *M_CreateMenuInfront (int extrasize)
{
	menu_t *menu;
	menu = Z_Malloc(sizeof(menu_t)+extrasize);
	menu->iszone=true;
	menu->data = menu+1;

	M_AddMenuFront(menu);
	menu->exclusive = false;

	return menu;
}
void M_HideMenu (menu_t *menu)
{
	if (menu == firstmenu)
		firstmenu = menu->parent;
	else
	{
		menu_t *prev;
		prev = menu->child;
		if (prev)
			prev->parent = menu->parent;
		if (menu->parent)
			menu->parent->child = menu;
	}
}
void M_RemoveMenu (menu_t *menu)
{
	menuoption_t *op, *oop;
	if (menu->reselection)
	{
		menu->reselection->x = menu->selecteditem->common.posx;
		menu->reselection->y = menu->selecteditem->common.posy;
	}

	if (menu->remove)
		menu->remove(menu);
	if (menu == firstmenu)
		firstmenu = menu->parent;
	else
	{
		menu_t *prev;
		prev = menu->child;
		if (prev)
			prev->parent = menu->parent;
		if (menu->parent)
			menu->parent->child = menu;
	}

	op = menu->options;
	while(op)
	{
		oop = op;
		op = op->common.next;
		if (oop->common.iszone)
			Z_Free(oop);
	}
	menu->options=NULL;

	if (menu->tooltip)
	{
		Z_Free(menu->tooltip);
		menu->tooltip = NULL;
	}

	if (menu->iszone)
	{
		menu->iszone=false;
		Z_Free(menu);
	}

	if (menu == currentmenu)
		currentmenu = firstmenu;
}

void M_ReloadMenus(void)
{
	menu_t *m;

	for (m = firstmenu; m; m = m->parent)
	{
		if (m->reset)
			m->reset(m);
	}
}

void M_RemoveAllMenus (qboolean leaveprompts)
{
	menu_t **link, *m;

	for (link = &firstmenu; *link; )
	{
		m = *link;
		if (!m->exclusive && leaveprompts)
		{
			//this is WEIRD.
			if (m == firstmenu)
				link = &m->parent;
			else
				link = &m->child;
		}
		else
			M_RemoveMenu(m);
	}

}
void M_MenuPop_f (void)
{
	if (!currentmenu)
		return;
	M_RemoveMenu(currentmenu);
}

void M_Complex_Draw(void)
{
	menu_t *menu, *cmenu;
	qboolean foundexclusive = false;

	if (!firstmenu)
	{
		Key_Dest_Remove(kdm_emenu);
		m_state = m_none;
		return;
	}

	M_CheckMouseMove();

	for (menu = firstmenu; menu; )
	{
		cmenu = menu;
		menu = menu->parent;	//this way we can remove the currently drawn menu easily (if needed)

		if (cmenu->exclusive)
		{
			if (foundexclusive)
				continue;
			foundexclusive=true;
		}
		MenuDraw(cmenu);
	}
}

menuoption_t *M_NextItem(menu_t *m, menuoption_t *old)
{
	menuoption_t *op = m->options;
	while(op->common.next)
	{
		if (op->common.next == old)
			return op;

		op = op->common.next;
	}
	return op;
}
menuoption_t *M_NextSelectableItem(menu_t *m, menuoption_t *old)
{
	menuoption_t *op;

	if (!old)
		old = M_NextItem(m, old);

	op = old;

	while (1)
	{
		if (!op)
			op = currentmenu->options;

		op = M_NextItem(m, op);
		if (!op)
			op = currentmenu->options;

		if (op == old)
		{
			if (op->common.type == mt_slider || op->common.type == mt_checkbox || op->common.type == mt_button || op->common.type == mt_hexen2buttonbigfont || op->common.type == mt_qbuttonbigfont || op->common.type == mt_edit || op->common.type == mt_combo || op->common.type == mt_bind || (op->common.type == mt_custom && op->custom.key))
				return op;
			return NULL;	//whoops.
		}

		if (op->common.type == mt_slider || op->common.type == mt_checkbox || op->common.type == mt_button || op->common.type == mt_hexen2buttonbigfont || op->common.type == mt_qbuttonbigfont || op->common.type == mt_edit || op->common.type == mt_combo || op->common.type == mt_bind || (op->common.type == mt_custom && op->custom.key))
			if (!op->common.ishidden)
				return op;
	}
}

menuoption_t *M_PrevSelectableItem(menu_t *m, menuoption_t *old)
{
	menuoption_t *op;

	if (!old)
		old = currentmenu->options;

	op = old;

	while (1)
	{
		if (!op)
			op = currentmenu->options;

		op = op->common.next;
		if (!op)
			op = currentmenu->options;

		if (op == old)
			return old;	//whoops.

		if (op->common.type == mt_slider || op->common.type == mt_checkbox || op->common.type == mt_button || op->common.type == mt_hexen2buttonbigfont || op->common.type == mt_qbuttonbigfont || op->common.type == mt_edit || op->common.type == mt_combo || op->common.type == mt_bind || (op->common.type == mt_custom && op->custom.key))
			if (!op->common.ishidden)
				return op;
	}
}

void M_Complex_Key(int key, int unicode)
{
	if (!currentmenu)
		return;	//erm...

	M_CheckMouseMove();

	if (currentmenu->key)
		if (currentmenu->key(key, currentmenu))
			return;

	if (currentmenu->selecteditem && currentmenu->selecteditem->common.type == mt_custom && (key == K_DOWNARROW || key == K_UPARROW || key == K_TAB))
		if (currentmenu->selecteditem->custom.key)
			if (currentmenu->selecteditem->custom.key(&currentmenu->selecteditem->custom, currentmenu, key, unicode))
				return;

	if (currentmenu->selecteditem && currentmenu->selecteditem->common.type == mt_bind)
	{
		if (bindingactive)
		{
			//don't let key 0 be bound here. unicode-only keys are also not bindable.
			if (key == 0)
				return;

#ifdef HEXEN2
			if (M_GameType() == MGT_HEXEN2)
				S_LocalSound ("raven/menu1.wav");
			else
#endif
				S_LocalSound ("misc/menu1.wav");

			if (key != K_ESCAPE && key != '`')
			{
				int modifiers = 0;
				extern qboolean keydown[];
				if (keydown[K_LSHIFT] && key != K_LSHIFT)
					modifiers |= 1;
				if (keydown[K_RSHIFT] && key != K_RSHIFT)
					modifiers |= 1;
				if (keydown[K_LALT] && key != K_LALT)
					modifiers |= 2;
				if (keydown[K_RALT] && key != K_RALT)
					modifiers |= 2;
				if (keydown[K_LCTRL] && key != K_LCTRL)
					modifiers |= 4;
				if (keydown[K_RCTRL] && key != K_RCTRL)
					modifiers |= 4;

				Cbuf_InsertText (va("bind \"%s\" \"%s\"\n", Key_KeynumToString (key, modifiers), currentmenu->selecteditem->bind.command), RESTRICT_LOCAL, false);
			}
			bindingactive = false;
			return;
		}
	}

	switch(key)
	{
	case K_MOUSE2:
	case K_ESCAPE:
		//remove
		M_RemoveMenu(currentmenu);
#ifdef HEXEN2
		if (M_GameType() == MGT_HEXEN2)
			S_LocalSound ("raven/menu3.wav");
		else
#endif
			S_LocalSound ("misc/menu3.wav");
		break;
	case K_TAB:
	case K_DOWNARROW:
		currentmenu->selecteditem = M_NextSelectableItem(currentmenu, currentmenu->selecteditem);

		if (currentmenu->selecteditem)
		{
#ifdef HEXEN2
			if (M_GameType() == MGT_HEXEN2)
				S_LocalSound ("raven/menu1.wav");
			else
#endif
				S_LocalSound ("misc/menu1.wav");

			if (currentmenu->cursoritem)
				currentmenu->cursoritem->common.posy = currentmenu->selecteditem->common.posy;
		}
		break;
	case K_UPARROW:
		currentmenu->selecteditem = M_PrevSelectableItem(currentmenu, currentmenu->selecteditem);

		if (currentmenu->selecteditem)
		{
#ifdef HEXEN2
			if (M_GameType() == MGT_HEXEN2)
				S_LocalSound ("raven/menu1.wav");
			else
#endif
				S_LocalSound ("misc/menu1.wav");

			if (currentmenu->cursoritem)
				currentmenu->cursoritem->common.posy = currentmenu->selecteditem->common.posy;
		}
		break;
	default:
		if (!currentmenu->selecteditem)
		{
			if (!currentmenu->options)
				return;
			currentmenu->selecteditem = currentmenu->options;
		}
		switch(currentmenu->selecteditem->common.type)
		{
		case mt_slider:
			MC_Slider_Key(&currentmenu->selecteditem->slider, key);
			break;
		case mt_checkbox:
			MC_CheckBox_Key(&currentmenu->selecteditem->check, currentmenu, key);
			break;
		case mt_button:
		case mt_hexen2buttonbigfont:
		case mt_qbuttonbigfont:
			if (!currentmenu->selecteditem->button.command)
				currentmenu->selecteditem->button.key(currentmenu->selecteditem, currentmenu, key);
			else if (key == K_ENTER || key == K_KP_ENTER || key == K_MOUSE1)
			{
				Cbuf_AddText(currentmenu->selecteditem->button.command, RESTRICT_LOCAL);
#ifdef HEXEN2
				if (M_GameType() == MGT_HEXEN2)
					S_LocalSound ("raven/menu2.wav");
				else
#endif
					S_LocalSound ("misc/menu2.wav");
			}
			break;
		case mt_custom:
			if (currentmenu->selecteditem->custom.key)
				currentmenu->selecteditem->custom.key(&currentmenu->selecteditem->custom, currentmenu, key, unicode);
			break;
		case mt_edit:
			MC_EditBox_Key(&currentmenu->selecteditem->edit, key, unicode);
			break;
		case mt_combo:
			MC_Combo_Key(&currentmenu->selecteditem->combo, key);
			break;
		case mt_bind:
			if (key == K_ENTER || key == K_KP_ENTER || key == K_MOUSE1)
				bindingactive = true;
			else if (key == K_BACKSPACE || key == K_DEL)
				M_UnbindCommand (currentmenu->selecteditem->bind.command);
		default:
			break;
		}
		break;
	}
}




extern int m_save_demonum;
qboolean MC_Main_Key (int key, menu_t *menu)	//here purly to restart demos.
{
	if (key == K_ESCAPE || key == K_MOUSE2)
	{
		extern cvar_t con_stayhidden;

		//don't spam menu open+close events if we're not going to be allowing the console to appear
		if (con_stayhidden.ival && cls.state == ca_disconnected)
			if (!CL_TryingToConnect())
				return true;

		Key_Dest_Remove(kdm_emenu);
		m_state = m_none;
		return true;
	}
	return false;
}

void M_Menu_Main_f (void)
{
	extern cvar_t m_helpismedia;
	menubutton_t *b;
	menu_t *mainm;
	mpic_t *p;
	static menuresel_t resel;

#ifdef CSQC_DAT
	if (CSQC_ConsoleCommand(va("%s %s", Cmd_Argv(0), Cmd_Args())))
		return;
#endif

/*	if (cls.demoplayback)
	{
		m_save_demonum = cls.demonum;
		cls.demonum = -1;
	}
*/
	SCR_EndLoadingPlaque();	//just in case...

/*
	if (0)
	{
		int x, i;
		guiinfo_t *gui;
		m_state = m_complex;
		key_dest = key_menu;
		m_entersound = true;

		mainm = M_CreateMenu(sizeof(guiinfo_t));
		mainm->key = MC_GuiKey;
		mainm->xpos=0;
		gui = (guiinfo_t *)mainm->data;
		gui->text[0] = "Single";
		gui->text[1] = "Multiplayer";
		gui->text[2] = "Quit";
		for (x = 0, i = 0; gui->text[i]; i++)
		{
			gui->op[i] = MC_AddRedText(mainm, x, 0, gui->text[i], false);
			x+=(strlen(gui->text[i])+1)*8;
		}
		return;
	}
*/

	S_LocalSound ("misc/menu2.wav");

#ifdef Q2CLIENT
	if (M_GameType() == MGT_QUAKE2)	//quake2 main menu.
	{
		if (R_GetShaderSizes(R2D_SafeCachePic("pics/m_main_quit"), NULL, NULL, true) > 0)
		{
			m_state = m_complex;
			Key_Dest_Add(kdm_emenu);

			mainm = M_CreateMenu(0);
			mainm->key = MC_Main_Key;

			MC_AddPicture(mainm, 0, 4, 38, 166, "pics/m_main_plaque");
			p = R2D_SafeCachePic("pics/m_main_logo");
			if (!p)
				return;
			MC_AddPicture(mainm, 0, 173, 36, 42, "pics/m_main_logo");
#ifndef CLIENTONLY
			MC_AddSelectablePicture(mainm, 68, 13, "pics/m_main_game");
#endif
			MC_AddSelectablePicture(mainm, 68, 53, "pics/m_main_multiplayer");
			MC_AddSelectablePicture(mainm, 68, 93, "pics/m_main_options");
			MC_AddSelectablePicture(mainm, 68, 133, "pics/m_main_video");
			MC_AddSelectablePicture(mainm, 68, 173, "pics/m_main_quit");

#ifndef CLIENTONLY
			b = MC_AddConsoleCommand	(mainm, 68, 320, 13,	"", "menu_single\n");
			b->common.tooltip = "Singleplayer.";
			mainm->selecteditem = (menuoption_t *)b;
			b->common.width = 12*20;
			b->common.height = 32;
#endif
			b = MC_AddConsoleCommand	(mainm, 68, 320, 53,	"", "menu_multi\n");
			b->common.tooltip = "Multiplayer.";
#ifdef CLIENTONLY
			mainm->selecteditem = (menuoption_t *)b;
#endif
			b->common.width = 12*20;
			b->common.height = 32;
			b = MC_AddConsoleCommand	(mainm, 68, 320, 93,	"", "menu_options\n");
			b->common.tooltip = "Options.";
			b->common.width = 12*20;
			b->common.height = 32;
			b = MC_AddConsoleCommand	(mainm, 68, 320, 133,	"", "menu_video\n");
			b->common.tooltip = "Video Options.";
			b->common.width = 12*20;
			b->common.height = 32;
			b = MC_AddConsoleCommand	(mainm, 68, 320, 173,	"", "menu_quit\n");
			b->common.tooltip = "Quit to DOS.";
			b->common.width = 12*20;
			b->common.height = 32;

			mainm->cursoritem = (menuoption_t *)MC_AddCursor(mainm, &resel, 42, mainm->selecteditem->common.posy);
		}
	}
	else
#endif
#ifdef HEXEN2
		if (M_GameType() == MGT_HEXEN2)
	{
		p = R2D_SafeCachePic("gfx/menu/title0.lmp");
		if (R_GetShaderSizes(p, NULL, NULL, true) <= 0)
			return;

		m_state = m_complex;
		Key_Dest_Add(kdm_emenu);
		mainm = M_CreateMenu(0);
		mainm->key = MC_Main_Key;

		MC_AddPicture(mainm, 16, 0, 35, 176, "gfx/menu/hplaque.lmp");
		MC_AddCenterPicture(mainm, 0, 60, "gfx/menu/title0.lmp");

#ifndef CLIENTONLY
		b=MC_AddConsoleCommandHexen2BigFont	(mainm, 80, 64,	"Single Player", "menu_single\n");
		mainm->selecteditem = (menuoption_t *)b;
		b->common.width = 12*20;
		b->common.height = 20;
#endif
		b=MC_AddConsoleCommandHexen2BigFont	(mainm, 80, 64+20,	"MultiPlayer", "menu_multi\n");
#ifdef CLIENTONLY
		mainm->selecteditem = (menuoption_t *)b;
#endif
		b->common.width = 12*20;
		b->common.height = 20;
		b=MC_AddConsoleCommandHexen2BigFont	(mainm, 80, 64+40,	"Options", "menu_options\n");
		b->common.width = 12*20;
		b->common.height = 20;
		if (m_helpismedia.value)
			b=MC_AddConsoleCommandHexen2BigFont	(mainm, 80, 64+60,	"Media", "menu_media\n");
		else
			b=MC_AddConsoleCommandHexen2BigFont	(mainm, 80, 64+60,	"Help", "help\n");
		b->common.width = 12*20;
		b->common.height = 20;
		b=MC_AddConsoleCommandHexen2BigFont	(mainm, 80, 64+80,	"Quit", "menu_quit\n");
		b->common.width = 12*20;
		b->common.height = 20;

		mainm->cursoritem = (menuoption_t *)MC_AddCursor(mainm, &resel, 56, mainm->selecteditem->common.posy);
	}
	else
#endif
		if (QBigFontWorks())
	{
		int y;
		m_state = m_complex;
		Key_Dest_Add(kdm_emenu);
		mainm = M_CreateMenu(0);

		p = R2D_SafeCachePic("gfx/ttl_main.lmp");
		if (R_GetShaderSizes(p, NULL, NULL, true) <= 0)
		{
			MC_AddRedText(mainm, 16, 170, 0,				"MAIN MENU", false);

			mainm->selecteditem = (menuoption_t *)
			MC_AddConsoleCommand	(mainm, 64, 170, 32,	"Join server", "menu_servers\n");
			MC_AddConsoleCommand	(mainm, 64, 170, 40,	"Options", "menu_options\n");
			MC_AddConsoleCommand	(mainm, 64, 170, 48,	"Quit", "menu_quit\n");
			return;
		}
		mainm->key = MC_Main_Key;
		MC_AddPicture(mainm, 16, 4, 32, 144, "gfx/qplaque.lmp");

		MC_AddCenterPicture(mainm, 4, 24, "gfx/ttl_main.lmp");

		y = 32;
		mainm->selecteditem = (menuoption_t *)
#ifndef CLIENTONLY
		MC_AddConsoleCommandQBigFont	(mainm, 72, y,	"Single        ", "menu_single\n");		y += 20;
#endif
		MC_AddConsoleCommandQBigFont	(mainm, 72, y,	"Multiplayer   ", "menu_multi\n");		y += 20;
		MC_AddConsoleCommandQBigFont	(mainm, 72, y,	"Options       ", "menu_options\n");	y += 20;
		if (m_helpismedia.value)
			{MC_AddConsoleCommandQBigFont(mainm, 72, y,	"Media         ", "menu_media\n");		y += 20;}
		else
			{MC_AddConsoleCommandQBigFont(mainm, 72, y,	"Help          ", "help\n");			y += 20;}
		if (Cmd_AliasExist("mod_menu", RESTRICT_LOCAL))
			{MC_AddConsoleCommandQBigFont(mainm, 72, y,	va("%-14s", Cvar_Get("mod_menu", "Mod Menu", 0, NULL)->string), "mod_menu\n");			y += 20;}
		if (Cmd_Exists("xmpp"))
			{MC_AddConsoleCommandQBigFont(mainm, 72, y,	"Social        ", "xmpp\n");			y += 20;}
		if (Cmd_Exists("irc"))
			{MC_AddConsoleCommandQBigFont(mainm, 72, y,	"IRC           ", "irc\n");				y += 20;}
		if (Cmd_Exists("qi"))
			{MC_AddConsoleCommandQBigFont(mainm, 72, y,	"Quake Injector", "qi\n");				y += 20;}
		if (Cmd_Exists("menu_download"))
			{MC_AddConsoleCommandQBigFont(mainm, 72, y,	"Updates       ", "menu_download\n");	y += 20;}
#ifdef FTE_TARGET_WEB
		MC_AddConsoleCommandQBigFont	(mainm, 72, y,	"Save Settings ", "menu_quit\n");		y += 20;
#else
		MC_AddConsoleCommandQBigFont	(mainm, 72, y,	"Quit          ", "menu_quit\n");		y += 20;
#endif

		mainm->cursoritem = (menuoption_t *)MC_AddCursor(mainm, &resel, 54, 32);
	}
	else
	{
		m_state = m_complex;
		Key_Dest_Add(kdm_emenu);
		mainm = M_CreateMenu(0);

		p = R2D_SafeCachePic("gfx/ttl_main.lmp");
		if (R_GetShaderSizes(p, NULL, NULL, true) <= 0)
		{
			MC_AddRedText(mainm, 16, 170, 0,				"MAIN MENU", false);

			mainm->selecteditem = (menuoption_t *)
			MC_AddConsoleCommand	(mainm, 64, 170, 32,	"Join server", "menu_servers\n");
			MC_AddConsoleCommand	(mainm, 64, 170, 40,	"Options", "menu_options\n");
			MC_AddConsoleCommand	(mainm, 64, 170, 48,	"Quit", "menu_quit\n");
			return;
		}
		mainm->key = MC_Main_Key;
		MC_AddPicture(mainm, 16, 4, 32, 144, "gfx/qplaque.lmp");

		MC_AddCenterPicture(mainm, 4, 24, "gfx/ttl_main.lmp");
		MC_AddPicture(mainm, 72, 32, 240, 112, "gfx/mainmenu.lmp");


		p = R2D_SafeCachePic("gfx/mainmenu.lmp");

		b=MC_AddConsoleCommand	(mainm, 72, 312, 32,	"", "menu_single\n");
		b->common.tooltip = "Start singleplayer Quake game.";
		mainm->selecteditem = (menuoption_t *)b;
		b->common.width = p->width;
		b->common.height = 20;
		b=MC_AddConsoleCommand	(mainm, 72, 312, 52,	"", "menu_multi\n");
		b->common.tooltip = "Multiplayer menu.";
		b->common.width = p->width;
		b->common.height = 20;
		b=MC_AddConsoleCommand	(mainm, 72, 312, 72,	"", "menu_options\n");
		b->common.tooltip = "Options menu.";
		b->common.width = p->width;
		b->common.height = 20;
		if (m_helpismedia.value)
		{
			b=MC_AddConsoleCommand(mainm, 72, 312, 92,	"", "menu_media\n");
			b->common.tooltip = "Media menu.";
		}
		else
		{
			b=MC_AddConsoleCommand(mainm, 72, 312, 92,	"", "help\n");
			b->common.tooltip = "Help menu.";
		}
		b->common.width = p->width;
		b->common.height = 20;
		b=MC_AddConsoleCommand	(mainm, 72, 312, 112,	"", "menu_quit\n");
#ifdef FTE_TARGET_WEB
		b->common.tooltip = "Save settings to local storage.";
#else
		b->common.tooltip = "Exit to DOS.";
#endif
		b->common.width = p->width;
		b->common.height = 20;

		mainm->cursoritem = (menuoption_t *)MC_AddCursor(mainm, &resel, 54, 32);
	}

	if (!m_preset_chosen.ival)
		M_Menu_Preset_f();
}

int MC_AddBulk(struct menu_s *menu, menuresel_t *resel, menubulk_t *bulk, int xstart, int xtextend, int y)
{
	int selectedy = y;
	menuoption_t *selected = NULL;

	while (bulk)
	{
		menuoption_t *control;
		int x = xtextend;
		int xleft;
		int spacing = 8;

		if (bulk->text)
		{	//lots of fancy code just to figure out the correct width of the string. yay. :(
			int px, py;
			conchar_t buffer[2048], *end;
			end = COM_ParseFunString(CON_WHITEMASK, bulk->text, buffer, sizeof(buffer), false);
			Font_BeginString(font_default, 0, 0, &px, &py);
			px = Font_LineWidth(buffer, end);
			Font_EndString(NULL);

			x -= ((float)px * vid.width) / vid.rotpixelwidth;
		}
		xleft = x - xstart;

		switch (bulk->type)
		{
		case mt_text:
			switch (bulk->variant)
			{
			case -1: // end of menu
			default:
				bulk = NULL;
				control = NULL;
				continue;
			case 0: // white text
				control = (union menuoption_s *)MC_AddWhiteText(menu, xleft, xtextend, y, bulk->text, bulk->rightalign);
				break;
			case 1: // red text
				control = (union menuoption_s *)MC_AddRedText(menu, xleft, xtextend, y, bulk->text, bulk->rightalign);
				break;
			case 2: // spacing
				spacing = bulk->spacing;
				control = NULL;
				break;
			}
			break;
		case mt_button:
			switch (bulk->variant)
			{
			default:
			case 0: // console command
				control = (union menuoption_s *)MC_AddConsoleCommand(menu, xleft, xtextend, y, bulk->text, bulk->consolecmd);
				break;
			case 1: // function command
				control = (union menuoption_s *)MC_AddCommand(menu, xleft, xtextend, y, bulk->text, bulk->command);
				break;
			}
			break;
		case mt_checkbox:
			control = (union menuoption_s *)MC_AddCheckBox(menu, xleft, xtextend, y, bulk->text, bulk->cvar, bulk->flags);
			control->check.func = bulk->func;
			break;
		case mt_slider:
			control = (union menuoption_s *)MC_AddSlider(menu, xleft, xtextend, y, bulk->text, bulk->cvar, bulk->min, bulk->max, bulk->delta);
			break;
		case mt_combo:
			switch (bulk->variant)
			{
			default:
			case 0: // cvar combo
				control = (union menuoption_s *)MC_AddCvarCombo(menu, xleft, xtextend, y, bulk->text, bulk->cvar, bulk->options, bulk->values);
				break;
			case 1: // combo with return value
				if (bulk->selectedoption < 0)
				{	//invalid...
					control = NULL;
					spacing = 0;
					break;
				}
				control = (union menuoption_s *)MC_AddCombo(menu, xleft, xtextend, y, bulk->text, bulk->options, bulk->selectedoption);
				break;
			}
			break;
		case mt_edit:
			switch (bulk->variant)
			{
			default:
			case 0:
				y += 4;
				control = (union menuoption_s *)MC_AddEditCvar(menu, xleft, xtextend, y, bulk->text, bulk->cvarname, false);
				spacing += 4;
				break;
			case 1:
				control = (union menuoption_s *)MC_AddEditCvar(menu, xleft, xtextend, y, bulk->text, bulk->cvarname, true);
				break;
			}
			break;
		default:
			Con_Printf(CON_ERROR "Invalid type in bulk menu!\n");
			bulk = NULL;
			continue;
		}

		if (bulk->ret)
			*bulk->ret = control;
		if (control && MI_Selectable(control) && !selected)
			selected = control;
		if (control && bulk->tooltip)
			control->common.tooltip = bulk->tooltip;
		if (control && xleft > 0)
			control->common.extracollide = xleft;
		y += spacing;

		bulk++;
	}

	menu->selecteditem = selected;
	if (selected)
		selectedy = selected->common.posy;
	menu->cursoritem = (menuoption_t*)MC_AddCursorSmall(menu, resel, xtextend + 8, selectedy);
	return y;
}
#endif
