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
