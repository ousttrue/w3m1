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


int writeBufferCache(Buffer *buf);
int readBufferCache(Buffer *buf);
void fmTerm(void);
void fmInit(void);

void addChar(char c, Lineprop mode);
#ifdef USE_M17N
void addMChar(char *c, Lineprop mode, size_t len);
#endif
void record_err_message(char *s);
Buffer *message_list_panel(void);
void message(char *s, int return_x, int return_y);
void disp_err_message(char *s, int redraw_current);
void disp_message_nsec(char *s, int redraw_current, int sec, int purge,
                       int mouse);
void disp_message(char *s, int redraw_current);
#ifdef USE_MOUSE
void disp_message_nomouse(char *s, int redraw_current);
#else
#define disp_message_nomouse disp_message
#endif
void set_delayed_message(char *s);
void cursorUp0(Buffer *buf, int n);
void cursorUp(Buffer *buf, int n);
void cursorDown0(Buffer *buf, int n);
void cursorDown(Buffer *buf, int n);
void cursorUpDown(Buffer *buf, int n);
void cursorRight(Buffer *buf, int n);
void cursorLeft(Buffer *buf, int n);
void cursorHome(Buffer *buf);
void arrangeCursor(Buffer *buf);
void arrangeLine(Buffer *buf);
void cursorXY(Buffer *buf, int x, int y);
void restorePosition(Buffer *buf, Buffer *orig);
int columnSkip(Buffer *buf, int offset);
int columnPos(Line *line, int column);
int columnLen(Line *line, int column);
Line *lineSkip(Buffer *buf, Line *line, int offset, int last);
Line *currentLineSkip(Buffer *buf, Line *line, int offset, int last);
int gethtmlcmd(char **s);


char *lastFileName(char *path);
char *mybasename(char *s);
char *mydirname(char *s);
int read_token(Str buf, char **instr, int *status, int pre, int append);
Str correct_irrtag(int status);
#ifdef USE_MIGEMO
void init_migemo(void);
#endif
#ifdef USE_M17N
char *conv_search_string(char *str, wc_ces f_ces);
#else
#define conv_search_string(str, f_ces) str
#endif
int forwardSearch(Buffer *buf, char *str);
int backwardSearch(Buffer *buf, char *str);
void pcmap(void);
void escmap(void);
void escbmap(void);
void multimap(void);
char *inputLineHistSearch(char *prompt, char *def_str, int flag,
                          Hist *hist, int (*incfunc)(int ch, Str buf, Lineprop *prop));
Str unescape_spaces(Str s);
#ifdef USE_HISTORY
Buffer *historyBuffer(Hist *hist);
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
void align(TextLine *lbuf, int width, int mode);
void print_item(struct table *t, int row, int col, int width, Str buf);
void print_sep(struct table *t, int row, int type, int maxcol, Str buf);
void do_refill(struct table *tbl, int row, int col, int maxlimit);
void renderTable(struct table *t, int max_width,
                 struct html_feed_environ *h_env);
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
struct form_list *newFormList(char *action, char *method, char *charset,
                              char *enctype, char *target, char *name,
                              struct form_list *_next);
char *form2str(FormItemList *fi);
int formtype(char *typestr);
void formRecheckRadio(Anchor *a, Buffer *buf, FormItemList *form);
void formResetBuffer(Buffer *buf, AnchorList *formitem);
void formUpdateBuffer(Anchor *a, Buffer *buf, FormItemList *form);
void preFormUpdateBuffer(Buffer *buf);
Str textfieldrep(Str s, int width);
void input_textarea(FormItemList *fi);
void do_internal(char *action, char *data);
void form_write_data(FILE *f, char *boundary, char *name, char *value);
void form_write_from_file(FILE *f, char *boundary, char *name,
                          char *filename, char *file);
MapList *searchMapList(Buffer *buf, char *name);
#ifndef MENU_MAP
Buffer *follow_map_panel(Buffer *buf, char *name);
#endif
#ifdef USE_IMAGE
int getMapXY(Buffer *buf, Anchor *a, int *x, int *y);
MapArea *retrieveCurrentMapArea(Buffer *buf);
#endif
Anchor *retrieveCurrentMap(Buffer *buf);
MapArea *newMapArea(char *url, char *target, char *alt, char *shape,
                    char *coords);
Buffer *page_info_panel(Buffer *buf);
struct frame_body *newFrame(struct parsed_tag *tag, Buffer *buf);
struct frameset *newFrameSet(struct parsed_tag *tag);
void deleteFrame(struct frame_body *b);
void deleteFrameSet(struct frameset *f);
struct frameset *copyFrameSet(struct frameset *of);
void pushFrameTree(struct frameset_queue **fqpp, struct frameset *fs,
                   Buffer *buf);
struct frameset *popFrameTree(struct frameset_queue **fqpp);
Buffer *renderFrame(Buffer *Cbuf, int force_reload);
union frameset_element *search_frame(struct frameset *fset, char *name);
int set_tty(void);
void set_cc(int spec, int val);
void close_tty(void);
char *ttyname_tty(void);
void reset_tty(void);
MySignalHandler reset_exit(SIGNAL_ARG);
MySignalHandler error_dump(SIGNAL_ARG);
void set_int(void);
void getTCstr(void);
void setlinescols(void);
void setupscreen(void);
pid_t open_pipe_rw(FILE **fr, FILE **fw);
int initscr(void);
void move(int line, int column);



void addch(char c);
void wrap(void);
void touch_line(void);
void standout(void);
void standend(void);
void bold(void);
void boldend(void);
void underline(void);
void underlineend(void);
void graphstart(void);
void graphend(void);
int graph_ok(void);
#ifdef USE_COLOR
void setfcolor(int color);
#ifdef USE_BG_COLOR
void setbcolor(int color);
#endif /* USE_BG_COLOR */
#endif /* USE_COLOR */
void refresh(void);
void clear(void);
#ifdef USE_RAW_SCROLL
void scroll(int);
void rscroll(int);
#endif
#if 0
void need_clrtoeol(void);
#endif
void clrtoeol(void);
void clrtoeolx(void);
void clrtobot(void);
void clrtobotx(void);
void no_clrtoeol(void);
void addstr(char *s);
void addnstr(char *s, int n);
void addnstr_sup(char *s, int n);
void crmode(void);
void nocrmode(void);
void term_echo(void);
void term_noecho(void);
void term_raw(void);
void term_cooked(void);
void term_cbreak(void);
void term_title(char *s);
void flush_tty(void);
void toggle_stand(void);
char getch(void);
void bell(void);
int sleep_till_anykey(int sec, int purge);
#ifdef USE_IMAGE
void touch_cursor();
#endif
void initMimeTypes();
void free_ssl_ctx();
ParsedURL *baseURL(Buffer *buf);
int openSocket(char *hostname, char *remoteport_name,
               unsigned short remoteport_num);
void parseURL(char *url, ParsedURL *p_url, ParsedURL *current);
void copyParsedURL(ParsedURL *p, ParsedURL *q);
void parseURL2(char *url, ParsedURL *pu, ParsedURL *current);
Str parsedURL2Str(ParsedURL *pu);
int getURLScheme(char **url);
void init_stream(URLFile *uf, int scheme, InputStream stream);
Str HTTPrequestMethod(HRequest *hr);
Str HTTPrequestURI(ParsedURL *pu, HRequest *hr);
URLFile openURL(char *url, ParsedURL *pu, ParsedURL *current,
                URLOption *option, FormList *request,
                TextList *extra_header, URLFile *ouf,
                HRequest *hr, unsigned char *status);
int mailcapMatch(struct mailcap *mcap, char *type);
struct mailcap *searchMailcap(struct mailcap *table, char *type);
void initMailcap();
char *acceptableMimeTypes();
struct mailcap *searchExtViewer(char *type);
Str unquote_mailcap(char *qstr, char *type, char *name, char *attr,
                    int *mc_stat);
char *guessContentType(char *filename);
TextList *make_domain_list(char *domain_list);
int check_no_proxy(char *domain);
InputStream openFTPStream(ParsedURL *pu, URLFile *uf);
#ifdef USE_M17N
Str loadFTPDir(ParsedURL *pu, wc_ces *charset);
#else
Str loadFTPDir0(ParsedURL *pu);
#define loadFTPDir(pu, charset) loadFTPDir0(pu)
#endif
void closeFTP(void);
void disconnectFTP(void);
#ifdef USE_NNTP
InputStream openNewsStream(ParsedURL *pu);
#ifdef USE_M17N
Str loadNewsgroup(ParsedURL *pu, wc_ces *charset);
#else
Str loadNewsgroup0(ParsedURL *pu);
#define loadNewsgroup(pu, charset) loadNewsgroup0(pu)
#endif
void closeNews(void);
void disconnectNews(void);
#endif
AnchorList *putAnchor(AnchorList *al, char *url, char *target,
                      Anchor **anchor_return, char *referer,
                      char *title, unsigned char key, int line,
                      int pos);
Anchor *registerHref(Buffer *buf, char *url, char *target,
                     char *referer, char *title, unsigned char key,
                     int line, int pos);
Anchor *registerName(Buffer *buf, char *url, int line, int pos);
Anchor *registerImg(Buffer *buf, char *url, char *title, int line,
                    int pos);
int onAnchor(Anchor *a, int line, int pos);
Anchor *retrieveAnchor(AnchorList *al, int line, int pos);
Anchor *retrieveCurrentAnchor(Buffer *buf);
Anchor *retrieveCurrentImg(Buffer *buf);
Anchor *retrieveCurrentForm(Buffer *buf);
Anchor *searchAnchor(AnchorList *al, char *str);
Anchor *searchURLLabel(Buffer *buf, char *url);
void reAnchorWord(Buffer *buf, Line *l, int spos, int epos);
char *reAnchor(Buffer *buf, char *re);
#ifdef USE_NNTP
char *reAnchorNews(Buffer *buf, char *re);
char *reAnchorNewsheader(Buffer *buf);
#endif /* USE_NNTP */
void addMultirowsForm(Buffer *buf, AnchorList *al);
Anchor *closest_next_anchor(AnchorList *a, Anchor *an, int x, int y);
Anchor *closest_prev_anchor(AnchorList *a, Anchor *an, int x, int y);
#ifdef USE_IMAGE
void addMultirowsImg(Buffer *buf, AnchorList *al);
#endif
HmarkerList *putHmarker(HmarkerList *ml, int line, int pos, int seq);
void shiftAnchorPosition(AnchorList *a, HmarkerList *hl, int line,
                         int pos, int shift);
char *getAnchorText(Buffer *buf, AnchorList *al, Anchor *a);
Buffer *link_list_panel(Buffer *buf);

Str decodeB(char **ww);
Str decodeQ(char **ww);
Str decodeQP(char **ww);
Str decodeU(char **ww);
#ifdef USE_M17N
Str decodeWord(char **ow, wc_ces *charset);
Str decodeMIME(Str orgstr, wc_ces *charset);
#else
Str decodeWord0(char **ow);
Str decodeMIME0(Str orgstr);
#define decodeWord(ow, charset) decodeWord0(ow)
#define decodeMIME(orgstr, charset) decodeMIME0(orgstr)
#endif
Str encodeB(char *a);
int set_param_option(char *option);
char *get_param_option(char *name);
void init_rc(void);
Buffer *load_option_panel(void);
void sync_with_option(void);

char *auxbinFile(char *base);
char *libFile(char *base);
char *helpFile(char *base);
Str localCookie(void);
Str loadLocalDir(char *dirname);
void set_environ(char *var, char *value);
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
char *last_modified(Buffer *buf);
Str romanNumeral(int n);
Str romanAlphabet(int n);
void setup_child(int child, int i, int f);
void myExec(char *command);
void mySystem(char *command, int background);
Str myExtCommand(char *cmd, char *arg, int redirect);
Str myEditor(char *cmd, char *file, int line);
char *file_to_url(char *file);
#ifdef USE_M17N
char *url_unquote_conv(char *url, wc_ces charset);
#else
char *url_unquote_conv0(char *url);
#define url_unquote_conv(url, charset) url_unquote_conv0(url)
#endif
char *expandName(char *name);
Str tmpfname(int type, char *ext);
time_t mymktime(char *timestr);

char *FQDN(char *host);
Str find_cookie(ParsedURL *pu);
int add_cookie(ParsedURL *pu, Str name, Str value, time_t expires,
               Str domain, Str path, int flag, Str comment, int version,
               Str port, Str commentURL);
void save_cookies(void);
void load_cookies(void);
void initCookie(void);
void cooLst(void);
Buffer *cookie_list_panel(void);
int check_cookie_accept_domain(char *domain);
