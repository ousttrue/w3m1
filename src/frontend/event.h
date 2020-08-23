#pragma once
#include "w3m.h"

enum AlarmStatus : short
{
    AL_UNSET = 0,
    AL_EXPLICIT = 1,
    AL_IMPLICIT = 2,
    AL_IMPLICIT_ONCE = 3,
};

struct AlarmEvent
{
    int sec;
    short status;
    Command cmd;
    void *data;
};

#ifndef SIGIOT
#define SIGIOT SIGABRT
#endif /* not SIGIOT */

#define TRAP_ON                                \
    if (w3mApp::Instance().TrapSignal)         \
    {                                          \
        prevtrap = mySignal(SIGINT, KeyAbort); \
        if (w3mApp::Instance().fmInitialized)  \
            Terminal::term_cbreak();           \
    }

#define TRAP_OFF                              \
    if (w3mApp::Instance().TrapSignal)        \
    {                                         \
        if (w3mApp::Instance().fmInitialized) \
            Terminal::term_raw();             \
        if (prevtrap)                         \
            mySignal(SIGINT, prevtrap);       \
    }
// typedef RETSIGTYPE MySignalHandler;
using MySignalHandler = void (*)(int);
MySignalHandler mySignal(int signal_number, MySignalHandler action);
AlarmEvent *DefaultAlarm();
AlarmEvent *CurrentAlarm();
void SetCurrentAlarm(AlarmEvent *);
void SigAlarm(int);
AlarmEvent *setAlarmEvent(AlarmEvent *event, int sec, short status, Command cmd, void *data);
void pushEvent(Command cmd, void *data);
int ProcessEvent();
void setup_child(int child, int i, int f);

#include <functional>
bool TrapJmp(bool enable, const std::function<bool()> &func);
inline bool TrapJmp(const std::function<bool()> &func)
{
    return TrapJmp(true, func);
}
void SigPipe(SIGNAL_ARG);
