#include <plog/Log.h>
#include "frontend/tab.h"
#include "frontend/display.h"
#include "frontend/terminal.h"
#include "frontend/screen.h"
#include "indep.h"
#include "w3m.h"
#include "loader.h"
#include <stdexcept>
#include <algorithm>
#include <assert.h>

Tab::~Tab()
{
}

bool Tab::SetCurrent(int index)
{
    if (index < 0 || index >= m_history.size())
    {
        return false;
    }
    if (index == m_current)
    {
        return false;
    }
    m_current = index;

    auto url = m_history[index];
    m_content = GetStream(url, m_content ? &m_content->url : nullptr);
    if (!m_content)
    {
        LOGE << "fail to GetStream";
        return false;
    }

    LOGI << m_content->content_type << ";" << m_content->content_charset;

    return true;
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

void Tab::Push(const URL &url)
{
    LOGI << url;

    if (m_history.size() > m_current + 1)
    {
        // drop
        m_history.resize(m_current + 1);
    }

    m_history.push_back(url);
    SetCurrent(m_history.size() - 1);
}

static void
writeBufferName(BufferPtr buf, int n)
{
    auto all = buf->LineCount();
    Screen::Instance().Move(n, 0);
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
    Screen::Instance().PutsColumns(msg->ptr, Terminal::columns() - 1);
}

static BufferPtr
listBuffer(Tab *tab, BufferPtr top, BufferPtr current)
{
    Screen::Instance().Move(0, 0);

    if (w3mApp::Instance().useColor)
    {
        Screen::Instance().SetFGColor(w3mApp::Instance().basic_color);
        Screen::Instance().SetBGColor(w3mApp::Instance().bg_color);
    }

    BufferPtr buf = top;
    int i, c = 0;
    Screen::Instance().CtrlToBottomEol();
    for (i = 0; i < (Terminal::lines() - 1); i++)
    {
        if (buf == current)
        {
            c = i;
            Screen::Instance().Enable(S_STANDOUT);
        }
        writeBufferName(buf, i);
        if (buf == current)
        {
            Screen::Instance().Disable(S_STANDOUT);
            Screen::Instance().CtrlToEolWithBGColor();
            Screen::Instance().Move(i, 0);
            Screen::Instance().StandToggle();
        }
        else
            Screen::Instance().CtrlToEolWithBGColor();
        if (!tab->Back())
        {
            Screen::Instance().Move(i + 1, 0);
            Screen::Instance().CtrlToBottomEol();
            break;
        }
        buf = GetCurrentBuffer();
    }
    Screen::Instance().Enable(S_STANDOUT);
    /* FIXME: gettextize? */
    message("Buffer selection mode: SPC for select / D for delete buffer", 0, 0);
    Screen::Instance().Disable(S_STANDOUT);
    /* 
     * move((Terminal::lines()-1), Terminal::columns() - 1); */
    Screen::Instance().Move(c, 0);
    Screen::Instance().Refresh();
    Terminal::flush();

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
//         maxbuf, sclimit = (Terminal::lines() - 1); /* Upper limit of line * number in
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
// 	 * move((Terminal::lines()-1), Terminal::columns() - 1);
// 	 */
//         move(spoint, 0);
//         refresh();
//     }
// }
