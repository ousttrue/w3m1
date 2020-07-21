
#include "tab.h"
#include "fm.h"
#include "indep.h"
#include "file.h"
#include "display.h"
#include "dispatcher.h"
#include "mouse.h"
#include "public.h"
#include "commands.h"

static TabBuffer *g_FirstTab = nullptr;
static TabBuffer *g_LastTab = nullptr;
static TabBuffer *g_CurrentTab = nullptr;
static int g_nTab = 0;

static TabBuffer *newTab(void)
{
    auto n = New(TabBuffer);
    n->nextTab = NULL;
    n->currentBuffer = NULL;
    n->firstBuffer = NULL;
    return n;
}

TabBuffer *TabBuffer::AddNext(Buffer *buf)
{
    auto tab = newTab();

    AddNext(tab);

    tab->firstBuffer = tab->currentBuffer = buf;
    return tab;
}

void TabBuffer::AddNext(TabBuffer *tab)
{
    g_nTab++;

    // tab <-> this->next
    tab->nextTab = this->nextTab;
    if (this->nextTab)
        this->nextTab->prevTab = tab;
    else
        SetLastTab(tab);

    // this <-> tab
    this->nextTab = tab;
    tab->prevTab = this;
}

void TabBuffer::AddPrev(TabBuffer *tab)
{
    g_nTab++;

    tab->prevTab = this->prevTab;
    tab->nextTab = this;
    if (this->prevTab)
        this->prevTab->nextTab = tab;
    else
        g_FirstTab = tab;
    this->prevTab = tab;
}

void TabBuffer::Remove(bool keepCurrent)
{
    if (this->prevTab)
    {
        // prev <-> tab
        if (this->nextTab)
            // prev <-> tab->next
            this->nextTab->prevTab = this->prevTab;
        else
            SetLastTab(this->prevTab);
        this->prevTab->nextTab = this->nextTab;

        if (!keepCurrent && this == g_CurrentTab)
            g_CurrentTab = this->prevTab;
    }
    else
    { /* this == FirstTab */
        this->nextTab->prevTab = NULL;
        g_FirstTab = this->nextTab;
        if (!keepCurrent && this == g_CurrentTab)
            g_CurrentTab = this->nextTab;
    }
    g_nTab--;
}

void TabBuffer::MoveTo(TabBuffer *dst, bool isRight)
{
    Remove(true);

    if (isRight)
    {
        dst->AddNext(this);
    }
    else
    {
        dst->AddPrev(this);
    }
}

void InitializeTab()
{
    g_FirstTab = g_LastTab = g_CurrentTab = newTab();
    g_nTab = 1;
}

int GetTabCount()
{
    return g_nTab;
}

TabBuffer *GetFirstTab()
{
    return g_FirstTab;
}

void SetFirstTab(TabBuffer *tab)
{
    g_FirstTab = tab;
}

TabBuffer *GetLastTab()
{
    return g_LastTab;
}

void SetLastTab(TabBuffer *tab)
{
    g_LastTab = tab;
}

TabBuffer *GetCurrentTab()
{
    return g_CurrentTab;
}

void SetCurrentTab(TabBuffer *tab)
{
    g_CurrentTab = tab;
}

void calcTabPos()
{
    TabBuffer *tab;
#if 0
    int lcol = 0, rcol = 2, col;
#else
    int lcol = 0, rcol = 0, col;
#endif
    int n1, n2, na, nx, ny, ix, iy;

    lcol = GetMouseActionMenuStr() ? GetMouseActionMenuWidth() : 0;

    if (g_nTab <= 0)
        return;
    n1 = (COLS - rcol - lcol) / TabCols;
    if (n1 >= g_nTab)
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
        ny = (g_nTab - n1 - 1) / n2 + 2;
    }
    na = n1 + n2 * (ny - 1);
    n1 -= (na - g_nTab) / ny;
    if (n1 < 0)
        n1 = 0;
    na = n1 + n2 * (ny - 1);
    tab = g_FirstTab;
    for (iy = 0; iy < ny && tab; iy++)
    {
        if (iy == 0)
        {
            nx = n1;
            col = COLS - rcol - lcol;
        }
        else
        {
            nx = n2 - (na - g_nTab + (iy - 1)) / (ny - 1);
            col = COLS;
        }
        for (ix = 0; ix < nx && tab; ix++, tab = tab->nextTab)
        {
            tab->x1 = col * ix / nx;
            tab->x2 = col * (ix + 1) / nx - 1;
            tab->y = iy;
            if (iy == 0)
            {
                tab->x1 += lcol;
                tab->x2 += lcol;
            }
        }
    }
}

TabBuffer *posTab(int x, int y)
{
    TabBuffer *tab;

    if (GetMouseActionMenuStr() && x < GetMouseActionMenuWidth() && y == 0)
        return NO_TABBUFFER;
    if (y > GetLastTab()->y)
        return NULL;
    for (tab = g_FirstTab; tab; tab = tab->nextTab)
    {
        if (tab->x1 <= x && x <= tab->x2 && tab->y == y)
            return tab;
    }
    return NULL;
}

TabBuffer *numTab(int n)
{
    TabBuffer *tab;
    int i;

    if (n == 0)
        return g_CurrentTab;
    if (n == 1)
        return g_FirstTab;
    if (g_nTab <= 1)
        return NULL;
    for (tab = g_FirstTab, i = 1; tab && i < n; tab = tab->nextTab, i++)
        ;
    return tab;
}

// setup
void _newT()
{
    // setup buffer
    auto buf = newBuffer(GetCurrentbuf()->width);
    copyBuffer(buf, GetCurrentbuf());
    buf->nextBuffer = NULL;
    for (int i = 0; i < MAX_LB; i++)
        buf->linkBuffer[i] = NULL;
    (*buf->clone)++;

    // new tab
    auto tab = g_CurrentTab->AddNext(buf);
    g_CurrentTab = tab;
}

void deleteTab(TabBuffer *tab)
{
    if (g_nTab <= 1)
        return;

    // clear buffer
    Buffer *buf, *next;
    buf = tab->firstBuffer;
    while (buf && buf != NO_BUFFER)
    {
        next = buf->nextBuffer;
        discardBuffer(buf);
        buf = next;
    }

    tab->Remove();

    return;
}

void DeleteCurrentTab()
{
    if (GetTabCount() > 1)
        deleteTab(g_CurrentTab);
}

void DeleteAllTabs()
{
    for (g_CurrentTab = GetFirstTab(); g_CurrentTab; g_CurrentTab = g_CurrentTab->nextTab)
    {
        while (HasFirstBuffer())
        {
            auto buf = GetFirstbuf()->nextBuffer;
            discardBuffer(GetFirstbuf());
            SetFirstbuf(buf);
        }
    }
}

void moveTab(TabBuffer *src, TabBuffer *dst, int right)
{
    if (!src || !dst || src == dst || src == NO_TABBUFFER || dst == NO_TABBUFFER)
        return;

    src->MoveTo(dst, right);
}

void SelectRelativeTab(int prec)
{
    if (g_nTab <= 1)
    {
        return;
    }

    if (prec > 0)
    {
        for (int i = 0; i < prec; i++)
        {
            if (g_CurrentTab->nextTab)
                g_CurrentTab = g_CurrentTab->nextTab;
            else
                g_CurrentTab = g_FirstTab;
        }
    }
    else if (prec < 0)
    {
        for (int i = 0; i < PREC_NUM(); i++)
        {
            if (g_CurrentTab->prevTab)
                g_CurrentTab = g_CurrentTab->prevTab;
            else
                g_CurrentTab = GetLastTab();
        }
    }
}

void SelectTabByPosition(int x, int y)
{
    TabBuffer *tab = posTab(x, y);
    if (!tab || tab == NO_TABBUFFER)
        return;
    g_CurrentTab = tab;
}

void MoveTab(int x)
{
    TabBuffer *tab;
    int i;
    if (x > 0)
    {
        for (tab = g_CurrentTab, i = 0; tab && i < x;
             tab = tab->nextTab, i++)
            ;
        moveTab(g_CurrentTab, tab ? tab : GetLastTab(), TRUE);
    }
    else if (x < 0)
    {
        for (tab = g_CurrentTab, i = 0; tab && i < PREC_NUM();
             tab = tab->prevTab, i++)
            ;
        moveTab(g_CurrentTab, tab ? tab : g_FirstTab, FALSE);
    }
}

Buffer *GetCurrentbuf()
{
    return g_CurrentTab->currentBuffer;
}

void SetCurrentbuf(Buffer *buf)
{
    g_CurrentTab->currentBuffer = buf;
}

Buffer *GetFirstbuf()
{
    return g_CurrentTab->firstBuffer;
}

int HasFirstBuffer()
{
    return GetFirstbuf() && GetFirstbuf() != NO_BUFFER;
}

void SetFirstbuf(Buffer *buffer)
{
    g_CurrentTab->firstBuffer = buffer;
}

void followTab(TabBuffer *tab)
{
    Buffer *buf;
    Anchor *a;

#ifdef USE_IMAGE
    a = retrieveCurrentImg(GetCurrentbuf());
    if (!(a && a->image && a->image->map))
#endif
        a = retrieveCurrentAnchor(GetCurrentbuf());
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
    buf = GetCurrentbuf();
    set_check_target(FALSE);
    followA();
    set_check_target(TRUE);
    if (tab == NULL)
    {
        if (buf != GetCurrentbuf())
            delBuffer(buf);
        else
            deleteTab(GetCurrentTab());
    }
    else if (buf != GetCurrentbuf())
    {
        /* buf <- p <- ... <- Currentbuf = c */
        Buffer *c, *p;

        c = GetCurrentbuf();
        p = prevBuffer(c, buf);
        p->nextBuffer = NULL;
        SetFirstbuf(buf);
        deleteTab(GetCurrentTab());
        SetCurrentTab(tab);
        for (buf = p; buf; buf = p)
        {
            p = prevBuffer(c, buf);
            pushBuffer(buf);
        }
    }
    displayBuffer(GetCurrentbuf(), B_FORCE_REDRAW);
}
