#pragma once

int INIT_BUFFER_WIDTH();
int INIT_BUFFER_WIDTH();
int FOLD_BUFFER_WIDTH();
void clrtoeol(void);
void clrtoeolx(void);
void clrtobot(void);
void clrtobotx(void);
void no_clrtoeol(void);
void addstr(const char *s);
void addnstr(const char *s, int n);
void addnstr_sup(const char *s, int n);


void term_title(const char *s);
char getch(void);
int graph_ok(void);
void refresh(void);
int sleep_till_anykey(int sec, int purge);
void reset_error_exit(int);
