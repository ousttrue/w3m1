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
