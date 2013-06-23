typedef struct xmlparams_s
{
	char val[256];	//FIXME: make pointer
	struct xmlparams_s *next;
	char name[64];	//FIXME: make variable sized
} xmlparams_t;

typedef struct subtree_s
{
	char name[64];												//FIXME: make pointer to tail of structure
	char xmlns[64];			//namespace of the element			//FIXME: make pointer to tail of structure
	char xmlns_dflt[64];	//default namespace of children		//FIXME: make pointer to tail of structure
	char body[2048];											//FIXME: make pointer+variablesized

	xmlparams_t *params;

	struct subtree_s *child;
	struct subtree_s *sibling;
} xmltree_t;



char *XML_GetParameter(xmltree_t *t, char *paramname, char *def);
void XML_AddParameter(xmltree_t *t, char *paramname, char *value);
void XML_AddParameteri(xmltree_t *t, char *paramname, int value);
xmltree_t *XML_CreateNode(xmltree_t *parent, char *name, char *xmlns, char *body);
char *XML_Markup(char *s, char *d, int dlen);
void XML_Unmark(char *s);
char *XML_GenerateString(xmltree_t *root);
xmltree_t *XML_Parse(char *buffer, int *startpos, int maxpos, qboolean headeronly, char *defaultnamespace);
void XML_Destroy(xmltree_t *t);
xmltree_t *XML_ChildOfTree(xmltree_t *t, char *name, int childnum);
void XML_ConPrintTree(xmltree_t *t, int indent);
