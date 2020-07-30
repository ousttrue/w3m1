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
