#include <string_view_util.h>
#include <unistd.h>
#include "textlist.h"
#include "history.h"
#include "frontend/terminal.h"
#include "indep.h"
#include "gc_helper.h"
#include "rc.h"
#include "urimethod.h"

#include "frontend/display.h"
#include "html/parsetag.h"
#include "public.h"
#include "file.h"
#include "frontend/lineinput.h"
#include <setjmp.h>
#include <signal.h>
#include "ucs.h"
#include "stream/network.h"
#include "command_dispatcher.h"
#include "html/image.h"
#include "commands.h"
#include "stream/url.h"
#include "stream/cookie.h"
#include "ctrlcode.h"
#include "frontend/anchor.h"
#include "html/maparea.h"
#include "frontend/mouse.h"
#include "frontend/buffer.h"
#include "frontend/tabbar.h"
#include "frontend/menu.h"
#include "entity.h"
#include "loader.h"
#include "charset.h"
#include "w3m.h"

static void dump_extra(const BufferPtr &buf)
{
    printf("W3m-current-url: %s\n", buf->url.ToStr()->ptr);
    // if (buf->baseURL)
    //     printf("W3m-base-url: %s\n", buf->baseURL.ToStr()->ptr);
    printf("W3m-document-charset: %s\n",
           wc_ces_to_charset(buf->m_document->document_charset));

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

static void dump_head(w3mApp *w3m, BufferPtr buf)
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
    //                           buf->m_document->document_charset)
    //                ->ptr);
    // }
    puts("");
}

void do_dump(w3mApp *w3m, BufferPtr buf)
{
    dump_extra(buf);
    dump_head(w3m, buf);
    buf->DumpSource();
    {
        int i;
        saveBuffer(buf, stdout, false);
        if (w3mApp::Instance().displayLinkNumber && buf->m_document->href)
        {
            printf("\nReferences:\n\n");
            for (i = 0; i < buf->m_document->href.size(); i++)
            {
                auto a = buf->m_document->href.anchors[i];
                if (a->slave)
                    continue;

                auto pu = URL::Parse(a->url, &buf->url);
                auto s = pu.ToStr();
                if (w3mApp::Instance().DecodeURL)
                    s = Strnew(url_unquote_conv(s->ptr, GetCurrentBuffer()->m_document->document_charset));
                printf("[%d] %s\n", a->hseq + 1, s->ptr);
            }
        }
    }
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
    //     if (w3mApp::Instance().RenderFrame && GetCurrentBuffer()->frameset != NULL)
    //         rFrame(&w3mApp::Instance());
    // }
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
    //     if (w3mApp::Instance().RenderFrame && GetCurrentBuffer()->frameset != NULL)
    //         rFrame(&w3mApp::Instance());
    // }
    GetCurrentTab()->Push(URL::Parse(url, current));
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
    pushHashHist(w3mApp::Instance().URLHist, url);
    return 1;
}

int prev_nonnull_line(BufferPtr buf, LinePtr line)
{
    LinePtr l;

    for (l = line; l != NULL && l->len() == 0; l = buf->m_document->PrevLine(l))
        ;
    if (l == NULL || l->len() == 0)
        return -1;

    GetCurrentBuffer()->SetCurrentLine(l);
    if (l != line)
        GetCurrentBuffer()->pos = GetCurrentBuffer()->CurrentLine()->len();
    return 0;
}

int next_nonnull_line(BufferPtr buf, LinePtr line)
{
    LinePtr l;

    for (l = line; l != NULL && l->len() == 0; l = buf->m_document->NextLine(l))
        ;

    if (l == NULL || l->len() == 0)
        return -1;

    GetCurrentBuffer()->SetCurrentLine(l);
    if (l != line)
        GetCurrentBuffer()->pos = 0;
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

int cur_real_linenumber(const BufferPtr &buf)
{
    LinePtr cur = buf->CurrentLine();
    if (!cur)
        return 1;
    auto n = cur->real_linenumber ? cur->real_linenumber : 1;
    for (auto l = buf->m_document->FirstLine(); l && l != cur && l->real_linenumber == 0; l = buf->m_document->NextLine(l))
    { /* header */
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
    auto buf = GetCurrentBuffer();
    if (buf->m_document->LineCount() == 0)
        return;
    auto a = buf->m_document->formitem.RetrieveAnchor(buf->CurrentPoint());
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
        auto p = inputStrHist("TEXT:", fi->value.size() ? fi->value.c_str() : "", w3mApp::Instance().TextHist, 1);
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
        auto p = inputFilenameHist("Filename:", fi->value.size() ? fi->value.c_str() : "", nullptr, 1);
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
        auto p = inputLine("Password:", fi->value.size() ? fi->value.c_str() : "", IN_PASSWORD, 1);
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
        auto multipart = (fi->parent.lock()->method == FORM_METHOD_POST &&
                          fi->parent.lock()->enctype == FORM_ENCTYPE_MULTIPART);

        auto form = fi->parent.lock();
        auto tmp = form->Query(fi, multipart);

        std::string tmp2 = fi->parent.lock()->action;
        if (tmp2 == "!CURRENT_URL!")
        {
            /* It means "current URL" */
            auto url = buf->url;
            url.query.clear();
            tmp2 = buf->url.ToStr()->ptr;
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
            tab->Push(URL::Parse(tmp2, &buf->url));
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
            tab->Push(URL::Parse(tmp2, &buf->url));
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
        for (auto i = 0; i < buf->m_document->formitem.size(); i++)
        {
            auto a2 = buf->m_document->formitem.anchors[i];
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
}

int on_target = 1;

/* follow HREF link in the buffer */
void bufferA(w3mApp *w3m, const CommandContext &context)
{
    on_target = false;
    followA(w3m, context);
    on_target = true;
}

// BufferPtr loadLink(const char *url, const char *target, HttpReferrerPolicy referer, FormList *request)
// {
//     BufferPtr nfbuf;
//     union frameset_element *f_element = NULL;

//     message(Sprintf("loading %s", url)->ptr, 0, 0);
//     refresh();

//     auto base = &GetCurrentBuffer()->currentURL;
//     if (base == NULL ||
//         base->scheme == SCM_LOCAL || base->scheme == SCM_LOCAL_CGI)
//         referer = HttpReferrerPolicy::NoReferer;

//     auto newBuf = loadGeneralFile(URL::Parse(url, &GetCurrentBuffer()->currentURL), &GetCurrentBuffer()->currentURL, referer, request);
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
//         !(GetCurrentBuffer()->bufferprop & BP_FRAME) /* This page is not a frame page */
//     )
//     {
//         return loadNormalBuf(newBuf, true);
//     }
//     nfbuf = GetCurrentBuffer()->linkBuffer[LB_N_FRAME];
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
//     pushFrameTree(&(nfbuf->frameQ), copyFrameSet(nfbuf->frameset), GetCurrentBuffer());
//     /* delete frame view buffer */
//     auto tab = GetCurrentTab();
//     // tab->DeleteBuffer(GetCurrentBuffer());
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
//         auto buf = GetCurrentBuffer();

//         if (!al)
//         {
//             label = std::string("_") + target;
//             al = buf->m_document->name.SearchByUrl(label.c_str());
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

    for (int i = 0; i < src->parent.lock()->items.size(); ++i)
    {
        if (src->parent.lock()->items[i] == src)
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
    else if (buf->m_document->document_charset && buf->m_document->document_charset != WC_CES_US_ASCII)
        charset = buf->m_document->document_charset;
    return wc_conv_strict(val.data(), w3mApp::Instance().InnerCharset, charset);
}

// BufferPtr loadNormalBuf(BufferPtr buf, int renderframe)
// {
//     GetCurrentTab()->Push(buf);
//     if (renderframe && w3mApp::Instance().RenderFrame && GetCurrentBuffer()->frameset != NULL)
//         rFrame(&w3mApp::Instance());
//     return buf;
// }

/* go to the next [visited] anchor */
void _nextA(int visited, int n)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    if (buf->m_document->LineCount() == 0)
        return;
    if (buf->m_document->hmarklist.empty())
        return;

    auto an = buf->m_document->href.RetrieveAnchor(buf->CurrentPoint());
    if (!visited && !an)
        an = buf->m_document->formitem.RetrieveAnchor(buf->CurrentPoint());

    auto y = buf->CurrentLine()->linenumber;
    auto x = buf->pos;

    // int n = w3mApp::Instance().w3mApp::Instance().searchKeyNum();
    if (visited == true)
    {
        n = buf->m_document->hmarklist.size();
    }

    for (int i = 0; i < n; i++)
    {
        auto pan = an;
        if (an && an->hseq >= 0)
        {
            int hseq = an->hseq + 1;
            do
            {
                if (hseq >= buf->m_document->hmarklist.size())
                {
                    if (visited == true)
                        return;
                    an = pan;
                    goto _end;
                }
                auto &po = buf->m_document->hmarklist[hseq];
                an = buf->m_document->href.RetrieveAnchor(po);
                if (visited != true && an == NULL)
                    an = buf->m_document->formitem.RetrieveAnchor(po);
                hseq++;
                if (visited == true && an)
                {
                    auto url = URL::Parse(an->url, &buf->url);
                    if (getHashHist(w3mApp::Instance().URLHist, url.ToStr()->ptr))
                    {
                        goto _end;
                    }
                }
            } while (an == NULL || an == pan);
        }
        else
        {
            an = buf->m_document->href.ClosestNext(NULL, x, y);
            if (!visited)
                an = buf->m_document->formitem.ClosestNext(an, x, y);
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
                auto url = URL::Parse(an->url, &buf->url);
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

    buf->Goto(buf->m_document->hmarklist[an->hseq]);
}

/* go to the previous anchor */
void _prevA(int visited, int n)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    if (buf->m_document->LineCount() == 0)
        return;
    if (buf->m_document->hmarklist.empty())
        return;

    auto an = buf->m_document->href.RetrieveAnchor(buf->CurrentPoint());
    if (!visited && !an)
        an = buf->m_document->formitem.RetrieveAnchor(buf->CurrentPoint());

    auto y = buf->CurrentLine()->linenumber;
    auto x = buf->pos;

    // int n = w3mApp::Instance().w3mApp::Instance().searchKeyNum();
    if (visited == true)
    {
        n = buf->m_document->hmarklist.size();
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
                auto &po = buf->m_document->hmarklist[hseq];
                an = buf->m_document->href.RetrieveAnchor(po);
                if (visited != true && an == NULL)
                    an = buf->m_document->formitem.RetrieveAnchor(po);
                hseq--;
                if (visited == true && an)
                {
                    auto url = URL::Parse(an->url, &buf->url);
                    if (getHashHist(w3mApp::Instance().URLHist, url.ToStr()->ptr))
                    {
                        goto _end;
                    }
                }
            } while (an == NULL || an == pan);
        }
        else
        {
            an = buf->m_document->href.ClosestPrev(NULL, x, y);
            if (visited != true)
                an = buf->m_document->formitem.ClosestPrev(an, x, y);
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
                auto url = URL::Parse(an->url, &buf->url);
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

    buf->Goto(buf->m_document->hmarklist[an->hseq]);
}

void gotoLabel(std::string_view label)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    auto al = buf->m_document->name.SearchByUrl(label.data());
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
    GetCurrentTab()->Push(buf->url);
    GetCurrentBuffer()->Goto(al->start, w3mApp::Instance().label_topline);
}

/* go to the next left/right anchor */
void nextX(int d, int dy, int n)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    if (buf->m_document->LineCount() == 0)
        return;
    if (buf->m_document->hmarklist.empty())
        return;

    auto an = buf->m_document->href.RetrieveAnchor(buf->CurrentPoint());
    if (an == NULL)
        an = buf->m_document->formitem.RetrieveAnchor(buf->CurrentPoint());

    auto l = buf->CurrentLine();
    auto x = buf->pos;
    auto y = l->linenumber;
    AnchorPtr pan;
    // int n = w3mApp::Instance().w3mApp::Instance().searchKeyNum();
    for (int i = 0; i < n; i++)
    {
        if (an)
            x = (d > 0) ? an->end.pos : an->start.pos - 1;
        an = NULL;
        while (1)
        {
            for (; x >= 0 && x < l->len(); x += d)
            {
                an = buf->m_document->href.RetrieveAnchor({y, x});
                if (!an)
                    an = buf->m_document->formitem.RetrieveAnchor({y, x});
                if (an)
                {
                    pan = an;
                    break;
                }
            }
            if (!dy || an)
                break;
            l = (dy > 0) ? buf->m_document->NextLine(l) : buf->m_document->PrevLine(l);
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
}

/* go to the next downward/upward anchor */
void nextY(int d, int n)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    if (buf->m_document->LineCount() == 0)
        return;
    if (buf->m_document->hmarklist.empty())
        return;

    auto an = buf->m_document->href.RetrieveAnchor(buf->CurrentPoint());
    if (!an)
        an = buf->m_document->formitem.RetrieveAnchor(buf->CurrentPoint());

    int x = buf->pos;
    int y = buf->CurrentLine()->linenumber + d;
    AnchorPtr pan;
    // int n = w3mApp::Instance().w3mApp::Instance().searchKeyNum();
    int hseq = -1;
    for (int i = 0; i < n; i++)
    {
        if (an)
            hseq = abs(an->hseq);
        an = NULL;
        for (; y >= 0 && y <= buf->m_document->LineCount(); y += d)
        {
            an = buf->m_document->href.RetrieveAnchor({y, x});
            if (!an)
                an = buf->m_document->formitem.RetrieveAnchor({y, x});
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
}

/* go to specified URL */
void goURL0(std::string_view url, std::string_view prompt, int relative)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();

    // auto url = w3mApp::Instance().searchKeyData();
    URL *current = nullptr;
    if (url.empty())
    {
        Hist *hist = copyHist(w3mApp::Instance().URLHist);
        current = &buf->url;
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
        auto a = buf->m_document->href.RetrieveAnchor(buf->CurrentPoint());
        if (a)
        {
            auto p_url = URL::Parse(a->url, current);
            auto a_url = p_url.ToStr()->ptr;
            if (Network::Instance().DefaultURLString == DEFAULT_URL_LINK)
            {
                url = a_url;
                if (w3mApp::Instance().DecodeURL)
                    url = url_unquote_conv(url, buf->m_document->document_charset);
            }
            else
                pushHist(hist, a_url);
        }
        url = inputLineHist(prompt, url, IN_URL, hist, 1);
        url = svu::strip_left(url);
    }

    if (url.size())
    {
        if ((relative || url[0] == '#') && buf->m_document->document_charset)
            url = wc_conv_strict(url.data(), w3mApp::Instance().InnerCharset,
                                 buf->m_document->document_charset)
                      ->ptr;
        else
            url = conv_to_system(url.data());
    }

    if (url.empty())
    {
        return;
    }
    if (url[0] == '#')
    {
        gotoLabel(url.substr(1));
        return;
    }

    HttpReferrerPolicy referer;
    if (relative)
    {
        current = &buf->url;
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
        pushHashHist(w3mApp::Instance().URLHist, buf->url.ToStr()->ptr);
}

void anchorMn(AnchorPtr (*menu_func)(const BufferPtr &), int go)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    if (!buf->m_document->href || buf->m_document->hmarklist.empty())
        return;

    auto a = menu_func(buf);
    if (!a || a->hseq < 0)
        return;

    buf->Goto(buf->m_document->hmarklist[a->hseq]);
    if (go)
        followA(&w3mApp::Instance(), {});
}

void _peekURL(int only_img, int n)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    if (buf->m_document->LineCount() == 0)
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

    auto a = (only_img ? NULL : buf->m_document->href.RetrieveAnchor(buf->CurrentPoint()));
    Str s = nullptr;
    if (a == NULL)
    {
        a = (only_img ? NULL : buf->m_document->formitem.RetrieveAnchor(buf->CurrentPoint()));
        if (a == NULL)
        {
            a = buf->m_document->img.RetrieveAnchor(buf->CurrentPoint());
            if (a == NULL)
                return;
        }
        else
            s = a->item->ToStr();
    }
    if (!s)
    {
        auto pu = URL::Parse(a->url, &buf->url);
        s = pu.ToStr();
    }
    if (w3mApp::Instance().DecodeURL)
        s = Strnew(url_unquote_conv(s->ptr, buf->m_document->document_charset));

    auto propstr = PropertiedString::create(s);

    // s = checkType(s, &pp, NULL);
    // p = NewAtom_N(Lineprop, s->Size());
    // bcopy((void *)pp, (void *)p, s->Size() * sizeof(Lineprop));

    // disp:
    // auto n = w3mApp::Instance().w3mApp::Instance().searchKeyNum();
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
    auto buf = GetCurrentBuffer();
    if (buf->bufferprop & BP_INTERNAL)
        return Strnew_size(0);
    return buf->url.ToStr();
}

void _docCSet(CharacterEncodingScheme charset)
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    if (buf->bufferprop & BP_INTERNAL)
        return;
    if (buf->sourcefile.empty())
    {
        disp_message("Can't reload...", false);
        return;
    }
    buf->m_document->document_charset = charset;
    buf->need_reshape = true;
}

static int s_display_ok = false;
int display_ok()
{
    return s_display_ok;
}

/* spawn external browser */
void invoke_browser(char *url, std::string_view browser, int prec_num)
{
    if (browser.empty())
    {
        switch (prec_num)
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
            browser = inputStr("Browse command: ", "", 1);
        }
    }

    browser = conv_to_system(browser.data());
    if (browser.empty())
    {
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
}

#define DICTBUFFERNAME "*dictionary*"
void execdict(char *word)
{
    char *w, *dictcmd;
    BufferPtr buf;

    if (!w3mApp::Instance().UseDictCommand || word == NULL || *word == '\0')
    {
        return;
    }
    w = conv_to_system(word);
    if (*w == '\0')
    {
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

void tabURL0(TabPtr tab, std::string_view url, const char *prompt, int relative)
{
    if (tab == GetCurrentTab())
    {
        goURL0(url, prompt, relative);
        return;
    }

    CreateTabSetCurrent();
    auto buf = GetCurrentBuffer();
    goURL0(url, prompt, relative);
    if (tab == NULL)
    {
        // if (buf != GetCurrentBuffer())
        //     GetCurrentTab()->DeleteBuffer(buf);
        // else
        //     deleteTab(GetCurrentTab());
    }
    else if (buf != GetCurrentBuffer())
    {
        // TODO:

        /* buf <- p <- ... <- Currentbuf = c */
        // BufferPtr c;
        // BufferPtr p;
        // c = GetCurrentBuffer();
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
//             buf->currentURL = GetCurrentBuffer()->currentURL;
//         if (linkid != LB_NOLINK)
//         {
//             buf->linkBuffer[linkid] = GetCurrentBuffer();
//             GetCurrentBuffer()->linkBuffer[linkid] = buf;
//         }
//         GetCurrentTab()->Push(buf);
//     }
//     displayCurrentbuf(B_FORCE_REDRAW);
// }
