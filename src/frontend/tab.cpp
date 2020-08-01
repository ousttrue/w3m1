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
    for (auto &buf : buffers)
    {
        if (buf == currentBuffer)
        {
            return true;
        }
    }
    return false;
}

int Tab::GetCurrentBufferIndex() const
{
    int i = 0;
    for (auto &buf : buffers)
    {
        if (buf == GetCurrentBuffer())
        {
            return i;
        }
        ++i;
    }
    return -1;
}

BufferPtr Tab::PrevBuffer(BufferPtr buf) const
{
    if (buf == buffers.front())
    {
        return nullptr;
    }
    auto it = find(buf);
    --it;
    return *it;
}

BufferPtr Tab::NextBuffer(BufferPtr buf) const
{
    auto it = find(buf);
    if (it == buffers.end())
    {
        assert(false);
        return nullptr;
    }
    ++it;
    return *it;
}

void Tab::SetFirstBuffer(BufferPtr buffer)
{
    // firstBuffer = buffer;
    // if (isCurrent)
    // {
    //     SetCurrentBuffer(buffer);
    // }

    buffers.clear();
    buffers.push_back(buffer);
    currentBuffer = buffer;
}

void Tab::SetCurrentBuffer(BufferPtr buffer)
{
    if (currentBuffer == buffer)
    {
        return;
    }
    if (currentBuffer)
    {
        deleteImage(currentBuffer);
        if (clear_buffer)
        {
            currentBuffer->TmpClear();
        }
    }

    currentBuffer = buffer;
    assert(IsConnectFirstCurrent());
}

// currentPrev -> buf -> current
void Tab::PushBufferCurrentPrev(BufferPtr buf)
{
    if (buffers.empty())
    {
        buffers.push_back(buf);
        SetCurrentBuffer(buf);
        return;
    }

    auto it = find(currentBuffer);
    assert(it != buffers.end());
    buffers.insert(it, buf);
    SetCurrentBuffer(buf);
#ifdef USE_BUFINFO
    saveBufferInfo();
#endif
}

void Tab::PushBufferCurrentNext(BufferPtr buf)
{
    auto it = find(currentBuffer);
    assert(it != buffers.end());
    ++it;
    buffers.insert(it, buf);
    SetCurrentBuffer(buf);
    SetCurrentBuffer(buf);
}

BufferPtr
Tab::GetBuffer(int n) const
{
    if (n <= 0)
        return buffers.front();

    auto it = buffers.begin();
    for (int i = 0; i < n; i++, ++it)
    {
        if (it == buffers.end())
        {
            return nullptr;
        }
    }
    return *it;
}

/* 
 * namedBuffer: Select buffer which have specified name
 */
BufferPtr
Tab::NamedBuffer(const char *name) const
{
    for (auto &buf : buffers)
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
            if (buf->currentURL.file != "-")
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
listBuffer(const Tab *tab, BufferPtr top, BufferPtr current)
{
    int i, c = 0;
    BufferPtr buf = top;

    move(0, 0);
#ifdef USE_COLOR
    if (useColor)
    {
        setfcolor(basic_color);
#ifdef USE_BG_COLOR
        setbcolor(bg_color);
#endif /* USE_BG_COLOR */
    }
#endif /* USE_COLOR */
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
        if (tab->NextBuffer(buf))
        {
            move(i + 1, 0);
            clrtobotx();
            break;
        }
        buf = tab->NextBuffer(buf);
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
    return tab->NextBuffer(buf);
}

/* 
 * Select buffer visually
 */
BufferPtr Tab::SelectBuffer(BufferPtr currentbuf, char *selectchar) const
{
    int i, cpoint,                     /* Current Buffer Number */
        spoint,                        /* Current Line on Screen */
        maxbuf, sclimit = (LINES - 1); /* Upper limit of line * number in 
					 * the * screen */

    BufferPtr topbuf;
    char c;

    i = cpoint = 0;
    for (auto &buf : buffers)
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
        topbuf = buffers.front();
        spoint = cpoint;
    }
    listBuffer(this, topbuf, currentbuf);

    auto it = find(currentBuffer);
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
                if (next == buffers.end())
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
void Tab::DeleteBuffer(BufferPtr delbuf)
{
    auto it = find(delbuf);
    if(it==buffers.end())
    {
        // TODO:
        return;
    }
    if (*it == currentBuffer)
    {
        assert(false);
        // TODO:
    }
    buffers.erase(it);
}

/* 
 * replaceBuffer: replace buffer
 */
void Tab::ReplaceBuffer(BufferPtr delbuf, BufferPtr newbuf)
{
    // TODO:
    assert(false);
}
