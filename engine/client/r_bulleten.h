#define MAXBULLETENS 12	//remove limits

extern cvar_t bul_advert1;
extern cvar_t bul_advert2;
extern cvar_t bul_advert3;
extern cvar_t bul_advert4;
extern cvar_t bul_advertvents;
extern cvar_t bul_advertq3;
extern cvar_t bul_scrollspeedx;
extern cvar_t bul_scrollspeedy;
extern cvar_t bul_backcol;
extern cvar_t bul_textpalette;
extern cvar_t bul_norender;


typedef struct bulletentexture_s
{
	texture_t *texture;
	int bultextleft;
	int bultexttop;
	int type;
	qbyte *normaltexture;

	struct bulletentexture_s *next;
} bulletentexture_t;

extern bulletentexture_t *bulletentexture;

extern qbyte		*draw_chars; //console text


extern int scoreboardlines;
extern int fragsort[];

qboolean R_AddBulleten (texture_t *textur);
void R_MakeBulleten (texture_t *textur, int lefttext, int toptext, char *text, qbyte *background);
//void R_MakeBulleten (texture_t *textur, int lefttext, int toptext, char *text);
void R_SetupBulleten (void);
void Draw_StringToMip(char *str, qbyte *mip, int x, int y, int width, int height);
void Draw_CharToMip (int num, qbyte *mip, int x, int y, int width, int height);

void WipeBulletenTextures(void);
void Bul_ParseMessage(void);
