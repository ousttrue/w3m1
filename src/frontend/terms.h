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
int graph_ok(void);
void refresh(void);
void reset_error_exit(int);
