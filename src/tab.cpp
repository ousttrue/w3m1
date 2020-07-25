#include "tab.h"
#include "fm.h"
#include "indep.h"
#include "file.h"
#include "display.h"
#include "dispatcher.h"
#include "mouse.h"
#include "public.h"
#include "commands.h"
#include "buffer.h"
#include "anchor.h"
#include "terms.h"
#include "ctrlcode.h"
#include <stdexcept>
#include <algorithm>
#include <assert.h>

using TabList = std::list<TabPtr>;
TabList g_tabs;
TabList::iterator find(const TabPtr &tab)
{
    return std::find(g_tabs.begin(), g_tabs.end(), tab);
}

std::weak_ptr<Tab> g_current;

void EachTab(const std::function<void(const TabPtr &)> callback)
{
    for (auto &tab : g_tabs)
    {
        callback(tab);
    }
}

bool Tab::IsConnectFirstCurrent() const
{
    for (auto buf = firstBuffer; buf; buf = buf->nextBuffer)
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
    for (auto buf = firstBuffer; buf; buf = buf->nextBuffer, ++i)
    {
        if (buf == GetCurrentBuffer())
        {
            return i;
        }
    }
    return -1;
}

BufferPtr Tab::PrevBuffer(BufferPtr buf) const
{
    BufferPtr b = firstBuffer;
    for (; b != NULL && b->nextBuffer != buf; b = b->nextBuffer)
        ;

    assert(b->nextBuffer == buf);

    return b;
}

BufferPtr Tab::NextBuffer(BufferPtr buf) const
{
    return buf->nextBuffer;
}

void Tab::SetFirstBuffer(BufferPtr buffer, bool isCurrent)
{
    firstBuffer = buffer;
    if (isCurrent)
    {
        currentBuffer = buffer;
    }

    if (!IsConnectFirstCurrent())
    {
        currentBuffer = nullptr;
    }
}

void Tab::SetCurrentBuffer(BufferPtr buffer)
{
    currentBuffer = buffer;
    assert(IsConnectFirstCurrent());
}

// currentPrev -> buf -> current
void Tab::PushBufferCurrentPrev(BufferPtr buf)
{
    deleteImage(GetCurrentBuffer());
    if (clear_buffer)
    {
        tmpClearBuffer(GetCurrentBuffer());
    }

    // if (GetCurrentTab()->GetFirstBuffer() == GetCurrentTab()->GetCurrentBuffer())
    {
        buf->nextBuffer = GetCurrentBuffer();
        SetFirstBuffer(buf, true);
    }
    // else
    // {
    //     auto b = prevBuffer(GetCurrentTab()->GetFirstBuffer(), GetCurrentTab()->GetCurrentBuffer());
    //     b->nextBuffer = buf;
    //     buf->nextBuffer = GetCurrentTab()->GetCurrentBuffer();
    //     GetCurrentTab()->SetCurrentBuffer(buf);
    // }
#ifdef USE_BUFINFO
    saveBufferInfo();
#endif
}

void Tab::PushBufferCurrentNext(BufferPtr buf)
{
    deleteImage(GetCurrentBuffer());
    if (clear_buffer)
    {
        tmpClearBuffer(GetCurrentBuffer());
    }

    // if (GetCurrentTab()->GetFirstBuffer() == GetCurrentTab()->GetCurrentBuffer())
    {
        GetCurrentBuffer()->nextBuffer = buf;
        SetCurrentBuffer(buf);
    }
}

BufferPtr
Tab::GetBuffer(int n) const
{
    if (n <= 0)
        return firstBuffer;

    BufferPtr buf = firstBuffer;
    for (int i = 0; i < n && buf; i++)
    {
        buf = buf->nextBuffer;
    }
    return buf;
}

/* 
 * namedBuffer: Select buffer which have specified name
 */
BufferPtr
Tab::NamedBuffer(const char *name) const
{
    BufferPtr buf;

    if (firstBuffer->buffername == name)
    {
        return firstBuffer;
    }
    for (buf = firstBuffer; buf->nextBuffer; buf = buf->nextBuffer)
    {
        if (buf->nextBuffer->buffername == name)
        {
            return buf->nextBuffer;
        }
    }
    return NULL;
}

static void
writeBufferName(BufferPtr buf, int n)
{
    Str msg;
    int all;

    all = buf->allLine;
    if (all == 0 && buf->lastLine != NULL)
        all = buf->lastLine->linenumber;
    move(n, 0);
    /* FIXME: gettextize? */
    msg = Sprintf("<%s> [%d lines]", buf->buffername, all);
    if (buf->filename != NULL)
    {
        switch (buf->currentURL.scheme)
        {
        case SCM_LOCAL:
        case SCM_LOCAL_CGI:
            if (strcmp(buf->currentURL.file, "-"))
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
            msg->Push(parsedURL2Str(&buf->currentURL));
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
    BufferPtr buf;
    BufferPtr topbuf;
    char c;

    i = cpoint = 0;
    for (buf = firstBuffer; buf != NULL; buf = buf->nextBuffer)
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
        topbuf = firstBuffer;
        spoint = cpoint;
    }
    listBuffer(this, topbuf, currentbuf);

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
            if (spoint < sclimit - 1)
            {
                if (currentbuf->nextBuffer == NULL)
                    continue;
                writeBufferName(currentbuf, spoint);
                currentbuf = currentbuf->nextBuffer;
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
                currentbuf = currentbuf->nextBuffer;
                cpoint++;
                spoint = 1;
                listBuffer(this, topbuf, currentbuf);
            }
            break;
        case CTRL_P:
        case 'k':
            if (spoint > 0)
            {
                writeBufferName(currentbuf, spoint);
                currentbuf = topbuf;
                for (int i = 0; i < --spoint && currentbuf; ++i)
                {
                    currentbuf = currentbuf->nextBuffer;
                }
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
    if (firstBuffer == delbuf && firstBuffer->nextBuffer != NULL)
    {
        firstBuffer = firstBuffer->nextBuffer;
        if (delbuf == currentBuffer)
        {
            currentBuffer = firstBuffer;
        }
    }
    else
    {
        auto buf = PrevBuffer(delbuf);
        buf->nextBuffer = delbuf->nextBuffer;
        if (delbuf == currentBuffer)
        {
            currentBuffer = buf->nextBuffer;
        }
    }

    // if (GetCurrentTab()->GetCurrentBuffer() == buf)
    //     GetCurrentTab()->SetCurrentBuffer(buf->nextBuffer);
    // if (!GetCurrentTab()->GetCurrentBuffer())
    //     GetCurrentTab()->SetCurrentBuffer(GetCurrentTab()->GetBuffer(i - 1));
    // if (GetCurrentTab()->GetFirstBuffer())
    // {
    //     GetCurrentTab()->SetFirstBuffer(nullBuffer());
    //     GetCurrentTab()->SetCurrentBuffer(GetCurrentTab()->GetFirstBuffer());
    // }
}

void Tab::ClearExceptCurrentBuffer()
{
    for (auto buf = firstBuffer; buf != NULL; buf = buf->nextBuffer)
    {
        if (buf == GetCurrentBuffer())
            continue;
        deleteImage(buf);
        if (clear_buffer)
            tmpClearBuffer(buf);
    }
}

/* 
 * replaceBuffer: replace buffer
 */
void Tab::ReplaceBuffer(BufferPtr delbuf, BufferPtr newbuf)
{
    BufferPtr buf;

    if (delbuf == NULL)
    {
        newbuf->nextBuffer = firstBuffer;
    }
    if (firstBuffer == delbuf)
    {
        newbuf->nextBuffer = delbuf->nextBuffer;
    }
    if (delbuf && (buf = PrevBuffer(delbuf)))
    {
        buf->nextBuffer = newbuf;
        newbuf->nextBuffer = delbuf->nextBuffer;
    }
    newbuf->nextBuffer = firstBuffer;
}

void InitializeTab()
{
    g_tabs.push_back(std::make_shared<Tab>());
    g_current = g_tabs.front();
}

int GetTabCount()
{
    return g_tabs.size();
}

int GetTabbarHeight()
{
    // 最後のtabのy + 境界線(1)
    return GetLastTab()->Y() + 1;
}

TabPtr GetTabByIndex(int index)
{
    int i = 0;
    for (auto tab : g_tabs)
    {
        if (i == index)
        {
            return tab;
        }
        ++i;
    }
    throw std::runtime_error("not found");
}

TabPtr GetFirstTab()
{
    return g_tabs.empty() ? nullptr : g_tabs.front();
}

TabPtr GetLastTab()
{
    return g_tabs.empty() ? nullptr : g_tabs.back();
}

TabPtr GetCurrentTab()
{
    auto current = g_current.lock();
    return current ? current : nullptr;
}

int GetCurrentTabIndex()
{
    auto current = g_current.lock();

    int i = 0;
    for (auto tab : g_tabs)
    {
        if (tab == current)
        {
            return i;
        }
        ++i;
    }
    throw std::runtime_error("not found");
}

void SetCurrentTab(TabPtr current)
{
    for (auto tab : g_tabs)
    {
        if (tab == current)
        {
            g_current = tab;
        }
    }
}

void calcTabPos()
{
    int lcol = GetMouseActionMenuStr() ? GetMouseActionMenuWidth() : 0;

    if (g_tabs.size() <= 0)
        return;
    int n1 = (COLS - lcol) / TabCols;
    int n2;
    int ny;
    if (n1 >= g_tabs.size())
    {
        n2 = 1;
        ny = 1;
    }
    else
    {
        if (n1 < 0)
            n1 = 0;
        n2 = COLS / TabCols;
        if (n2 == 0)
            n2 = 1;
        ny = (g_tabs.size() - n1 - 1) / n2 + 2;
    }
    int na = n1 + n2 * (ny - 1);
    n1 -= (na - g_tabs.size()) / ny;
    if (n1 < 0)
        n1 = 0;
    na = n1 + n2 * (ny - 1);
    auto it = g_tabs.begin();
    for (int iy = 0; iy < ny && *it; iy++)
    {
        int col;
        int nx;
        if (iy == 0)
        {
            nx = n1;
            col = COLS - lcol;
        }
        else
        {
            nx = n2 - (na - g_tabs.size() + (iy - 1)) / (ny - 1);
            col = COLS;
        }
        int ix = 0;
        for (ix = 0; ix < nx && it != g_tabs.end(); ix++, ++it)
        {
            (*it)->Calc(col / nx, ix, iy, lcol);
        }
    }
}

TabPtr posTab(int x, int y)
{
    TabPtr tab;

    if (GetMouseActionMenuStr() && x < GetMouseActionMenuWidth() && y == 0)
        return nullptr;
    for (auto tab : g_tabs)
    {
        if (tab->IsHit(x, y))
        {
            return tab;
        }
    }
    return NULL;
}

// setup
void _newT()
{
    // setup buffer
    auto buf = GetCurrentTab()->GetCurrentBuffer()->Copy();
    buf->ClearLink();

    // new tab
    auto current = g_current.lock();
    auto found = find(current);
    ++found;
    auto tab = std::make_shared<Tab>();
    g_tabs.insert(found, tab);

    g_current = tab;
    tab->SetFirstBuffer(buf, true);
}

void deleteTab(TabPtr tab)
{
    if (g_tabs.size() <= 1)
        return;

    auto found = find(tab);

    bool isCurrent = (*found == g_current.lock());

    auto eraseNext = g_tabs.erase(found);

    if (isCurrent)
    {
        // update current tab
        if (eraseNext != g_tabs.end())
        {
            g_current = *eraseNext;
        }
        else
        {
            g_current = g_tabs.back();
        }
    }

    return;
}

void DeleteCurrentTab()
{
    deleteTab(g_current.lock());
}

void DeleteAllTabs()
{
    g_tabs.clear();
}

void moveTab(TabPtr src, TabPtr dst, int right)
{
    if (!src || !dst || src == dst)
        return;

    auto s = find(src);
    auto d = find(dst);

    if (right)
    {
        ++d;
    }

    auto end = s;
    ++end;
    g_tabs.insert(d, s, end);
}

void SelectRelativeTab(int prec)
{
    if (g_tabs.size() <= 1)
    {
        return;
    }
    if (prec == 0)
    {
        return;
    }

    auto current = GetCurrentTabIndex();
    auto index = current + prec;
    if (index < 0)
    {
        index = 0;
    }
    else if (index >= GetTabCount())
    {
        index = GetTabCount() - 1;
    }

    SetCurrentTab(GetTabByIndex(index));
}

void SelectTabByPosition(int x, int y)
{
    TabPtr tab = posTab(x, y);
    if (!tab)
        return;
    SetCurrentTab(tab);
}

void MoveTab(int x)
{
    if (x == 0)
    {
        return;
    }
    auto current = GetCurrentTabIndex();
    auto index = current + x;
    if (index < 0)
    {
        index = 0;
    }
    else if (index >= GetTabCount())
    {
        index = GetTabCount() - 1;
    }
    auto tab = GetTabByIndex(index);
    moveTab(GetCurrentTab(), tab ? tab : GetLastTab(), x > 0);
}

void followTab(TabPtr tab)
{
    BufferPtr buf;

    auto a = retrieveCurrentImg(GetCurrentTab()->GetCurrentBuffer());
    if (!(a && a->image && a->image->map))

        a = retrieveCurrentAnchor(GetCurrentTab()->GetCurrentBuffer());
    if (a == NULL)
        return;

    if (tab == GetCurrentTab())
    {
        set_check_target(FALSE);
        followA();
        set_check_target(TRUE);
        return;
    }
    _newT();
    buf = GetCurrentTab()->GetCurrentBuffer();
    set_check_target(FALSE);
    followA();
    set_check_target(TRUE);
    if (tab == NULL)
    {
        if (buf != GetCurrentTab()->GetCurrentBuffer())
            GetCurrentTab()->DeleteBuffer(buf);
        else
            deleteTab(GetCurrentTab());
    }
    else if (buf != GetCurrentTab()->GetCurrentBuffer())
    {
        // TODO:

        /* buf <- p <- ... <- Currentbuf = c */
        deleteTab(GetCurrentTab());
        // auto c = GetCurrentTab()->GetCurrentBuffer();
        // auto p = prevBuffer(c, buf);
        // p->nextBuffer = NULL;
        tab->SetFirstBuffer(buf);
        SetCurrentTab(tab);
        // for (buf = p; buf; buf = p)
        // {
        //     p = prevBuffer(c, buf);
        //     GetCurrentTab()->BufferPushBeforeCurrent(buf);
        // }
    }
    displayBuffer(GetCurrentTab()->GetCurrentBuffer(), B_FORCE_REDRAW);
}
