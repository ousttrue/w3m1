#include "event.h"
#include "commands.h"
#include "dispatcher.h"
#include "fm.h"
#include "terms.h"
#include "tab.h"

#include <signal.h>

static AlarmEvent s_DefaultAlarm = {
    0, AL_UNSET, &nulcmd, nullptr};
AlarmEvent *DefaultAlarm()
{
    return &s_DefaultAlarm;
}
static AlarmEvent *s_CurrentAlarm = &s_DefaultAlarm;
AlarmEvent *CurrentAlarm()
{
    return s_CurrentAlarm;
}
void SetCurrentAlarm(AlarmEvent *alarm)
{
    s_CurrentAlarm = alarm;
}

void SigAlarm(int)
{
    char *data;

    if (CurrentAlarm()->sec > 0)
    {
        ClearCurrentKey();
        ClearCurrentKeyData();
        CurrentCmdData = data = (char *)CurrentAlarm()->data;
#ifdef USE_MOUSE
        if (use_mouse)
            mouse_inactive();
#endif
        CurrentAlarm()->cmd();
#ifdef USE_MOUSE
        if (use_mouse)
            mouse_active();
#endif
        CurrentCmdData = NULL;
        if (CurrentAlarm()->status == AL_IMPLICIT_ONCE)
        {
            CurrentAlarm()->sec = 0;
            CurrentAlarm()->status = AL_UNSET;
        }
        if (GetCurrentTab()->GetCurrentBuffer()->event)
        {
            if (GetCurrentTab()->GetCurrentBuffer()->event->status != AL_UNSET)
                SetCurrentAlarm(GetCurrentTab()->GetCurrentBuffer()->event);
            else
                GetCurrentTab()->GetCurrentBuffer()->event = NULL;
        }
        if (!GetCurrentTab()->GetCurrentBuffer()->event)
            SetCurrentAlarm(DefaultAlarm());
        if (CurrentAlarm()->sec > 0)
        {
            mySignal(SIGALRM, SigAlarm);
            alarm(CurrentAlarm()->sec);
        }
    }
    SIGNAL_RETURN;
}

MySignalHandler mySignal(int signal_number, MySignalHandler action)
{
#ifdef SA_RESTART
    struct sigaction new_action;
    sigemptyset(&new_action.sa_mask);
    new_action.sa_handler = action;
    if (signal_number == SIGALRM)
    {
#ifdef SA_INTERRUPT
        new_action.sa_flags = SA_INTERRUPT;
#else
        new_action.sa_flags = 0;
#endif
    }
    else
    {
        new_action.sa_flags = SA_RESTART;
    }

    struct sigaction old_action;
    sigaction(signal_number, &new_action, &old_action);
    return (old_action.sa_handler);
    
#else
    return (signal(signal_number, action));
#endif
}
