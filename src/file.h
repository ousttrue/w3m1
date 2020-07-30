#pragma once

#include "frontend/buffer.h"
#include "frontend/event.h"
#include "transport/urlfile.h"

struct HRequest;
struct Hist;

char *guess_save_name(BufferPtr buf, std::string_view file);
void examineFile(char *path, URLFile *uf);

Str convertLine(URLFile *uf, Str line, int mode, CharacterEncodingScheme *charset, CharacterEncodingScheme doc_charset);
char *guess_filename(std::string_view file);
int is_boundary(unsigned char *, unsigned char *);
int is_blank_line(char *line, int indent);
Str process_n_select(void);
void feed_select(char *str);
void process_option(void);
Str process_n_textarea(void);
void feed_textarea(char *str);
Str process_n_form(void);
int getMetaRefreshParam(char *q, Str *refresh_uri);
BufferPtr loadHTMLBuffer(URLFile *f, BufferPtr newBuf);
char *convert_size(clen_t size, int usefloat);
char *convert_size2(clen_t size1, clen_t size2, int usefloat);
void showProgress(clen_t *linelen, clen_t *trbyte);
BufferPtr loadHTMLString(Str page);

BufferPtr loadImageBuffer(URLFile *uf, BufferPtr newBuf);
void saveBuffer(BufferPtr buf, FILE *f, int cont);
void saveBufferBody(BufferPtr buf, FILE *f, int cont);
BufferPtr getshell(char *cmd);
BufferPtr getpipe(char *cmd);
BufferPtr openPagerBuffer(InputStream *stream, BufferPtr buf);
BufferPtr openGeneralPagerBuffer(InputStream *stream);
Line *getNextPage(BufferPtr buf, int plen);
int doExternal(URLFile uf, char *path, const char *type, BufferPtr *bufp,
               BufferPtr defaultbuf);

int doFileMove(char *tmpf, char *defstr);
void addnewline(BufferPtr buf, char *line, Lineprop *prop,
                       Linecolor *color, int pos, int width, int nlines);

int checkSaveFile(InputStream *stream, char *path);
int checkOverWrite(char *path);
char *inputAnswer(const char* prompt);
int matchattr(char *p, const char *attr, int len, Str *value);

char *checkHeader(BufferPtr buf, char *field);


void fmTerm(void);
void fmInit(void);

void addChar(char c, Lineprop mode);

void addMChar(char *c, Lineprop mode, size_t len);

BufferPtr message_list_panel(void);


void cursorUp0(BufferPtr buf, int n);
void cursorUp(BufferPtr buf, int n);
void cursorDown0(BufferPtr buf, int n);
void cursorDown(BufferPtr buf, int n);
void cursorUpDown(BufferPtr buf, int n);
void cursorRight(BufferPtr buf, int n);
void cursorLeft(BufferPtr buf, int n);
void cursorHome(BufferPtr buf);
void arrangeCursor(BufferPtr buf);
void arrangeLine(BufferPtr buf);
void cursorXY(BufferPtr buf, int x, int y);
void restorePosition(BufferPtr buf, BufferPtr orig);
char *lastFileName(char *path);
char *mydirname(const char *s);

#ifdef USE_MIGEMO
void init_migemo(void);
#endif
char *conv_search_string(char *str, CharacterEncodingScheme f_ces);
int forwardSearch(BufferPtr buf, char *str);
int backwardSearch(BufferPtr buf, char *str);
void pcmap(void);
void escmap(void);
void escbmap(void);
void multimap(void);
Str unescape_spaces(Str s);
#ifdef USE_HISTORY
BufferPtr historyBuffer(Hist *hist);
void loadHistory(Hist *hist);
void saveHistory(Hist *hist, size_t size);
void ldHist(void);
#else /* not USE_HISTORY */
#define ldHist nulcmd
#endif /* not USE_HISTORY */
double log_like(int x);
struct table *newTable(void);
void pushdata(struct table *t, int row, int col, char *data);
int visible_length(char *str);
void print_item(struct table *t, int row, int col, int width, Str buf);
void print_sep(struct table *t, int row, int type, int maxcol, Str buf);
void do_refill(struct table *tbl, int row, int col, int maxlimit);
struct table *begin_table(int border, int spacing, int padding,
                          int vspace);
void end_table(struct table *tbl);
void check_rowcol(struct table *tbl, struct table_mode *mode);
int minimum_length(char *line);
int feed_table(struct table *tbl, char *line, struct table_mode *mode,
               int width, int internal);
void feed_table1(struct table *tbl, Str tok, struct table_mode *mode,
                 int width);
void pushTable(struct table *, struct table *);
char *form2str(FormItemList *fi);


void formResetBuffer(BufferPtr buf, AnchorList &formitem);

void preFormUpdateBuffer(BufferPtr buf);
Str textfieldrep(Str s, int width);
void input_textarea(FormItemList *fi);
void do_internal(char *action, char *data);
void form_write_data(FILE *f, char *boundary, char *name, char *value);
void form_write_from_file(FILE *f, char *boundary, char *name,
                          char *filename, char *file);
int getMapXY(BufferPtr buf, const Anchor *a, int *x, int *y);
BufferPtr page_info_panel(BufferPtr buf);
struct frame_body *newFrame(struct parsed_tag *tag, BufferPtr buf);
struct frameset *newFrameSet(struct parsed_tag *tag);
void deleteFrame(struct frame_body *b);
void deleteFrameSet(struct frameset *f);
struct frameset *copyFrameSet(struct frameset *of);
void pushFrameTree(struct frameset_queue **fqpp, struct frameset *fs,
                   BufferPtr buf);
struct frameset *popFrameTree(struct frameset_queue **fqpp);
BufferPtr renderFrame(BufferPtr Cbuf, int force_reload);
union frameset_element *search_frame(struct frameset *fset, char *name);

MySignalHandler reset_exit(SIGNAL_ARG);
MySignalHandler error_dump(SIGNAL_ARG);

void free_ssl_ctx();

int openSocket(const char *hostname, const char *remoteport_name,
               unsigned short remoteport_num);

char *acceptableMimeTypes();
int check_no_proxy(char *domain);
InputStream *openFTPStream(ParsedURL *pu, URLFile *uf);
Str loadFTPDir(ParsedURL *pu, CharacterEncodingScheme *charset);
void closeFTP(void);
void disconnectFTP(void);
InputStream *openNewsStream(ParsedURL *pu);
Str loadNewsgroup(ParsedURL *pu, CharacterEncodingScheme *charset);
void closeNews(void);
void disconnectNews(void);

BufferPtr link_list_panel(BufferPtr buf);

int set_param_option(const char *option);
char *get_param_option(char *name);
void init_rc(void);
BufferPtr load_option_panel(void);
void sync_with_option(void);

char *libFile(char *base);
char *helpFile(char *base);
Str localCookie(void);
Str loadLocalDir(char *dirname);

FILE *localcgi_post(char *, char *, FormList *, char *);
#define localcgi_get(u, q, r) localcgi_post((u), (q), NULL, (r))
FILE *openSecretFile(char *fname);

void loadPreForm(void);
char *last_modified(BufferPtr buf);

void myExec(char *command);
void mySystem(char *command, int background);
Str myExtCommand(char *cmd, char *arg, int redirect);
Str myEditor(char *cmd, char *file, int line);

Str tmpfname(int type, const char *ext);

int
is_plain_text_type(const char *type);
int
is_dump_text_type(const char *type);


void HTMLlineproc2(BufferPtr buf, TextLineList *tl);

void loadHTMLstream(URLFile *f, BufferPtr newBuf, FILE *src,
                    int internal);

void SetMetaCharset(CharacterEncodingScheme ces);
ParsedURL *GetCurBaseUrl();
int setModtime(char *path, time_t modtime);

BufferPtr
loadcmdout(char *cmd,
           BufferPtr (*loadproc)(URLFile *, BufferPtr), BufferPtr defaultbuf);
