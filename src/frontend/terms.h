/* $Id: terms.h,v 1.10 2004/07/15 16:32:39 ukai Exp $ */
#ifndef TERMS_H
#define TERMS_H

extern int LINES, COLS;
int INIT_BUFFER_WIDTH();
int INIT_BUFFER_WIDTH();
int FOLD_BUFFER_WIDTH();

class Terminal
{
};

void mouse_active();
void mouse_inactive();
void mouse_end();
void addmch(const char *p, int len);
void clrtoeol(void);
void clrtoeolx(void);
void clrtobot(void);
void clrtobotx(void);
void no_clrtoeol(void);
void addstr(const char *s);
void addnstr(const char *s, int n);
void addnstr_sup(const char *s, int n);
void crmode(void);
void nocrmode(void);
void term_echo(void);
void term_noecho(void);
void term_raw(void);
void term_cooked(void);
void term_cbreak(void);
void term_title(const char *s);
void flush_tty(void);
void toggle_stand(void);
char getch(void);
void bell(void);
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
int sleep_till_anykey(int sec, int purge);
int set_tty(void);
void set_cc(int spec, int val);
void close_tty(void);
char *ttyname_tty(void);
void reset_tty(void);
void set_int(void);

void setlinescols(void);
void setupscreen(void);
void touch_cursor();
int initscr(void);
void move(int line, int column);

int write1(char c);
void writestr(char *s);
void reset_error_exit(int);

#endif /* not TERMS_H */
