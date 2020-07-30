/* $Id: etc.c,v 1.81 2007/05/23 15:06:05 inu Exp $ */

#include "fm.h"

#include "indep.h"
#include "file.h"
#ifndef __MINGW32_VERSION
#include <pwd.h>
#endif
#include "myctype.h"
#include "html/html.h"
#include "transport/local.h"
#include "html/tagtable.h"
#include "frontend/terms.h"
#include "frontend/display.h"
#include "frontend/buffer.h"
#include "transport/url.h"

// #include <sys/socket.h>
#include <netdb.h>

#include <fcntl.h>
#include <sys/types.h>

#if defined(HAVE_WAITPID) || defined(HAVE_WAIT3)
#include <sys/wait.h>
#endif
#include <signal.h>

#ifdef __WATT32__
#define read(a, b, c) read_s(a, b, c)
#define close(x) close_s(x)
#endif /* __WATT32__ */



int columnSkip(BufferPtr buf, int offset)
{
    int i, maxColumn;
    int column = buf->currentColumn + offset;
    int nlines = buf->LINES + 1;
    Line *l;

    maxColumn = 0;
    for (i = 0, l = buf->topLine; i < nlines && l != NULL; i++, l = l->next)
    {
        if (l->width < 0)
            l->CalcWidth();
        if (l->width - 1 > maxColumn)
            maxColumn = l->width - 1;
    }
    maxColumn -= buf->COLS - 1;
    if (column < maxColumn)
        maxColumn = column;
    if (maxColumn < 0)
        maxColumn = 0;

    if (buf->currentColumn == maxColumn)
        return 0;
    buf->currentColumn = maxColumn;
    return 1;
}

int columnPos(Line *line, int column)
{
    int i;

    for (i = 1; i < line->len; i++)
    {
        if (line->COLPOS(i) > column)
            break;
    }
    for (i--; i > 0 && line->propBuf[i] & PC_WCHAR2; i--)
        ;
    return i;
}
