#pragma once
#include "types.h"
#include "buffer.h"

struct HRequest;

Str process_img(struct parsed_tag *tag, int width);
Str process_anchor(struct parsed_tag *tag, char *tagbuf);
Str process_input(struct parsed_tag *tag);
Str process_select(struct parsed_tag *tag);
Str process_textarea(struct parsed_tag *tag, int width);
Str process_form(struct parsed_tag *tag);

BufferPtr loadGeneralFile(char *path, ParsedURL *current, char *referer, int flag, FormList *request);
Str getLinkNumberStr(int correction);
char *guess_save_name(BufferPtr buf, char *file);
void examineFile(char *path, URLFile *uf);
char *acceptableEncoding();
int dir_exist(char *path);
int is_html_type(const char *type);
Str convertLine(URLFile *uf, Str line, int mode, wc_ces *charset, wc_ces doc_charset);
BufferPtr loadFile(char *path);
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

BufferPtr loadBuffer(URLFile *uf, BufferPtr newBuf);
BufferPtr loadImageBuffer(URLFile *uf, BufferPtr newBuf);
void saveBuffer(BufferPtr buf, FILE *f, int cont);
void saveBufferBody(BufferPtr buf, FILE *f, int cont);
BufferPtr getshell(char *cmd);
BufferPtr getpipe(char *cmd);
BufferPtr openPagerBuffer(InputStream *stream, BufferPtr buf);
BufferPtr openGeneralPagerBuffer(InputStream *stream);
Line *getNextPage(BufferPtr buf, int plen);
int save2tmp(URLFile uf, char *tmpf);
int doExternal(URLFile uf, char *path, const char *type, BufferPtr *bufp,
               BufferPtr defaultbuf);
int _doFileCopy(char *tmpf, char *defstr, int download);
#define doFileCopy(tmpf, defstr) _doFileCopy(tmpf, defstr, FALSE);
int doFileMove(char *tmpf, char *defstr);
int doFileSave(URLFile uf, char *defstr);
int checkCopyFile(char *path1, char *path2);
int checkSaveFile(InputStream *stream, char *path);
int checkOverWrite(char *path);
char *inputAnswer(char *prompt);
int matchattr(char *p, const char *attr, int len, Str *value);
void readHeader(URLFile *uf, BufferPtr newBuf, int thru, ParsedURL *pu);
char *checkHeader(BufferPtr buf, char *field);
BufferPtr newBuffer(int width);

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
int columnSkip(BufferPtr buf, int offset);
int columnPos(Line *line, int column);
int columnLen(Line *line, int column);
Line *lineSkip(BufferPtr buf, Line *line, int offset, int last);
Line *currentLineSkip(BufferPtr buf, Line *line, int offset, int last);
int gethtmlcmd(char **s);

char *lastFileName(char *path);
char *mybasename(char *s);
char *mydirname(char *s);
int read_token(Str buf, char **instr, int *status, int pre, int append);
Str correct_irrtag(int status);
#ifdef USE_MIGEMO
void init_migemo(void);
#endif
char *conv_search_string(char *str, wc_ces f_ces);
int forwardSearch(BufferPtr buf, char *str);
int backwardSearch(BufferPtr buf, char *str);
void pcmap(void);
void escmap(void);
void escbmap(void);
void multimap(void);
char *inputLineHistSearch(char *prompt, char *def_str, int flag,
                          Hist *hist, int (*incfunc)(int ch, Str buf, Lineprop *prop));
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
FormList *newFormList(char *action, const char *method, char *charset,
                              char *enctype, char *target, char *name,
                              FormList *_next);
char *form2str(FormItemList *fi);
int formtype(char *typestr);

void formResetBuffer(BufferPtr buf, AnchorList &formitem);

void preFormUpdateBuffer(BufferPtr buf);
Str textfieldrep(Str s, int width);
void input_textarea(FormItemList *fi);
void do_internal(char *action, char *data);
void form_write_data(FILE *f, char *boundary, char *name, char *value);
void form_write_from_file(FILE *f, char *boundary, char *name,
                          char *filename, char *file);
MapList *searchMapList(BufferPtr buf, char *name);
#ifndef MENU_MAP
BufferPtr follow_map_panel(BufferPtr buf, char *name);
#endif
int getMapXY(BufferPtr buf, const Anchor *a, int *x, int *y);
MapArea *retrieveCurrentMapArea(BufferPtr buf);

MapArea *newMapArea(char *url, char *target, char *alt, char *shape,
                    char *coords);
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
ParsedURL *baseURL(BufferPtr buf);
int openSocket(const char *hostname, const char *remoteport_name,
               unsigned short remoteport_num);
void parseURL(char *url, ParsedURL *p_url, ParsedURL *current);
void copyParsedURL(ParsedURL *p, ParsedURL *q);




int mailcapMatch(struct mailcap *mcap, const char *type);
struct mailcap *searchMailcap(struct mailcap *table, char *type);
void initMailcap();
char *acceptableMimeTypes();
struct mailcap *searchExtViewer(const char *type);
Str unquote_mailcap(const char *qstr, const char *type, char *name, char *attr,
                    int *mc_stat);
int check_no_proxy(char *domain);
InputStream *openFTPStream(ParsedURL *pu, URLFile *uf);
Str loadFTPDir(ParsedURL *pu, wc_ces *charset);
void closeFTP(void);
void disconnectFTP(void);
InputStream *openNewsStream(ParsedURL *pu);
Str loadNewsgroup(ParsedURL *pu, wc_ces *charset);
void closeNews(void);
void disconnectNews(void);


BufferPtr link_list_panel(BufferPtr buf);

int set_param_option(const char *option);
char *get_param_option(char *name);
void init_rc(void);
BufferPtr load_option_panel(void);
void sync_with_option(void);

char *auxbinFile(const char *base);
char *libFile(char *base);
char *helpFile(char *base);
Str localCookie(void);
Str loadLocalDir(char *dirname);

FILE *localcgi_post(char *, char *, FormList *, char *);
#define localcgi_get(u, q, r) localcgi_post((u), (q), NULL, (r))
FILE *openSecretFile(char *fname);
void loadPasswd(void);
void loadPreForm(void);
int find_auth_user_passwd(ParsedURL *pu, char *realm,
                          Str *uname, Str *pwd, int is_proxy);
void add_auth_user_passwd(ParsedURL *pu, char *realm,
                          Str uname, Str pwd, int is_proxy);
void invalidate_auth_user_passwd(ParsedURL *pu, char *realm,
                                 Str uname, Str pwd, int is_proxy);
char *last_modified(BufferPtr buf);
Str romanNumeral(int n);
Str romanAlphabet(int n);
void setup_child(int child, int i, int f);
void myExec(char *command);
void mySystem(char *command, int background);
Str myExtCommand(char *cmd, char *arg, int redirect);
Str myEditor(char *cmd, char *file, int line);


char *expandName(char *name);
Str tmpfname(int type, const char *ext);
time_t mymktime(char *timestr);

char *FQDN(char *host);


void HTMLlineproc2(BufferPtr buf, TextLineList *tl);

void loadHTMLstream(URLFile *f, BufferPtr newBuf, FILE *src,
                    int internal);

int GetCurHSeq();
void SetCurHSeq(int seq);
void SetMetaCharset(wc_ces ces);
ParsedURL *GetCurBaseUrl();
