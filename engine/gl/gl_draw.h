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
void GLSurf_DeInit (void);
void GLDraw_Character (int x, int y, unsigned int num);
void GLDraw_ColouredCharacter (int x, int y, unsigned int num);
void GLDraw_TinyCharacter (int x, int y, unsigned int num);
void GLDraw_DebugChar (qbyte num);
void GLDraw_SubPic(int x, int y, mpic_t *pic, int srcx, int srcy, int width, int height);
void GLDraw_Pic (int x, int y, mpic_t *pic);
void GLDraw_ScalePic (int x, int y, int width, int height, mpic_t *pic);
void GLDraw_TransPic (int x, int y, mpic_t *pic);
void GLDraw_TransPicTranslate (int x, int y, int w, int h, qbyte *pic, qbyte *translation);
void GLDraw_ConsoleBackground (int firstline, int lastline, qboolean forceopaque);
void GLDraw_EditorBackground (int lines);
void GLDraw_BeginDisc (void);
void GLDraw_EndDisc (void);
void GLDraw_TileClear (int x, int y, int w, int h);
void GLDraw_FillRGB (int x, int y, int w, int h, float r, float g, float b);
void GLDraw_Fill (int x, int y, int w, int h, unsigned int c);
void GLDraw_FadeScreen (void);
void GLDraw_String (int x, int y, const qbyte *str);
void GLDraw_Alt_String (int x, int y, const qbyte *str);
mpic_t *GLDraw_SafePicFromWad (char *name);
mpic_t *GLDraw_SafeCachePic (char *path);
mpic_t *GLDraw_CachePic (char *path);
void GLDraw_Crosshair(void);
void GLDraw_LevelPic (mpic_t *pic);

void GLDraw_ImageColours(float r, float g, float b, float a);
void GLDraw_Image(float x, float y, float w, float h, float s1, float t1, float s2, float t2, mpic_t *pic);

void R_BloomRegister(void);
