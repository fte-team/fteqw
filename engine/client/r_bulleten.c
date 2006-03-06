/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// r_bulleten.c
//
//draws new stuff onto the mip each frame!
//text set each frame

#include "quakedef.h"
#ifdef PEXT_BULLETENS

#ifdef SWQUAKE
#include "r_local.h"
#include "d_local.h"
#endif
#ifdef RGLQUAKE
#include "glquake.h"//hack
#endif

/*
Effects:
0:	standard plain background
1:	normal texture + scrolling text
2:	Sparkling
3:	Simply a scrolling texture
4:	Ripples (for water) - it is treated as a bulleten, because that way, we can easily hook into it. There is never any text though.
5:
6:
7:
8:
9:
*/

cvar_t bul_text1 = SCVAR("bul_text1", "0Cheesy Forethoug\\nht entertainment");
cvar_t bul_text2 = SCVAR("bul_text2", "2");
cvar_t bul_text3 = SCVAR("bul_text3", "0Join Shubs Army\\nFight for Fear");
cvar_t bul_text4 = SCVAR("bul_text4", "0Need a gun?\\nGoto bobs place!");
cvar_t bul_text5 = SCVAR("bul_text5", "0Beware the fans\\nThey can hurt.");
cvar_t bul_text6 = SCVAR("bul_text6", "2Quake B Arena");

cvar_t bul_scrollspeedx = SCVAR("bul_scrollspeedx", "-20");	//pixels per second
cvar_t bul_scrollspeedy = SCVAR("bul_scrollspeedy", "-10");	//pixels per second
cvar_t bul_backcol = SCVAR("bul_backcolour", "1");
cvar_t bul_textpalette = SCVAR("bul_textpalette", "0");
cvar_t bul_norender = SCVAR("bul_norender", "0");
cvar_t bul_sparkle = SCVAR("bul_sparkle", "7");
cvar_t bul_forcemode = SCVAR("bul_forcemode", "-1");
cvar_t bul_ripplespeed = SCVAR("bul_ripplespeed", "32");
cvar_t bul_rippleamount = SCVAR("bul_rippleamount", "2");
cvar_t bul_nowater = SCVAR("bul_nowater", "1");

int bultextpallete = 0;

bulletentexture_t *bulletentexture;

int nlstrlen(char *str, int *lines) //strlen, but for longest line in string
{
	int cl = 0, ol = 0;

	if (*str >= '0' && *str <= '9')	//used to set an effect
		str++;	

	*lines = 1;
	for (;*str;str++,cl++)
	{
		if (*str == '\\')
		{
			str++;
			if (*str == 'n')
			{
				if (ol < cl)
					ol = cl;
				cl = 0;
				*lines += 1;
			}
		}		
	}
	if (cl > ol)
		return cl;
	return ol;
}

void WipeBulletenTextures(void)
{
	bulletentexture_t *a;
//	return;
	for (a = bulletentexture; a; a=a->next)
	{
		a->texture = NULL;		
	}
	bulletentexture = NULL;
}

qboolean R_AddBulleten (texture_t *textur)
{
	bulletentexture_t *a;
	int len;
	int lines;

	int type;
	char *text="";
#ifndef CLIENTONLY
	if (isDedicated)
		return false;
#endif

	if (!Q_strncmp(textur->name,"b_lead",6))
	{
		type = 0; // name winner
		text = bul_text1.string;
	}
	else if (!Q_strncmp(textur->name,"b_loose",7))
	{
		type = 1; // name looser
		text = bul_text2.string;
	}
	else if (!Q_strncmp(textur->name, "b_text\0",7))
	{
		type = 2 + rand() % 6; //random advert (all of these end up the same (first found))
//		;
	}
	else if (!Q_strncmp(textur->name, "b_text_1",8))
	{
		type = 2; // advert 1
		text = bul_text1.string;
	}
	else if (!Q_strncmp(textur->name, "b_text_2", 8))
	{
		type = 3; // advert 2
		text = bul_text2.string;
	}
	else if (!Q_strncmp(textur->name, "b_text_3", 8))
	{
		type = 4; // advert 3
		text = bul_text3.string;
	}
	else if (!Q_strncmp(textur->name, "b_text_4", 8))
	{
		type = 5; // advert 4
		text = bul_text4.string;
	}
	else if (!Q_strncmp(textur->name, "b_text_5", 8))
	{
		type = 6;
		text = bul_text5.string;
	}
	else if (!Q_strncmp(textur->name, "b_text_6", 8))
	{
		type = 7;
		text = bul_text6.string;
	}
	// water ripples
	else if (!Q_strncmp(textur->name,"*", 1) && !bul_nowater.value)
	{
		type = -1;
		text = "";
	}

	else if (!Q_strncmp(textur->name,"bul_", 4))
	{
		type = atoi(textur->name+4);
		text = "";
	}
	else // not a bulleten
		return false;	


	for (a = bulletentexture; a; a=a->next)
	{
		if (a->texture == textur)
			return true; //texture address already used		
	}
	if (a == NULL)
	{	//not found it, create a new texture
		a = Hunk_AllocName(sizeof(struct bulletentexture_s) + ((textur->width) * (textur->height)), "bulleten");
		a->next = bulletentexture;	//add in first
		bulletentexture = a;

		len = nlstrlen(text, &lines);
		a->texture = textur;
		a->bultextleft = (a->texture->width - (len*8)) / 2;
		a->bultexttop = (a->texture->height - (lines*8)) / 2;
		a->type = type;
		a->normaltexture = (qbyte *) a + sizeof(struct bulletentexture_s);
		memcpy(a->normaltexture, (qbyte *) textur + textur->offsets[0],  textur->width * textur->height);
		return true;
	}
	return false;
}

//user wants to force a world texture into a bulleten board.
void R_BulletenForce_f (void)
{
	extern model_t	mod_known[];
	extern int	mod_numknown;

	model_t *mod;
	texture_t *tx;
	char *match = Cmd_Argv(1);

	int i, m, s;


	for (m=0 , mod=mod_known ; m<mod_numknown ; m++, mod++)
	{
		if (mod->type == mod_brush && !mod->needload)
		{
			for (i = 0; i < mod->numtextures; i++)
			{
				tx = mod->textures[i];
				if (!tx)
					continue;	//happens on e1m2

				if (!stricmp(tx->name, match))
				{
					char *text = "";
					int len, lines;
					bulletentexture_t *a;

					for (a = bulletentexture; a; a=a->next)
					{
						if (a->texture == tx)
						{
							a->type = atoi(Cmd_Argv(2));
							break; //texture address already used		
						}
					}
					if (a == NULL)
					{	//not found it, create a new texture
						a = Hunk_AllocName(sizeof(struct bulletentexture_s) + ((tx->width) * (tx->height)), "bulleten");
						a->next = bulletentexture;	//add in first
						bulletentexture = a;

						len = nlstrlen(text, &lines);
						a->texture = tx;
						a->bultextleft = (a->texture->width - (len*8)) / 2;
						a->bultexttop = (a->texture->height - (lines*8)) / 2;
						a->type = atoi(Cmd_Argv(2));
						a->normaltexture = (qbyte *) a + sizeof(struct bulletentexture_s);
						memcpy(a->normaltexture, (qbyte *) tx + tx->offsets[0],  tx->width * tx->height);

						for (s = 0; s < mod->numsurfaces; s++)
						{
							mod->surfaces[s].flags |= SURF_BULLETEN;
						}
					}
				}
			}
		}
	}
}

void R_SetupBulleten (void)
{
	bulletentexture_t *a;
	int len;
	int lines;	
player_info_t	*s;
	char text[256];

	if (bul_norender.value || cl.paused || !bulletentexture) //don't scroll when paused
		return;

	bultextpallete = bul_textpalette.value * 16;

	if (bultextpallete < 0) //not the negatives
		bultextpallete = 0;
	if (bultextpallete > 255 - vid.fullbright) // don't allow shifting into the fullbrights, (compensate for pallete scale
		bultextpallete = 0;

	Sbar_SortFrags (false); //find who's winning and who's loosing

	for (a = bulletentexture; a; a=a->next)
	{
		if (a->texture != NULL)
		{
			switch (a->type)
			{
			case -1:
				sprintf(text, "4");	//negative values have no text
				break;

			case 0: //leader
				s = &cl.players[fragsort[0]];
				if (!s->name[0])
				{
					sprintf(text, "0%s", bul_text1.string);
					break;
				}
				if (s->frags == 1)
					sprintf(text, "0%s is leading\nwith 1 frag!", s->name);
				else
					sprintf(text, "0%s is leading\nwith %i frags!", s->name, s->frags);
				break;

			case 1: //looser			
				s = &cl.players[fragsort[scoreboardlines-1]];
				if (!s->name[0])
				{
					sprintf(text, bul_text2.string);
					break;
				}
				if (s->frags == 1)
					sprintf(text, "0%s is behind\nwith 1 frag!", s->name);
				else
					sprintf(text, "0%s is behind\nwith %i frags!", s->name, s->frags);
				break;

			case 2: //an add
				sprintf(text, bul_text1.string);
				break;

			case 3: //another add
				sprintf(text, bul_text2.string);
				break;

			case 4: //yet another add
				sprintf(text, bul_text3.string);
				break;

			case 5:
				sprintf(text, bul_text4.string);
				break;

			case 6:
				sprintf(text, bul_text5.string);
				break;

			case 7:
				sprintf(text, bul_text6.string);
				break;

			case 8:
				*text = 0;
				break;

			default:
				sprintf(text, "Unrecognised Bulleten");
				break;
			}


			len = nlstrlen(text, &lines);
#if 1
			if (lines*8 <= a->texture->height)
				a->bultexttop = (a->texture->height - lines*8)/2;
			else
				a->bultexttop = ((int)(cl.time * bul_scrollspeedy.value) % (a->texture->height + lines * 8)) - lines * 8;
#else
			if (lines*8 <= a->texture->height)
				a->bultexttop = (a->texture->height - (lines*8)) / 2;
			else
			{
				a->bultexttop += bul_scrollspeedy.value;
				if (a->bultexttop < lines * -8)
					a->bultexttop = (signed int) a->texture->height;
				if (a->bultexttop > (signed int) a->texture->height)
					a->bultexttop = lines * -8;
			}
#endif

#if 1
			if (len*8 <= a->texture->width)
				a->bultextleft = a->texture->width/2 - len*4;
			else
				a->bultextleft = ((int)(cl.time * bul_scrollspeedx.value) % (a->texture->width + len * 8)) - len * 8;
#else
			if (len*8 <= a->texture->width)
				a->bultextleft = (a->texture->width - (len*8)) / 2;
			else
			{
				a->bultextleft += bul_scrollspeedx.value;
				if (a->bultextleft < len * -8)
					a->bultextleft = (signed int) a->texture->width;
				if (a->bultextleft > (signed int) a->texture->width)
					a->bultextleft = len * -8;
			}
#endif
			R_MakeBulleten(a->texture, a->bultextleft, a->bultexttop, text, a->normaltexture);

#ifdef RGLQUAKE
			if (qrenderer == QR_OPENGL)
			{
				GL_Bind(a->texture->gl_texturenum);

				GL_Upload8 ("bulleten", (qbyte *)a->texture + a->texture->offsets[0], a->texture->width, a->texture->height, false, false);
			}
#endif
		}
	}

//	PR_SwitchProgs(mainprogs);
}

void Draw_CharToMip (int num, qbyte *mip, int x, int y, int width, int height)
{
	int		row, col;
	qbyte	*source;
	int		drawline;
	int		i;

	int s, e;

	s = 0;
	e = 8;
	if (x<0)
		s = s - x;

	if (x > width - e)
		e = width - x;

	if (s > e)
		return;
	if (y >= height)
		return;
	if (y < -8)
		return;
	if (!draw_chars)
		return;

	if (y <= 0)
		mip += x;
	else
		mip += (width*y) + x;


	row = num>>4;
	col = num&15;
	source = draw_chars + (row<<10) + (col<<3);
	if (y < 0)
		source -= 128*y;

	drawline = height-y;
	if (drawline > 8)
		drawline = 8;

	if (y < 0)
			drawline += y;

	while (drawline--)
	{
		for (i=s ; i<e ; i++)
			if (source[i] != 255 && source[i])
				mip[i] = source[i] + bultextpallete;
		source += 128;
		mip += width;
	}

}

void Draw_StringToMip(char *str, qbyte *mip, int x, int y, int width, int height)
{
	int nx = x;
	for (; *str; str++, nx+=8)
	{
		if (*str == '\\')
		{
			str++;
			if (!*str)
				break;

			switch (*str)
			{
			case 'n':
				nx = x-8; // compensate for the 'for' increment
				y+=8;
				continue;
			case '\\':
				break;
			default:
				nx = x-8; // compensate for the 'for' increment
				continue;
			}
		}
		Draw_CharToMip(*str, mip, nx, y, width, height);
	}
}

void R_MakeBulleten (texture_t *textur, int lefttext, int toptext, char *text, qbyte *background)
{
	qbyte bc;
	int x;
	int y;
	int effect;
	int progress;
	qbyte *mip;

	if (*text >= '0' && *text <= '9')
	{
		effect = *text - '0';
		text++;
	}
	else
		effect = 0;

	if (bul_forcemode.value != -1.0)
		effect = bul_forcemode.value;

	switch (effect)
	{
	default:	//solid block colour
		bc = bul_backcol.value;
		mip = (qbyte *) textur + textur->offsets[0];
		Q_memset (mip, bc, textur->width*textur->height);
		break;

	case 1:	//maintain background
		mip = (qbyte *) textur + textur->offsets[0];
		memcpy (mip, background, textur->width*textur->height);
		break;

	case 2:
//put in a wierd sparkly effect - interference
		bc = bul_sparkle.value;
		mip = (qbyte *) textur + textur->offsets[0];

		for (x=0; x<textur->width*textur->height; x++, mip++)
			*mip = rand() & bc;

		break;

	case 3:	//scrolling mip
		progress = (int) (realtime*-bul_scrollspeedx.value) % (textur->width);
		mip = (qbyte *) textur + textur->offsets[0] + progress * textur->height;
		for (x=0; x<progress; x++, mip++, background++)
			*mip = *background;

		mip = (qbyte *) textur + textur->offsets[0];
		for (; x<textur->width*textur->height; x++, mip++, background++)
			*mip = *background;
		break;

	case 4:	//water distortions
		mip = (qbyte *) textur + textur->offsets[0];
		memcpy (mip, background, textur->width*textur->height);

		mip = (qbyte *) textur + textur->offsets[0];
		for (y = 0; y < textur->height; y++)
		{
			for (x = 0; x < textur->width; x++)
			{
				progress = *mip & 0x0F;

/*			assume no full brights.
				if ((*mip & 0xF0) == 0xf0)
				{
					continue;
				}
				else 
*/					if (*mip >= 0x80 && *mip < 0xe0)	//backwards ranges
					progress = progress - (sin(((x+(sin((y/2+cl.time))*3) + (cl.time*bul_ripplespeed.value))/textur->width) * (2 * 3.14))*bul_rippleamount.value);
				else
					progress = progress + (sin(((x+(sin((y/2+cl.time))*3) + (cl.time*bul_ripplespeed.value))/textur->width) * (2 * 3.14))*bul_rippleamount.value);

				if (progress > 15)
					progress = 15;
				if (progress < 0)
					progress = 0;
					

				*mip = progress | (*mip & 0xF0);
				mip++;
			}
		}
		break;
	}	

	if (*text == '\0')
		return;

	mip = (qbyte *) textur + textur->offsets[0];

	Draw_StringToMip (text, mip, lefttext, toptext, textur->width, textur->height);
}


void Bul_ParseMessage(void)
{
	cvar_t *cv;	
	int num;
	char *str;
	num = MSG_ReadByte ();
	str = MSG_ReadString ();

	switch (num)
	{
		default:
		case 1: cv = &bul_text1; break;
		case 2: cv = &bul_text2; break;
		case 3: cv = &bul_text3; break;
		case 4: cv = &bul_text4; break;
		case 5: cv = &bul_text5; break;
		case 6: cv = &bul_text6; break;
	}
	Cvar_Set(cv, str);
}

#endif	//usebulletens
