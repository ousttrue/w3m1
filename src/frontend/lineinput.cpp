#include "frontend/terminal.h"
#include "frontend/screen.h"
#include "lineinput.h"
#include "gc_helper.h"
#include "indep.h"
#include "stream/local_cgi.h"
#include "myctype.h"
#include "file.h"
#include "public.h"
#include "html/form.h"
#include "ctrlcode.h"
#include "frontend/display.h"

#include "frontend/tabbar.h"
#include "history.h"

#define STR_LEN 1024
#define CLEN (Terminal::columns() - 2)

static Str strBuf;
static Lineprop strProp[STR_LEN];

static Str CompleteBuf;
static Str CFileName;
static Str CBeforeBuf;
static Str CAfterBuf;
static Str CDirBuf;
static char **CFileBuf = NULL;
static int NCFileBuf;
static int NCFileOffset;

static void insertself(char c),
    _mvR(void), _mvL(void), _mvRw(void), _mvLw(void), delC(void), insC(void),
    _mvB(void), _mvE(void), _enter(void), _quo(void), _bs(void), _bsw(void),
    killn(void), killb(void), _inbrk(void), _esc(void), _editor(void),
    _prev(void), _next(void), _compl(void), _tcompl(void),
    _dcompl(void), _rdcompl(void), _rcompl(void);
#ifdef __EMX__
static int getcntrl(void);
#endif

static int terminated(unsigned char c);
#define iself ((void (*)())insertself)

static void next_compl(int next);
static void next_dcompl(int next);
static Str doComplete(Str ifn, int *status, int next);

/* *INDENT-OFF* */
void (*InputKeymap[32])() = {
    /*  C-@     C-a     C-b     C-c     C-d     C-e     C-f     C-g     */
    _compl, _mvB, _mvL, _inbrk, delC, _mvE, _mvR, _inbrk,
    /*  C-h     C-i     C-j     C-k     C-l     C-m     C-n     C-o     */
    _bs, iself, _enter, killn, iself, _enter, _next, _editor,
    /*  C-p     C-q     C-r     C-s     C-t     C-u     C-v     C-w     */
    _prev, _quo, _bsw, iself, _mvLw, killb, _quo, _bsw,
    /*  C-x     C-y     C-z     C-[     C-\     C-]     C-^     C-_     */
    _tcompl, _mvRw, iself, _esc, iself, iself, iself, iself,
    //
};
/* *INDENT-ON* */

static int setStrType(Str str, Lineprop *prop);
static void addPasswd(char *p, Lineprop *pr, int len, int pos, int limit);
static void addStr(char *p, Lineprop *pr, int len, int pos, int limit);

static int CPos, CLen, offset;
static int i_cont, i_broken, i_quote;
static int cm_mode, cm_next, cm_clear, cm_disp_next, cm_disp_clear;
static int need_redraw, is_passwd;
static int move_word;

static Hist *CurrentHist;
static Str strCurrentBuf;
static int use_hist;
static void ins_char(Str str);

static void addPasswd(char *p, Lineprop *pr, int len, int offset, int limit)
{
    auto ncol = PropertiedString(p, pr, len).calcPosition(len, CP_AUTO);
    if (ncol > offset + limit)
        ncol = offset + limit;

    int rcol = 0;
    if (offset)
    {
        addChar('{');
        rcol = offset + 1;
    }
    for (; rcol < ncol; rcol++)
        addChar('*');
}

static void addStr(char *p, Lineprop *pr, int len, int offset, int limit)
{
    int i = 0, rcol = 0, ncol, delta = 1;

    if (offset)
    {
        for (i = 0; i < len; i++)
        {
            if (PropertiedString(p, pr, len).calcPosition(i, CP_AUTO) > offset)
                break;
        }
        if (i >= len)
            return;

        while (pr[i] & PC_WCHAR2)
            i++;

        addChar('{');
        rcol = offset + 1;
        ncol = PropertiedString(p, pr, len).calcPosition(i, CP_AUTO);
        for (; rcol < ncol; rcol++)
            addChar(' ');
    }
    for (; i < len; i += delta)
    {
        delta = wtf_len((uint8_t *)&p[i]);

        ncol = PropertiedString(p, pr, len).calcPosition(i + delta, CP_AUTO);
        if (ncol - offset > limit)
            break;
        if (p[i] == '\t')
        {
            for (; rcol < ncol; rcol++)
                addChar(' ');
            continue;
        }
        else
        {
            addMChar(&p[i], pr[i], delta);
        }
        rcol = ncol;
    }
}

static void ins_char(Str str)
{
    char *p = str->ptr, *ep = p + str->Size();
    Lineprop ctype;
    int len;

    if (CLen + str->Size() >= STR_LEN)
        return;
    while (p < ep)
    {
        len = get_mclen(p);
        ctype = get_mctype(*p);
        if (is_passwd)
        {
            if (ctype & PC_CTRL)
                ctype = PC_ASCII;
            if (ctype & PC_UNKNOWN)
                ctype = PC_WCHAR1;
        }
        insC();
        strBuf->ptr[CPos] = *(p++);
        strProp[CPos] = ctype;
        CPos++;
        if (--len)
        {
            ctype = (ctype & ~PC_WCHAR1) | PC_WCHAR2;
            while (len--)
            {
                insC();
                strBuf->ptr[CPos] = *(p++);
                strProp[CPos] = ctype;
                CPos++;
            }
        }
    }
}

static void _esc(void)
{
    char c;

    switch (c = Terminal::getch())
    {
    case '[':
    case 'O':
        switch (c = Terminal::getch())
        {
        case 'A':
            _prev();
            break;
        case 'B':
            _next();
            break;
        case 'C':
            _mvR();
            break;
        case 'D':
            _mvL();
            break;
        }
        break;
    case CTRL_I:
    case ' ':
        if (w3mApp::Instance().emacs_like_lineedit)
        {
            _rdcompl();
            cm_clear = false;
            need_redraw = true;
        }
        else
            _rcompl();
        break;
    case CTRL_D:
        if (!w3mApp::Instance().emacs_like_lineedit)
            _rdcompl();
        need_redraw = true;
        break;
    case 'f':
        if (w3mApp::Instance().emacs_like_lineedit)
            _mvRw();
        break;
    case 'b':
        if (w3mApp::Instance().emacs_like_lineedit)
            _mvLw();
        break;
    case CTRL_H:
        if (w3mApp::Instance().emacs_like_lineedit)
            _bsw();
        break;

    default:
        if (wc_char_conv(ESC_CODE) == NULL && wc_char_conv(c) == NULL)
            i_quote = true;
    }
}

static void insC()
{
    strBuf->Insert(CPos, ' ');
    CLen = strBuf->Size();
    for (int i = CLen; i > CPos; i--)
    {
        strProp[i] = strProp[i - 1];
    }
}

static void delC(void)
{
    int i = CPos;
    int delta = 1;

    if (CLen == CPos)
        return;

    while (i + delta < CLen && strProp[i + delta] & PC_WCHAR2)
        delta++;

    for (i = CPos; i < CLen; i++)
    {
        strProp[i] = strProp[i + delta];
    }
    strBuf->Delete(CPos, delta);
    CLen -= delta;
}

static void _mvL(void)
{
    if (CPos > 0)
        CPos--;
    while (CPos > 0 && strProp[CPos] & PC_WCHAR2)
        CPos--;
}

static void _mvLw(void)
{
    int first = 1;
    while (CPos > 0 && (first || !terminated(strBuf->ptr[CPos - 1])))
    {
        CPos--;
        first = 0;
        if (CPos > 0 && strProp[CPos] & PC_WCHAR2)
            CPos--;
        if (!move_word)
            break;
    }
}

static void _mvRw(void)
{
    int first = 1;
    while (CPos < CLen && (first || !terminated(strBuf->ptr[CPos - 1])))
    {
        CPos++;
        first = 0;
        if (CPos < CLen && strProp[CPos] & PC_WCHAR2)
            CPos++;
        if (!move_word)
            break;
    }
}

static void _mvR(void)
{
    if (CPos < CLen)
        CPos++;
    while (CPos < CLen && strProp[CPos] & PC_WCHAR2)
        CPos++;
}

static void _bs(void)
{
    if (CPos > 0)
    {
        _mvL();
        delC();
    }
}

static void _bsw(void)
{
    int t = 0;
    while (CPos > 0 && !t)
    {
        _mvL();
        t = (move_word && terminated(strBuf->ptr[CPos - 1]));
        delC();
    }
}

static void _enter(void)
{
    i_cont = false;
}

static void insertself(char c)
{
    if (CLen >= STR_LEN)
        return;
    insC();
    strBuf->ptr[CPos] = c;
    strProp[CPos] = (is_passwd) ? PC_ASCII : PC_CTRL;
    CPos++;
}

static void _quo(void)
{
    i_quote = true;
}

static void _mvB(void)
{
    CPos = 0;
}

static void _mvE(void)
{
    CPos = CLen;
}

static void killn(void)
{
    CLen = CPos;
    strBuf->Truncate(CLen);
}

static void killb(void)
{
    while (CPos > 0)
        _bs();
}

static void _inbrk(void)
{
    i_cont = false;
    i_broken = true;
}

/* Completion status. */
#define CPL_OK 0
#define CPL_AMBIG 1
#define CPL_FAIL 2
#define CPL_MENU 3

#define CPL_NEVER 0x0
#define CPL_OFF 0x1
#define CPL_ON 0x2
#define CPL_ALWAYS 0x4
#define CPL_URL 0x8

static void _compl(void)
{
    next_compl(1);
}

static void _rcompl(void)
{
    next_compl(-1);
}

static void _tcompl(void)
{
    if (cm_mode & CPL_OFF)
        cm_mode = CPL_ON;
    else if (cm_mode & CPL_ON)
        cm_mode = CPL_OFF;
}

static void next_compl(int next)
{
    int status;
    int b, a;
    Str buf;
    Str s;

    if (cm_mode == CPL_NEVER || cm_mode & CPL_OFF)
        return;
    cm_clear = false;
    if (!cm_next)
    {
        if (cm_mode & CPL_ALWAYS)
        {
            b = 0;
        }
        else
        {
            for (b = CPos - 1; b >= 0; b--)
            {
                if ((strBuf->ptr[b] == ' ' || strBuf->ptr[b] == CTRL_I) &&
                    !((b > 0) && strBuf->ptr[b - 1] == '\\'))
                    break;
            }
            b++;
        }
        a = CPos;
        CBeforeBuf = strBuf->Substr(0, b);
        buf = strBuf->Substr(b, a - b);
        CAfterBuf = strBuf->Substr(a, strBuf->Size() - a);
        s = doComplete(buf, &status, next);
    }
    else
    {
        s = doComplete(strBuf, &status, next);
    }
    if (next == 0)
        return;

    if (status != CPL_OK && status != CPL_MENU)
    {
        // bell
        Terminal::write1(7);
    }

    if (status == CPL_FAIL)
        return;

    strBuf = Strnew_m_charp(CBeforeBuf->ptr, s->ptr, CAfterBuf->ptr, NULL);
    CLen = setStrType(strBuf, strProp);
    CPos = CBeforeBuf->Size() + s->Size();
    if (CPos > CLen)
        CPos = CLen;
}

static void _dcompl(void)
{
    next_dcompl(1);
}

static void _rdcompl(void)
{
    next_dcompl(-1);
}

static void next_dcompl(int next)
{
    static int col, row, len;
    static Str d;
    int i, j, n, y;
    Str f;
    char *p;
    struct stat st;
    int comment, nline;

    if (cm_mode == CPL_NEVER || cm_mode & CPL_OFF)
        return;
    cm_disp_clear = false;
    // if (GetCurrentTab())
    //     displayCurrentbuf(B_FORCE_REDRAW);
    if ((Terminal::lines() - 1) >= 3)
    {
        comment = true;
        nline = (Terminal::lines() - 1) - 2;
    }
    else if ((Terminal::lines() - 1))
    {
        comment = false;
        nline = (Terminal::lines() - 1);
    }
    else
    {
        return;
    }

    if (cm_disp_next >= 0)
    {
        if (next == 1)
        {
            cm_disp_next += col * nline;
            if (cm_disp_next >= NCFileBuf)
                cm_disp_next = 0;
        }
        else if (next == -1)
        {
            cm_disp_next -= col * nline;
            if (cm_disp_next < 0)
                cm_disp_next = 0;
        }
        row = (NCFileBuf - cm_disp_next + col - 1) / col;
        goto disp_next;
    }

    cm_next = false;
    next_compl(0);
    if (NCFileBuf == 0)
        return;
    cm_disp_next = 0;

    d = Str_conv_to_system(CDirBuf->Clone());
    if (d->Size() > 0 && d->Back() != '/')
        d->Push('/');
    if (cm_mode & CPL_URL && d->ptr[0] == 'f')
    {
        p = d->ptr;
        if (strncmp(p, "file://localhost/", 17) == 0)
            p = &p[16];
        else if (strncmp(p, "file:///", 8) == 0)
            p = &p[7];
        else if (strncmp(p, "file:/", 6) == 0 && p[6] != '/')
            p = &p[5];
        d = Strnew(p);
    }

    len = 0;
    for (i = 0; i < NCFileBuf; i++)
    {
        n = strlen(CFileBuf[i]) + 3;
        if (len < n)
            len = n;
    }
    col = Terminal::columns() / len;
    if (col == 0)
        col = 1;
    row = (NCFileBuf + col - 1) / col;

disp_next:
    if (comment)
    {
        if (row > nline)
        {
            row = nline;
            y = 0;
        }
        else
            y = nline - row + 1;
    }
    else
    {
        if (row >= nline)
        {
            row = nline;
            y = 0;
        }
        else
            y = nline - row - 1;
    }
    if (y)
    {
        Screen::Instance().Move(y - 1, 0);
        Screen::Instance().CtrlToEolWithBGColor();
    }
    if (comment)
    {
        Screen::Instance().Move(y, 0);
        Screen::Instance().CtrlToEolWithBGColor();
        Screen::Instance().Enable(S_BOLD);
        /* FIXME: gettextize? */
        Screen::Instance().Puts("----- Completion list -----");
        Screen::Instance().Disable(S_BOLD);
        y++;
    }
    for (i = 0; i < row; i++)
    {
        for (j = 0; j < col; j++)
        {
            n = cm_disp_next + j * row + i;
            if (n >= NCFileBuf)
                break;
            Screen::Instance().Move(y, j * len);
            Screen::Instance().CtrlToEolWithBGColor();
            f = d->Clone();
            f->Push(CFileBuf[n]);
            Screen::Instance().Puts(conv_from_system(CFileBuf[n]));
            if (stat(expandPath(f->ptr), &st) != -1 && S_ISDIR(st.st_mode))
                Screen::Instance().Puts("/");
        }
        y++;
    }
    if (comment && y == (Terminal::lines() - 1) - 1)
    {
        Screen::Instance().Move(y, 0);
        Screen::Instance().CtrlToEolWithBGColor();
        Screen::Instance().Enable(S_BOLD);
        if (w3mApp::Instance().emacs_like_lineedit)
            /* FIXME: gettextize? */
            Screen::Instance().Puts("----- Press TAB to continue -----");
        else
            /* FIXME: gettextize? */
            Screen::Instance().Puts("----- Press CTRL-D to continue -----");
        Screen::Instance().Disable(S_BOLD);
    }
}

Str escape_spaces(Str s)
{
    Str tmp = NULL;
    char *p;

    if (s == NULL)
        return s;
    for (p = s->ptr; *p; p++)
    {
        if (*p == ' ' || *p == CTRL_I)
        {
            if (tmp == NULL)
                tmp = Strnew_charp_n(s->ptr, (int)(p - s->ptr));
            tmp->Push('\\');
        }
        if (tmp)
            tmp->Push(*p);
    }
    if (tmp)
        return tmp;
    return s;
}

Str unescape_spaces(Str s)
{
    Str tmp = NULL;
    char *p;

    if (s == NULL)
        return s;
    for (p = s->ptr; *p; p++)
    {
        if (*p == '\\' && (*(p + 1) == ' ' || *(p + 1) == CTRL_I))
        {
            if (tmp == NULL)
                tmp = Strnew_charp_n(s->ptr, (int)(p - s->ptr));
        }
        else
        {
            if (tmp)
                tmp->Push(*p);
        }
    }
    if (tmp)
        return tmp;
    return s;
}

static Str doComplete(Str ifn, int *status, int next)
{
    int fl, i;
    char *fn, *p;
    DIR *d;
    Directory *dir;
    struct stat st;

    if (!cm_next)
    {
        NCFileBuf = 0;
        ifn = Str_conv_to_system(ifn);
        if (cm_mode & CPL_ON)
            ifn = unescape_spaces(ifn);
        CompleteBuf = ifn->Clone();
        while (CompleteBuf->Back() != '/' && CompleteBuf->Size() > 0)
            CompleteBuf->Pop(1);
        CDirBuf = CompleteBuf->Clone();
        if (cm_mode & CPL_URL)
        {
            if (strncmp(CompleteBuf->ptr, "file://localhost/", 17) == 0)
                CompleteBuf->Delete(0, 16);
            else if (strncmp(CompleteBuf->ptr, "file:///", 8) == 0)
                CompleteBuf->Delete(0, 7);
            else if (strncmp(CompleteBuf->ptr, "file:/", 6) == 0 &&
                     CompleteBuf->ptr[6] != '/')
                CompleteBuf->Delete(0, 5);
            else
            {
                CompleteBuf = ifn->Clone();
                *status = CPL_FAIL;
                return Str_conv_to_system(CompleteBuf);
            }
        }
        if (CompleteBuf->Size() == 0)
        {
            CompleteBuf->Push('.');
        }
        if (CompleteBuf->Back() == '/' && CompleteBuf->Size() > 1)
        {
            CompleteBuf->Pop(1);
        }
        if ((d = opendir(expandPath(CompleteBuf->ptr))) == NULL)
        {
            CompleteBuf = ifn->Clone();
            *status = CPL_FAIL;
            if (cm_mode & CPL_ON)
                CompleteBuf = escape_spaces(CompleteBuf);
            return CompleteBuf;
        }
        fn = lastFileName(ifn->ptr);
        fl = strlen(fn);
        CFileName = Strnew();
        for (;;)
        {
            dir = readdir(d);
            if (dir == NULL)
                break;
            if (fl == 0 && (!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, "..")))
                continue;
            if (!strncmp(dir->d_name, fn, fl))
            { /* match */
                NCFileBuf++;
                CFileBuf = New_Reuse(char *, CFileBuf, NCFileBuf);
                CFileBuf[NCFileBuf - 1] =
                    NewAtom_N(char, strlen(dir->d_name) + 1);
                strcpy(CFileBuf[NCFileBuf - 1], dir->d_name);
                if (NCFileBuf == 1)
                {
                    CFileName = Strnew(dir->d_name);
                }
                else
                {
                    for (i = 0; CFileName->ptr[i] == dir->d_name[i]; i++)
                        ;
                    CFileName->Truncate(i);
                }
            }
        }
        closedir(d);
        if (NCFileBuf == 0)
        {
            CompleteBuf = ifn->Clone();
            *status = CPL_FAIL;
            if (cm_mode & CPL_ON)
                CompleteBuf = escape_spaces(CompleteBuf);
            return CompleteBuf;
        }
        qsort(CFileBuf, NCFileBuf, sizeof(CFileBuf[0]), strCmp);
        NCFileOffset = 0;
        if (NCFileBuf >= 2)
        {
            cm_next = true;
            *status = CPL_AMBIG;
        }
        else
        {
            *status = CPL_OK;
        }
    }
    else
    {
        CFileName = Strnew(CFileBuf[NCFileOffset]);
        NCFileOffset = (NCFileOffset + next + NCFileBuf) % NCFileBuf;
        *status = CPL_MENU;
    }
    CompleteBuf = CDirBuf->Clone();
    if (CompleteBuf->Size() && CompleteBuf->Back() != '/')
        CompleteBuf->Push('/');
    CompleteBuf->Push(CFileName);
    if (*status != CPL_AMBIG)
    {
        p = CompleteBuf->ptr;
        if (cm_mode & CPL_URL)
        {
            if (strncmp(p, "file://localhost/", 17) == 0)
                p = &p[16];
            else if (strncmp(p, "file:///", 8) == 0)
                p = &p[7];
            else if (strncmp(p, "file:/", 6) == 0 && p[6] != '/')
                p = &p[5];
        }
        if (stat(expandPath(p), &st) != -1 && S_ISDIR(st.st_mode))
            CompleteBuf->Push('/');
    }
    if (cm_mode & CPL_ON)
        CompleteBuf = escape_spaces(CompleteBuf);
    return Str_conv_from_system(CompleteBuf);
}

static void _prev(void)
{
    Hist *hist = CurrentHist;
    char *p;

    if (!use_hist)
        return;
    if (strCurrentBuf)
    {
        p = prevHist(hist);
        if (p == NULL)
            return;
    }
    else
    {
        p = lastHist(hist);
        if (p == NULL)
            return;
        strCurrentBuf = strBuf;
    }
    if (w3mApp::Instance().DecodeURL && (cm_mode & CPL_URL))
        p = url_unquote_conv(p, WC_CES_NONE);
    strBuf = Strnew(p);
    CLen = CPos = setStrType(strBuf, strProp);
    offset = 0;
}

static void _next(void)
{
    Hist *hist = CurrentHist;
    char *p;

    if (!use_hist)
        return;
    if (strCurrentBuf == NULL)
        return;
    p = nextHist(hist);
    if (p)
    {
        if (w3mApp::Instance().DecodeURL && (cm_mode & CPL_URL))
            p = url_unquote_conv(p, WC_CES_NONE);
        strBuf = Strnew(p);
    }
    else
    {
        strBuf = strCurrentBuf;
        strCurrentBuf = NULL;
    }
    CLen = CPos = setStrType(strBuf, strProp);
    offset = 0;
}

static int setStrType(Str str, Lineprop *prop)
{
    Lineprop ctype;
    char *p = str->ptr, *ep = p + str->Size();
    int i, len = 1;

    for (i = 0; p < ep;)
    {
        len = get_mclen(p);

        if (i + len > STR_LEN)
            break;
        ctype = get_mctype(*p);
        if (is_passwd)
        {
            if (ctype & PC_CTRL)
                ctype = PC_ASCII;

            if (ctype & PC_UNKNOWN)
                ctype = PC_WCHAR1;

        }
        prop[i++] = ctype;

        p += len;
        if (--len)
        {
            ctype = (ctype & ~PC_WCHAR1) | PC_WCHAR2;
            while (len--)
                prop[i++] = ctype;
        }
    }
    return i;
}

static int terminated(unsigned char c)
{
    int termchar[] = {'/', '&', '?', ' ', -1};
    int *tp;

    for (tp = termchar; *tp > 0; tp++)
    {
        if (c == *tp)
        {
            return 1;
        }
    }

    return 0;
}

static void _editor(void)
{
    if (is_passwd)
        return;

    FormItem fi;
    fi.readonly = false;
    fi.value = strBuf->ptr;
    fi.value.push_back('\n');
    fi.input_textarea();

    strBuf = Strnew();
    for (auto p = fi.value.c_str(); *p; p++)
    {
        if (*p == '\r' || *p == '\n')
            continue;
        strBuf->Push(*p);
    }
    CLen = CPos = setStrType(strBuf, strProp);
}

char *inputLineHistSearch(std::string_view prompt, std::string_view def_str, LineInputFlags flag, Hist *hist, IncFunc incrfunc, int prec_num)
{
    int opos, x, y, lpos, rpos, epos;
    unsigned char c;
    Str tmp;

    is_passwd = false;
    move_word = true;

    CurrentHist = hist;

    if (hist != NULL)
    {
        use_hist = true;
        strCurrentBuf = NULL;
    }
    else
    {
        use_hist = false;
    }

    if (flag & IN_URL)
    {
        cm_mode = CPL_ALWAYS | CPL_URL;
    }
    else if (flag & IN_FILENAME)
    {
        cm_mode = CPL_ALWAYS;
    }
    else if (flag & IN_PASSWORD)
    {
        cm_mode = CPL_NEVER;
        is_passwd = true;
        move_word = false;
    }
    else if (flag & IN_COMMAND)
        cm_mode = CPL_ON;
    else
        cm_mode = CPL_OFF;
    opos = get_strwidth(prompt.data());
    epos = CLEN - opos;
    if (epos < 0)
        epos = 0;
    lpos = epos / 3;
    rpos = epos * 2 / 3;
    offset = 0;

    if (def_str.size())
    {
        strBuf = Strnew(def_str);
        CLen = CPos = setStrType(strBuf, strProp);
    }
    else
    {
        strBuf = Strnew();
        CLen = CPos = 0;
    }

#ifdef SUPPORT_WIN9X_CONSOLE_MBCS
    enable_win9x_console_input();
#endif
    i_cont = true;
    i_broken = false;
    i_quote = false;
    cm_next = false;
    cm_disp_next = -1;
    need_redraw = false;

    wc_char_conv_init(wc_guess_8bit_charset(w3mApp::Instance().DisplayCharset), w3mApp::Instance().InnerCharset);

    do
    {
        x = PropertiedString(strBuf->ptr, strProp, CLen).calcPosition(CPos, CP_FORCE);
        if (x - rpos > offset)
        {
            y = PropertiedString(strBuf->ptr, strProp, CLen).calcPosition(CLen, CP_AUTO);
            if (y - epos > x - rpos)
                offset = x - rpos;
            else if (y - epos > 0)
                offset = y - epos;
        }
        else if (x - lpos < offset)
        {
            if (x - lpos > 0)
                offset = x - lpos;
            else
                offset = 0;
        }
        Screen::Instance().Move(Terminal::lines() - 1, 0);
        Screen::Instance().Puts(prompt.data());
        if (is_passwd)
            addPasswd(strBuf->ptr, strProp, CLen, offset, Terminal::columns() - opos);
        else
            addStr(strBuf->ptr, strProp, CLen, offset, Terminal::columns() - opos);
        Screen::Instance().CtrlToEolWithBGColor();
        Screen::Instance().Move((Terminal::lines() - 1), opos + x - offset);
        Screen::Instance().Refresh();
        Terminal::flush();

    next_char:
        c = Terminal::getch();
        cm_clear = true;
        cm_disp_clear = true;
        if (!i_quote &&
            (((cm_mode & CPL_ALWAYS) && (c == CTRL_I || c == ' ')) ||
             ((cm_mode & CPL_ON) && (c == CTRL_I))))
        {
            if (w3mApp::Instance().emacs_like_lineedit && cm_next)
            {
                _dcompl();
                need_redraw = true;
            }
            else
            {
                _compl();
                cm_disp_next = -1;
            }
        }
        else if (!i_quote && CLen == CPos &&
                 (cm_mode & CPL_ALWAYS || cm_mode & CPL_ON) && c == CTRL_D)
        {
            if (!w3mApp::Instance().emacs_like_lineedit)
            {
                _dcompl();
                need_redraw = true;
            }
        }
        else if (!i_quote && c == DEL_CODE)
        {
            _bs();
            cm_next = false;
            cm_disp_next = -1;
        }
        else if (!i_quote && c < 0x20)
        { /* Control code */
            if (incrfunc == NULL || (c = incrfunc((int)c, strBuf, strProp, prec_num)) < 0x20)
                (*InputKeymap[(int)c])();
            if (incrfunc && c != (unsigned char)-1 && c != CTRL_J)
                incrfunc(-1, strBuf, strProp, prec_num);
            if (cm_clear)
                cm_next = false;
            if (cm_disp_clear)
                cm_disp_next = -1;
        }
        else
        {
            tmp = wc_char_conv(c);
            if (tmp == NULL)
            {
                i_quote = true;
                goto next_char;
            }
            i_quote = false;
            cm_next = false;
            cm_disp_next = -1;
            if (CLen + tmp->Size() > STR_LEN || !tmp->Size())
                goto next_char;
            ins_char(tmp);
            if (incrfunc)
                incrfunc(-1, strBuf, strProp, prec_num);
        }
        if (CLen && (flag & IN_CHAR))
            break;
    } while (i_cont);

    if (GetCurrentTab())
    {
        if (need_redraw)
        {
            // displayCurrentbuf(B_FORCE_REDRAW);
        }
    }

    if (i_broken)
        return "";

    Screen::Instance().Move((Terminal::lines() - 1), 0);
    Screen::Instance().Refresh();
    Terminal::flush();

    auto p = strBuf->ptr;
    if (flag & (IN_FILENAME | IN_COMMAND))
    {
        SKIP_BLANKS(&p);
    }
    if (use_hist && !(flag & IN_URL) && *p != '\0')
    {
        char *q = lastHist(hist);
        if (!q || strcmp(q, p))
            pushHist(hist, p);
    }
    if (flag & IN_FILENAME)
        return expandPath(p);
    else
        return allocStr(p, -1);
}
