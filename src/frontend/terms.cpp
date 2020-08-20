/* 
 * An original curses library for EUC-kanji by Akinori ITO,     December 1989
 * revised by Akinori ITO, January 1995
 */
#include "config.h"
#include "termcap_str.h"
#include "frontend/terminal.h"
#include "frontend/terms.h"
#include "commands.h"
#include "indep.h"
#include "gc_helper.h"
#include "public.h"
#include "ctrlcode.h"
#include "myctype.h"
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <termios.h>
#include "screen.h"

Screen g_screen;

void mouse_init(), mouse_end();
int mouseActive = 0;
static const char *title_str = NULL;

void reset_exit(SIGNAL_ARG);

void setlinescols(void);


#ifndef SIGIOT
#define SIGIOT SIGABRT
#endif /* not SIGIOT */

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#include <unistd.h>
typedef struct termios TerminalMode;

#define MODEFLAG(d) ((d).c_lflag)
#define IMODEFLAG(d) ((d).c_iflag)
#endif /* HAVE_TERMIOS_H */

#define MAX_LINE 200
#define MAX_COLUMN 400

// termcap
extern "C" int tgetnum(const char *);

void clear(), wrap(), touch_line();
void clrtoeol(void); /* conflicts with curs_clear(3)? */

inline void MOVE(int line, int column)
{
    Terminal::move(line, column);
}

void ttymode_set(int mode, int imode)
{
#ifndef __MINGW32_VERSION
    TerminalMode ioval;

    Terminal::tcgetattr(&ioval);
    MODEFLAG(ioval) |= mode;
#ifndef HAVE_SGTTY_H
    IMODEFLAG(ioval) |= imode;
#endif /* not HAVE_SGTTY_H */

    while (Terminal::tcsetattr(&ioval) == -1)
    {
        if (errno == EINTR || errno == EAGAIN)
            continue;
        printf("Error occured while set %x: errno=%d\n", mode, errno);
        reset_error_exit(SIGNAL_ARGLIST);
    }
#endif
}

void ttymode_reset(int mode, int imode)
{
#ifndef __MINGW32_VERSION
    TerminalMode ioval;

    Terminal::tcgetattr(&ioval);
    MODEFLAG(ioval) &= ~mode;
#ifndef HAVE_SGTTY_H
    IMODEFLAG(ioval) &= ~imode;
#endif /* not HAVE_SGTTY_H */

    while (Terminal::tcsetattr(&ioval) == -1)
    {
        if (errno == EINTR || errno == EAGAIN)
            continue;
        printf("Error occured while reset %x: errno=%d\n", mode, errno);
        reset_error_exit(SIGNAL_ARGLIST);
    }
#endif /* __MINGW32_VERSION */
}

#ifndef HAVE_SGTTY_H
void set_cc(int spec, int val)
{
    TerminalMode ioval;

    Terminal::tcgetattr(&ioval);
    ioval.c_cc[spec] = val;
    while (Terminal::tcsetattr(&ioval) == -1)
    {
        if (errno == EINTR || errno == EAGAIN)
            continue;
        printf("Error occured: errno=%d\n", errno);
        reset_error_exit(SIGNAL_ARGLIST);
    }
}
#endif /* not HAVE_SGTTY_H */

static void
reset_exit_with_value(SIGNAL_ARG, int rval)
{
    if (mouseActive)
        mouse_end();

    // w3m_exit(rval);
    exit(rval);
    SIGNAL_RETURN;
}

void
    reset_error_exit(SIGNAL_ARG)
{
    reset_exit_with_value(SIGNAL_ARGLIST, 1);
}

void
    reset_exit(SIGNAL_ARG)
{
    reset_exit_with_value(SIGNAL_ARGLIST, 0);
}

void
    error_dump(SIGNAL_ARG)
{
    mySignal(SIGIOT, SIG_DFL);    
    abort();
    SIGNAL_RETURN;
}

void set_int(void)
{
    mySignal(SIGHUP, reset_exit);
    mySignal(SIGINT, reset_exit);
    mySignal(SIGQUIT, reset_exit);
    mySignal(SIGTERM, reset_exit);
    mySignal(SIGILL, error_dump);
    mySignal(SIGIOT, error_dump);
    mySignal(SIGFPE, error_dump);
#ifdef SIGBUS
    mySignal(SIGBUS, error_dump);
#endif /* SIGBUS */
    /* mySignal(SIGSEGV, error_dump); */
}

void setlinescols(void)
{
    char *p;
    int i;

    // struct winsize wins;
    // i = ioctl(tty, TIOCGWINSZ, &wins);
    // if (i >= 0 && wins.ws_row != 0 && wins.ws_col != 0)
    // {
    //     LINES = wins.ws_row;
    //     COLS = wins.ws_col;
    // }

    if (LINES <= 0 && (p = getenv("LINES")) != NULL && (i = atoi(p)) >= 0)
        LINES = i;
    if (COLS <= 0 && (p = getenv("COLUMNS")) != NULL && (i = atoi(p)) >= 0)
        COLS = i;
    if (LINES <= 0)
        LINES = tgetnum("li"); /* number of line */
    if (COLS <= 0)
        COLS = tgetnum("co"); /* number of column */
    if (COLS > MAX_COLUMN)
        COLS = MAX_COLUMN;
    if (LINES > MAX_LINE)
        LINES = MAX_LINE;
}

void setupscreen(void)
{
    g_screen.Setup(LINES, COLS);
    clear();
}

/* 
 * Screen initialize
 */
int initscr(void)
{
    Terminal::Instance();
    
    set_int();
    getTCstr();
    if (T_ti && !w3mApp::Instance().Do_not_use_ti_te)
        Terminal::writestr(T_ti);
    setupscreen();
    return 0;
}

void move(int line, int column)
{
    g_screen.Move(line, column);
}

void addch(char c)
{
    addmch(&c, 1);
}

void addmch(const char *pc, int len)
{
    g_screen.AddChar(pc, len);
}

void wrap(void)
{
    g_screen.Wrap();
}

void touch_column(int col)
{
    g_screen.TouchColumn(col);
}

void touch_line(void)
{
    g_screen.TouchCurrentLine();
}

void standout(void)
{
    g_screen.Enable(S_STANDOUT);
}

void standend(void)
{
    g_screen.Disable(S_STANDOUT);
}

void toggle_stand(void)
{
    g_screen.StandToggle();
}

void bold(void)
{
    g_screen.Enable(S_BOLD);
}

void boldend(void)
{
    g_screen.Disable(S_BOLD);
}

void underline(void)
{
    g_screen.Enable(S_UNDERLINE);
}

void underlineend(void)
{
    g_screen.Disable(S_UNDERLINE);
}

void graphstart(void)
{
    g_screen.Enable(S_GRAPHICS);
}

void graphend(void)
{
    g_screen.Disable(S_GRAPHICS);
}

int graph_ok(void)
{
    if (w3mApp::Instance().UseGraphicChar != GRAPHIC_CHAR_DEC)
        return 0;
    return T_as[0] != 0 && T_ae[0] != 0 && T_ac[0] != 0;
}

void setfcolor(int color)
{
    g_screen.SetFGColor(color);
}

void setbcolor(int color)
{
    g_screen.SetBGColor(color);
}

void refresh(void)
{
    g_screen.Refresh();
    Terminal::flush();
}

void clear(void)
{
    g_screen.Clear();
}

/* XXX: conflicts with curses's clrtoeol(3) ? */
void clrtoeol(void)
{ /* Clear to the end of line */
    g_screen.CtrlToEol();
}

void clrtoeol_with_bcolor(void)
{
    g_screen.CtrlToEolWithBGColor();
}

void clrtoeolx(void)
{
    clrtoeol_with_bcolor();
}

void clrtobot_eol(void (*clrtoeol)())
{
    g_screen.CtrlToBottomEol();
}

void clrtobot(void)
{
    clrtobot_eol(clrtoeol);
}

void clrtobotx(void)
{
    clrtobot_eol(clrtoeolx);
}

void addstr(const char *s)
{
    if (!s)
    {
        return;
    }

    while (*s != '\0')
    {
        int len = wtf_len((uint8_t *)s);
        addmch(s, len);
        s += len;
    }
}

void addnstr(const char *s, int n)
{
    if (!s)
    {
        return;
    }
    for (int i = 0; *s != '\0';)
    {
        int width = wtf_width((uint8_t *)s);
        if (i + width > n)
            break;
        int len = wtf_len((uint8_t *)s);
        addmch(s, len);
        s += len;
        i += width;
    }
}

void addnstr_sup(const char *s, int n)
{
    int i;
    int len, width;

    for (i = 0; *s != '\0';)
    {
        width = wtf_width((uint8_t *)s);
        if (i + width > n)
            break;
        len = wtf_len((uint8_t *)s);
        addmch(s, len);
        s += len;
        i += width;
    }
    for (; i < n; i++)
        addch(' ');
}

void crmode(void)
#ifndef HAVE_SGTTY_H
{
    ttymode_reset(ICANON, IXON);
    ttymode_set(ISIG, 0);
#ifdef HAVE_TERMIOS_H
    set_cc(VMIN, 1);
#else  /* not HAVE_TERMIOS_H */
    set_cc(VEOF, 1);
#endif /* not HAVE_TERMIOS_H */
}
#else  /* HAVE_SGTTY_H */
{
    ttymode_set(CBREAK, 0);
}
#endif /* HAVE_SGTTY_H */

void nocrmode(void)
#ifndef HAVE_SGTTY_H
{
    ttymode_set(ICANON, 0);
#ifdef HAVE_TERMIOS_H
    set_cc(VMIN, 4);
#else  /* not HAVE_TERMIOS_H */
    set_cc(VEOF, 4);
#endif /* not HAVE_TERMIOS_H */
}
#else  /* HAVE_SGTTY_H */
{
    ttymode_reset(CBREAK, 0);
}
#endif /* HAVE_SGTTY_H */

void term_echo(void)
{
    ttymode_set(ECHO, 0);
}

void term_noecho(void)
{
    ttymode_reset(ECHO, 0);
}

void term_raw(void)
#ifndef HAVE_SGTTY_H
#ifdef IEXTEN
#define TTY_MODE ISIG | ICANON | ECHO | IEXTEN
#else /* not IEXTEN */
#define TTY_MODE ISIG | ICANON | ECHO
#endif /* not IEXTEN */
{
    ttymode_reset(TTY_MODE, IXON | IXOFF);
#ifdef HAVE_TERMIOS_H
    set_cc(VMIN, 1);
#else  /* not HAVE_TERMIOS_H */
    set_cc(VEOF, 1);
#endif /* not HAVE_TERMIOS_H */
}
#else  /* HAVE_SGTTY_H */
{
    ttymode_set(RAW, 0);
}
#endif /* HAVE_SGTTY_H */

void term_cooked(void)
#ifndef HAVE_SGTTY_H
{
#ifdef __EMX__
    /* On XFree86/OS2, some scrambled characters
     * will appear when asserting IEXTEN flag.
     */
    ttymode_set((TTY_MODE) & ~IEXTEN, 0);
#else
    ttymode_set(TTY_MODE, 0);
#endif
#ifdef HAVE_TERMIOS_H
    set_cc(VMIN, 4);
#else  /* not HAVE_TERMIOS_H */
    set_cc(VEOF, 4);
#endif /* not HAVE_TERMIOS_H */
}
#else  /* HAVE_SGTTY_H */
{
    ttymode_reset(RAW, 0);
}
#endif /* HAVE_SGTTY_H */

void term_cbreak(void)
{
    term_cooked();
    term_noecho();
}

void term_title(const char *s)
{
    if (!w3mApp::Instance().fmInitialized)
        return;
    if (title_str != NULL)
    {
        // fprintf(ttyf, title_str, s);
    }
}

char getch(void)
{
    char c;
    while (read(Terminal::tty(), &c, 1) < (int)1)
    {
        if (errno == EINTR || errno == EAGAIN)
            continue;
        /* error happend on read(2) */
        quitfm(&w3mApp::Instance());
        break; /* unreachable */
    }
    return c;
}

void bell(void)
{
    Terminal::write1(7);
}

void skip_escseq(void)
{
    int c;

    c = getch();
    if (c == '[' || c == 'O')
    {
        c = getch();
        if (Terminal::is_xterm() && c == 'M')
        {
            getch();
            getch();
            getch();
        }
        else
            while (IS_DIGIT(c))
                c = getch();
    }
}

int sleep_till_anykey(int sec, int purge)
{
    fd_set rfd;
    struct timeval tim;
    int er, c, ret;
    TerminalMode ioval;

    Terminal::tcgetattr(&ioval);
    term_raw();

    tim.tv_sec = sec;
    tim.tv_usec = 0;

    FD_ZERO(&rfd);
    FD_SET(Terminal::tty(), &rfd);

    ret = select(Terminal::tty() + 1, &rfd, 0, 0, &tim);
    if (ret > 0 && purge)
    {
        c = getch();
        if (c == ESC_CODE)
            skip_escseq();
    }
    er = Terminal::tcsetattr(&ioval);
    if (er == -1)
    {
        printf("Error occured: errno=%d\n", errno);
        reset_error_exit(SIGNAL_ARGLIST);
    }
    return ret;
}

void XTERM_ON()
{
    fputs("\033[?1001s\033[?1000h", Terminal::file());
    Terminal::flush();
}

void XTERM_OFF()
{
    fputs("\033[?1000l\033[?1001r", Terminal::file());
    Terminal::flush();
}

void mouse_init()
{
    if (mouseActive)
        return;

    Terminal::xterm_on();

    mouseActive = 1;
}

void mouse_end()
{
    if (mouseActive == 0)
        return;

    Terminal::xterm_off();

    mouseActive = 0;
}

void mouse_active()
{
    if (!mouseActive)
        mouse_init();
}

void mouse_inactive()
{
    if (mouseActive && Terminal::is_xterm())
        mouse_end();
}

void touch_cursor()
{
    g_screen.TouchCursor();
}

int _INIT_BUFFER_WIDTH()
{
    return COLS - (w3mApp::Instance().showLineNum ? 6 : 1);
}
int INIT_BUFFER_WIDTH()
{
    return (_INIT_BUFFER_WIDTH() > 0) ? _INIT_BUFFER_WIDTH() : 0;
}
int FOLD_BUFFER_WIDTH()
{
    return w3mApp::Instance().FoldLine ? (INIT_BUFFER_WIDTH() + 1) : -1;
}
