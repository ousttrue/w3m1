/* $Id: terms.h,v 1.10 2004/07/15 16:32:39 ukai Exp $ */
#ifndef TERMS_H
#define TERMS_H

extern int LINES, COLS;
#if defined(__CYGWIN__)
extern int LASTLINE;
#endif

#ifdef USE_MOUSE
/* Addition:mouse event */
#define MOUSE_BTN1_DOWN 0
#define MOUSE_BTN2_DOWN 1
#define MOUSE_BTN3_DOWN 2
#define MOUSE_BTN4_DOWN_RXVT 3
#define MOUSE_BTN5_DOWN_RXVT 4
#define MOUSE_BTN4_DOWN_XTERM 64
#define MOUSE_BTN5_DOWN_XTERM 65
#define MOUSE_BTN_UP 3
#define MOUSE_BTN_RESET -1
#endif

#ifdef __CYGWIN__
#if CYGWIN_VERSION_DLL_MAJOR < 1005 && defined(USE_MOUSE)
extern int cygwin_mouse_btn_swapped;
#endif
#ifdef SUPPORT_WIN9X_CONSOLE_MBCS
extern void enable_win9x_console_input(void);
extern void disable_win9x_console_input(void);
#endif
#endif

void mouse_active();
void mouse_inactive();
void mouse_end();
void addmch(const char *p, size_t len);
void clrtoeol(void);
void clrtoeolx(void);
void clrtobot(void);
void clrtobotx(void);
void no_clrtoeol(void);
void addstr(char *s);
void addnstr(const char *s, int n);
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
void getTCstr(void);
void setlinescols(void);
void setupscreen(void);
void touch_cursor();
int initscr(void);
void move(int line, int column);

#endif				/* not TERMS_H */
