#include "event.h"
#include "commands.h"
#include "dispatcher.h"
#include "terminal.h"
#include "terms.h"
#include "tab.h"
#include "buffer.h"
#include "tabbar.h"
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

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
        w3mApp::Instance().CurrentCmdData = data = (char *)CurrentAlarm()->data;

        Terminal::mouse_on();
        CurrentAlarm()->cmd(&w3mApp::Instance());
        Terminal::mouse_off();

        w3mApp::Instance().CurrentCmdData.clear();
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

static void
reset_signals(void)
{
#ifdef SIGHUP
    mySignal(SIGHUP, SIG_DFL); /* terminate process */
#endif
    mySignal(SIGINT, SIG_DFL); /* terminate process */
#ifdef SIGQUIT
    mySignal(SIGQUIT, SIG_DFL); /* terminate process */
#endif
    mySignal(SIGTERM, SIG_DFL); /* terminate process */
    mySignal(SIGILL, SIG_DFL);  /* create core image */
    mySignal(SIGIOT, SIG_DFL);  /* create core image */
    mySignal(SIGFPE, SIG_DFL);  /* create core image */
#ifdef SIGBUS
    mySignal(SIGBUS, SIG_DFL); /* create core image */
#endif                         /* SIGBUS */
#ifdef SIGCHLD
    mySignal(SIGCHLD, SIG_IGN);
#endif
#ifdef SIGPIPE
    mySignal(SIGPIPE, SIG_IGN);
#endif
}

static void
close_all_fds_except(int i, int f)
{
    switch (i)
    { /* fall through */
    case 0:
        dup2(open(DEV_NULL_PATH, O_RDONLY), 0);
    case 1:
        dup2(open(DEV_NULL_PATH, O_WRONLY), 1);
    case 2:
        dup2(open(DEV_NULL_PATH, O_WRONLY), 2);
    }
    /* close all other file descriptors (socket, ...) */
    for (i = 3; i < FOPEN_MAX; i++)
    {
        if (i != f)
            close(i);
    }
}

void setup_child(int child, int i, int f)
{
    reset_signals();
    mySignal(SIGINT, SIG_IGN);
#ifndef __MINGW32_VERSION
    if (!child)
        SETPGRP();
#endif /* __MINGW32_VERSION */
    // close_tty();
    close_all_fds_except(i, f);
    w3mApp::Instance().QuietMessage = true;
    w3mApp::Instance().fmInitialized = false;
    w3mApp::Instance().TrapSignal = false;
}

#ifndef FOPEN_MAX
#define FOPEN_MAX 1024 /* XXX */
#endif

#include <signal.h>
#include <setjmp.h>

static JMP_BUF AbortLoading;
static void KeyAbort(SIGNAL_ARG)
{
    LONGJMP(AbortLoading, 1);
    SIGNAL_RETURN;
}

struct ScopedTrap
{
    MySignalHandler prevtrap;

    ScopedTrap()
    {
        if (w3mApp::Instance().TrapSignal)
        {
            prevtrap = mySignal(SIGINT, KeyAbort);
            if (w3mApp::Instance().fmInitialized)
                term_cbreak();
        }
    }
    ~ScopedTrap()
    {
        if (w3mApp::Instance().TrapSignal)
        {
            if (w3mApp::Instance().fmInitialized)
                term_raw();
            if (prevtrap)
                mySignal(SIGINT, prevtrap);
        }
    }
};

bool TrapJmp(bool enable, const std::function<bool()> &func)
{
    if (enable)
    {
        ScopedTrap trap;
        if (SETJMP(AbortLoading) == 0)
        {
            return func();
        }
        else
        {
            // jmp from SIGINT
            return false;
        }
    }
    else
    {
        return func();
    }
}
