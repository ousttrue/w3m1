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

bool Tab::IsConnectFirstCurrent() const
{
    auto c = GetCurrentBuffer();
    if (!c)
    {
        return false;
    }
    for (auto &buf : m_buffers)
    {
        if (buf == c)
        {
            return true;
        }
    }
    return false;
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

bool Tab::Forward()
{
    if (m_buffers.empty())
    {
        return false;
    }
    if (m_current <= 0)
    {
        return false;
    }
    --m_current;
    return true;
}

bool Tab::Back()
{
    if (m_current >= m_buffers.size() - 1)
    {
        return false;
    }
    ++m_current;
    return true;
}

void Tab::DeleteBack()
{
    auto it = findCurrent();
    if (it == m_buffers.end())
    {
        return;
    }
    ++it;
    if (it == m_buffers.end())
    {
        return;
    }
    m_buffers.erase(it);
}

BufferPtr Tab::ForwardBuffer(const BufferPtr &buf) const
{
    if (buf == m_buffers.front())
    {
        return nullptr;
    }
    auto it = find(buf);
    --it;
    return *it;
}

BufferPtr Tab::BackBuffer(const BufferPtr &buf) const
{
    auto it = find(buf);
    if (it == m_buffers.end())
    {
        assert(false);
        return nullptr;
    }
    ++it;
    return *it;
}

void Tab::SetCurrentBuffer(const BufferPtr &buf)
{
    auto c = GetCurrentBuffer();
    if (c == buf)
    {
        return;
    }
    if (c)
    {
        deleteImage(c.get());
        if (clear_buffer)
        {
            c->TmpClear();
        }
    }

    m_current = GetBufferIndex(buf);
    assert(IsConnectFirstCurrent());
}

// currentPrev -> buf -> current
void Tab::Push(const BufferPtr &buf)
{
    if (m_buffers.empty())
    {
        m_buffers.push_back(buf);
        SetCurrentBuffer(buf);
        return;
    }

    auto it = findCurrent();
    assert(it != m_buffers.end());
    m_buffers.insert(it, buf);
    SetCurrentBuffer(buf);
#ifdef USE_BUFINFO
    saveBufferInfo();
#endif
}

// void Tab::PushBufferCurrentNext(const BufferPtr &buf)
// {
//     auto it = findCurrent();
//     assert(it != buffers.end());
//     ++it;
//     buffers.insert(it, buf);
//     SetCurrentBuffer(buf);
//     SetCurrentBuffer(buf);
// }

/* 
 * namedBuffer: Select buffer which have specified name
 */
BufferPtr
Tab::NamedBuffer(const char *name) const
{
    for (auto &buf : m_buffers)
    {
        if (buf->buffername == name)
        {
            return buf;
        }
    }
    return nullptr;
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
BufferPtr Tab::SelectBuffer(BufferPtr currentbuf, char *selectchar)
{
    int i, cpoint,                     /* Current Buffer Number */
        spoint,                        /* Current Line on Screen */
        maxbuf, sclimit = (LINES - 1); /* Upper limit of line * number in 
					 * the * screen */

    BufferPtr topbuf;
    char c;

    i = cpoint = 0;
    for (auto &buf : m_buffers)
    {
        if (buf == currentbuf)
            cpoint = i;
        i++;
    }
    maxbuf = i;

    if (cpoint >= sclimit)
    {
        spoint = sclimit / 2;
        topbuf = GetBuffer(cpoint - spoint);
    }
    else
    {
        topbuf = m_buffers.front();
        spoint = cpoint;
    }
    listBuffer(this, topbuf, currentbuf);

    auto it = findCurrent();
    for (;;)
    {
        if ((c = getch()) == ESC_CODE)
        {
            if ((c = getch()) == '[' || c == 'O')
            {
                switch (c = getch())
                {
                case 'A':
                    c = 'k';
                    break;
                case 'B':
                    c = 'j';
                    break;
                case 'C':
                    c = ' ';
                    break;
                case 'D':
                    c = 'B';
                    break;
                }
            }
        }
#ifdef __EMX__
        else if (!c)
            switch (getch())
            {
            case K_UP:
                c = 'k';
                break;
            case K_DOWN:
                c = 'j';
                break;
            case K_RIGHT:
                c = ' ';
                break;
            case K_LEFT:
                c = 'B';
            }
#endif
        switch (c)
        {
        case CTRL_N:
        case 'j':
        {
            auto next = it;
            ++next;
            if (spoint < sclimit - 1)
            {
                if (next == m_buffers.end())
                    continue;
                writeBufferName(currentbuf, spoint);
                currentbuf = *next;
                cpoint++;
                spoint++;
                standout();
                writeBufferName(currentbuf, spoint);
                standend();
                move(spoint, 0);
                toggle_stand();
            }
            else if (cpoint < maxbuf - 1)
            {
                topbuf = currentbuf;
                currentbuf = *next;
                cpoint++;
                spoint = 1;
                listBuffer(this, topbuf, currentbuf);
            }
            break;
        }
        case CTRL_P:
        case 'k':
        {
            if (spoint > 0)
            {
                writeBufferName(currentbuf, spoint);
                currentbuf = topbuf;
                // for (int i = 0; i < --spoint && currentbuf; ++i)
                // {
                //     currentbuf = currentbuf->nextBuffer;
                // }
                --it;
                cpoint--;
                standout();
                writeBufferName(currentbuf, spoint);
                standend();
                move(spoint, 0);
                toggle_stand();
            }
            else if (cpoint > 0)
            {
                i = cpoint - sclimit;
                if (i < 0)
                    i = 0;
                cpoint--;
                spoint = cpoint - i;
                currentbuf = GetBuffer(cpoint);
                topbuf = GetBuffer(i);
                listBuffer(this, topbuf, currentbuf);
            }
            break;
        }
        default:
            *selectchar = c;
            return currentbuf;
        }
        /* 
	 * move((LINES-1), COLS - 1);
	 */
        move(spoint, 0);
        refresh();
    }
}

/* 
 * deleteBuffer: delete buffer & return fistbuffer
 */
void Tab::DeleteBuffer(const BufferPtr &delbuf)
{
    auto it = find(delbuf);
    if (it == m_buffers.end())
    {
        // TODO:
        return;
    }
    auto c = GetCurrentBuffer();
    if (*it == c)
    {
        // assert(false);
        // TODO:
    }
    auto next = m_buffers.erase(it);
    if (next == m_buffers.end())
    {
        m_current = m_buffers.size() - 1;
    }
    else
    {
        m_current = GetBufferIndex(*next);
    }
}

bool Tab::CheckBackBuffer()
{
    auto it = find(GetCurrentBuffer());
    if (it == m_buffers.end())
    {
        return false;
    }
    ++it;

    // TODO:
    // BufferPtr fbuf = currentBuffer->linkBuffer[LB_N_FRAME];
    // if (fbuf)
    // {
    //     if (fbuf->frameQ)
    //     {
    //         return TRUE; /* Currentbuf has stacked frames */
    //     }

    //     /* when no frames stacked and next is frame source, try next's
    //     * nextBuffer */

    //     if (w3mApp::Instance().RenderFrame && fbuf == tab->BackBuffer(buf))
    //     {
    //         if (tab->BackBuffer(fbuf) != NULL)
    //             return TRUE;
    //         else
    //             return FALSE;
    //     }
    // }

    if (it == m_buffers.end())
    {
        return false;
    }

    return TRUE;
}
