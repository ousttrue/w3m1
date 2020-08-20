#include "config.h"
#include "terminal.h"
#include "termcap_str.h"
#include "event.h"
#include "w3m.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <assert.h>
#include <termios.h>
// #include <term.h> // danger macros
extern "C" int tputs(const char *, int, int (*)(int));
extern "C" char *tgoto(const char *, int, int);
extern "C" int tgetnum(const char *);

#define W3M_TERM_INFO(name, title, mouse) name, title, mouse
#define NEED_XTERM_ON (1)
#define NEED_XTERM_OFF (1 << 1)

static char XTERM_TITLE[] = "\033]0;w3m: %s\007";
static char SCREEN_TITLE[] = "\033k%s\033\134";

/* *INDENT-OFF* */
static struct w3m_term_info
{
    const char *term;
    const char *title_str;
    int mouse_flag;
} w3m_term_info_list[] = {
    {W3M_TERM_INFO("xterm", XTERM_TITLE, (NEED_XTERM_ON | NEED_XTERM_OFF))},
    {W3M_TERM_INFO("kterm", XTERM_TITLE, (NEED_XTERM_ON | NEED_XTERM_OFF))},
    {W3M_TERM_INFO("rxvt", XTERM_TITLE, (NEED_XTERM_ON | NEED_XTERM_OFF))},
    {W3M_TERM_INFO("Eterm", XTERM_TITLE, (NEED_XTERM_ON | NEED_XTERM_OFF))},
    {W3M_TERM_INFO("mlterm", XTERM_TITLE, (NEED_XTERM_ON | NEED_XTERM_OFF))},
    {W3M_TERM_INFO("screen", SCREEN_TITLE, 0)},
    {W3M_TERM_INFO(NULL, NULL, 0)}};
#undef W3M_TERM_INFO
/* *INDENT-ON * */

FILE *g_ttyf = nullptr;


Terminal::Terminal()
{
    // name
    const char *tty_name;
    if (isatty(0)) /* stdin */
        tty_name = ttyname(0);
    else
        tty_name = DEV_TTY_PATH;

    // open tty
    m_tty = open(tty_name, O_RDWR);
    if (m_tty < 0)
    {
        /* use stderr instead of stdin... is it OK???? */
        assert(false);
        return;
    }

    g_ttyf = fdopen(m_tty, "w");
    if (!g_ttyf)
    {
        assert(false);
        return;
    }

    termios d_ioval;
    if (::tcgetattr(m_tty, &d_ioval) < 0)
    {
        assert(false);
        return;
    }

    {
        m_term = getenv("TERM");
        if (m_term.size())
        {
            struct w3m_term_info *p;
            for (p = w3m_term_info_list; p->term != NULL; p++)
            {
                if (m_term.starts_with(p->term))
                {
                    m_is_xterm = p->mouse_flag;
                    break;
                }
            }
        }
    }

//     mySignal(SIGHUP, reset_exit);
//     mySignal(SIGINT, reset_exit);
//     mySignal(SIGQUIT, reset_exit);
//     mySignal(SIGTERM, reset_exit);
//     mySignal(SIGILL, error_dump);
//     mySignal(SIGIOT, error_dump);
//     mySignal(SIGFPE, error_dump);
// #ifdef SIGBUS
//     mySignal(SIGBUS, error_dump);
// #endif /* SIGBUS */
//     /* mySignal(SIGSEGV, error_dump); */

    getTCstr();
    if (T_ti && !w3mApp::Instance().Do_not_use_ti_te)
        Terminal::writestr(T_ti);
}

// if (w3mApp::Instance().displayTitleTerm.size())
// {
//     struct w3m_term_info *p;
//     for (p = w3m_term_info_list; p->term != NULL; p++)
//     {
//         if (!strncmp(w3mApp::Instance().displayTitleTerm.c_str(), p->term, strlen(p->term)))
//         {
//             title_str = p->title_str;
//             break;
//         }
//     }
// }

Terminal::~Terminal()
{
    Terminal::writestr(T_op); /* turn off */
    Terminal::writestr(T_me);
    if (!w3mApp::Instance().Do_not_use_ti_te)
    {
        if (T_te && *T_te)
            Terminal::writestr(T_te);
        else
            Terminal::writestr(T_cl);
    }
    Terminal::writestr(T_se); /* reset terminal */
    flush();
    // Terminal::tcsetattr(&d_ioval);

    if (m_tty > 2)
    {
        // except stdin, stdout or stderr
        close(m_tty);
    }
}

// int initscr(void)
// {
//     Terminal::Instance();

//     setupscreen();
//     return 0;
// }

Terminal &Terminal::Instance()
{
    static std::unique_ptr<Terminal> t;
    if (!t)
    {
        t.reset(new Terminal());
    }
    return *t;
}

int Terminal::tty()
{
    return Instance().m_tty;
}

FILE *Terminal::file()
{
    return g_ttyf;
}

void Terminal::flush()
{
    fflush(g_ttyf);
}

int Terminal::is_xterm()
{
    return Instance().m_is_xterm;
}

int Terminal::write1(int c)
{
    return putc(c, g_ttyf);
}

extern "C" int write1(int c)
{
    return putc(c, g_ttyf);
}

void Terminal::writestr(const char *s)
{
    tputs(s, 1, ::write1);
}

int Terminal::tcgetattr(struct termios *__termios_p)
{
    return ::tcgetattr(Instance().m_tty, __termios_p);
}

int Terminal::tcsetattr(const struct termios *__termios_p)
{
    return ::tcsetattr(Instance().m_tty, TCSANOW, __termios_p);
}

const char *Terminal::ttyname_tty(void)
{
    return ttyname(Instance().m_tty);
}

void Terminal::move(int line, int column)
{
    writestr(tgoto(T_cm, column, line));
}

void Terminal::xterm_on()
{
    if (is_xterm() & NEED_XTERM_ON)
    {
        fputs("\033[?1001s\033[?1000h", Terminal::file());
        Terminal::flush();
    }
}

void Terminal::xterm_off()
{
    if (is_xterm() & NEED_XTERM_OFF)
    {
        fputs("\033[?1000l\033[?1001r", Terminal::file());
        Terminal::flush();
    }
}

int Terminal::lines()
{
    Instance();
    return tgetnum("li");
};

int Terminal::columns()
{
    Instance();
    return tgetnum("co");
}
// void setlinescols(void)
// {
//     char *p;
//     int i;

//     // struct winsize wins;
//     // i = ioctl(tty, TIOCGWINSZ, &wins);
//     // if (i >= 0 && wins.ws_row != 0 && wins.ws_col != 0)
//     // {
//     //     LINES = wins.ws_row;
//     //     Terminal::columns() = wins.ws_col;
//     // }

//     if (LINES <= 0 && (p = getenv("LINES")) != NULL && (i = atoi(p)) >= 0)
//         LINES = i;
//     if (Terminal::columns() <= 0 && (p = getenv("COLUMNS")) != NULL && (i = atoi(p)) >= 0)
//         Terminal::columns() = i;
//     if (LINES <= 0)
//         LINES =Terminal::lines();
//     if (Terminal::columns() <= 0)
//         Terminal::columns() = Terminal::columns();
//     if (Terminal::columns() > MAX_COLUMN)
//         Terminal::columns() = MAX_COLUMN;
//     if (LINES > MAX_LINE)
//         LINES = MAX_LINE;
// }
