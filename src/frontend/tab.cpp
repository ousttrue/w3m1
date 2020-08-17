#include "frontend/tab.h"
#include "fm.h"
#include "indep.h"
#include "file.h"
#include "frontend/display.h"
#include "dispatcher.h"
#include "frontend/mouse.h"
#include "public.h"
#include "commands.h"
#include "frontend/buffer.h"
#include "html/anchor.h"
#include "html/image.h"
#include "frontend/terms.h"
#include "ctrlcode.h"
#include <stdexcept>
#include <algorithm>
#include <assert.h>

Tab::~Tab()
{
}

int Tab::GetBufferIndex(const BufferPtr &b) const
{
    int i = 0;
    for (auto &buf : m_buffers)
    {
        if (buf == b)
        {
            return i;
        }
        ++i;
    }
    return -1;
}

void Tab::DeleteBack()
{
    assert(false);
    // auto it = findCurrent();
    // if (it == m_buffers.end())
    // {
    //     return;
    // }
    // ++it;
    // if (it == m_buffers.end())
    // {
    //     return;
    // }
    // m_buffers.erase(it);
}

void Tab::Push(const BufferPtr &buf)
{
    if (m_buffers.size() > m_current + 1)
    {
        // drop
        m_buffers.resize(m_current + 1);
    }
    m_buffers.push_back(buf);
    m_current = m_buffers.size() - 1;
}

static void
writeBufferName(BufferPtr buf, int n)
{
    auto all = buf->LineCount();
    move(n, 0);
    /* FIXME: gettextize? */
    auto msg = Sprintf("<%s> [%d lines]", buf->buffername, all);
    if (buf->filename.size())
    {
        switch (buf->currentURL.scheme)
        {
        case SCM_LOCAL:
        case SCM_LOCAL_CGI:
            if (buf->currentURL.path != "-")
            {
                msg->Push(' ');
                msg->Push(conv_from_system(buf->currentURL.real_file));
            }
            break;
        case SCM_UNKNOWN:
        case SCM_MISSING:
            break;
        default:
            msg->Push(' ');
            msg->Push(buf->currentURL.ToStr());
            break;
        }
    }
    addnstr_sup(msg->ptr, COLS - 1);
}

static BufferPtr
listBuffer(Tab *tab, BufferPtr top, BufferPtr current)
{
    move(0, 0);

    if (w3mApp::Instance().useColor)
    {
        setfcolor(basic_color);
        setbcolor(bg_color);
    }

    BufferPtr buf = top;
    int i, c = 0;
    clrtobotx();
    for (i = 0; i < (LINES - 1); i++)
    {
        if (buf == current)
        {
            c = i;
            standout();
        }
        writeBufferName(buf, i);
        if (buf == current)
        {
            standend();
            clrtoeolx();
            move(i, 0);
            toggle_stand();
        }
        else
            clrtoeolx();
        if (!tab->Back())
        {
            move(i + 1, 0);
            clrtobotx();
            break;
        }
        buf = tab->GetCurrentBuffer();
    }
    standout();
    /* FIXME: gettextize? */
    message("Buffer selection mode: SPC for select / D for delete buffer", 0,
            0);
    standend();
    /* 
     * move((LINES-1), COLS - 1); */
    move(c, 0);
    refresh();
    // return tab->BackBuffer(buf);
    return buf;
}

/* 
 * Select buffer visually
 */
// BufferPtr Tab::SelectBuffer(BufferPtr currentbuf, char *selectchar)
// {
//     int i, cpoint,                     /* Current Buffer Number */
//         spoint,                        /* Current Line on Screen */
//         maxbuf, sclimit = (LINES - 1); /* Upper limit of line * number in 
// 					 * the * screen */

//     BufferPtr topbuf;
//     char c;

//     i = cpoint = 0;
//     for (auto &buf : m_buffers)
//     {
//         if (buf == currentbuf)
//             cpoint = i;
//         i++;
//     }
//     maxbuf = i;

//     if (cpoint >= sclimit)
//     {
//         spoint = sclimit / 2;
//         topbuf = GetBuffer(cpoint - spoint);
//     }
//     else
//     {
//         topbuf = m_buffers.front();
//         spoint = cpoint;
//     }
//     listBuffer(this, topbuf, currentbuf);

//     auto it = findCurrent();
//     for (;;)
//     {
//         if ((c = getch()) == ESC_CODE)
//         {
//             if ((c = getch()) == '[' || c == 'O')
//             {
//                 switch (c = getch())
//                 {
//                 case 'A':
//                     c = 'k';
//                     break;
//                 case 'B':
//                     c = 'j';
//                     break;
//                 case 'C':
//                     c = ' ';
//                     break;
//                 case 'D':
//                     c = 'B';
//                     break;
//                 }
//             }
//         }
// #ifdef __EMX__
//         else if (!c)
//             switch (getch())
//             {
//             case K_UP:
//                 c = 'k';
//                 break;
//             case K_DOWN:
//                 c = 'j';
//                 break;
//             case K_RIGHT:
//                 c = ' ';
//                 break;
//             case K_LEFT:
//                 c = 'B';
//             }
// #endif
//         switch (c)
//         {
//         case CTRL_N:
//         case 'j':
//         {
//             auto next = it;
//             ++next;
//             if (spoint < sclimit - 1)
//             {
//                 if (next == m_buffers.end())
//                     continue;
//                 writeBufferName(currentbuf, spoint);
//                 currentbuf = *next;
//                 cpoint++;
//                 spoint++;
//                 standout();
//                 writeBufferName(currentbuf, spoint);
//                 standend();
//                 move(spoint, 0);
//                 toggle_stand();
//             }
//             else if (cpoint < maxbuf - 1)
//             {
//                 topbuf = currentbuf;
//                 currentbuf = *next;
//                 cpoint++;
//                 spoint = 1;
//                 listBuffer(this, topbuf, currentbuf);
//             }
//             break;
//         }
//         case CTRL_P:
//         case 'k':
//         {
//             if (spoint > 0)
//             {
//                 writeBufferName(currentbuf, spoint);
//                 currentbuf = topbuf;
//                 // for (int i = 0; i < --spoint && currentbuf; ++i)
//                 // {
//                 //     currentbuf = currentbuf->nextBuffer;
//                 // }
//                 --it;
//                 cpoint--;
//                 standout();
//                 writeBufferName(currentbuf, spoint);
//                 standend();
//                 move(spoint, 0);
//                 toggle_stand();
//             }
//             else if (cpoint > 0)
//             {
//                 i = cpoint - sclimit;
//                 if (i < 0)
//                     i = 0;
//                 cpoint--;
//                 spoint = cpoint - i;
//                 currentbuf = GetBuffer(cpoint);
//                 topbuf = GetBuffer(i);
//                 listBuffer(this, topbuf, currentbuf);
//             }
//             break;
//         }
//         default:
//             *selectchar = c;
//             return currentbuf;
//         }
//         /* 
// 	 * move((LINES-1), COLS - 1);
// 	 */
//         move(spoint, 0);
//         refresh();
//     }
// }
