#pragma once

/* 
 * Command functions: These functions are called with a keystroke.
 */
void escKeyProc(int c, int esc, unsigned char *map);
int prec_num();
void set_prec_num(int n);
int PREC_NUM();
inline int PREC_LIMIT() { return 10000; }
int searchKeyNum();
void nscroll(int n, int mode);
void srch_nxtprv(int reverse);
int dispincsrch(int ch, Str buf, Lineprop *prop);
void isrch(int (*func)(Buffer *, char *), char *prompt);
void srch(int (*func)(Buffer *, char *), char *prompt);
void clear_mark(Line *l);
void disp_srchresult(int result, char *prompt, char *str);
void shiftvisualpos(Buffer *buf, int shift);
void pushBuffer(Buffer *buf);
void cmd_loadfile(char *fn);
void cmd_loadURL(char *url, ParsedURL *current, char *referer, FormList *request);
int handleMailto(char *url);
void _movL(int n);
void _movD(int n);
void _movU(int n);
void _movR(int n);
int prev_nonnull_line(Line *line);
int next_nonnull_line(Line *line);
char *getCurWord(Buffer *buf, int *spos, int *epos);
void prevChar(int *s, Line *l);
void nextChar(int *s, Line *l);
wc_uint32 getChar(char *p);
int is_wordchar(wc_uint32 c);
int srchcore(char *volatile str, int (*func)(Buffer *, char *));
void _quitfm(int confirm);
void delBuffer(Buffer *buf);
void _goLine(char *l);
int cur_real_linenumber(Buffer *buf);
char *inputLineHist(char *prompt, char *def_str, int flag, Hist *hist);
char *inputStrHist(char *prompt, char *def_str, Hist *hist);
char *inputLine(char *prompt, char *def_str, int flag);
char *MarkString();
void SetMarkString(char *str);
void do_dump(Buffer *buf);
void _followForm(int submit);
void query_from_followform(Str *query, FormItemList *fi, int multipart);
Buffer *loadLink(char *url, char *target, char *referer, FormList *request);
FormItemList *save_submit_formlist(FormItemList *src);
Str conv_form_encoding(Str val, FormItemList *fi, Buffer *buf);
void bufferA();
Buffer *loadNormalBuf(Buffer *buf, int renderframe);
void _nextA(int visited);
void _prevA(int visited);
