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
#include <termios.h>
#include <unistd.h>

static const char *title_str = NULL;

int graph_ok(void)
{
    if (w3mApp::Instance().UseGraphicChar != GRAPHIC_CHAR_DEC)
        return 0;
    return T_as[0] != 0 && T_ae[0] != 0 && T_ac[0] != 0;
}

void refresh(void)
{
    Screen::Instance().Refresh();
    Terminal::flush();
}

/* XXX: conflicts with curses's clrtoeol(3) ? */
void clrtoeol(void)
{ /* Clear to the end of line */
    Screen::Instance().CtrlToEol();
}

void clrtoeol_with_bcolor(void)
{
    Screen::Instance().CtrlToEolWithBGColor();
}

void clrtoeolx(void)
{
    clrtoeol_with_bcolor();
}

void clrtobot_eol(void (*clrtoeol)())
{
    Screen::Instance().CtrlToBottomEol();
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
        Screen::Instance().Puts(s, len);
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
        Screen::Instance().Puts(s, len);
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
        Screen::Instance().Puts(s, len);
        s += len;
        i += width;
    }
    for (; i < n; i++)
        Screen::Instance().Putc(' ');
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
