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
