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

// draw.h -- these are the only functions outside the refresh allowed
// to touch the vid buffer

void GLDraw_Init (void);
void GLDraw_ReInit (void);
void GLDraw_DeInit (void);
void Surf_DeInit (void);
void GLDraw_TransPicTranslate (int x, int y, int w, int h, qbyte *pic, qbyte *translation);
void GLDraw_Crosshair(void);
void GLDraw_LevelPic (mpic_t *pic);

void R2D_Init(void);
mpic_t	*R2D_SafeCachePic (char *path);
mpic_t *R2D_SafePicFromWad (char *name);
void R2D_ImageColours(float r, float g, float b, float a);
void R2D_Image(float x, float y, float w, float h, float s1, float t1, float s2, float t2, mpic_t *pic);
void R2D_ScalePic (int x, int y, int width, int height, mpic_t *pic);
void R2D_SubPic(int x, int y, int width, int height, mpic_t *pic, int srcx, int srcy, int srcwidth, int srcheight);
void R2D_ConsoleBackground (int firstline, int lastline, qboolean forceopaque);
void R2D_EditorBackground (void);
void R2D_TileClear (int x, int y, int w, int h);
void R2D_FadeScreen (void);
void R2D_Init(void);
void R2D_Shutdown(void);

void R2D_PolyBlend (void);
void R2D_BrightenScreen (void);

void R2D_Conback_Callback(struct cvar_s *var, char *oldvalue);
