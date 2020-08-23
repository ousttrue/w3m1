#include <string_view_util.h>
#include <unistd.h>
#include "textlist.h"
#include "history.h"
#include "frontend/terminal.h"
#include "indep.h"
#include "gc_helper.h"
#include "rc.h"
#include "urimethod.h"
#include "html/frame.h"
#include "frontend/display.h"
#include "html/parsetag.h"
#include "public.h"
#include "file.h"
#include "frontend/lineinput.h"
#include <setjmp.h>
#include <signal.h>
#include "ucs.h"
#include "stream/network.h"
#include "dispatcher.h"
#include "html/image.h"
#include "commands.h"
#include "stream/url.h"
#include "stream/cookie.h"
#include "ctrlcode.h"
#include "html/anchor.h"
#include "html/maparea.h"
#include "frontend/mouse.h"
#include "frontend/buffer.h"
#include "frontend/tabbar.h"
#include "frontend/menu.h"
#include "entity.h"
#include "loader.h"
#include "charset.h"
#include "w3m.h"

static const char *SearchString = NULL;
SearchFunc searchRoutine = nullptr;

void disp_srchresult(int result, const char *prompt, const char *str)
{
    if (str == NULL)
        str = "";
    if (result & SR_NOTFOUND)
        disp_message(Sprintf("Not found: %s", str)->ptr, true);
    else if (result & SR_WRAPPED)
        disp_message(Sprintf("Search wrapped: %s", str)->ptr, true);
    else if (w3mApp::Instance().show_srch_str)
        disp_message(Sprintf("%s%s", prompt, str)->ptr, true);
}

void srch_nxtprv(int reverse)
{
    int result;
    /* *INDENT-OFF* */
    static SearchFunc routine[2] = {
        forwardSearch, backwardSearch};
    /* *INDENT-ON* */

    if (searchRoutine == NULL)
    {
        /* FIXME: gettextize? */
        disp_message("No previous regular expression", true);
        return;
    }

    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();

    if (reverse != 0)
        reverse = 1;
    if (searchRoutine == backwardSearch)
        reverse ^= 1;
    if (reverse == 0)
        buf->pos += 1;
    result = srchcore(SearchString, routine[reverse]);
    if (result & SR_FOUND)
        buf->CurrentLine()->clear_mark();
    displayCurrentbuf(B_NORMAL);
    disp_srchresult(result, (char *)(reverse ? "Backward: " : "Forward: "),
                    SearchString);
}

static void
dump_extra(const BufferPtr &buf)
{
    printf("W3m-current-url: %s\n", buf->currentURL.ToStr()->ptr);
    if (buf->baseURL)
        printf("W3m-base-url: %s\n", buf->baseURL.ToStr()->ptr);
    printf("W3m-document-charset: %s\n",
           wc_ces_to_charset(buf->document_charset));

    if (buf->ssl_certificate.size())
    {
        Str tmp = Strnew();
        for (auto p = buf->ssl_certificate.c_str(); *p; p++)
        {
            tmp->Push(*p);
            if (*p == '\n')
            {
                for (; *(p + 1) == '\n'; p++)
                    ;
                if (*(p + 1))
                    tmp->Push('\t');
            }
        }
        if (tmp->Back() != '\n')
            tmp->Push('\n');
        printf("W3m-ssl-certificate: %s", tmp->ptr);
    }
}

static void
dump_head(w3mApp *w3m, BufferPtr buf)
{
    TextListItem *ti;

    // if (buf->document_header == NULL)
    // {
    //     if (w3m->w3m_dump & DUMP_EXTRA)
    //         printf("\n");
    //     return;
    // }
    // for (ti = buf->document_header->first; ti; ti = ti->next)
    // {
    //     printf("%s",
    //            wc_conv_strict(ti->ptr, w3m->InnerCharset,
    //                           buf->document_charset)
    //                ->ptr);
    // }
    puts("");
}

void do_dump(w3mApp *w3m, BufferPtr buf)
{
    TrapJmp([&]() {
        dump_extra(buf);
        dump_head(w3m, buf);
        buf->DumpSource();
        {
            int i;
            saveBuffer(buf, stdout, false);
            if (w3mApp::Instance().displayLinkNumber && buf->href)
            {
                printf("\nReferences:\n\n");
                for (i = 0; i < buf->href.size(); i++)
                {
                    auto a = buf->href.anchors[i];
                    if (a->slave)
                        continue;

                    auto pu = URL::Parse(a->url, buf->BaseURL());
                    auto s = pu.ToStr();
                    if (w3mApp::Instance().DecodeURL)
                        s = Strnew(url_unquote_conv(s->ptr, GetCurrentTab()->GetCurrentBuffer()->document_charset));
                    printf("[%d] %s\n", a->hseq + 1, s->ptr);
                }
            }
        }

        return true;
    });
}

/* search by regular expression */
SearchResultTypes srchcore(const char *str, SearchFunc func)
{
    int i;
    SearchResultTypes result = SR_NOTFOUND;

    if (str != NULL && str != SearchString)
        SearchString = str;
    if (SearchString == NULL || *SearchString == '\0')
        return SR_NOTFOUND;

    str = conv_search_string(SearchString, w3mApp::Instance().DisplayCharset);

    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();

    TrapJmp([&]() {
        {
            int prec = prec_num() ? prec_num() : 1;
            for (i = 0; i < prec; i++)
            {
                result = func(buf, str);
                if (i < prec - 1 && result & SR_FOUND)
                    buf->CurrentLine()->clear_mark();
            }
        }
        return true;
    });

    return result;
}

int dispincsrch(int ch, Str src, Lineprop *prop)
{
    static BufferPtr sbuf = std::shared_ptr<Buffer>(new Buffer);
    static LinePtr currentLine;
    static int pos;
    int do_next_search = false;

    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();

    if (ch == 0 && buf == NULL)
    {
        sbuf->COPY_BUFPOSITION_FROM(buf); /* search starting point */
        currentLine = sbuf->CurrentLine();
        pos = sbuf->pos;
        return -1;
    }

    auto str = src->ptr;
    switch (ch)
    {
    case 022: /* C-r */
        searchRoutine = backwardSearch;
        do_next_search = true;
        break;
    case 023: /* C-s */
        searchRoutine = forwardSearch;
        do_next_search = true;
        break;

#ifdef USE_MIGEMO
    case 034:
        migemo_active = -migemo_active;
        goto done;
#endif

    default:
        if (ch >= 0)
            return ch; /* use InputKeymap */
    }

    if (do_next_search)
    {
        if (*str)
        {
            if (searchRoutine == forwardSearch)
                buf->pos += 1;
            sbuf->COPY_BUFPOSITION_FROM(buf);
            if (srchcore(str, searchRoutine) == SR_NOTFOUND && searchRoutine == forwardSearch)
            {
                buf->pos -= 1;
                sbuf->COPY_BUFPOSITION_FROM(buf);
            }
            buf->ArrangeCursor();
            displayCurrentbuf(B_FORCE_REDRAW);
            buf->CurrentLine()->clear_mark();
            return -1;
        }
        else
            return 020; /* _prev completion for C-s C-s */
    }
    else if (*str)
    {
        buf->COPY_BUFPOSITION_FROM(sbuf);
        buf->ArrangeCursor();
        srchcore(str, searchRoutine);
        buf->ArrangeCursor();
        currentLine = buf->CurrentLine();
        pos = buf->pos;
    }
    displayCurrentbuf(B_FORCE_REDRAW);
    buf->CurrentLine()->clear_mark();
#ifdef USE_MIGEMO
done:
    while (*str++ != '\0')
    {
        if (migemo_active > 0)
            *prop++ |= PE_UNDER;
        else
            *prop++ &= ~PE_UNDER;
    }
#endif
    return -1;
}

void isrch(SearchFunc func, const char *prompt)
{
    char *str;
    BufferPtr sbuf = std::shared_ptr<Buffer>(new Buffer);
    sbuf->COPY_BUFPOSITION_FROM(GetCurrentTab()->GetCurrentBuffer());
    dispincsrch(0, NULL, NULL); /* initialize incremental search state */

    searchRoutine = func;
    str = inputLineHistSearch(prompt, NULL, IN_STRING, w3mApp::Instance().TextHist, dispincsrch);
    if (str == NULL)
    {
        GetCurrentTab()->GetCurrentBuffer()->COPY_BUFPOSITION_FROM(sbuf);
    }
    displayCurrentbuf(B_FORCE_REDRAW);
}

void srch(SearchFunc func, const char *prompt)
{
    int disp = false;
    const char *str = w3mApp::Instance().searchKeyData();
    if (str == NULL || *str == '\0')
    {
        str = inputStrHist(prompt, NULL, w3mApp::Instance().TextHist);
        if (str != NULL && *str == '\0')
            str = SearchString;
        if (str == NULL)
        {
            displayCurrentbuf(B_NORMAL);
            return;
        }
        disp = true;
    }

    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();

    auto pos = buf->pos;
    if (func == forwardSearch)
        buf->pos += 1;
    auto result = srchcore(str, func);
    if (result & SR_FOUND)
        buf->CurrentLine()->clear_mark();
    else
        buf->pos = pos;
    displayCurrentbuf(B_NORMAL);
    if (disp)
        disp_srchresult(result, prompt, str);
    searchRoutine = func;
}

void shiftvisualpos(BufferPtr buf, int shift)
{
    LinePtr l = buf->CurrentLine();
    buf->visualpos -= shift;
    if (buf->visualpos - l->bwidth >= buf->rect.cols)
        buf->visualpos = l->bwidth + buf->rect.cols - 1;
    else if (buf->visualpos - l->bwidth < 0)
        buf->visualpos = l->bwidth;
    buf->ArrangeLine();
    if (buf->visualpos - l->bwidth == -shift && buf->rect.cursorX == 0)
        buf->visualpos = l->bwidth;
}

void cmd_loadfile(const char *fn)
{
    GetCurrentTab()->Push(URL::LocalPath(fn));
    // auto buf = loadGeneralFile(URL::ParsePath(fn), NULL, HttpReferrerPolicy::NoReferer, NULL);
    // if (buf == NULL)
    // {
    //     /* FIXME: gettextize? */
    //     char *emsg = Sprintf("%s not found", conv_from_system(fn))->ptr;
    //     disp_err_message(emsg, false);
    // }
    // else
    // {
    //     GetCurrentTab()->Push(buf);
    //     if (w3mApp::Instance().RenderFrame && GetCurrentTab()->GetCurrentBuffer()->frameset != NULL)
    //         rFrame(&w3mApp::Instance());
    // }
    displayCurrentbuf(B_NORMAL);
}

void cmd_loadURL(std::string_view url, URL *current, HttpReferrerPolicy referer, FormPtr request)
{

    if (handleMailto(url.data()))
        return;

    // Screen::Instance().Refresh();
    // Terminal::flush();

    // BufferPtr buf;
    // buf = loadGeneralFile(URL::Parse(url), current, referer, request);
    // if (buf == NULL)
    // {
    //     /* FIXME: gettextize? */
    //     char *emsg = Sprintf("Can't load %s", conv_from_system(url))->ptr;
    //     disp_err_message(emsg, false);
    // }
    // else
    // {
    //     GetCurrentTab()->Push(buf);
    //     if (w3mApp::Instance().RenderFrame && GetCurrentTab()->GetCurrentBuffer()->frameset != NULL)
    //         rFrame(&w3mApp::Instance());
    // }
    GetCurrentTab()->Push(URL::Parse(url, current));
    displayCurrentbuf(B_NORMAL);
}

int handleMailto(const char *url)
{
    Str to;
    char *pos;

    if (strncasecmp(url, "mailto:", 7))
        return 0;
    if (svu::is_null_or_space(w3mApp::Instance().Mailer))
    {
        /* FIXME: gettextize? */
        disp_err_message("no mailer is specified", true);
        return 1;
    }

    /* invoke external mailer */
    if (w3mApp::Instance().MailtoOptions == MAILTO_OPTIONS_USE_MAILTO_URL)
    {
        to = Strnew(html_unquote(const_cast<char *>(url), w3mApp::Instance().InnerCharset));
    }
    else
    {
        to = Strnew(url + 7);
        if ((pos = strchr(to->ptr, '?')) != NULL)
            to->Truncate(pos - to->ptr);
    }
    fmTerm();
    system(myExtCommand(w3mApp::Instance().Mailer.c_str(), shell_quote(file_unquote(to->ptr)),
                        false)
               ->ptr);
    fmInit();
    displayCurrentbuf(B_FORCE_REDRAW);
    pushHashHist(w3mApp::Instance().URLHist, url);
    return 1;
}

/* Move cursor left */
void _movL(int n)
{
    int i, m = w3mApp::Instance().w3mApp::Instance().searchKeyNum();
    if (GetCurrentTab()->GetCurrentBuffer()->LineCount() == 0)
        return;
    for (i = 0; i < m; i++)
        GetCurrentTab()->GetCurrentBuffer()->CursorLeft(n);
    displayCurrentbuf(B_NORMAL);
}

/* Move cursor downward */
void _movD(int n)
{
    int i, m = w3mApp::Instance().w3mApp::Instance().searchKeyNum();
    if (GetCurrentTab()->GetCurrentBuffer()->LineCount() == 0)
        return;
    for (i = 0; i < m; i++)
        GetCurrentTab()->GetCurrentBuffer()->CursorDown(n);
    displayCurrentbuf(B_NORMAL);
}

/* move cursor upward */
void _movU(int n)
{
    int i, m = w3mApp::Instance().w3mApp::Instance().searchKeyNum();
    if (GetCurrentTab()->GetCurrentBuffer()->LineCount() == 0)
        return;
    for (i = 0; i < m; i++)
        GetCurrentTab()->GetCurrentBuffer()->CursorUp(n);
    displayCurrentbuf(B_NORMAL);
}

/* Move cursor right */
void _movR(int n)
{
    int i, m = w3mApp::Instance().w3mApp::Instance().searchKeyNum();
    if (GetCurrentTab()->GetCurrentBuffer()->LineCount() == 0)
        return;
    for (i = 0; i < m; i++)
        GetCurrentTab()->GetCurrentBuffer()->CursorRight(n);
    displayCurrentbuf(B_NORMAL);
}

int prev_nonnull_line(BufferPtr buf, LinePtr line)
{
    LinePtr l;

    for (l = line; l != NULL && l->len() == 0; l = buf->PrevLine(l))
        ;
    if (l == NULL || l->len() == 0)
        return -1;

    GetCurrentTab()->GetCurrentBuffer()->SetCurrentLine(l);
    if (l != line)
        GetCurrentTab()->GetCurrentBuffer()->pos = GetCurrentTab()->GetCurrentBuffer()->CurrentLine()->len();
    return 0;
}

int next_nonnull_line(BufferPtr buf, LinePtr line)
{
    LinePtr l;

    for (l = line; l != NULL && l->len() == 0; l = buf->NextLine(l))
        ;

    if (l == NULL || l->len() == 0)
        return -1;

    GetCurrentTab()->GetCurrentBuffer()->SetCurrentLine(l);
    if (l != line)
        GetCurrentTab()->GetCurrentBuffer()->pos = 0;
    return 0;
}

char *
getCurWord(BufferPtr buf, int *spos, int *epos)
{
    char *p;
    LinePtr l = buf->CurrentLine();
    int b, e;

    *spos = 0;
    *epos = 0;
    if (l == NULL)
        return NULL;
    p = l->lineBuf();
    e = buf->pos;
    while (e > 0 && !is_wordchar(getChar(&p[e])))
        prevChar(&e, l);
    if (!is_wordchar(getChar(&p[e])))
        return NULL;
    b = e;
    while (b > 0)
    {
        int tmp = b;
        prevChar(&tmp, l);
        if (!is_wordchar(getChar(&p[tmp])))
            break;
        b = tmp;
    }
    while (e < l->len() && is_wordchar(getChar(&p[e])))
        nextChar(&e, l);
    *spos = b;
    *epos = e;
    return &p[b];
}

/*
 * From: Takashi Nishimoto <g96p0935@mse.waseda.ac.jp> Date: Mon, 14 Jun
 * 1999 09:29:56 +0900
 */
void prevChar(int *s, LinePtr l)
{
    do
    {
        (*s)--;
    } while ((*s) > 0 && (l)->propBuf()[*s] & PC_WCHAR2);
}

void nextChar(int *s, LinePtr l)
{
    do
    {
        (*s)++;
    } while ((*s) < (l)->len() && (l)->propBuf()[*s] & PC_WCHAR2);
}

uint32_t getChar(char *p)
{
    if (!p)
    {
        return 0;
    }
    if (!*p)
    {
        return 0;
    }
    return wc_any_to_ucs(wtf_parse1((uint8_t **)&p));
}

int is_wordchar(uint32_t c)
{
    return wc_is_ucs_alnum(c);
}

/* Go to specified line */
void _goLine(std::string_view l)
{
    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();
    if (l.empty() || buf->CurrentLine() == NULL)
    {
        displayCurrentbuf(B_FORCE_REDRAW);
        return;
    }

    buf->pos = 0;
    if (((l[0] == '^') || (l[0] == '$')) && prec_num())
    {
        buf->GotoRealLine(prec_num());
    }
    else if (l[0] == '^')
    {
        buf->SetCurrentLine(buf->FirstLine());
        buf->SetTopLine(buf->FirstLine());
    }
    else if (l[0] == '$')
    {
        buf->LineSkip(buf->LastLine(),
                      -(buf->rect.lines + 1) / 2, true);
        buf->SetCurrentLine(buf->LastLine());
    }
    else
    {
        buf->GotoRealLine(atoi(l.data()));
    }
    buf->ArrangeCursor();
    displayCurrentbuf(B_FORCE_REDRAW);
}

int cur_real_linenumber(const BufferPtr &buf)
{
    LinePtr cur = buf->CurrentLine();
    if (!cur)
        return 1;
    auto n = cur->real_linenumber ? cur->real_linenumber : 1;
    for (auto l = buf->FirstLine(); l && l != cur && l->real_linenumber == 0; l = buf->NextLine(l))
    { /* header */
        if (l->bpos == 0)
            n++;
    }
    return n;
}

static const char *s_MarkString = NULL;

const char *MarkString()
{
    return s_MarkString;
}

void SetMarkString(const char *str)
{
    s_MarkString = str;
}

void _followForm(bool submit)
{
    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();
    if (buf->LineCount() == 0)
        return;
    auto a = buf->formitem.RetrieveAnchor(buf->CurrentPoint());
    if (a == NULL)
        return;

    auto fi = a->item;
    switch (fi->type)
    {
    case FORM_INPUT_TEXT:
    {
        if (submit)
            goto do_submit;
        if (fi->readonly)
            /* FIXME: gettextize? */
            disp_message_nsec("Read only field!", false, 1, true, false);
        /* FIXME: gettextize? */
        auto p = inputStrHist("TEXT:", fi->value.size() ? fi->value.c_str() : NULL, w3mApp::Instance().TextHist);
        if (p == NULL || fi->readonly)
            break;
        fi->value = p;
        formUpdateBuffer(a, buf, fi);
        if (fi->accept || fi->parent.lock()->nitems() == 1)
            goto do_submit;
        break;
    }
    case FORM_INPUT_FILE:
    {
        if (submit)
            goto do_submit;
        if (fi->readonly)
            /* FIXME: gettextize? */
            disp_message_nsec("Read only field!", false, 1, true, false);
        /* FIXME: gettextize? */
        auto p = inputFilenameHist("Filename:", fi->value.size() ? fi->value.c_str() : NULL,
                                   NULL);
        if (p == NULL || fi->readonly)
            break;
        fi->value = p;
        formUpdateBuffer(a, buf, fi);
        if (fi->accept || fi->parent.lock()->nitems() == 1)
            goto do_submit;
        break;
    }
    case FORM_INPUT_PASSWORD:
    {
        if (submit)
            goto do_submit;
        if (fi->readonly)
        {
            /* FIXME: gettextize? */
            disp_message_nsec("Read only field!", false, 1, true, false);
            break;
        }
        /* FIXME: gettextize? */
        auto p = inputLine("Password:", fi->value.size() ? fi->value.c_str() : NULL,
                           IN_PASSWORD);
        if (p == NULL)
            break;
        fi->value = p;
        formUpdateBuffer(a, buf, fi);
        if (fi->accept)
            goto do_submit;
        break;
    }
    case FORM_TEXTAREA:
    {
        if (submit)
            goto do_submit;
        if (fi->readonly)
            /* FIXME: gettextize? */
            disp_message_nsec("Read only field!", false, 1, true, false);
        fi->input_textarea();
        formUpdateBuffer(a, buf, fi);
        break;
    }
    case FORM_INPUT_RADIO:
    {
        if (submit)
            goto do_submit;
        if (fi->readonly)
        {
            /* FIXME: gettextize? */
            disp_message_nsec("Read only field!", false, 1, true, false);
            break;
        }
        formRecheckRadio(a, buf, fi);
        break;
    }
    case FORM_INPUT_CHECKBOX:
    {
        if (submit)
            goto do_submit;
        if (fi->readonly)
        {
            /* FIXME: gettextize? */
            disp_message_nsec("Read only field!", false, 1, true, false);
            break;
        }
        fi->checked = !fi->checked;
        formUpdateBuffer(a, buf, fi);
        break;
    }
    case FORM_SELECT:
    {
        if (submit)
            goto do_submit;
        if (!fi->formChooseOptionByMenu(
                                    buf->rect.cursorX - buf->pos +
                                        a->start.pos + buf->rect.rootX,
                                    buf->rect.cursorY + buf->rect.rootY))
            break;
        formUpdateBuffer(a, buf, fi);
        if (fi->parent.lock()->nitems() == 1)
            goto do_submit;
        break;
    }
    case FORM_INPUT_IMAGE:
    case FORM_INPUT_SUBMIT:
    case FORM_INPUT_BUTTON:
    {
    do_submit:
        auto tmp = Strnew();
        auto multipart = (fi->parent.lock()->method == FORM_METHOD_POST &&
                          fi->parent.lock()->enctype == FORM_ENCTYPE_MULTIPART);
        query_from_followform(&tmp, fi, multipart);

        std::string tmp2 = fi->parent.lock()->action;
        if (tmp2 == "!CURRENT_URL!")
        {
            /* It means "current URL" */
            auto url = buf->currentURL;
            url.query.clear();
            tmp2 = buf->currentURL.ToStr()->ptr;
        }

        if (fi->parent.lock()->method == FORM_METHOD_GET)
        {
            auto pos = tmp2.find('?');
            if (pos != std::string::npos)
            {
                tmp2 = tmp2.substr(0, pos);
            }
            tmp2.push_back('?');
            for (auto p = tmp->ptr; *p; ++p)
            {
                tmp2.push_back(*p);
            }
            // loadLink(tmp2->ptr, a->target.c_str(), HttpReferrerPolicy::StrictOriginWhenCrossOrigin, NULL);
            tab->Push(URL::Parse(tmp2, buf->BaseURL()));
        }
        else if (fi->parent.lock()->method == FORM_METHOD_POST)
        {
            BufferPtr buf;
            if (multipart)
            {
                struct stat st;
                stat(fi->parent.lock()->body, &st);
                fi->parent.lock()->length = st.st_size;
            }
            else
            {
                fi->parent.lock()->body = tmp->ptr;
                fi->parent.lock()->length = tmp->Size();
            }
            // buf = loadLink(tmp2->ptr, a->target.c_str(), HttpReferrerPolicy::StrictOriginWhenCrossOrigin, fi->parent);
            tab->Push(URL::Parse(tmp2, buf->BaseURL()));
            if (multipart)
            {
                unlink(fi->parent.lock()->body);
            }
            if (buf && !(buf->bufferprop & BP_REDIRECTED))
            { /* buf must be Currentbuf */
                /* BP_REDIRECTED means that the buffer is obtained through
         * Location: header. In this case, buf->form_submit must not be set
         * because the page is not loaded by POST method but GET method.
         */
                buf->form_submit = save_submit_formlist(fi);
            }
        }
        else if ((fi->parent.lock()->method == FORM_METHOD_INTERNAL && (fi->parent.lock()->action == "map" || fi->parent.lock()->action == "none")) || buf->bufferprop & BP_INTERNAL)
        { /* internal */
            do_internal(tmp2, tmp->ptr);
        }
        else
        {
            disp_err_message("Can't send form because of illegal method.",
                             false);
        }
        break;
    }
    case FORM_INPUT_RESET:
    {
        for (auto i = 0; i < buf->formitem.size(); i++)
        {
            auto a2 = buf->formitem.anchors[i];
            auto f2 = a2->item;
            if (f2->parent.lock() == fi->parent.lock() &&
                f2->name.size() && f2->value.size() &&
                f2->type != FORM_INPUT_SUBMIT &&
                f2->type != FORM_INPUT_HIDDEN &&
                f2->type != FORM_INPUT_RESET)
            {
                f2->value = f2->init_value;
                f2->checked = f2->init_checked;
                f2->label = f2->init_label;
                f2->selected = f2->init_selected;
                formUpdateBuffer(a2, buf, f2);
            }
        }
        break;
    }
    case FORM_INPUT_HIDDEN:
    default:
        break;
    }
    displayCurrentbuf(B_FORCE_REDRAW);
}

void query_from_followform(Str *query, FormItemPtr fi, int multipart)
{
    FILE *body = NULL;
    if (multipart)
    {
        *query = tmpfname(TMPF_DFL, NULL);
        body = fopen((*query)->ptr, "w");
        if (body == NULL)
        {
            return;
        }
        fi->parent.lock()->body = (*query)->ptr;
        fi->parent.lock()->boundary =
            Sprintf("------------------------------%d%ld%ld%ld", w3mApp::Instance().CurrentPid,
                    fi->parent, fi->parent.lock()->body, fi->parent.lock()->boundary)
                ->ptr;
    }

    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();
    *query = Strnew();
    for (auto &f2: fi->parent.lock()->items)
    {
        if (f2->name.empty())
            continue;
        /* <ISINDEX> is translated into single text form */
        // if (f2->name->Size() == 0 &&
        //     (multipart || f2->type != FORM_INPUT_TEXT))
        //     continue;
        switch (f2->type)
        {
        case FORM_INPUT_RESET:
            /* do nothing */
            continue;
        case FORM_INPUT_SUBMIT:
        case FORM_INPUT_IMAGE:
            if (f2 != fi || f2->value.empty())
                continue;
            break;
        case FORM_INPUT_RADIO:
        case FORM_INPUT_CHECKBOX:
            if (!f2->checked)
                continue;
        }
        if (multipart)
        {
            if (f2->type == FORM_INPUT_IMAGE)
            {
                int x = 0, y = 0;

                getMapXY(buf, buf->img.RetrieveAnchor(buf->CurrentPoint()), &x, &y);

                *query = conv_form_encoding(f2->name, fi, buf)->Clone();
                (*query)->Push(".x");
                form_write_data(body, fi->parent.lock()->boundary, (*query)->ptr,
                                Sprintf("%d", x)->ptr);
                *query = conv_form_encoding(f2->name, fi, buf)->Clone();
                (*query)->Push(".y");
                form_write_data(body, fi->parent.lock()->boundary, (*query)->ptr,
                                Sprintf("%d", y)->ptr);
            }
            else if (f2->name.size() && f2->value.size())
            {
                /* not IMAGE */
                *query = conv_form_encoding(f2->value.c_str(), fi, buf);
                if (f2->type == FORM_INPUT_FILE)
                    form_write_from_file(body, fi->parent.lock()->boundary,
                                         conv_form_encoding(f2->name, fi,
                                                            buf)
                                             ->ptr,
                                         (*query)->ptr,
                                         conv_to_system(f2->value.c_str()));
                else
                    form_write_data(body, fi->parent.lock()->boundary,
                                    conv_form_encoding(f2->name, fi,
                                                       buf)
                                        ->ptr,
                                    (*query)->ptr);
            }
        }
        else
        {
            /* not multipart */
            if (f2->type == FORM_INPUT_IMAGE)
            {
                int x = 0, y = 0;
                getMapXY(buf, buf->img.RetrieveAnchor(buf->CurrentPoint()), &x, &y);
                (*query)->Push(
                    UrlEncode(conv_form_encoding(f2->name, fi, buf)));
                (*query)->Push(Sprintf(".x=%d&", x));
                (*query)->Push(UrlEncode(conv_form_encoding(f2->name, fi, buf)));
                (*query)->Push(Sprintf(".y=%d", y));
            }
            else
            {
                /* not IMAGE */
                if (f2->name.size())
                {
                    (*query)->Push(UrlEncode(conv_form_encoding(f2->name, fi, buf)));
                    (*query)->Push('=');
                }
                if (f2->value.size())
                {
                    if (fi->parent.lock()->method == FORM_METHOD_INTERNAL)
                        (*query)->Push(UrlEncode(Strnew(f2->value)));
                    else
                    {
                        (*query)->Push(UrlEncode(conv_form_encoding(f2->value, fi, buf)));
                    }
                }
            }
            if (&f2 != &f2->parent.lock()->items.back())
                (*query)->Push('&');
        }
    }
    if (multipart)
    {
        fprintf(body, "--%s--\r\n", fi->parent.lock()->boundary);
        fclose(body);
    }
    else
    {
        /* remove trailing & */
        while ((*query)->Back() == '&')
            (*query)->Pop(1);
    }
}

int on_target = 1;

/* follow HREF link in the buffer */
void bufferA(void)
{
    on_target = false;
    followA(&w3mApp::Instance());
    on_target = true;
}

// BufferPtr loadLink(const char *url, const char *target, HttpReferrerPolicy referer, FormList *request)
// {
//     BufferPtr nfbuf;
//     union frameset_element *f_element = NULL;

//     message(Sprintf("loading %s", url)->ptr, 0, 0);
//     refresh();

//     auto base = GetCurrentTab()->GetCurrentBuffer()->BaseURL();
//     if (base == NULL ||
//         base->scheme == SCM_LOCAL || base->scheme == SCM_LOCAL_CGI)
//         referer = HttpReferrerPolicy::NoReferer;

//     auto newBuf = loadGeneralFile(URL::Parse(url, GetCurrentTab()->GetCurrentBuffer()->BaseURL()), GetCurrentTab()->GetCurrentBuffer()->BaseURL(), referer, request);
//     if (newBuf == NULL)
//     {
//         char *emsg = Sprintf("Can't load %s", url)->ptr;
//         disp_err_message(emsg, false);
//         return NULL;
//     }

//     auto pu = URL::Parse(url, base);
//     pushHashHist(w3mApp::Instance().URLHist, pu.ToStr()->ptr);

//     if (!on_target) /* open link as an indivisual page */
//         return loadNormalBuf(newBuf, true);

//     if (do_download) /* download (thus no need to render frame) */
//         return loadNormalBuf(newBuf, false);

//     if (target == NULL ||                                             /* no target specified (that means this page is not a frame page) */
//         !strcmp(target, "_top") ||                                    /* this link is specified to be opened as an indivisual * page */
//         !(GetCurrentTab()->GetCurrentBuffer()->bufferprop & BP_FRAME) /* This page is not a frame page */
//     )
//     {
//         return loadNormalBuf(newBuf, true);
//     }
//     nfbuf = GetCurrentTab()->GetCurrentBuffer()->linkBuffer[LB_N_FRAME];
//     if (nfbuf == NULL)
//     {
//         /* original page (that contains <frameset> tag) doesn't exist */
//         return loadNormalBuf(newBuf, true);
//     }

//     f_element = search_frame(nfbuf->frameset, const_cast<char *>(target));
//     if (f_element == NULL)
//     {
//         /* specified target doesn't exist in this frameset */
//         return loadNormalBuf(newBuf, true);
//     }

//     /* frame page */

//     /* stack current frameset */
//     pushFrameTree(&(nfbuf->frameQ), copyFrameSet(nfbuf->frameset), GetCurrentTab()->GetCurrentBuffer());
//     /* delete frame view buffer */
//     auto tab = GetCurrentTab();
//     // tab->DeleteBuffer(tab->GetCurrentBuffer());
//     // GetCurrentTab()->SetCurrentBuffer(nfbuf);
//     tab->Push(nfbuf);
//     /* nfbuf->frameset = copyFrameSet(nfbuf->frameset); */
//     resetFrameElement(f_element, newBuf, referer, request);
//     rFrame(&w3mApp::Instance());
//     {
//         const AnchorPtr al = NULL;
//         auto label = pu.fragment;

//         if (label.size() && f_element->element->attr == F_BODY)
//         {
//             al = f_element->body->nameList.SearchByUrl(label.c_str());
//         }

//         auto tab = GetCurrentTab();
//         auto buf = tab->GetCurrentBuffer();

//         if (!al)
//         {
//             label = std::string("_") + target;
//             al = buf->name.SearchByUrl(label.c_str());
//         }
//         if (al)
//         {
//             buf->Goto(al->start, label_topline);
//         }
//     }
//     displayCurrentbuf(B_NORMAL);
//     return newBuf;
// }

FormItemPtr save_submit_formlist(FormItemPtr src)
{
    if (src == NULL)
        return NULL;

    FormPtr list;
    FormPtr srclist;
    FormItemPtr srcitem;
    FormItemPtr item;
    FormItemPtr ret = NULL;
    FormSelectOptionItem *opt;
    FormSelectOptionItem *curopt;
    FormSelectOptionItem *srcopt;

    srclist = src->parent.lock();
    list = std::make_shared<Form>(srclist->action, srclist->method);
    list->charset = srclist->charset;
    list->enctype = srclist->enctype;
    list->items = srclist->items;
    list->body = srclist->body;
    list->boundary = srclist->boundary;
    list->length = srclist->length;

    for(int i=0; i<src->parent.lock()->items.size(); ++i)
    {
        if(src->parent.lock()->items[i] == src)
        {
            return list->items[i]; 
        }
    }

    assert(false);
    return nullptr;

    // for (srcitem = srclist->item; srcitem; srcitem = srcitem->next)
    // {
    //     item = new FormItemList;
    //     item->type = srcitem->type;
    //     item->name = srcitem->name;
    //     item->value = srcitem->value;
    //     item->checked = srcitem->checked;
    //     item->accept = srcitem->accept;
    //     item->size = srcitem->size;
    //     item->rows = srcitem->rows;
    //     item->maxlength = srcitem->maxlength;
    //     item->readonly = srcitem->readonly;

    //     opt = curopt = NULL;
    //     for (srcopt = srcitem->select_option; srcopt; srcopt = srcopt->next)
    //     {
    //         if (!srcopt->checked)
    //             continue;
    //         opt = new FormSelectOptionItem;
    //         opt->value = srcopt->value;
    //         opt->label = srcopt->label;
    //         opt->checked = srcopt->checked;
    //         if (item->select_option == NULL)
    //         {
    //             item->select_option = curopt = opt;
    //         }
    //         else
    //         {
    //             curopt->next = opt;
    //             curopt = curopt->next;
    //         }
    //     }
    //     item->select_option = opt;
    //     if (srcitem->label.size())
    //         item->label = srcitem->label;

    //     item->parent = list;
    //     item->next = NULL;

    //     if (list->lastitem == NULL)
    //     {
    //         list->item = list->lastitem = item;
    //     }
    //     else
    //     {
    //         list->lastitem->next = item;
    //         list->lastitem = item;
    //     }

    //     if (srcitem == src)
    //         ret = item;
    // }

    // return ret;
}

Str conv_form_encoding(std::string_view val, FormItemPtr fi, BufferPtr buf)
{
    CharacterEncodingScheme charset = w3mApp::Instance().SystemCharset;

    if (fi->parent.lock()->charset)
        charset = fi->parent.lock()->charset;
    else if (buf->document_charset && buf->document_charset != WC_CES_US_ASCII)
        charset = buf->document_charset;
    return wc_conv_strict(val.data(), w3mApp::Instance().InnerCharset, charset);
}

// BufferPtr loadNormalBuf(BufferPtr buf, int renderframe)
// {
//     GetCurrentTab()->Push(buf);
//     if (renderframe && w3mApp::Instance().RenderFrame && GetCurrentTab()->GetCurrentBuffer()->frameset != NULL)
//         rFrame(&w3mApp::Instance());
//     return buf;
// }

/* go to the next [visited] anchor */
void _nextA(int visited)
{
    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();
    if (buf->LineCount() == 0)
        return;
    if (buf->hmarklist.empty())
        return;

    auto an = buf->href.RetrieveAnchor(buf->CurrentPoint());
    if (!visited && !an)
        an = buf->formitem.RetrieveAnchor(buf->CurrentPoint());

    auto y = buf->CurrentLine()->linenumber;
    auto x = buf->pos;

    int n = w3mApp::Instance().w3mApp::Instance().searchKeyNum();
    if (visited == true)
    {
        n = buf->hmarklist.size();
    }

    for (int i = 0; i < n; i++)
    {
        auto pan = an;
        if (an && an->hseq >= 0)
        {
            int hseq = an->hseq + 1;
            do
            {
                if (hseq >= buf->hmarklist.size())
                {
                    if (visited == true)
                        return;
                    an = pan;
                    goto _end;
                }
                auto &po = buf->hmarklist[hseq];
                an = buf->href.RetrieveAnchor(po);
                if (visited != true && an == NULL)
                    an = buf->formitem.RetrieveAnchor(po);
                hseq++;
                if (visited == true && an)
                {
                    auto url = URL::Parse(an->url, buf->BaseURL());
                    if (getHashHist(w3mApp::Instance().URLHist, url.ToStr()->ptr))
                    {
                        goto _end;
                    }
                }
            } while (an == NULL || an == pan);
        }
        else
        {
            an = buf->href.ClosestNext(NULL, x, y);
            if (!visited)
                an = buf->formitem.ClosestNext(an, x, y);
            if (an == NULL)
            {
                if (visited)
                    return;
                an = pan;
                break;
            }
            x = an->start.pos;
            y = an->start.line;
            if (visited == true)
            {
                auto url = URL::Parse(an->url, buf->BaseURL());
                if (getHashHist(w3mApp::Instance().URLHist, url.ToStr()->ptr))
                {
                    goto _end;
                }
            }
        }
    }
    if (visited == true)
        return;

_end:
    if (an == NULL || an->hseq < 0)
        return;

    buf->Goto(buf->hmarklist[an->hseq]);
    displayCurrentbuf(B_NORMAL);
}

/* go to the previous anchor */
void _prevA(int visited)
{
    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();
    if (buf->LineCount() == 0)
        return;
    if (buf->hmarklist.empty())
        return;

    auto an = buf->href.RetrieveAnchor(buf->CurrentPoint());
    if (!visited && !an)
        an = buf->formitem.RetrieveAnchor(buf->CurrentPoint());

    auto y = buf->CurrentLine()->linenumber;
    auto x = buf->pos;

    int n = w3mApp::Instance().w3mApp::Instance().searchKeyNum();
    if (visited == true)
    {
        n = buf->hmarklist.size();
    }

    for (int i = 0; i < n; i++)
    {
        auto pan = an;
        if (an && an->hseq >= 0)
        {
            int hseq = an->hseq - 1;
            do
            {
                if (hseq < 0)
                {
                    if (visited == true)
                        return;
                    an = pan;
                    goto _end;
                }
                auto &po = buf->hmarklist[hseq];
                an = buf->href.RetrieveAnchor(po);
                if (visited != true && an == NULL)
                    an = buf->formitem.RetrieveAnchor(po);
                hseq--;
                if (visited == true && an)
                {
                    auto url = URL::Parse(an->url, buf->BaseURL());
                    if (getHashHist(w3mApp::Instance().URLHist, url.ToStr()->ptr))
                    {
                        goto _end;
                    }
                }
            } while (an == NULL || an == pan);
        }
        else
        {
            an = buf->href.ClosestPrev(NULL, x, y);
            if (visited != true)
                an = buf->formitem.ClosestPrev(an, x, y);
            if (an == NULL)
            {
                if (visited == true)
                    return;
                an = pan;
                break;
            }
            x = an->start.pos;
            y = an->start.line;
            if (visited == true && an)
            {
                auto url = URL::Parse(an->url, buf->BaseURL());
                if (getHashHist(w3mApp::Instance().URLHist, url.ToStr()->ptr))
                {
                    goto _end;
                }
            }
        }
    }
    if (visited == true)
        return;

_end:
    if (an == NULL || an->hseq < 0)
        return;

    buf->Goto(buf->hmarklist[an->hseq]);
    displayCurrentbuf(B_NORMAL);
}

void gotoLabel(std::string_view label)
{
    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();
    auto al = buf->name.SearchByUrl(label.data());
    if (al == NULL)
    {
        /* FIXME: gettextize? */
        disp_message(Sprintf("%s is not found", label)->ptr, true);
        return;
    }

    // auto copy = buf->Copy();

    // for (int i = 0; i < MAX_LB; i++)
    //     copy->linkBuffer[i] = NULL;
    // copy->currentURL.fragment = label;
    // pushHashHist(w3mApp::Instance().URLHist, copy->currentURL.ToStr()->ptr);
    // (*copy->clone)++;
    GetCurrentTab()->Push(buf->currentURL);
    GetCurrentTab()->GetCurrentBuffer()->Goto(al->start, w3mApp::Instance().label_topline);
    displayCurrentbuf(B_FORCE_REDRAW);
    return;
}

/* go to the next left/right anchor */
void nextX(int d, int dy)
{
    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();
    if (buf->LineCount() == 0)
        return;
    if (buf->hmarklist.empty())
        return;

    auto an = buf->href.RetrieveAnchor(buf->CurrentPoint());
    if (an == NULL)
        an = buf->formitem.RetrieveAnchor(buf->CurrentPoint());

    auto l = buf->CurrentLine();
    auto x = buf->pos;
    auto y = l->linenumber;
    AnchorPtr pan;
    int n = w3mApp::Instance().w3mApp::Instance().searchKeyNum();
    for (int i = 0; i < n; i++)
    {
        if (an)
            x = (d > 0) ? an->end.pos : an->start.pos - 1;
        an = NULL;
        while (1)
        {
            for (; x >= 0 && x < l->len(); x += d)
            {
                an = buf->href.RetrieveAnchor({y, x});
                if (!an)
                    an = buf->formitem.RetrieveAnchor({y, x});
                if (an)
                {
                    pan = an;
                    break;
                }
            }
            if (!dy || an)
                break;
            l = (dy > 0) ? buf->NextLine(l) : buf->PrevLine(l);
            if (!l)
                break;
            x = (d > 0) ? 0 : l->len() - 1;
            y = l->linenumber;
        }
        if (!an)
            break;
    }

    if (pan == NULL)
        return;
    buf->GotoLine(y);
    buf->pos = pan->start.pos;
    buf->ArrangeCursor();
    displayCurrentbuf(B_NORMAL);
}

/* go to the next downward/upward anchor */
void nextY(int d)
{
    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();
    if (buf->LineCount() == 0)
        return;
    if (buf->hmarklist.empty())
        return;

    auto an = buf->href.RetrieveAnchor(buf->CurrentPoint());
    if (!an)
        an = buf->formitem.RetrieveAnchor(buf->CurrentPoint());

    int x = buf->pos;
    int y = buf->CurrentLine()->linenumber + d;
    AnchorPtr pan;
    int n = w3mApp::Instance().w3mApp::Instance().searchKeyNum();
    int hseq = -1;
    for (int i = 0; i < n; i++)
    {
        if (an)
            hseq = abs(an->hseq);
        an = NULL;
        for (; y >= 0 && y <= buf->LineCount(); y += d)
        {
            an = buf->href.RetrieveAnchor({y, x});
            if (!an)
                an = buf->formitem.RetrieveAnchor({y, x});
            if (an && hseq != abs(an->hseq))
            {
                pan = an;
                break;
            }
        }
        if (!an)
            break;
    }

    if (pan == NULL)
        return;
    buf->GotoLine(pan->start.line);
    buf->ArrangeLine();
    displayCurrentbuf(B_NORMAL);
}

/* go to specified URL */
void goURL0(const char *prompt, int relative)
{
    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();

    auto url = w3mApp::Instance().searchKeyData();
    URL *current = nullptr;
    if (url == NULL)
    {
        Hist *hist = copyHist(w3mApp::Instance().URLHist);
        current = buf->BaseURL();
        if (current)
        {
            char *c_url = current->ToStr()->ptr;
            if (Network::Instance().DefaultURLString == DEFAULT_URL_CURRENT)
            {
                url = c_url;
                if (w3mApp::Instance().DecodeURL)
                    url = url_unquote_conv(url, WC_CES_NONE);
            }
            else
                pushHist(hist, c_url);
        }
        auto a = buf->href.RetrieveAnchor(buf->CurrentPoint());
        if (a)
        {
            auto p_url = URL::Parse(a->url, current);
            auto a_url = p_url.ToStr()->ptr;
            if (Network::Instance().DefaultURLString == DEFAULT_URL_LINK)
            {
                url = a_url;
                if (w3mApp::Instance().DecodeURL)
                    url = url_unquote_conv(url, buf->document_charset);
            }
            else
                pushHist(hist, a_url);
        }
        url = inputLineHist(prompt, url, IN_URL, hist);
        if (url != NULL)
            SKIP_BLANKS(&url);
    }

    if (url != NULL)
    {
        if ((relative || *url == '#') && buf->document_charset)
            url = wc_conv_strict(url, w3mApp::Instance().InnerCharset,
                                 buf->document_charset)
                      ->ptr;
        else
            url = conv_to_system(url);
    }

    if (url == NULL || *url == '\0')
    {
        displayCurrentbuf(B_FORCE_REDRAW);
        return;
    }
    if (*url == '#')
    {
        gotoLabel(url + 1);
        return;
    }

    HttpReferrerPolicy referer;
    if (relative)
    {
        current = buf->BaseURL();
        referer = HttpReferrerPolicy::StrictOriginWhenCrossOrigin;
    }
    else
    {
        current = NULL;
        referer = HttpReferrerPolicy::NoReferer;
    }
    auto p_url = URL::Parse(url, current);
    pushHashHist(w3mApp::Instance().URLHist, p_url.ToStr()->ptr);
    cmd_loadURL(url, current, referer, NULL);
    if (buf != buf) /* success */
        pushHashHist(w3mApp::Instance().URLHist, buf->currentURL.ToStr()->ptr);
}

void anchorMn(AnchorPtr (*menu_func)(const BufferPtr &), int go)
{
    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();
    if (!buf->href || buf->hmarklist.empty())
        return;
    auto a = menu_func(buf);
    if (!a || a->hseq < 0)
        return;

    buf->Goto(buf->hmarklist[a->hseq]);
    displayCurrentbuf(B_NORMAL);
    if (go)
        followA(&w3mApp::Instance());
}

void _peekURL(int only_img)
{
    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();
    if (buf->LineCount() == 0)
        return;

    int offset = 0;
    // if (CurrentKey == PrevKey && s != NULL)
    // {
    //     if (s->Size() - offset >= Terminal::columns())
    //         offset++;
    //     else if (s->Size() <= offset) /* bug ? */
    //         offset = 0;
    //     goto disp;
    // }
    // else
    // {
    //     offset = 0;
    // }

    auto a = (only_img ? NULL : buf->href.RetrieveAnchor(buf->CurrentPoint()));
    Str s = nullptr;
    if (a == NULL)
    {
        a = (only_img ? NULL : buf->formitem.RetrieveAnchor(buf->CurrentPoint()));
        if (a == NULL)
        {
            a = buf->img.RetrieveAnchor(buf->CurrentPoint());
            if (a == NULL)
                return;
        }
        else
            s = a->item->ToStr();
    }
    if (!s)
    {
        auto pu = URL::Parse(a->url, buf->BaseURL());
        s = pu.ToStr();
    }
    if (w3mApp::Instance().DecodeURL)
        s = Strnew(url_unquote_conv(s->ptr, buf->document_charset));

    auto propstr = PropertiedString::create(s);

    // s = checkType(s, &pp, NULL);
    // p = NewAtom_N(Lineprop, s->Size());
    // bcopy((void *)pp, (void *)p, s->Size() * sizeof(Lineprop));

    // disp:
    auto n = w3mApp::Instance().w3mApp::Instance().searchKeyNum();
    if (n > 1 && s->Size() > (n - 1) * (Terminal::columns() - 1))
        offset = (n - 1) * (Terminal::columns() - 1);

    while (offset < propstr.len() && propstr.propBuf()[offset] & PC_WCHAR2)
        offset++;

    disp_message_nomouse(&s->ptr[offset], true);
}

/* show current URL */
Str currentURL(void)
{
    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();
    if (buf->bufferprop & BP_INTERNAL)
        return Strnew_size(0);
    return buf->currentURL.ToStr();
}

void _docCSet(CharacterEncodingScheme charset)
{
    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();
    if (buf->bufferprop & BP_INTERNAL)
        return;
    if (buf->sourcefile.empty())
    {
        disp_message("Can't reload...", false);
        return;
    }
    buf->document_charset = charset;
    buf->need_reshape = true;
    displayCurrentbuf(B_FORCE_REDRAW);
}

static int s_display_ok = false;
int display_ok()
{
    return s_display_ok;
}

/* spawn external browser */
void invoke_browser(char *url)
{
    ClearCurrentKeyData(); /* not allowed in w3m-control: */
    std::string_view browser = w3mApp::Instance().searchKeyData();
    if (browser.empty())
    {
        switch (prec_num())
        {
        case 0:
        case 1:
            browser = w3mApp::Instance().ExtBrowser;
            break;
        case 2:
            browser = w3mApp::Instance().ExtBrowser2;
            break;
        case 3:
            browser = w3mApp::Instance().ExtBrowser3;
            break;
        }
        if (browser.empty())
        {
            browser = inputStr("Browse command: ", NULL);
        }
    }

    browser = conv_to_system(browser.data());
    if (browser.empty())
    {
        displayCurrentbuf(B_NORMAL);
        return;
    }

    bool bg = 0;
    int len;
    if ((len = browser.size()) >= 2 && browser[len - 1] == '&' &&
        browser[len - 2] != '\\')
    {
        browser.remove_suffix(2);
        bg = 1;
    }
    auto cmd = myExtCommand(browser.data(), shell_quote(url), false);
    StripRight(cmd);
    fmTerm();
    mySystem(cmd->ptr, bg);
    fmInit();
    displayCurrentbuf(B_FORCE_REDRAW);
}

#define DICTBUFFERNAME "*dictionary*"
void execdict(char *word)
{
    char *w, *dictcmd;
    BufferPtr buf;

    if (!w3mApp::Instance().UseDictCommand || word == NULL || *word == '\0')
    {
        displayCurrentbuf(B_NORMAL);
        return;
    }
    w = conv_to_system(word);
    if (*w == '\0')
    {
        displayCurrentbuf(B_NORMAL);
        return;
    }
    dictcmd = Sprintf("%s?%s", w3mApp::Instance().DictCommand, UrlEncode(Strnew(w))->ptr)->ptr;
    GetCurrentTab()->Push(URL::LocalPath(dictcmd));
    // buf = loadGeneralFile(URL::ParsePath(dictcmd), NULL, HttpReferrerPolicy::NoReferer, NULL);
    // if (buf == NULL)
    // {
    //     disp_message("Execution failed", true);
    //     return;
    // }
    // else
    // {
    //     buf->filename = w;
    //     buf->buffername = Sprintf("%s %s", DICTBUFFERNAME, word)->ptr;
    //     if (buf->type.empty())
    //         buf->type = "text/plain";
    //     GetCurrentTab()->Push(buf);
    // }
    displayCurrentbuf(B_FORCE_REDRAW);
}

char *GetWord(const BufferPtr &buf)
{
    int b, e;
    char *p;

    if ((p = getCurWord(buf, &b, &e)) != NULL)
    {
        return Strnew_charp_n(p, e - b)->ptr;
    }
    return NULL;
}

void tabURL0(TabPtr tab, const char *prompt, int relative)
{
    if (tab == GetCurrentTab())
    {
        goURL0(prompt, relative);
        return;
    }

    CreateTabSetCurrent();
    auto buf = GetCurrentTab()->GetCurrentBuffer();
    goURL0(prompt, relative);
    if (tab == NULL)
    {
        // if (buf != GetCurrentTab()->GetCurrentBuffer())
        //     GetCurrentTab()->DeleteBuffer(buf);
        // else
        //     deleteTab(GetCurrentTab());
    }
    else if (buf != GetCurrentTab()->GetCurrentBuffer())
    {
        // TODO:

        /* buf <- p <- ... <- Currentbuf = c */
        // BufferPtr c;
        // BufferPtr p;
        // c = GetCurrentTab()->GetCurrentBuffer();
        // p = prevBuffer(c, buf);
        // p->nextBuffer = NULL;
        // GetCurrentTab()->SetFirstBuffer(buf);
        deleteTab(GetCurrentTab());
        // tab->Push(buf);
        SetCurrentTab(tab);
        // for (buf = p; buf; buf = p)
        // {
        //     p = prevBuffer(c, buf);
        //     GetCurrentTab()->BufferPushBeforeCurrent(buf);
        // }
    }
    displayCurrentbuf(B_FORCE_REDRAW);
}

// void cmd_loadBuffer(BufferPtr buf, BufferProps prop, LinkBufferTypes linkid)
// {
//     if (buf == NULL)
//     {
//         disp_err_message("Can't load string", false);
//     }
//     else
//     {
//         buf->bufferprop |= (BP_INTERNAL | prop);
//         if (!(buf->bufferprop & BP_NO_URL))
//             buf->currentURL = GetCurrentTab()->GetCurrentBuffer()->currentURL;
//         if (linkid != LB_NOLINK)
//         {
//             buf->linkBuffer[linkid] = GetCurrentTab()->GetCurrentBuffer();
//             GetCurrentTab()->GetCurrentBuffer()->linkBuffer[linkid] = buf;
//         }
//         GetCurrentTab()->Push(buf);
//     }
//     displayCurrentbuf(B_FORCE_REDRAW);
// }
