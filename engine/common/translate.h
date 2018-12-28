#ifndef TRANSLATE_H
#define TRANSLATE_H
typedef const char* translation_t;

#define MAX_LANGUAGES 64

void TranslateInit(void);

void SV_InitLanguages(void);

struct language_s
{
	char *name;
	struct po_s *po;
};
extern struct language_s languages[MAX_LANGUAGES];
extern int com_language;
extern cvar_t language;
#define langtext(t,l) PO_GetText(languages[l].po, t)
int TL_FindLanguage(const char *lang);

#endif
