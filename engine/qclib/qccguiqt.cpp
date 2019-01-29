/*Copyright Spoike, license is GPLv2+ */

/*Todo (in no particular order):
	tooltips for inspecting variables/types.
	variables/watch list.
	calltips for argument info
	autocompletion calltips, for people who can't remember function names
	ctrl+f/f3 stuff
	options window
	initial open project prompt
	shpuld's styling
	decompiler output saving
	right-click popup
		goto-def
		grep-for
		toggle-breakpoint
		set-next
	bracket/brace highlights
	autoindentation on enter, etc
	different displays for non-text files?
	utf-16? mneh, who gives a shit
	give focus back to the engine on resume
*/


#ifdef __PIC__
	#undef __PIE__	//QT is being annoying.
#endif
#include <QtWidgets>
#include <Qsci/qsciscintilla.h>
#include <Qsci/qscilexercpp.h>
#include <unistd.h>
#include <sys/stat.h>

extern "C"
{
#include "qcc.h"
#include "gui.h"

extern pbool fl_nondfltopts;
extern pbool fl_hexen2;
extern pbool fl_ftetarg;
extern pbool fl_compileonstart;
extern pbool fl_showall;
extern pbool fl_log;
extern pbool fl_extramargins;
extern int fl_tabsize;

extern char enginebinary[MAX_OSPATH];
extern char enginebasedir[MAX_OSPATH];
extern char enginecommandline[8192];
};
static char *cmdlineargs;

#undef NULL
#define NULL nullptr

#undef Sys_Error

//c++ sucks and just pisses me off with its lack of support for void*
template<class T> inline T cpprealloc(T p, size_t s) {return static_cast<T>(realloc(static_cast<void*>(p),s));};
#define STRINGIFY2(s) #s
#define STRINGIFY(s) STRINGIFY2(s)


static void DebuggerStop(void);
static bool DebuggerSendCommand(const char *msg, ...);
static void DebuggerStart(void);


void Sys_Error(const char *text, ...)
{
	va_list argptr;
	static char msg[2048];

	va_start (argptr,text);
	QC_vsnprintf (msg,sizeof(msg)-1, text,argptr);
	va_end (argptr);

	QCC_Error(ERR_INTERNAL, "%s", msg);
}

void RunCompiler(const char *args, pbool quick);
static void *QCC_ReadFile(const char *fname, unsigned char *(*buf_get)(void *ctx, size_t len), void *buf_ctx, size_t *out_size)
{
	size_t len;
	unsigned char *buffer;

	vfile_t *v = QCC_FindVFile(fname);
	if (v)
	{
		len = v->size;
		if (buf_get)
			buffer = buf_get(buf_ctx, len+1);
		else
			buffer = static_cast<unsigned char *>(malloc(len+1));
		if (!buffer)
			return NULL;
		buffer[len] = 0;
		if (len > v->size)
			len = v->size;
		memcpy(buffer, v->file, len);
		if (out_size)
			*out_size = len;
		return buffer;
	}

	auto f = fopen(fname, "rb");
	if (!f)
	{
		if (out_size)
			*out_size = 0;
		return nullptr;
	}

	fseek(f, 0, SEEK_END);
	len = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (buf_get)
		buffer = buf_get(buf_ctx, len+1);
	else
		buffer = static_cast<unsigned char *>(malloc(len+1));
	buffer[len] = 0;
	if (len != fread(buffer, 1, len, f))
	{
		if (!buf_get)
			free(buffer);
		buffer = nullptr;
	}
	fclose(f);

	if (out_size)
		*out_size = len;
	return buffer;
}
static int PDECL QCC_FileSize (const char *fname)
{
	vfile_t *v = QCC_FindVFile(fname);
	if (v)
		return v->size;

	long    length;
	auto f = fopen(fname, "rb");
	if (!f)
		return -1;
	fseek(f, 0, SEEK_END);
	length = ftell(f);
	fclose(f);
	return length;
}
static int PDECL QCC_PopFileSize (const char *fname)
{	//populates the file list, as well as returning the size
	extern int qcc_compileactive;
	int len = QCC_FileSize(fname);
	if (len >= 0 && qcc_compileactive)
	{
		AddSourceFile(compilingrootfile,    fname);
	}
	return len;
}

static int PDECL QCC_StatFile (const char *fname, struct stat *sbuf)
{
	vfile_t *v = QCC_FindVFile(fname);
	if (v)
	{
		memset(sbuf, 0, sizeof(*sbuf));
		sbuf->st_size = v->size;
		return 0;
	}
	return stat(fname, sbuf);
}

pbool PDECL QCC_WriteFile (const char *name, void *data, int len)
{
	long    length;
	FILE *f;

	auto *ext = strrchr(name, '.');
	if (ext && !stricmp(ext, ".gz"))
	{
#ifdef AVAIL_ZLIB
		pbool okay = true;
		char out[1024*8];

		z_stream strm = {
			data,
			len,
			0,

			out,
			sizeof(out),
			0,

			NULL,
			NULL,

			NULL,
			NULL,
			NULL,

			Z_BINARY,
			0,
			0
		};

		f = fopen(name, "wb");
		if (!f)
			return false;
		deflateInit2(&strm, Z_BEST_COMPRESSION, Z_DEFLATED, MAX_WBITS|16, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
		while(okay && deflate(&strm, Z_FINISH) == Z_OK)
		{
			if (sizeof(out) - strm.avail_out != fwrite(out, 1, sizeof(out) - strm.avail_out, f))
				okay = false;
			strm.next_out = out;
			strm.avail_out = sizeof(out);
		}
		if (sizeof(out) - strm.avail_out != fwrite(out, 1, sizeof(out) - strm.avail_out, f))
			okay = false;
		deflateEnd(&strm);
		fclose(f);
		if (!okay)
			unlink(name);
		return okay;
#else
		return false;
#endif
	}

	if (QCC_FindVFile(name))
		return !!QCC_AddVFile(name, data, len);

	f = fopen(name, "wb");
	if (!f)
		return false;
	length = fwrite(data, 1, len, f);
	fclose(f);

	if (length != len)
		return false;

	return true;
}

//for the project's treeview to work, we need a subclass to provide the info to be displayed
class filelist : public QAbstractItemModel
{
public:
	struct filenode_s
	{
		filenode_s *parent = nullptr;
		int numchildren;
		filenode_s **children;

		char *name;

		~filenode_s()
		{
			while(numchildren)
			{
				delete(children[--numchildren]);
			}
			free(children);
			free(name);
		}
	} *root;

	filenode_s *getItem(const QModelIndex &idx) const
	{
		if (idx.isValid())
			return static_cast<filenode_s*>(idx.internalPointer());
		return root;
	}
	virtual int 	rowCount(const QModelIndex &parent = QModelIndex()) const
	{
		const filenode_s *n = getItem(parent);
		return n->numchildren;
	}
	virtual int		columnCount(const QModelIndex &parent = QModelIndex()) const
	{
		return 1;
	}
	virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const
	{
		const filenode_s *n = getItem(index);
		switch(role)
		{
		case Qt::DisplayRole:
			return QVariant(n->name);
		}
		return QVariant();
	}
	virtual QModelIndex index(int row, int column, const QModelIndex &parent) const
	{
		filenode_s *n;
		if (column)
			return QModelIndex();
		n = getItem(parent);
		if (row >= 0 && row < n->numchildren)
			return createIndex(row, column, n->children[row]);
		return QModelIndex();
	}
	virtual QModelIndex parent(const QModelIndex &index) const
	{
		if (!index.isValid())
			return QModelIndex();

		filenode_s *n = getItem(index);
		filenode_s *p = n->parent;

		if (p == root)
			return QModelIndex();
		else
		{
			int parentrow = 0;
			if (n->parent)
				while (n != n->parent->children[parentrow])
					parentrow++;
			return createIndex(parentrow, 0, p);
		}
	}

	virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const
	{
		switch(role)
		{
		case Qt::DisplayRole:
			return QVariant("Files");
		}
		return QVariant();
	}

public:
	filelist()
	{
		root = new filenode_s();
	}

	void GrepAll(const char *search, filenode_s *n = nullptr)
	{
		if (!n)
		{
			GUIprintf("");
			GUIprintf("Grep for %s\n", search);
			n = root;
		}

		Grep(n->name, search);
		for (int i = 0; i < n->numchildren; i++)
		{
			auto c = n->children[i];
			GrepAll(search, c);
		}
	}

	filenode_s *FindChild(filenode_s *p, const char *filename)
	{
		for (int i = 0; i < p->numchildren; i++)
		{
			auto c = p->children[i];
			if (!strcasecmp(c->name, filename))
				return c;
		}
		return nullptr;
	}
	void AddFile(const char *parentpath, const char *filename)
	{
		filenode_s *p = root, *c;

		if (!filename)
		{
			delete root;
			root = new filenode_s();
			return;
		}

		while (parentpath && *parentpath)
		{
			auto sl = strchr(parentpath, '/');

			c = FindChild(p, parentpath);
			if (!c)
				break;
			p = c;

			if (sl)
				sl++;
			parentpath = sl;
		}

		while(!strncmp(filename, "./", 2))
			filename+=2;

		if (p->parent && FindChild(p->parent, filename) == p)
			return;	//matches its parent. probably the .src file itself
		c = FindChild(p, filename);
		if (c)
			return;	//already in there.

		c = new filenode_s();
		c->name = strdup(filename);
		c->parent = p;

		p->children = cpprealloc(p->children, sizeof(*p->children)*(p->numchildren+1));
		p->children[p->numchildren] = c;
		p->numchildren++;
	}
};

class documentlist : public QAbstractListModel
{
	QsciScintilla *s;	//this is the widget that we load our documents into

	int numdocuments;
	struct document_s
	{	//these are swapped in/out of the scintilla widget
		const char *fname;
		const char *shortname;
		time_t filemodifiedtime;
		bool modified;
		int cursorline;
		int savefmt;	//encoding to save as
		QsciDocument doc;
		QsciLexer *l;
	} **docs, *curdoc;

	class docstacklock
	{
		struct document_s *oldval;
		documentlist &dl;
	public:
		docstacklock(documentlist *ptr_, struct document_s *newval) : dl(*ptr_)
		{	//pick new stuff
			oldval = dl.curdoc;
			dl.curdoc = newval;
			dl.s->setDocument(dl.curdoc->doc);
		}
		~docstacklock()
		{	//restore state to how it used to be
			if (!oldval)
				return;
			dl.curdoc = oldval;
			dl.s->setDocument(dl.curdoc->doc);
		}
	};

	document_s *getItem(const QModelIndex &idx) const
	{
		if (idx.isValid())
			return docs[idx.row()];
		return nullptr;
	}
	virtual int 	rowCount(const QModelIndex &parent = QModelIndex()) const
	{
		return numdocuments;
	}
	virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const
	{
		const document_s *n = getItem(index);
		if(n)
		switch(role)
		{
		case Qt::DisplayRole:
			if (n->modified)
				return QVariant(QString::asprintf("%s*", n->fname));
			else
				return QVariant(n->fname);
		}
		return QVariant();
	}

	virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const
	{
		switch(role)
		{
		case Qt::DisplayRole:
			return QVariant("Files");
		}
		return QVariant();
	}
	void UpdateTitle(void);
public:
	documentlist(QsciScintilla *editor)
	{
		s = editor;
		numdocuments = 0;
		docs = nullptr;

		connect(s, &QsciScintilla::cursorPositionChanged,
			[=](int line, int index)
			{
				if (curdoc)
				{
					curdoc->cursorline = line+1;
					UpdateTitle();
				}
			});

		connect(s, &QsciScintilla::modificationChanged,
			[=](bool m)
			{
				if (curdoc)
				{
					curdoc->modified = m;

					for(int row = 0; row < numdocuments; row++)
					{
						if(docs[row] == curdoc)
						{
							auto i = index(row);
							this->dataChanged(i, i);
						}
					}
				}
			});
	}

	void SetupScintilla(document_s *ed)
	{
//		ed->l = new QsciLexerCPP (s);
		s->SendScintilla(QsciScintillaBase::SCI_STYLERESETDEFAULT);
//		s->SendScintilla(QsciScintillaBase::SCI_STYLESETFONT, QsciScintillaBase::STYLE_DEFAULT, "Consolas");
		s->setFont(QFont(QString("Consolas"), 8));
		s->SendScintilla(QsciScintillaBase::SCI_STYLECLEARALL);

		s->SendScintilla(QsciScintillaBase::SCI_SETCODEPAGE, QsciScintillaBase::SC_CP_UTF8);
		s->SendScintilla(QsciScintillaBase::SCI_SETLEXER,     QsciScintillaBase::SCLEX_CPP);
		s->SendScintilla(QsciScintillaBase::SCI_STYLESETFORE, QsciLexerCPP::Default,					QColor(0x00, 0x00, 0x00));
		s->SendScintilla(QsciScintillaBase::SCI_STYLECLEARALL);
		s->SendScintilla(QsciScintillaBase::SCI_STYLESETFORE, QsciLexerCPP::Comment,					QColor(0x00, 0x80, 0x00));
		s->SendScintilla(QsciScintillaBase::SCI_STYLESETFORE, QsciLexerCPP::CommentLine,				QColor(0x00, 0x80, 0x00));
		s->SendScintilla(QsciScintillaBase::SCI_STYLESETFORE, QsciLexerCPP::CommentDoc,					QColor(0x00, 0x80, 0x00));
		s->SendScintilla(QsciScintillaBase::SCI_STYLESETFORE, QsciLexerCPP::Number,						QColor(0xA0, 0x10, 0x10));
		s->SendScintilla(QsciScintillaBase::SCI_STYLESETFORE, QsciLexerCPP::Keyword,						QColor(0x00, 0x00, 0xFF));
		s->SendScintilla(QsciScintillaBase::SCI_STYLESETFORE, QsciLexerCPP::DoubleQuotedString,			QColor(0xA0, 0x10, 0x10));
		s->SendScintilla(QsciScintillaBase::SCI_STYLESETFORE, QsciLexerCPP::SingleQuotedString,			QColor(0xA0, 0x10, 0x10));
//      s->SendScintilla(QsciScintillaBase::SCI_STYLESETFORE, QsciLexerCPP::UUID,						QColor(0xA0, 0x10, 0x10));
		s->SendScintilla(QsciScintillaBase::SCI_STYLESETFORE, QsciLexerCPP::PreProcessor,				QColor(0x00, 0x00, 0xFF));
//      s->SendScintilla(QsciScintillaBase::SCI_STYLESETFORE, QsciLexerCPP::Operator,					QColor(0x00, 0x00, 0x00));
//      s->SendScintilla(QsciScintillaBase::SCI_STYLESETFORE, QsciLexerCPP::Identifier,					QColor(0x00, 0x00, 0x00));
		s->SendScintilla(QsciScintillaBase::SCI_STYLESETFORE, QsciLexerCPP::UnclosedString,				QColor(0xA0, 0x10, 0x10));
		s->SendScintilla(QsciScintillaBase::SCI_STYLESETFORE, QsciLexerCPP::VerbatimString,				QColor(0xA0, 0x10, 0x10));
		s->SendScintilla(QsciScintillaBase::SCI_STYLESETFORE, QsciLexerCPP::Regex,						QColor(0xA0, 0x10, 0x10));
		s->SendScintilla(QsciScintillaBase::SCI_STYLESETFORE, QsciLexerCPP::CommentLineDoc,				QColor(0xA0, 0x10, 0x10));
		s->SendScintilla(QsciScintillaBase::SCI_STYLESETFORE, QsciLexerCPP::KeywordSet2,				QColor(0xA0, 0x10, 0x10));
		s->SendScintilla(QsciScintillaBase::SCI_STYLESETFORE, QsciLexerCPP::CommentDocKeyword,			QColor(0xA0, 0x10, 0x10));
		s->SendScintilla(QsciScintillaBase::SCI_STYLESETFORE, QsciLexerCPP::CommentDocKeywordError,		QColor(0xA0, 0x10, 0x10));
		s->SendScintilla(QsciScintillaBase::SCI_STYLESETFORE, QsciLexerCPP::GlobalClass,				QColor(0xA0, 0x10, 0x10));
		s->SendScintilla(QsciScintillaBase::SCI_STYLESETFORE, QsciLexerCPP::RawString,					QColor(0xA0, 0x00, 0x00));
		s->SendScintilla(QsciScintillaBase::SCI_STYLESETFORE, QsciLexerCPP::TripleQuotedVerbatimString,	QColor(0xA0, 0x10, 0x10));
		s->SendScintilla(QsciScintillaBase::SCI_STYLESETFORE, QsciLexerCPP::HashQuotedString,			QColor(0xA0, 0x10, 0x10));
		s->SendScintilla(QsciScintillaBase::SCI_STYLESETFORE, QsciLexerCPP::PreProcessorComment,		QColor(0xA0, 0x10, 0x10));
		s->SendScintilla(QsciScintillaBase::SCI_STYLESETFORE, QsciLexerCPP::PreProcessorCommentLineDoc,	QColor(0xA0, 0x10, 0x10));
		s->SendScintilla(QsciScintillaBase::SCI_STYLESETFORE, QsciLexerCPP::UserLiteral,				QColor(0xA0, 0x10, 0x10));
		s->SendScintilla(QsciScintillaBase::SCI_STYLESETFORE, QsciLexerCPP::TaskMarker,					QColor(0xA0, 0x10, 0x10));
		s->SendScintilla(QsciScintillaBase::SCI_STYLESETFORE, QsciLexerCPP::EscapeSequence,				QColor(0xA0, 0x10, 0x10));

		s->SendScintilla(QsciScintillaBase::SCI_STYLESETFORE, QsciScintillaBase::STYLE_BRACELIGHT,		QColor(0x00, 0x00, 0x3F));
		s->SendScintilla(QsciScintillaBase::SCI_STYLESETBACK, QsciScintillaBase::STYLE_BRACELIGHT,		QColor(0xef, 0xaf, 0xaf));
		s->SendScintilla(QsciScintillaBase::SCI_STYLESETBOLD, QsciScintillaBase::STYLE_BRACELIGHT,		true);
		s->SendScintilla(QsciScintillaBase::SCI_STYLESETFORE, QsciScintillaBase::STYLE_BRACEBAD,		QColor(0x3F, 0x00, 0x00));
		s->SendScintilla(QsciScintillaBase::SCI_STYLESETBACK, QsciScintillaBase::STYLE_BRACEBAD,		QColor(0xff, 0xaf, 0xaf));

		//SCE_C_WORD
		s->SendScintilla(QsciScintillaBase::SCI_SETKEYWORDS,  0ul,
					"if else for do not while asm break case const continue "
					"default enum enumflags extern "
					"float goto __in __out __inout noref "
					"nosave shared __state optional string "
					"struct switch thinktime until loop "
					"typedef union var "
					"accessor get set inline "
					"virtual nonvirtual class static nonstatic local return "
					"string float vector void int integer __variant entity"
					);

		//SCE_C_WORD2
		{
			char buffer[65536];
			GenBuiltinsList(buffer, sizeof(buffer));
			s->SendScintilla(QsciScintillaBase::SCI_SETKEYWORDS,  1ul,  buffer);
		}
		//SCE_C_COMMENTDOCKEYWORDERROR
		//SCE_C_GLOBALCLASS
		s->SendScintilla(QsciScintillaBase::SCI_SETKEYWORDS,  3ul,
					""
					);
//preprocessor listing
		{
			char *deflist = QCC_PR_GetDefinesList();
			if (!deflist)
				deflist = strdup("");
			s->SendScintilla(QsciScintillaBase::SCI_SETKEYWORDS,  4ul,  deflist);
			free(deflist);
		}
		//task markers (in comments only)
		s->SendScintilla(QsciScintillaBase::SCI_SETKEYWORDS,  5ul,
					"TODO FIXME BUG"
					);

		s->SendScintilla(QsciScintillaBase::SCI_USEPOPUP, QsciScintillaBase::SC_POPUP_NEVER);    //so we can do right-click menus ourselves.

		s->SendScintilla(QsciScintillaBase::SCI_SETMOUSEDWELLTIME, 1000);
		s->SendScintilla(QsciScintillaBase::SCI_AUTOCSETORDER, QsciScintillaBase::SC_ORDER_PERFORMSORT);
		s->SendScintilla(QsciScintillaBase::SCI_AUTOCSETFILLUPS, nullptr, ".,[<>(*/+-=\t\n");

		//Set up gui options.
		s->SendScintilla(QsciScintillaBase::SCI_SETMARGINWIDTHN,      0, fl_extramargins?40:0);   //line numbers+folding
		s->SendScintilla(QsciScintillaBase::SCI_SETTABWIDTH,          fl_tabsize);     //tab size

		//add margin for breakpoints
		s->SendScintilla(QsciScintillaBase::SCI_SETMARGINMASKN,       1, ~QsciScintillaBase::SC_MASK_FOLDERS);
		s->SendScintilla(QsciScintillaBase::SCI_SETMARGINWIDTHN,      1, 16);
		s->SendScintilla(QsciScintillaBase::SCI_SETMARGINSENSITIVEN,  1, true);
		//give breakpoints a nice red circle.
		s->SendScintilla(QsciScintillaBase::SCI_MARKERDEFINE,         0,  QsciScintillaBase::SC_MARK_CIRCLE);
		s->SendScintilla(QsciScintillaBase::SCI_MARKERSETFORE,        0,  QColor(0x7F, 0x00, 0x00));
		s->SendScintilla(QsciScintillaBase::SCI_MARKERSETBACK,        0,  QColor(0xFF, 0x00, 0x00));
		//give current line a yellow arrow
		s->SendScintilla(QsciScintillaBase::SCI_MARKERDEFINE,         1,  QsciScintillaBase::SC_MARK_SHORTARROW);
		s->SendScintilla(QsciScintillaBase::SCI_MARKERSETFORE,        1,  QColor(0xFF, 0xFF, 0x00));
		s->SendScintilla(QsciScintillaBase::SCI_MARKERSETBACK,        1,  QColor(0x7F, 0x7F, 0x00));
		s->SendScintilla(QsciScintillaBase::SCI_MARKERDEFINE,         2,  QsciScintillaBase::SC_MARK_BACKGROUND);
		s->SendScintilla(QsciScintillaBase::SCI_MARKERSETFORE,        2,  QColor(0x00, 0x00, 0x00));
		s->SendScintilla(QsciScintillaBase::SCI_MARKERSETBACK,        2,  QColor(0xFF, 0xFF, 0x00));
		s->SendScintilla(QsciScintillaBase::SCI_MARKERSETALPHA,       2,  0x40);

		//add margin for folding

		s->SendScintilla(QsciScintillaBase::SCI_SETPROPERTY,  "fold", "1");
		s->SendScintilla(QsciScintillaBase::SCI_SETMARGINWIDTHN,      2, fl_extramargins?16:0);
		s->SendScintilla(QsciScintillaBase::SCI_SETMARGINMASKN,       2, QsciScintillaBase::SC_MASK_FOLDERS);
		s->SendScintilla(QsciScintillaBase::SCI_SETMARGINSENSITIVEN,  2, true);
		//stop the images from being stupid
		s->SendScintilla(QsciScintillaBase::SCI_MARKERDEFINE,		QsciScintillaBase::SC_MARKNUM_FOLDEROPEN,		QsciScintillaBase::SC_MARK_BOXMINUS);
		s->SendScintilla(QsciScintillaBase::SCI_MARKERDEFINE,		QsciScintillaBase::SC_MARKNUM_FOLDER,			QsciScintillaBase::SC_MARK_BOXPLUS);
		s->SendScintilla(QsciScintillaBase::SCI_MARKERDEFINE,		QsciScintillaBase::SC_MARKNUM_FOLDERSUB,		QsciScintillaBase::SC_MARK_VLINE);
		s->SendScintilla(QsciScintillaBase::SCI_MARKERDEFINE,		QsciScintillaBase::SC_MARKNUM_FOLDERTAIL,		QsciScintillaBase::SC_MARK_LCORNERCURVE);
		s->SendScintilla(QsciScintillaBase::SCI_MARKERDEFINE,		QsciScintillaBase::SC_MARKNUM_FOLDEREND,		QsciScintillaBase::SC_MARK_BOXPLUSCONNECTED);
		s->SendScintilla(QsciScintillaBase::SCI_MARKERDEFINE,		QsciScintillaBase::SC_MARKNUM_FOLDEROPENMID,	QsciScintillaBase::SC_MARK_BOXMINUSCONNECTED);
		s->SendScintilla(QsciScintillaBase::SCI_MARKERDEFINE,		QsciScintillaBase::SC_MARKNUM_FOLDERMIDTAIL,	QsciScintillaBase::SC_MARK_TCORNERCURVE);
		//and fuck with colours so that its visible.
#define FOLDBACK QColor(0x50, 0x50, 0x50)
		s->SendScintilla(QsciScintillaBase::SCI_MARKERSETFORE,    QsciScintillaBase::SC_MARKNUM_FOLDER,          QColor(0xFF, 0xFF, 0xFF));
		s->SendScintilla(QsciScintillaBase::SCI_MARKERSETBACK,    QsciScintillaBase::SC_MARKNUM_FOLDER,          FOLDBACK);
		s->SendScintilla(QsciScintillaBase::SCI_MARKERSETFORE,    QsciScintillaBase::SC_MARKNUM_FOLDEROPEN,      QColor(0xFF, 0xFF, 0xFF));
		s->SendScintilla(QsciScintillaBase::SCI_MARKERSETBACK,    QsciScintillaBase::SC_MARKNUM_FOLDEROPEN,      FOLDBACK);
		s->SendScintilla(QsciScintillaBase::SCI_MARKERSETFORE,    QsciScintillaBase::SC_MARKNUM_FOLDEROPENMID,   QColor(0xFF, 0xFF, 0xFF));
		s->SendScintilla(QsciScintillaBase::SCI_MARKERSETBACK,    QsciScintillaBase::SC_MARKNUM_FOLDEROPENMID,   FOLDBACK);
		s->SendScintilla(QsciScintillaBase::SCI_MARKERSETBACK,    QsciScintillaBase::SC_MARKNUM_FOLDERSUB,       FOLDBACK);
		s->SendScintilla(QsciScintillaBase::SCI_MARKERSETFORE,    QsciScintillaBase::SC_MARKNUM_FOLDEREND,       QColor(0xFF, 0xFF, 0xFF));
		s->SendScintilla(QsciScintillaBase::SCI_MARKERSETBACK,    QsciScintillaBase::SC_MARKNUM_FOLDEREND,       FOLDBACK);
		s->SendScintilla(QsciScintillaBase::SCI_MARKERSETBACK,    QsciScintillaBase::SC_MARKNUM_FOLDERTAIL,      FOLDBACK);
		s->SendScintilla(QsciScintillaBase::SCI_MARKERSETBACK,    QsciScintillaBase::SC_MARKNUM_FOLDERMIDTAIL,   FOLDBACK);

		//disable preprocessor tracking, because QC preprocessor is not specific to an individual file, and even if it was, includes would be messy.
//      s->SendScintilla(QsciScintillaBase::SCI_SETPROPERTY,  (WPARAM)"lexer.cpp.track.preprocessor", (LPARAM)"0");

		for (int i = 0; i < 0x100; i++)
		{
			const char *lowtab[32] = {"QNUL",NULL,NULL,NULL,NULL,".",NULL,NULL,NULL,NULL,NULL,"#",NULL,">",".",".",
								"[","]","0","1","2","3","4","5","6","7","8","9",".","<-","-","->"};
			const char *hightab[32] = {"(=","=","=)","=#=","White",".","Green","Red","Yellow","Blue",NULL,"Purple",NULL,">",".",".",
								"[","]","0","1","2","3","4","5","6","7","8","9",".","<-","-","->"};
			char foo[4];
			char bar[4];
			unsigned char c = i&0xff;
			foo[0] = i; //these are invalid encodings or control chars.
			foo[1] = 0;

			if (c < 32)
			{
				if (lowtab[c])
					s->SendScintilla(QsciScintillaBase::SCI_SETREPRESENTATION,    foo,    lowtab[c]);
			}
			else if (c >= (128|0) && c < (128|32))
			{
				if (hightab[c-128])
					s->SendScintilla(QsciScintillaBase::SCI_SETREPRESENTATION,    foo,    hightab[c-128]);
			}
			else if (c < 128)
				continue;   //don't do anything weird for ascii (other than control chars)
			else
			{
				int b = 0;
				bar[b++] = c&0x7f;
				bar[b++] = 0;
				s->SendScintilla(QsciScintillaBase::SCI_SETREPRESENTATION,    foo,    bar);
			}
		}

		for (int i = 0xe000; i < 0xe100; i++)
		{
			const char *lowtab[32] = {"QNUL",NULL,NULL,NULL,NULL,".",NULL,NULL,NULL,NULL,NULL,"#",NULL,">",".",".",
								"[","]","0","1","2","3","4","5","6","7","8","9",".","<-","-","->"};
			const char *hightab[32] = {"(=","=","=)","=#=","White",".","Green","Red","Yellow","Blue",NULL,"Purple",NULL,">",".",".",
								"[","]","^0","^1","^2","^3","^4","^5","^6","^7","^8","^9",".","^<-","^-","^->"};
			char foo[4];
			char bar[4];
			unsigned char c = i&0xff;
			foo[0] = ((i>>12) & 0xf) | 0xe0;
			foo[1] = ((i>>6) & 0x3f) | 0x80;
			foo[2] = ((i>>0) & 0x3f) | 0x80;
			foo[3] = 0;

			if (c < 32)
			{
				if (lowtab[c])
					s->SendScintilla(QsciScintillaBase::SCI_SETREPRESENTATION,    foo,    lowtab[c]);
			}
			else if (c >= (128|0) && c < (128|32))
			{
				if (hightab[c-128])
					s->SendScintilla(QsciScintillaBase::SCI_SETREPRESENTATION,    foo,    hightab[c-128]);
			}
			else
			{
				int b = 0;
				if (c >= 128)
					bar[b++] = '^';
				bar[b++] = c&0x7f;
				bar[b++] = 0;
				s->SendScintilla(QsciScintillaBase::SCI_SETREPRESENTATION,    foo,    bar);
			}
		}

/*		auto f = fopen("scintilla.cfg", "rt");
		if (f)
		{
			char buf[256];
			while(fgets(buf, sizeof(buf)-1, f))
			{
				int msg;
				long lparam;
				long wparam;
				char *c;
				buf[sizeof(buf)-1] = 0;
				c = buf;
				while(*c == ' ' || *c == '\t')
					c++;
				if (c[0] == '#')
					continue;
				if (c[0] == '/' && c[1] == '/')
					continue;
				if (c[0] == '\r' || c[0] == '\n' || !c[0])
					continue;
				msg = strtoul(c, &c, 0);
				while(*c == ' ' || *c == '\t')
					c++;
				if (*c == '\"')
				{
					c++;
					wparam = c;
					c = strrchr(c, '\"');
					if (c)
						*c++ = 0;
				}
				else
					wparam = strtoul(c, &c, 0);
				while(*c == ' ' || *c == '\t')
					c++;
				if (*c == '\"')
				{
					c++;
					lparam = c;
					c = strrchr(c, '\"');
					if (c)
						*c++ = 0;
				}
				else
					lparam = strtoul(c, &c, 0);

				s->SendScintilla(QsciScintillaBase::msg,  wparam, lparam);
			}
			if (!ftell(f))
			{
				fclose(f);
				f = fopen("scintilla.cfg", "wt");
				if (f)
				{
					int i;
					int val;
					for (i = 0; i < STYLE_LASTPREDEFINED; i++)
					{
						val = s->SendScintilla(QsciScintillaBase::SCI_STYLEGETFORE,   i,  0);
						fprintf(f, "%i\t%i\t%#x\n", SCI_STYLESETFORE, i, val);
						val = s->SendScintilla(QsciScintillaBase::SCI_STYLEGETBACK,   i,  0);
						fprintf(f, "%i\t%i\t%#x\n", SCI_STYLESETBACK, i, val);
						val = s->SendScintilla(QsciScintillaBase::SCI_STYLEGETBOLD,   i,  0);
						fprintf(f, "%i\t%i\t%#x\n", SCI_STYLESETBOLD, i, val);
						val = s->SendScintilla(QsciScintillaBase::SCI_STYLEGETITALIC, i,  0);
						fprintf(f, "%i\t%i\t%#x\n", SCI_STYLESETITALIC, i, val);
						val = s->SendScintilla(QsciScintillaBase::SCI_STYLEGETSIZE,   i,  0);
						fprintf(f, "%i\t%i\t%#x\n", SCI_STYLESETSIZE, i, val);
						val = s->SendScintilla(QsciScintillaBase::SCI_STYLEGETFONT,   i,  (LPARAM)buf);
						fprintf(f, "%i\t%i\t\"%s\"\n", SCI_STYLESETFONT, i, buf);
						val = s->SendScintilla(QsciScintillaBase::SCI_STYLEGETUNDERLINE,  i,  0);
						fprintf(f, "%i\t%i\t%#x\n", SCI_STYLESETUNDERLINE, i, val);
						val = s->SendScintilla(QsciScintillaBase::SCI_STYLEGETCASE,   i,  0);
						fprintf(f, "%i\t%i\t%#x\n", SCI_STYLESETCASE, i, val);
					}
					fclose(f);
				}
			}
			else
				fclose(f);
		}
*/	}

	bool CreateDocument(document_s *ed)
	{
		size_t flensz;
		auto rawfile = QCC_ReadFile(ed->fname, nullptr, nullptr, &flensz);
		if (!rawfile)
			return false;
		auto flen = flensz;
		pbool dofree;
		auto file = QCC_SanitizeCharSet(static_cast<char*>(rawfile), &flen, &dofree, &ed->savefmt);
		struct stat sbuf;
		QCC_StatFile(ed->fname, &sbuf);
		ed->filemodifiedtime = sbuf.st_mtime;

		int endings = 0;
		char *e, *stop;
		for (e = file, stop=file+flen; e < stop; )
		{
			if (*e == '\r')
			{
				e++;
				if (*e == '\n')
				{
					e++;
					endings |= 4;
				}
				else
					endings |= 2;
			}
			else if (*e == '\n')
			{
				e++;
				endings |= 1;
			}
			else
				e++;
		}

		curdoc = ed;
		s->setDocument(ed->doc);

		switch(endings)
		{
		case 0: //new file with no endings, default to windows on windows.
		case 4: //windows
			s->setEolMode(QsciScintilla::EolMode::EolWindows);
			s->setEolVisibility(false);
			break;
		case 1: //unix
			s->setEolMode(QsciScintilla::EolMode::EolUnix);
			s->setEolVisibility(false);
			break;
		case 2: //mac. traditionally qccs have never supported this. one of the mission packs has a \r in the middle of some single-line comment.
			s->setEolMode(QsciScintilla::EolMode::EolMac);
			s->setEolVisibility(false);
			break;
		default:    //panic! everyone panic!
			s->setEolMode(QsciScintilla::EolMode::EolUnix);
			s->setEolVisibility(true);
			break;
		}

		s->setUtf8(ed->savefmt != UTF_ANSI);
		s->setText(QString(file));

		SetupScintilla(ed);

		s->SendScintilla(QsciScintillaBase::SCI_SETSAVEPOINT);
		ed->modified = false;
		return true;
	}

	void SwitchToDocument(document_s *ed)
	{
		struct stat sbuf;
		QCC_StatFile(ed->fname, &sbuf);
		if (ed->filemodifiedtime < sbuf.st_mtime)
		{
			CreateDocument(ed);
			return;
		}

		curdoc = ed;
		s->setDocument(ed->doc);
	}

	document_s *FindFile(const char *filename)
	{
		for (int i = 0; i < numdocuments; i++)
		{
			if (!strcasecmp(filename, docs[i]->fname))
				return docs[i];
		}
		return nullptr;
	}

	void *getFileData(document_s *d, unsigned char *(*buf_get)(void *ctx, size_t len), void *buf_ctx, size_t *out_size)
	{
		docstacklock lock(this, d);
		unsigned char *ret = NULL;
		auto text = s->text().toUtf8();
		*out_size = text.length();
		if (buf_get)
			ret = buf_get(buf_ctx, *out_size+1);
		else
			ret = static_cast<unsigned char*>(malloc(*out_size+1));
		memcpy(ret, text.data(), *out_size);
		ret[*out_size] = 0;
		return ret;
	}
	int getFileSize(document_s *d)
	{
		docstacklock lock(this,d);
		int ret = -1;
		auto text = s->text().toUtf8();
		ret = text.length();
		return ret;
	}


	bool saveDocument(document_s *d)
	{
		struct stat sbuf;
		bool saved = false;

		//wordpad will corrupt any embedded quake chars if we force a bom, because it'll re-save using the wrong char encoding by default.
		int bomlen = 0;
		const char *bom = "";
		if (!d)
			d = curdoc;
		if (!d)
			return false;

		if (d->savefmt == UTF32BE || d->savefmt == UTF32LE || d->savefmt == UTF16BE)
			d->savefmt = UTF16LE;

		if (d->savefmt == UTF8_BOM)
		{
			bomlen = 3;
			bom = "\xEF\xBB\xBF";
		}
		else if (d->savefmt == UTF16BE)
		{
			bomlen = 2;
			bom = "\xFE\xFF";
		}
		else if (d->savefmt == UTF16LE)
		{
			bomlen = 2;
			bom = "\xFF\xFE";
		}
		else if (d->savefmt == UTF32BE)
		{
			bomlen = 4;
			bom = "\x00\x00\xFE\xFF";
		}
		else if (d->savefmt == UTF32LE)
		{
			bomlen = 4;
			bom = "\xFF\xFE\x00\x00";
		}

		docstacklock lock(this, d);

		auto text = s->text().toUtf8();
		text.prepend(bom, bomlen);

		//because wordpad saves in ansi by default instead of the format the file was originally saved in, we HAVE to use ansi without
		if (d->savefmt != UTF8_BOM && d->savefmt != UTF8_RAW)
		{
			/*int mchars;
			char *mc;
			int wchars = MultiByteToWideChar(CP_UTF8, 0, text.data(), text.length(), NULL, 0);
			if (wchars)
			{
				wchar_t *wc = malloc(wchars * sizeof(wchar_t));
				MultiByteToWideChar(CP_UTF8, 0, text.data(), text.length(), wc, wchars);

				if (d->savefmt == UTF_ANSI)
				{
					mchars = WideCharToMultiByte(CP_ACP, 0, wc, wchars, NULL, 0, "", &failed);
					if (mchars)
					{
						mc = malloc(mchars);
						WideCharToMultiByte(CP_ACP, 0, wc, wchars, mc, mchars, "", &failed);
						if (!failed)
						{
							saved = QCC_WriteFile(d->filename, mc, mchars))
						}
						free(mc);
					}
				}
				else
					saved = QCC_WriteFile(d->filename, wc, wchars);
				free(wc);
			}*/
		}
		else
			saved = QCC_WriteFile(d->fname, text.data(), bomlen+text.length());
		if (!saved)
		{
			QMessageBox::critical(NULL, "Failure", "Save failed\nCheck path and ReadOnly flags");
			return false;
		}
		else
		{
			s->SendScintilla(QsciScintillaBase::SCI_SETSAVEPOINT);

			/*now whatever is on disk should have the current time*/
			//d->modified = false;
			QCC_StatFile(d->fname, &sbuf);
			d->filemodifiedtime = sbuf.st_mtime;

			//remove the * in a silly way.
			//d->oldline=~0;
			//UpdateEditorTitle(d);
		}

		return true;
	}

	bool saveAll(void)
	{
		document_s *d;
		struct stat sbuf;
		for (int i = 0; i < numdocuments; i++)
		{
			d = docs[i];
			QCC_StatFile(d->fname, &sbuf);
			if (d->modified)
			{
				if (d->filemodifiedtime != sbuf.st_mtime)
				{
					switch(QMessageBox::question(nullptr, "Modification conflict", QString::asprintf("%s is modified in both memory and on disk. Overwrite external modification? (saying no will reload from disk)", d->fname), QMessageBox::Save|QMessageBox::Reset|QMessageBox::Ignore, QMessageBox::Ignore))
					{
					case QMessageBox::Save:
						if (!saveDocument(d))
							QMessageBox::critical(nullptr, "Error", QString::asprintf("Unable to write %s, file was not saved", d->fname));
						break;
					case QMessageBox::Reset:
						CreateDocument(d);
						break;
					default:
					case QMessageBox::Ignore:
						break; /*compiling will use whatever is in memory*/
					}
				}
				else
				{
					/*not modified on disk, but modified in memory? try and save it, cos we might as well*/
					if (!saveDocument(d))
						QMessageBox::critical(nullptr, "Error", QString::asprintf("Unable to write %s, file was not saved", d->fname));
				}
			}
			else
			{
				/*modified on disk but not in memory? just reload it off disk*/
				if (d->filemodifiedtime != sbuf.st_mtime)
					CreateDocument(d);
			}
		}
		return true;
	}

	void reapplyAllBreakpoints(void)
	{
		for (int i = 0; i < numdocuments; i++)
		{
			document_s *d = docs[i];
			int line = -1;
			s->setDocument(d->doc);
			for (;;)
            {
                line = s->SendScintilla(QsciScintillaBase::SCI_MARKERNEXT, line, 1);
                if (line == -1)
                    break;  //no more.
                line++;

				DebuggerSendCommand("qcbreakpoint 1 \"%s\" %i\n", d->fname, line);
            }
		}
		if (curdoc)
			s->setDocument(curdoc->doc);
	}
	void toggleBreak(void)
	{
		if (!curdoc)
			return;
		int mode = !(s->SendScintilla(QsciScintillaBase::SCI_MARKERGET, curdoc->cursorline-1) & 1);
		s->SendScintilla(mode?QsciScintillaBase::SCI_MARKERADD:QsciScintillaBase::SCI_MARKERDELETE, curdoc->cursorline-1);

		DebuggerSendCommand("qcbreakpoint %i \"%s\" %i\n", mode, curdoc->fname, curdoc->cursorline);
	}
	void setNext(void)
	{
		if (!curdoc)
			return;
		DebuggerSendCommand("qcjump \"%s\" %i\n", curdoc->fname, curdoc->cursorline);
	}

	void EditFile(const char *filename, int linenum=-1, bool setcontrol=false);
};

template<class T>
class keyEnterReceiver : public QObject
{
	void(*cb)(T &ctx);
	T ctx;
public:
	keyEnterReceiver(void(*cb_)(T &ctx), T &ctx_) : cb(cb_), ctx(T(ctx_))
	{
	}
protected:
	bool eventFilter(QObject* obj, QEvent* event)
	{
		if (event->type()==QEvent::KeyPress)
		{
			QKeyEvent* key = static_cast<QKeyEvent*>(event);
			if ( (key->key()==Qt::Key_Enter) || (key->key()==Qt::Key_Return) )
				cb(ctx);
			else
				return QObject::eventFilter(obj, event);
			return true;
		}
		else
			return QObject::eventFilter(obj, event);
		return false;
	}
};

class optionswindow : public QDialog
{
	QWidget *newlineedit(const char *initial, void(*changed)(const QString&))
	{
		auto w = new QLineEdit(initial);
		connect(w, &QLineEdit::textEdited, changed);
		return w;
	}
	void SetOptsEnabled(QGridLayout *layout, int lev = -1)
	{
		for(int i = 0; i < layout->count(); i++)
		{
			auto w = static_cast<QCheckBox*>(layout->itemAt(i)->widget());
			if (!w)
				break;
			auto id = w->property("optnum").toInt();
			if (lev >= 0 && lev <= 3)
				w->setCheckState((lev >= optimisations[id].optimisationlevel)?Qt::Checked:Qt::Unchecked);
			else if (lev == 4)
			{
				if (optimisations[id].flags & FLAG_KILLSDEBUGGERS)
					w->setCheckState(Qt::Unchecked);
			}
			else if (lev == 5)
				w->setCheckState((optimisations[id].flags&FLAG_ASDEFAULT)?Qt::Checked:Qt::Unchecked);
			w->setEnabled(fl_nondfltopts);
		}
	}

public:
	optionswindow()
	{
		setModal(true);

		auto toplayout = new QHBoxLayout;
		auto leftlayout = new QVBoxLayout;
		auto rightlayout = new QVBoxLayout;

		{
			auto layout = new QGridLayout;
			for (int i = 0,n=0; optimisations[i].fullname; i++)
			{
				if (optimisations[i].flags & FLAG_HIDDENINGUI)
					continue;
				auto w = new QCheckBox(optimisations[i].fullname);
				w->setProperty("optnum", i);
				w->setCheckState((optimisations[i].flags & FLAG_SETINGUI)?Qt::Checked:Qt::Unchecked);
				w->setEnabled(!fl_nondfltopts);
				connect(w, &QCheckBox::stateChanged, [=](int v){if (v)optimisations[i].flags |= FLAG_SETINGUI;else optimisations[i].flags &= ~FLAG_SETINGUI;});
				layout->addWidget(w, n>>1, n&1);
				n++;
			}
			SetOptsEnabled(layout, -1);
			auto gb = new QGroupBox("Optimisations");
			auto opts = new QVBoxLayout;
			opts->addLayout(layout);
			auto optlevels = new QHBoxLayout;
			for (int i=0; i<6; i++)
			{
				const char *buttons[] = {"O0", "O1", "O2", "O3", "Debug", "Default"};
				auto w = new QPushButton(buttons[i]);
				if (i == 5)
				{
					w->setCheckable(true);
					w->setChecked(fl_nondfltopts);
				}
				connect(w, &QPushButton::clicked, [=](bool checked)
				{
					if (i == 5)
						fl_nondfltopts = !checked;
					else
						fl_nondfltopts = true;
					SetOptsEnabled(layout, i);
				});
				optlevels->addWidget(w);
			}
			opts->addLayout(optlevels);
			gb->setLayout(opts);
			leftlayout->addWidget(gb);
		}
		{
			auto layout = new QFormLayout;
			layout->addRow("Engine:", newlineedit(enginebinary, [](const QString&str){QC_strlcpy(enginebinary, str.toUtf8().data(), sizeof(enginebinary));}));
			layout->addRow("Basedir:", newlineedit(enginebasedir, [](const QString&str){QC_strlcpy(enginebasedir, str.toUtf8().data(), sizeof(enginebasedir));}));
			layout->addRow("Cmdline:", newlineedit(enginecommandline, [](const QString&str){QC_strlcpy(enginecommandline, str.toUtf8().data(), sizeof(enginecommandline));}));
			leftlayout->addLayout(layout);
		}

		{
			auto layout = new QGridLayout;
			int n = 0;

			auto w = new QCheckBox("HexenC");
			w->setStatusTip("Compile for hexen2.\nThis changes the opcodes slightly, the progs crc, and enables some additional keywords.");
			w->setCheckState((fl_hexen2)?Qt::Checked:Qt::Unchecked);
			connect(w, &QCheckBox::stateChanged, [=](int v){fl_hexen2=!!v;});
			layout->addWidget(w, n/3, n%3);
			n++;

			w = new QCheckBox("Extended Instructions");
			w->setStatusTip("Enables the use of additional opcodes, which only FTE supports at this time.\nThis gives both smaller and faster code, as well as allowing pointers, ints, and other extensions not possible with the vanilla QCVM");
			w->setCheckState((fl_ftetarg)?Qt::Checked:Qt::Unchecked);
			connect(w, &QCheckBox::stateChanged, [=](int v){fl_ftetarg=!!v;});
			layout->addWidget(w, n/3, n%3);
			n++;

			for (int i = 0; compiler_flag[i].fullname; i++)
			{
				if (compiler_flag[i].flags & FLAG_HIDDENINGUI)
					continue;
				auto w = new QCheckBox(compiler_flag[i].fullname);
				w->setCheckState((compiler_flag[i].flags & FLAG_SETINGUI)?Qt::Checked:Qt::Unchecked);
				connect(w, &QCheckBox::stateChanged, [=](int v){if (v)compiler_flag[i].flags |= FLAG_SETINGUI;else compiler_flag[i].flags &= ~FLAG_SETINGUI;});
				layout->addWidget(w, n/3, n%3);
				n++;
			}

			auto gb = new QGroupBox("Compiler Flags");
			gb->setLayout(layout);
			rightlayout->addWidget(gb);

			rightlayout->addWidget(new QTextEdit());
		}
		toplayout->addLayout(leftlayout);
		toplayout->addLayout(rightlayout);

		setLayout(toplayout);
	}
};

static class guimainwindow : public QMainWindow
{
public:
	QsciScintilla s;
	QSplitter leftrightsplit;
	QSplitter logsplit;
	QSplitter leftsplit;
	QTreeView files_w;
	QListView docs_w;

	QTextEdit log;
	filelist files;
	documentlist docs;

private:
	void CreateMenus(void)
	{
		auto prefs = new QAction(tr("&Preferences"), this);
		prefs->setShortcuts(QKeySequence::Preferences);
		prefs->setStatusTip(tr("Reconfigure stuff"));
		connect(prefs, &QAction::triggered, [](){(new optionswindow())->show();});

		auto grep = new QAction(tr("&Grep"), this);
		grep->setShortcuts(QKeySequence::listFromString("Ctrl+G"));
		grep->setStatusTip(tr("Search through all project files"));
		connect(grep, &QAction::triggered, [=]()
			{
				struct grepargs_s
				{
					guimainwindow *mw;
					QDialog *d;
					QLineEdit *t;
				} args = {this, new QDialog()};
				args.t = new QLineEdit(QString(""));
				auto l = new QVBoxLayout;
				l->addWidget(args.t);
				args.d->setLayout(l);
				args.d->setWindowTitle("FTEQCC Grep");
				args.t->installEventFilter(new keyEnterReceiver<grepargs_s>([](grepargs_s &ctx)
					{
						ctx.mw->files.GrepAll(ctx.t->text().toUtf8().data());
						ctx.d->done(0);
					}, args));
				args.d->show();
			});

		auto fileMenu = menuBar()->addMenu(tr("&File"));
		fileMenu->addAction(prefs);
		fileMenu->addAction(grep);
	}

public:
	~guimainwindow()
	{	//if we're dying, make sure there's no engine waiting for us
		DebuggerStop();
	}
	guimainwindow() : s(this), leftrightsplit(Qt::Horizontal, this), logsplit(Qt::Vertical, this), leftsplit(Qt::Vertical, this), files_w(this), docs_w(this), docs(&s)
	{
		setWindowTitle(QString("FTEQCC Gui"));

		s.setReadOnly(true);

		files_w.setModel(&files);
		connect(&files_w, &QTreeView::clicked,
			[=](const QModelIndex &index)
			{
				docs.EditFile(files.getItem(index)->name);
			});

		leftrightsplit.addWidget(&leftsplit);
		leftrightsplit.addWidget(&logsplit);
		leftsplit.addWidget(&files_w);
		leftsplit.addWidget(&docs_w);
		docs_w.setModel(&docs);
		logsplit.addWidget(&s);
		logsplit.addWidget(&log);
		QList<int> sizes;
		sizes.append(64);
		sizes.append(256);
		leftrightsplit.setSizes(sizes);
		QList<int> sizes2;
		sizes.append(1);
		sizes.append(0);
		logsplit.setSizes(sizes2);
		setCentralWidget(&leftrightsplit);

		log.setReadOnly(true);
		log.clear();

		connect(&log, &QTextEdit::selectionChanged,
			[=]()
			{
				auto foo = log.textCursor();
				foo.select(QTextCursor::LineUnderCursor);
				auto txt = foo.selectedText();
				auto colon = txt.indexOf(':');
				if (colon>0)
				{
					auto colon2 = txt.indexOf(':', colon+1);
					if (colon2>0)
					{
						auto line = txt.mid(colon+1, colon2-colon-1).toInt();
						EditFile(txt.mid(0, colon).toUtf8().data(), line, true);
					}
					else
						EditFile(txt.mid(0, colon).toUtf8().data(), -1, true);
				}
			});

		//editor UI things
		connect(new QShortcut(QKeySequence(tr("Ctrl+O", "File|Open")), this), &QShortcut::activated,
			[=]()
			{
				GUIprintf("Ctrl+O hit\n");
			});
		connect(new QShortcut(QKeySequence(tr("Ctrl+S", "File|Save")), this), &QShortcut::activated,
			[=]()
			{
				GUIprintf("Ctrl+S hit\n");
			});

		//compiler things
		connect(new QShortcut(QKeySequence(tr("F7", "File|Compile")), this), &QShortcut::activated,
			[=]()
			{
				RunCompiler("", false);
			});

		//debug things.
		connect(new QShortcut(QKeySequence(tr("F5", "File|Debug")), this), &QShortcut::activated,
			[=]()
			{
				if (!DebuggerSendCommand("qcresume\n"))
					DebuggerStart();	//unable to send? assume its not running.
			});
		connect(new QShortcut(QKeySequence(tr("F10", "File|DebugOver")), this), &QShortcut::activated,
			[=]()
			{
				if (!DebuggerSendCommand("qcstep over\n"))
					DebuggerStart();
			});
		connect(new QShortcut(QKeySequence(tr("F11", "File|DebugInto")), this), &QShortcut::activated,
			[=]()
			{
				if (!DebuggerSendCommand("qcstep into\n"))
					DebuggerStart();
			});
		connect(new QShortcut(QKeySequence(tr("Shift+F1", "File|DebugOut")), this), &QShortcut::activated,
			[=]()
			{
				if (!DebuggerSendCommand("qcstep out\n"))
					DebuggerStart();
			});
		connect(new QShortcut(QKeySequence(tr("F9", "File|DebugToggle")), this), &QShortcut::activated,
			[=]()
			{
				docs.toggleBreak();
			});

		CreateMenus();
	}
} *mainwnd;

//called when progssrcname has changed.
//progssrcname should already have been set.
void UpdateFileList(void)
{
	char *buffer;

	AddSourceFile(nullptr, progssrcname);

	size_t size;
	buffer = static_cast<char*>(QCC_ReadFile(progssrcname, nullptr, nullptr, &size));

	pr_file_p = QCC_COM_Parse(buffer);
	if (*qcc_token == '#')
	{
		//aaaahhh! newstyle!
	}
	else
	{
		pr_file_p = QCC_COM_Parse(pr_file_p);   //we dont care about the produced progs.dat
		while(pr_file_p)
		{
			if (*qcc_token == '#')  //panic if there's preprocessor in there.
				break;

			AddSourceFile(progssrcname, qcc_token);
			pr_file_p = QCC_COM_Parse(pr_file_p);   //we dont care about the produced progs.dat
		}
	}
	free(buffer);

	//handle any #includes in there
	RunCompiler(parameters, true);

	//expand everything, so the user doesn't get annoyed.
	mainwnd->files_w.expandAll();
}
void AddSourceFile(const char *parentpath, const char *filename)
{
	mainwnd->files.AddFile(parentpath, filename);
}

static int Dummyprintf(const char *msg, ...){return 0;}

void RunCompiler(const char *args, pbool quick)
{
	static FILE *logfile;
	const char *argv[128];
	int argc;
	progexterns_t ext;
	progfuncs_t funcs;

	mainwnd->docs.saveAll();

	memset(&funcs, 0, sizeof(funcs));
	funcs.funcs.parms = &ext;
	memset(&ext, 0, sizeof(ext));
	ext.ReadFile = GUIReadFile;
	ext.FileSize = GUIFileSize;
	ext.WriteFile = QCC_WriteFile;
	ext.Sys_Error = Sys_Error;

	if (quick)
		ext.Printf = Dummyprintf;
	else
	{
		ext.Printf = GUIprintf;
		GUIprintf("");
	}
	ext.DPrintf = ext.Printf;

	if (logfile)
		fclose(logfile);
	if (fl_log && !quick)
		logfile = fopen("fteqcc.log", "wb");
	else
		logfile = NULL;

	argc = GUI_BuildParms(args, argv, quick);

	if (CompileParams(&funcs, NULL, argc, argv))
	{
		if (!quick)
		{
			//DebuggerGiveFocus();
			DebuggerSendCommand("qcresume\nqcreload\n");
		}
	}

	if (logfile)
	{
		fclose(logfile);
		logfile = NULL;
	}
}

void GUI_DoDecompile(void *buf, size_t size)
{
	const char *c = ReadProgsCopyright((char*)buf, size);
	if (!c || !*c)
		c = "COPYRIGHT OWNER NOT KNOWN";	//all work is AUTOMATICALLY copyrighted under the terms of the Berne Convention in all major nations. It _IS_ copyrighted, even if there's no license etc included. Good luck guessing what rights you have.
	if (QMessageBox::Open == QMessageBox::question(mainwnd, "Copyright", QString::asprintf("The copyright message from this progs is\n%s\n\nPlease respect the wishes and legal rights of the person who created this.", c), QMessageBox::Open|QMessageBox::Cancel, QMessageBox::Cancel))
	{
		GUIprintf("");

		DecompileProgsDat(progssrcname, buf, size);

//		QCC_SaveVFiles();
	}
}

static void QCC_EnumerateFilesResult(const char *name, const void *compdata, size_t compsize, int method, size_t plainsize)
{
	auto buffer = new char[plainsize];
	if (QC_decode(nullptr, compsize, plainsize, method, compdata, buffer))
		QCC_AddVFile(name, buffer, plainsize);

	delete [] buffer;
}

static void SetMainSrcFile(const char *src)
{
	//if its a path, chdir to it instead
	const char *sl = strrchr(src, '/');
	if (sl)
	{
		sl++;
		auto gah = static_cast<char*>(malloc(sl-src+1));
		memcpy(gah, src, sl-src);
		gah[sl-src] = 0;
		chdir(gah);
		free(gah);
		src = sl;
	}

	strcpy(progssrcname, src);

	QCC_CloseAllVFiles();

	GUI_SetDefaultOpts();
	GUI_ParseCommandLine(cmdlineargs, true);
	GUI_RevealOptions();

	//if the project is a .dat or .zip then decompile it now (so we can access the 'source')
	{
		char *ext = strrchr(progssrcname, '.');
		if (ext && (!QC_strcasecmp(ext, ".dat") || !QC_strcasecmp(ext, ".pak") || !QC_strcasecmp(ext, ".zip") || !QC_strcasecmp(ext, ".pk3")))
		{
			FILE *f = fopen(progssrcname, "rb");
			if (f)
			{
				size_t size;

				fseek(f, 0, SEEK_END);
				size = ftell(f);
				fseek(f, 0, SEEK_SET);
				auto buf = new char[size];
				fread(buf, 1, size, f);
				fclose(f);
				if (!QC_EnumerateFilesFromBlob(buf, size, QCC_EnumerateFilesResult) && !QC_strcasecmp(ext, ".dat"))
				{   //its a .dat and contains no .src files
					GUI_DoDecompile(buf, size);
				}
				else if (!QCC_FindVFile("progs.src"))
				{
					vfile_t *f;
					char *archivename = progssrcname;
					while(strchr(archivename, '\\'))
						 archivename = strchr(archivename, '\\')+1;
					AddSourceFile(NULL, archivename);
//					for (f = qcc_vfiles; f; f = f->next)
//						AddSourceFile(archivename,  f->filename);

					f = QCC_FindVFile("progs.dat");
					if (f)
						GUI_DoDecompile(f->file, f->size);
				}
				delete [] buf;
				strcpy(progssrcname, "progs.src");
			}
			else
				strcpy(progssrcname, "progs.src");

			for (int i = 0; ; i++)
			{
				if (!strcmp("embedsrc", compiler_flag[i].abbrev))
				{
					compiler_flag[i].flags |= FLAG_SETINGUI;
					break;
				}
			}
		}
	}

	//then populate the file list.
	UpdateFileList();
	EditFile(progssrcname, -1, false);
}

int main(int argc, char* argv[])
{
	//handle initial commandline args
	GUI_SetDefaultOpts();
	{
		size_t argl = 1;
		for (int i = 1; i < argc; i++)
			argl += strlen(argv[i])+1;
		cmdlineargs = (char*)malloc(argl);
		argl = 0;
		for (int i = 1; i < argc; i++)
		{
			memcpy(cmdlineargs+argl, argv[i], strlen(argv[i]));
			argl += strlen(argv[i]);
			cmdlineargs[argl++] = ' ';
		}
		cmdlineargs[argl] = 0;
		int mode = GUI_ParseCommandLine(cmdlineargs, false);
		if (mode == 1)
		{	//compile-only
			RunCompiler(cmdlineargs, false);
			return EXIT_SUCCESS;
		}
	}

	//start up the gui parts
	QCoreApplication *app = new QApplication(argc, argv);
	mainwnd = new guimainwindow();
	mainwnd->show();
	GUIprintf("Welcome to FTEQCC!\n(QT edition)\n");
#ifdef SVNREVISION
	if (strcmp(STRINGIFY(SVNREVISION), "-"))
		GUIprintf("FTE SVN Revision: %s\n",STRINGIFY(SVNREVISION));
#endif

	//and now set up our project...
	if (*progssrcname)
		SetMainSrcFile(progssrcname);
	else
		SetMainSrcFile("progs.src");

	//done, run the gui's main loop.
	return app->exec();
}
void compilecb(void)
{
}

int GUIprintf(const char *msg, ...)
{
	static QString l;
	va_list va;

	if (!*msg)
	{	//starting a compile or something.
		//clear the text and make sure the log is visible.
		mainwnd->log.setText(QString(""));
		if (!mainwnd->logsplit.sizes()[1])
		{	//force it visible
			QList<int> sizes;
			sizes.append(2);
			sizes.append(1);
			mainwnd->logsplit.setSizes(sizes);
		}
		return 0;
	}

	va_start(va, msg);
	l.append(QString::vasprintf(msg, va));
	va_end(va);
	for (;;)
	{
		auto idx = l.indexOf('\n');
		if (idx >= 0)
		{
			QString s = l.mid(0, idx);
			l = l.mid(idx+1);

			if (s.contains(": error") || s.contains(": werror") || !s.mid(0,5).compare("error", Qt::CaseInsensitive))
				mainwnd->log.setTextColor(QColor(255, 0, 0));
			else if (s.contains(": warning"))
				mainwnd->log.setTextColor(QColor(128, 128, 0));
			else
				mainwnd->log.setTextColor(QColor(0, 0, 0));
			mainwnd->log.append(s);
		}
		else
			break;
	}
	return 0;
}
void documentlist::UpdateTitle(void)
{
	if (curdoc)
		mainwnd->setWindowTitle(QString::asprintf("%s%s:%i", curdoc->fname, curdoc->modified?"*":"", curdoc->cursorline));
	else
		mainwnd->setWindowTitle("FTEQCC");
}
void documentlist::EditFile(const char *filename, int linenum, bool setcontrol)
{
	if (setcontrol)
	{
		for (int i = 0; i < numdocuments; i++)
		{
			docstacklock lock(this, docs[i]);
			s->SendScintilla(QsciScintillaBase::SCI_MARKERDELETEALL, 1);
			s->SendScintilla(QsciScintillaBase::SCI_MARKERDELETEALL, 2);
		}
	}

	auto c = FindFile(filename);
	if (!c)
	{
		c = new document_s();
		c->fname = strdup(filename);

		docs = cpprealloc(docs, sizeof(*docs)*(numdocuments+1));

		if (!CreateDocument(c))
		{
			delete(c);
			numdocuments--;
			return;
		}

		beginInsertRows(QModelIndex(), numdocuments, numdocuments);
		docs[numdocuments] = c;
		numdocuments++;
		endInsertRows();
	}
	else
	{
		SwitchToDocument(c);
	}

	UpdateTitle();

	if (linenum >= 1)
	{
		linenum--;	//scintilla is 0-based, apparently.
		s->ensureLineVisible(max(1, linenum - 3));
		s->ensureLineVisible(linenum + 3);
		s->setCursorPosition(linenum, 0);
		s->ensureCursorVisible();
		s->setFocus();

		if (setcontrol)
		{
			s->SendScintilla(QsciScintillaBase::SCI_MARKERADD, linenum, 1);
			s->SendScintilla(QsciScintillaBase::SCI_MARKERADD, linenum, 2);
		}
	}
}
void EditFile(const char *name, int line, pbool setcontrol)
{
	mainwnd->docs.EditFile(name, line, setcontrol);
}
void GUI_DialogPrint(const char *title, const char *text)
{
	QMessageBox::information(mainwnd, title, text);
}
void *GUIReadFile(const char *fname, unsigned char *(*buf_get)(void *ctx, size_t len), void *buf_ctx, size_t *out_size, pbool issourcefile)
{
	auto d = mainwnd->docs.FindFile(fname);
	if (d)
		return mainwnd->docs.getFileData(d, buf_get, buf_ctx, out_size);

	if (issourcefile)
		AddSourceFile(compilingrootfile,    fname);

	return QCC_ReadFile(fname, buf_get, buf_ctx, out_size);
}
int GUIFileSize(const char *fname)
{
	auto d = mainwnd->docs.FindFile(fname);
	if (d)
		return mainwnd->docs.getFileSize(d);

	return QCC_PopFileSize(fname);
}






static QProcess *qcdebugger;
static void DebuggerStop(void)
{
	if (qcdebugger)
	{
		//GUIprintf("Detatching from debuggee\n");
		qcdebugger->closeWriteChannel();
		qcdebugger->closeReadChannel(QProcess::StandardOutput);
		qcdebugger->closeReadChannel(QProcess::StandardError);
		qcdebugger->waitForFinished();
		delete qcdebugger;
		qcdebugger = NULL;
	}
}
static bool DebuggerSendCommand(const char *msg, ...)
{
	va_list va;
	//qcresume
	//qcstep out|over|into
	//qcjump "file" line
	//debuggerwnd windowid
	//qcinspect "variable"
	//qcreload
	//qcbreakpoint off=0|on=1|toggle=2 "file" line

	if (!qcdebugger || qcdebugger->state() == QProcess::NotRunning)
		return false;	//not running, can't send.

	va_start(va, msg);
	qcdebugger->write(QString::vasprintf(msg, va).toUtf8().data());
	va_end(va);
	return true;
}
static void DebuggerStart(void)
{
	DebuggerStop();
	qcdebugger = new QProcess(mainwnd);
	qcdebugger->setProgram(enginebinary);
	qcdebugger->setWorkingDirectory(enginebasedir);
	qcdebugger->setArguments(QStringList(enginecommandline));

	QObject::connect(qcdebugger, static_cast<void(QProcess::*)(int)>(&QProcess::finished),
		[=](int exitcode)
		{
//			GUIprintf("Debuggee finished\n");
//			DebuggerStop();	//can't kill it here, there's still code running inside it
			mainwnd->activateWindow();	//try and grab the user's attention
		});
	QObject::connect(qcdebugger, &QProcess::readyReadStandardOutput,
		[=]()
		{
			while(qcdebugger->canReadLine())
			{
				auto l = qcdebugger->readLine();
				const char *line = l.data();
				if (!strncmp(line, "qcstep ", 7) || !strncmp(line, "qcfault ", 8))
				{	//engine hit a breakpoint or some such.
					//file, linenum, message
					line = QCC_COM_Parse (line+7);
					QString s(qcc_token);
					if (*line == ':')
						line++;	//grr
					line = QCC_COM_Parse (line);
					EditFile(s.toUtf8().data(), atoi(qcc_token), true);
					line = QCC_COM_Parse (line);
					mainwnd->activateWindow();
					if (*qcc_token)
						QMessageBox::critical(mainwnd, "Debugger Fault", qcc_token);
				}
				else if (!strncmp(line, "qcreloaded ", 10))
				{	//vmname, progsname
					mainwnd->docs.reapplyAllBreakpoints(); //send all breakpoints...
					DebuggerSendCommand("qcresume\n");	//and let it run
				}
				//status
				//curserver
				//qcstack
				//qcvalue
				//refocuswindow
				else
					GUIprintf(line);
			}
		});

	qcdebugger->start(QProcess::ReadWrite|QProcess::Unbuffered);
	qcdebugger->waitForStarted();
	switch (qcdebugger->state())
	{
	case QProcess::NotRunning:
		GUIprintf("Child process not running\n");
		break;
	case QProcess::Starting:
		GUIprintf("Child starting up\n");	//still forking?
		break;
	case QProcess::Running:
//		GUIprintf("Child is running\n");
		break;
	}
}