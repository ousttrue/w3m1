#pragma once

using Command = void (*)();

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
