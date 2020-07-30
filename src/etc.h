#pragma once
#include "config.h"
#include <stdio.h>
#include <wc.h>


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




