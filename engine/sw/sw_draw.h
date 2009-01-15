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

void SWDraw_Init (void);
void SWDraw_Shutdown(void);
void SWDraw_Character (int x, int y, unsigned int num);
void SWDraw_TinyCharacter (int x, int y, unsigned int num);
void SWDraw_ImageColours (float r, float g, float b, float a);
void SWDraw_Image (float xp, float yp, float wp, float hp, float s1, float t1, float s2, float t2, mpic_t *pic);
void SWDraw_ColouredCharacter (int x, int y, unsigned int num);
void SWDraw_DebugChar (qbyte num);
void SWDraw_SubPic(int x, int y, mpic_t *pic, int srcx, int srcy, int width, int height);
void SWDraw_Pic (int x, int y, mpic_t *pic);
void SWDraw_TransPic (int x, int y, mpic_t *pic);
void SWDraw_TransPicTranslate (int x, int y, int w, int h, qbyte *pic, qbyte *translation);
void SWDraw_ConsoleBackground (int lines);
void SWDraw_EditorBackground (int lines);
void SWDraw_BeginDisc (void);
void SWDraw_EndDisc (void);
void SWDraw_TileClear (int x, int y, int w, int h);
void SWDraw_Fill (int x, int y, int w, int h, unsigned int c);
void SWDraw_FillRGB (int x, int y, int w, int h, float r, float g, float b);
void SWDraw_FadeScreen (void);
void SWDraw_String (int x, int y, const qbyte *str);
void SWDraw_Alt_String (int x, int y, const qbyte *str);
mpic_t *SWDraw_SafePicFromWad (char *name);
mpic_t *SWDraw_PicFromWad (char *name);
mpic_t *SWDraw_SafeCachePic (char *path);
mpic_t *SWDraw_CachePic (char *path);
void SWDraw_Crosshair(void);

