#pragma once
#include "config.h"
#include "line.h"

pid_t open_pipe_rw(FILE **fr, FILE **fw);
int next_status(char c, int *status);
int calcPosition(char *l, Lineprop *pr, int len, int pos, int bpos, int mode);
#define COLPOS(l,c)	calcPosition(l->lineBuf,l->propBuf,l->len,c,0,CP_AUTO)
MySignalHandler mySignal(int signal_number, MySignalHandler action);
#define TRAP_ON if (TrapSignal) { \
    prevtrap = mySignal(SIGINT, KeyAbort); \
    if (fmInitialized) \
	term_cbreak(); \
}
#define TRAP_OFF if (TrapSignal) { \
    if (fmInitialized) \
	term_raw(); \
    if (prevtrap) \
	mySignal(SIGINT, prevtrap); \
}
