#pragma once
#include "config.h"
#include "types.h"

pid_t open_pipe_rw(FILE **fr, FILE **fw);
int next_status(char c, int *status);

/* Flags for calcPosition() */
#define CP_AUTO		0
#define CP_FORCE	1
int calcPosition(char *l, Lineprop *pr, int len, int pos, int bpos, int mode);

MySignalHandler mySignal(int signal_number, MySignalHandler action);
#define TRAP_ON                                \
    if (TrapSignal)                            \
    {                                          \
        prevtrap = mySignal(SIGINT, KeyAbort); \
        if (fmInitialized)                     \
            term_cbreak();                     \
    }
#define TRAP_OFF                        \
    if (TrapSignal)                     \
    {                                   \
        if (fmInitialized)              \
            term_raw();                 \
        if (prevtrap)                   \
            mySignal(SIGINT, prevtrap); \
    }
char *url_unquote_conv(const char *url, wc_ces charset);