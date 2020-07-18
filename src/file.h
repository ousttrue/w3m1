#pragma once
#include "parsetagx.h"
#include "form.h"

typedef struct
{
    int pos;
    int len;
    int tlen;
    long flag;
    Anchor anchor;
    Str img_alt;
    char fontstat[FONTSTAT_SIZE];
    short nobr_level;
    Lineprop prev_ctype;
    char init_flag;
    short top_margin;
    short bottom_margin;
} Breakpoint;

struct readbuffer
{
    Str line;
    Lineprop cprop;
    short pos;
    Str prevchar;
    long flag;
    long flag_stack[RB_STACK_SIZE];
    int flag_sp;
    int status;
    unsigned char end_tag;
    short table_level;
    short nobr_level;
    Anchor anchor;
    Str img_alt;
    char fontstat[FONTSTAT_SIZE];
    char fontstat_stack[FONT_STACK_SIZE][FONTSTAT_SIZE];
    int fontstat_sp;
    Lineprop prev_ctype;
    Breakpoint bp;
    struct cmdtable *tag_stack[TAG_STACK_SIZE];
    int tag_sp;
    short top_margin;
    short bottom_margin;
};

Str process_img(struct parsed_tag *tag, int width);
Str process_anchor(struct parsed_tag *tag, char *tagbuf);
Str process_input(struct parsed_tag *tag);
Str process_select(struct parsed_tag *tag);
Str process_textarea(struct parsed_tag *tag, int width);
Str process_form(struct parsed_tag *tag);
int HTMLtagproc1(struct parsed_tag *tag, struct html_feed_environ *h_env);
Buffer *loadGeneralFile(char *path, ParsedURL *current, char *referer, int flag, FormList *request);
Str getLinkNumberStr(int correction);
char *guess_save_name(Buffer *buf, char *file);
void examineFile(char *path, URLFile *uf);
char *acceptableEncoding();
int dir_exist(char *path);
int is_html_type(char *type);
Str convertLine(URLFile *uf, Str line, int mode, wc_ces *charset, wc_ces doc_charset);
Buffer *loadFile(char *path);
int is_boundary(unsigned char *, unsigned char *);
int is_blank_line(char *line, int indent);
void push_render_image(Str str, int width, int limit,
                       struct html_feed_environ *h_env);
void flushline(struct html_feed_environ *h_env, struct readbuffer *obuf,
               int indent, int force, int width);
void do_blankline(struct html_feed_environ *h_env,
                  struct readbuffer *obuf, int indent, int indent_incr,
                  int width);
void purgeline(struct html_feed_environ *h_env);
void save_fonteffect(struct html_feed_environ *h_env,
                     struct readbuffer *obuf);
void restore_fonteffect(struct html_feed_environ *h_env,
                        struct readbuffer *obuf);
Str process_n_select(void);
void feed_select(char *str);
void process_option(void);
Str process_n_textarea(void);
void feed_textarea(char *str);
Str process_n_form(void);
int getMetaRefreshParam(char *q, Str *refresh_uri);
void HTMLlineproc2(Buffer *buf, TextLineList *tl);
void HTMLlineproc0(char *istr, struct html_feed_environ *h_env,
                   int internal);
#define HTMLlineproc1(x, y) HTMLlineproc0(x, y, TRUE)
Buffer *loadHTMLBuffer(URLFile *f, Buffer *newBuf);
char *convert_size(clen_t size, int usefloat);
char *convert_size2(clen_t size1, clen_t size2, int usefloat);
void showProgress(clen_t *linelen, clen_t *trbyte);
void init_henv(struct html_feed_environ *, struct readbuffer *,
               struct environment *, int, TextLineList *, int, int);
void completeHTMLstream(struct html_feed_environ *,
                        struct readbuffer *);
void loadHTMLstream(URLFile *f, Buffer *newBuf, FILE *src,
                    int internal);
Buffer *loadHTMLString(Str page);

Buffer *loadBuffer(URLFile *uf, Buffer *newBuf);
Buffer *loadImageBuffer(URLFile *uf, Buffer *newBuf);
void saveBuffer(Buffer *buf, FILE *f, int cont);
void saveBufferBody(Buffer *buf, FILE *f, int cont);
Buffer *getshell(char *cmd);
Buffer *getpipe(char *cmd);
Buffer *openPagerBuffer(InputStream stream, Buffer *buf);
Buffer *openGeneralPagerBuffer(InputStream stream);
Line *getNextPage(Buffer *buf, int plen);
int save2tmp(URLFile uf, char *tmpf);
int doExternal(URLFile uf, char *path, char *type, Buffer **bufp,
                      Buffer *defaultbuf);
int _doFileCopy(char *tmpf, char *defstr, int download);
#define doFileCopy(tmpf, defstr) _doFileCopy(tmpf, defstr, FALSE);
int doFileMove(char *tmpf, char *defstr);
int doFileSave(URLFile uf, char *defstr);
int checkCopyFile(char *path1, char *path2);
int checkSaveFile(InputStream stream, char *path);
int checkOverWrite(char *path);
char *inputAnswer(char *prompt);
int matchattr(char *p, char *attr, int len, Str *value);
void readHeader(URLFile *uf, Buffer *newBuf, int thru, ParsedURL *pu);
char *checkHeader(Buffer *buf, char *field);
Buffer *newBuffer(int width);
Buffer *nullBuffer(void);
void clearBuffer(Buffer *buf);
void discardBuffer(Buffer *buf);
Buffer *namedBuffer(Buffer *first, char *name);
Buffer *deleteBuffer(Buffer *first, Buffer *delbuf);
Buffer *replaceBuffer(Buffer *first, Buffer *delbuf, Buffer *newbuf);
Buffer *nthBuffer(Buffer *firstbuf, int n);
void gotoRealLine(Buffer *buf, int n);
void gotoLine(Buffer *buf, int n);
Buffer *selectBuffer(Buffer *firstbuf, Buffer *currentbuf,
                            char *selectchar);
