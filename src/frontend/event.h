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

AlarmEvent *DefaultAlarm();
AlarmEvent *CurrentAlarm();
void SetCurrentAlarm(AlarmEvent *);
void SigAlarm(int);
AlarmEvent *setAlarmEvent(AlarmEvent *event, int sec, short status, Command cmd, void *data);
void pushEvent(Command cmd, void *data);
int ProcessEvent();
