#include "config.h"
#include "terminal.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <termios.h>
// #include <term.h> // danger macros
extern "C" int tputs (const char *, int, int (*)(int));

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
    if (tcgetattr(m_tty, &d_ioval) < 0)
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
                    is_xterm = p->mouse_flag;
                    break;
                }
            }
        }
    }
}

Terminal::~Terminal()
{
}

Terminal &Terminal::Instance()
{
    static Terminal t;
    return t;
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
