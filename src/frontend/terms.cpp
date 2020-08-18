/* 
 * An original curses library for EUC-kanji by Akinori ITO,     December 1989
 * revised by Akinori ITO, January 1995
 */
#include "config.h"
#include "termcap_str.h"
#include "fm.h"
#include "commands.h"
#include "frontend/terms.h"
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
#include "screen.h"

Screen g_screen;

static int is_xterm = 0;
void mouse_init(), mouse_end();
int mouseActive = 0;
static char *title_str = NULL;
static int tty = -1;

void reset_exit(SIGNAL_ARG);

void setlinescols(void);
void flush_tty();

#ifndef SIGIOT
#define SIGIOT SIGABRT
#endif /* not SIGIOT */

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#include <unistd.h>
typedef struct termios TerminalMode;
#define TerminalSet(fd, x) tcsetattr(fd, TCSANOW, x)
#define TerminalGet(fd, x) tcgetattr(fd, x)
#define MODEFLAG(d) ((d).c_lflag)
#define IMODEFLAG(d) ((d).c_iflag)
#endif /* HAVE_TERMIOS_H */

#define MAX_LINE 200
#define MAX_COLUMN 400












#define ISDIRTY(d) ((d)&L_DIRTY)
#define ISUNUSED(d) ((d)&L_UNUSED)
#define NEED_CE(d) ((d)&L_NEED_CE)

static TerminalMode d_ioval;

extern "C" int tputs(char *, int, int (*)(char));
static FILE *ttyf = NULL;
int write1(char c)
{
    putc(c, ttyf);
#ifdef SCREEN_DEBUG
    flush_tty();
#endif /* SCREEN_DEBUG */
    return 0;
}
void writestr(char *s)
{
    tputs(s, 1, write1);
}

// termcap
extern "C" int tgetnum(char *);
extern "C" char *tgoto(char *, int, int);

void clear(), wrap(), touch_line();
void clrtoeol(void); /* conflicts with curs_clear(3)? */

#define MOVE(line, column) writestr(tgoto(T_cm, column, line));

#ifdef USE_MOUSE
#define W3M_TERM_INFO(name, title, mouse) name, title, mouse
#define NEED_XTERM_ON (1)
#define NEED_XTERM_OFF (1 << 1)
#else
#define W3M_TERM_INFO(name, title, mouse) name, title
#endif

static char XTERM_TITLE[] = "\033]0;w3m: %s\007";
static char SCREEN_TITLE[] = "\033k%s\033\134";

/* *INDENT-OFF* */
static struct w3m_term_info
{
    char *term;
    char *title_str;
#ifdef USE_MOUSE
    int mouse_flag;
#endif
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

int set_tty(void)
{
    char *ttyn;

    if (isatty(0)) /* stdin */
        ttyn = ttyname(0);
    else
        ttyn = DEV_TTY_PATH;
    tty = open(ttyn, O_RDWR);
    if (tty < 0)
    {
        /* use stderr instead of stdin... is it OK???? */
        tty = 2;
    }
    ttyf = fdopen(tty, "w");
    TerminalGet(tty, &d_ioval);
    if (w3mApp::Instance().displayTitleTerm.size())
    {
        struct w3m_term_info *p;
        for (p = w3m_term_info_list; p->term != NULL; p++)
        {
            if (!strncmp(w3mApp::Instance().displayTitleTerm.c_str(), p->term, strlen(p->term)))
            {
                title_str = p->title_str;
                break;
            }
        }
    }
#ifdef USE_MOUSE
    {
        char *term = getenv("TERM");
        if (term != NULL)
        {
            struct w3m_term_info *p;
            for (p = w3m_term_info_list; p->term != NULL; p++)
            {
                if (!strncmp(term, p->term, strlen(p->term)))
                {
                    is_xterm = p->mouse_flag;
                    break;
                }
            }
        }
    }
#endif
    return 0;
}

void ttymode_set(int mode, int imode)
{
#ifndef __MINGW32_VERSION
    TerminalMode ioval;

    TerminalGet(tty, &ioval);
    MODEFLAG(ioval) |= mode;
#ifndef HAVE_SGTTY_H
    IMODEFLAG(ioval) |= imode;
#endif /* not HAVE_SGTTY_H */

    while (TerminalSet(tty, &ioval) == -1)
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

    TerminalGet(tty, &ioval);
    MODEFLAG(ioval) &= ~mode;
#ifndef HAVE_SGTTY_H
    IMODEFLAG(ioval) &= ~imode;
#endif /* not HAVE_SGTTY_H */

    while (TerminalSet(tty, &ioval) == -1)
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

    TerminalGet(tty, &ioval);
    ioval.c_cc[spec] = val;
    while (TerminalSet(tty, &ioval) == -1)
    {
        if (errno == EINTR || errno == EAGAIN)
            continue;
        printf("Error occured: errno=%d\n", errno);
        reset_error_exit(SIGNAL_ARGLIST);
    }
}
#endif /* not HAVE_SGTTY_H */

void close_tty(void)
{
    if (tty > 2)
        close(tty);
}

char *
ttyname_tty(void)
{
    return ttyname(tty);
}

void reset_tty(void)
{
    writestr(T_op); /* turn off */
    writestr(T_me);
    if (!w3mApp::Instance().Do_not_use_ti_te)
    {
        if (T_te && *T_te)
            writestr(T_te);
        else
            writestr(T_cl);
    }
    writestr(T_se); /* reset terminal */
    flush_tty();
    TerminalSet(tty, &d_ioval);
    close_tty();
}

static void
reset_exit_with_value(SIGNAL_ARG, int rval)
{
#ifdef USE_MOUSE
    if (mouseActive)
        mouse_end();
#endif /* USE_MOUSE */
    reset_tty();
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
    reset_tty();
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
#ifdef __EMX__
    {
        int s[2];
        _scrsize(s);
        COLS = s[0];
        LINES = s[1];

        if (getenv("WINDOWID"))
        {
            FILE *fd = popen("scrsize", "rt");
            if (fd)
            {
                fscanf(fd, "%i %i", &COLS, &LINES);
                pclose(fd);
            }
        }
    }
#elif defined(HAVE_TERMIOS_H) && defined(TIOCGWINSZ)
    struct winsize wins;

    i = ioctl(tty, TIOCGWINSZ, &wins);
    if (i >= 0 && wins.ws_row != 0 && wins.ws_col != 0)
    {
        LINES = wins.ws_row;
        COLS = wins.ws_col;
    }
#endif /* defined(HAVE-TERMIOS_H) && defined(TIOCGWINSZ) */
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
    if (set_tty() < 0)
        return -1;
    set_int();
    getTCstr();
    if (T_ti && !w3mApp::Instance().Do_not_use_ti_te)
        writestr(T_ti);
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

void addmch(const char *pc, size_t len)
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
    g_screen.Refresh(ttyf);
    flush_tty();
}

void clear(void)
{
    g_screen.Clear();
}

#ifdef USE_RAW_SCROLL
static void
scroll_raw(void)
{ /* raw scroll */
    MOVE(LINES - 1, 0);
    write1('\n');
}

void scroll(int n)
{ /* scroll up */
    int cli = CurLine, cco = CurColumn;
    Screen *t;
    int i, j, k;

    i = LINES;
    j = n;
    do
    {
        k = j;
        j = i % k;
        i = k;
    } while (j);
    do
    {
        k--;
        i = k;
        j = (i + n) % LINES;
        t = ScreenImage[k];
        while (j != k)
        {
            ScreenImage[i] = ScreenImage[j];
            i = j;
            j = (i + n) % LINES;
        }
        ScreenImage[i] = t;
    } while (k);

    for (i = 0; i < n; i++)
    {
        t = ScreenImage[LINES - 1 - i];
        t->isdirty = 0;
        for (j = 0; j < COLS; j++)
            t->lineprop[j] = S_EOL;
        scroll_raw();
    }
    move(cli, cco);
}

void rscroll(int n)
{ /* scroll down */
    int cli = CurLine, cco = CurColumn;
    Screen *t;
    int i, j, k;

    i = LINES;
    j = n;
    do
    {
        k = j;
        j = i % k;
        i = k;
    } while (j);
    do
    {
        k--;
        i = k;
        j = (LINES + i - n) % LINES;
        t = ScreenImage[k];
        while (j != k)
        {
            ScreenImage[i] = ScreenImage[j];
            i = j;
            j = (LINES + i - n) % LINES;
        }
        ScreenImage[i] = t;
    } while (k);
    if (T_sr && *T_sr)
    {
        MOVE(0, 0);
        for (i = 0; i < n; i++)
        {
            t = ScreenImage[i];
            t->isdirty = 0;
            for (j = 0; j < COLS; j++)
                t->lineprop[j] = S_EOL;
            writestr(T_sr);
        }
        move(cli, cco);
    }
    else
    {
        for (i = 0; i < LINES; i++)
        {
            t = ScreenImage[i];
            t->isdirty |= L_DIRTY | L_NEED_CE;
            for (j = 0; j < COLS; j++)
            {
                t->lineprop[j] |= S_DIRTY;
            }
        }
    }
}
#endif

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
    if(!s)
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

void term_title(char *s)
{
    if (!w3mApp::Instance().fmInitialized)
        return;
    if (title_str != NULL)
    {
        fprintf(ttyf, title_str, s);
    }
}

char getch(void)
{
    char c;

    while (
#ifdef SUPPORT_WIN9X_CONSOLE_MBCS
        read_win32_console(&c, 1)
#else
        read(tty, &c, 1)
#endif
        < (int)1)
    {
        if (errno == EINTR || errno == EAGAIN)
            continue;
        /* error happend on read(2) */
        quitfm(&w3mApp::Instance());
        break; /* unreachable */
    }
    return c;
}

#ifdef USE_MOUSE
#ifdef USE_GPM
char wgetch(void *p)
{
    char c;

    /* read(tty, &c, 1); */
    while (read(tty, &c, 1) < (ssize_t)1)
    {
        if (errno == EINTR || errno == EAGAIN)
            continue;
        /* error happend on read(2) */
        quitfm();
        break; /* unreachable */
    }
    return c;
}

int do_getch()
{
    if (is_xterm || !gpm_handler)
        return getch();
    else
        return Gpm_Getch();
}
#endif /* USE_GPM */

#ifdef USE_SYSMOUSE
int sysm_getch()
{
    fd_set rfd;
    int key, x, y;

    FD_ZERO(&rfd);
    FD_SET(tty, &rfd);
    while (select(tty + 1, &rfd, NULL, NULL, NULL) <= 0)
    {
        if (errno == EINTR)
        {
            x = xpix / cwidth;
            y = ypix / cheight;
            key = (*sysm_handler)(x, y, nbs, obs);
            if (key != 0)
                return key;
        }
    }
    return getch();
}

int do_getch()
{
    if (is_xterm || !sysm_handler)
        return getch();
    else
        return sysm_getch();
}

MySignalHandler
    sysmouse(SIGNAL_ARG)
{
    struct mouse_info mi;

    mi.operation = MOUSE_GETINFO;
    if (ioctl(tty, CONS_MOUSECTL, &mi) == -1)
        return;
    xpix = mi.u.data.x;
    ypix = mi.u.data.y;
    obs = nbs;
    nbs = mi.u.data.buttons & 0x7;
    /* for cosmetic bug in syscons.c on FreeBSD 3.[34] */
    mi.operation = MOUSE_HIDE;
    ioctl(tty, CONS_MOUSECTL, &mi);
    mi.operation = MOUSE_SHOW;
    ioctl(tty, CONS_MOUSECTL, &mi);
}
#endif /* USE_SYSMOUSE */
#endif /* USE_MOUSE */

void bell(void)
{
    write1(7);
}

void skip_escseq(void)
{
    int c;

    c = getch();
    if (c == '[' || c == 'O')
    {
        c = getch();
#ifdef USE_MOUSE
        if (is_xterm && c == 'M')
        {
            getch();
            getch();
            getch();
        }
        else
#endif
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

    TerminalGet(tty, &ioval);
    term_raw();

    tim.tv_sec = sec;
    tim.tv_usec = 0;

    FD_ZERO(&rfd);
    FD_SET(tty, &rfd);

    ret = select(tty + 1, &rfd, 0, 0, &tim);
    if (ret > 0 && purge)
    {
        c = getch();
        if (c == ESC_CODE)
            skip_escseq();
    }
    er = TerminalSet(tty, &ioval);
    if (er == -1)
    {
        printf("Error occured: errno=%d\n", errno);
        reset_error_exit(SIGNAL_ARGLIST);
    }
    return ret;
}

#ifdef USE_MOUSE

#define XTERM_ON                               \
    {                                          \
        fputs("\033[?1001s\033[?1000h", ttyf); \
        flush_tty();                           \
    }
#define XTERM_OFF                              \
    {                                          \
        fputs("\033[?1000l\033[?1001r", ttyf); \
        flush_tty();                           \
    }

#ifdef USE_GPM
/* Linux console with GPM support */

void mouse_init()
{
    Gpm_Connect conn;
    int r;

    if (mouseActive)
        return;
    conn.eventMask = ~0;
    conn.defaultMask = 0;
    conn.maxMod = 0;
    conn.minMod = 0;

    gpm_handler = NULL;
    r = Gpm_Open(&conn, 0);
    if (r == -2)
    {
        /*
	 * If Gpm_Open() success, returns >= 0
	 * Gpm_Open() returns -2 in case of xterm.
	 * Gpm_Close() is necessary here. Otherwise,
	 * xterm is being left in the mode where the mouse clicks are
	 * passed through to the application.
	 */
        Gpm_Close();
        is_xterm = (NEED_XTERM_ON | NEED_XTERM_OFF);
    }
    else if (r >= 0)
    {
        gpm_handler = gpm_process_mouse;
        is_xterm = 0;
    }
    if (is_xterm)
    {
        XTERM_ON;
    }
    mouseActive = 1;
}

void mouse_end()
{
    if (mouseActive == 0)
        return;
    if (is_xterm)
    {
        XTERM_OFF;
    }
    else
        Gpm_Close();
    mouseActive = 0;
}

#elif defined(USE_SYSMOUSE)
/* *BSD console with sysmouse support */
void mouse_init()
{
    mouse_info_t mi;

    if (mouseActive)
        return;
    if (is_xterm)
    {
        XTERM_ON;
    }
    else
    {
#if defined(FBIO_MODEINFO) || defined(CONS_MODEINFO) /* FreeBSD > 2.x */
#ifndef FBIO_GETMODE                                 /* FreeBSD 3.x */
#define FBIO_GETMODE CONS_GET
#define FBIO_MODEINFO CONS_MODEINFO
#endif /* FBIO_GETMODE */
        video_info_t vi;

        if (ioctl(tty, FBIO_GETMODE, &vi.vi_mode) != -1 &&
            ioctl(tty, FBIO_MODEINFO, &vi) != -1)
        {
            cwidth = vi.vi_cwidth;
            cheight = vi.vi_cheight;
        }
#endif /* defined(FBIO_MODEINFO) || \
        * defined(CONS_MODEINFO) */
        mySignal(SIGUSR2, SIG_IGN);
        mi.operation = MOUSE_MODE;
        mi.u.mode.mode = 0;
        mi.u.mode.signal = SIGUSR2;
        sysm_handler = NULL;
        if (ioctl(tty, CONS_MOUSECTL, &mi) != -1)
        {
            mySignal(SIGUSR2, sysmouse);
            mi.operation = MOUSE_SHOW;
            ioctl(tty, CONS_MOUSECTL, &mi);
            sysm_handler = sysm_process_mouse;
        }
    }
    mouseActive = 1;
}

void mouse_end()
{
    if (mouseActive == 0)
        return;
    if (is_xterm)
    {
        XTERM_OFF;
    }
    else
    {
        mouse_info_t mi;
        mi.operation = MOUSE_MODE;
        mi.u.mode.mode = 0;
        mi.u.mode.signal = 0;
        ioctl(tty, CONS_MOUSECTL, &mi);
    }
    mouseActive = 0;
}

#else
/* not GPM nor SYSMOUSE, but use mouse with xterm */

void mouse_init()
{
    if (mouseActive)
        return;
    if (is_xterm & NEED_XTERM_ON)
    {
        XTERM_ON;
    }
    mouseActive = 1;
}

void mouse_end()
{
    if (mouseActive == 0)
        return;
    if (is_xterm & NEED_XTERM_OFF)
    {
        XTERM_OFF;
    }
    mouseActive = 0;
}

#endif /* not USE_GPM nor USE_SYSMOUSE */

void mouse_active()
{
    if (!mouseActive)
        mouse_init();
}

void mouse_inactive()
{
    if (mouseActive && is_xterm)
        mouse_end();
}

#endif /* USE_MOUSE */

void flush_tty()
{
    if (ttyf)
        fflush(ttyf);
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
