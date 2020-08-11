#include "html_processor.h"
#include "html/html_context.h"
#include "fm.h"
#include "indep.h"
#include "gc_helper.h"
#include "html/form.h"
#include "transport/loader.h"
#include "file.h"
#include "myctype.h"
#include "entity.h"
#include "http/compression.h"
#include "html/image.h"
#include "symbol.h"
#include "tagstack.h"
#include "frame.h"
#include "public.h"
#include "commands.h"
#include "html/maparea.h"
#include "html/tokenizer.h"
#include "w3m.h"
#include "frontend/terms.h"
#include "frontend/line.h"
#include "transport/istream.h"
#include <signal.h>
#include <setjmp.h>
#include <vector>

static void
print_internal_information(struct html_feed_environ *henv)
{
    // TDOO:
    //     TextLineList *tl = newTextLineList();

    //     {
    //         auto s = Strnew("<internal>");
    //         pushTextLine(tl, newTextLine(s, 0));
    //         if (henv->title)
    //         {
    //             s = Strnew_m_charp("<title_alt title=\"",
    //                                html_quote(henv->title), "\">");
    //             pushTextLine(tl, newTextLine(s, 0));
    //         }
    //     }

    //     get_formselect()->print_internal(tl);
    //     get_textarea()->print_internal(tl);

    //     {
    //         auto s = Strnew("</internal>");
    //         pushTextLine(tl, newTextLine(s, 0));
    //     }

    //     if (henv->buf)
    //     {
    //         appendTextLineList(henv->buf, tl);
    //     }
    //     else if (henv->f)
    //     {
    //         TextLineListItem *p;
    //         for (p = tl->first; p; p = p->next)
    //             fprintf(henv->f, "%s\n", Str_conv_to_halfdump(p->ptr->line)->ptr);
    //     }
}

static int currentLn(BufferPtr buf)
{
    if (buf->CurrentLine())
        /*     return buf->currentLine->real_linenumber + 1;      */
        return buf->CurrentLine()->linenumber + 1;
    else
        return 1;
}

///
/// HTML parse state
///
class HtmlProcessor
{
public:
    Lineprop effect = P_UNKNOWN;
    Lineprop ex_effect = P_UNKNOWN;
    char symbol = '\0';

private:
    Anchor *a_href = nullptr;
    Anchor *a_img = nullptr;
    Anchor *a_form = nullptr;

    std::vector<Anchor *> a_textarea;

    std::vector<Anchor *> a_select;

    HtmlTags internal = HTML_UNKNOWN;

    frameset_element *idFrame = nullptr;
    std::vector<struct frameset *> frameset_s;

public:
    /* end of processing for one line */
    bool EndLineAddBuffer()
    {
        if (internal == HTML_UNKNOWN)
        {
            // add non html line
            return true;
        }

        // not add html line
        if (internal == HTML_N_INTERNAL)
        {
            // exit internal
            internal = HTML_UNKNOWN;
        }
        return false;
    }

    void Process(parsed_tag *tag, BufferPtr buf, int pos, const char *str, HtmlContext *context)
    {
        switch (tag->tagid)
        {
        case HTML_B:
            this->effect |= PE_BOLD;
            break;
        case HTML_N_B:
            this->effect &= ~PE_BOLD;
            break;
        case HTML_I:
            this->ex_effect |= PE_EX_ITALIC;
            break;
        case HTML_N_I:
            this->ex_effect &= ~PE_EX_ITALIC;
            break;
        case HTML_INS:
            this->ex_effect |= PE_EX_INSERT;
            break;
        case HTML_N_INS:
            this->ex_effect &= ~PE_EX_INSERT;
            break;
        case HTML_U:
            this->effect |= PE_UNDER;
            break;
        case HTML_N_U:
            this->effect &= ~PE_UNDER;
            break;
        case HTML_S:
            this->ex_effect |= PE_EX_STRIKE;
            break;
        case HTML_N_S:
            this->ex_effect &= ~PE_EX_STRIKE;
            break;
        case HTML_A:
        {
            char *p;
            if (renderFrameSet &&
                tag->TryGetAttributeValue(ATTR_FRAMENAME, &p))
            {
                p = wc_conv_strict(p, w3mApp::Instance().InnerCharset, buf->document_charset)->ptr;
                if (!this->idFrame || strcmp(this->idFrame->body->name, p))
                {
                    this->idFrame = search_frame(renderFrameSet, p);
                    if (this->idFrame && this->idFrame->body->attr != F_BODY)
                        this->idFrame = nullptr;
                }
            }
            p = nullptr;
            auto q = Strnew(buf->baseTarget)->ptr;
            char *id = nullptr;
            if (tag->TryGetAttributeValue(ATTR_NAME, &id))
            {
                id = wc_conv_strict(id, w3mApp::Instance().InnerCharset, buf->document_charset)->ptr;
                buf->name.Put(Anchor::CreateName(id, currentLn(buf), pos));
            }
            if (tag->TryGetAttributeValue(ATTR_HREF, &p))
                p = wc_conv_strict(remove_space(p), w3mApp::Instance().InnerCharset,
                                   buf->document_charset)
                        ->ptr;
            if (tag->TryGetAttributeValue(ATTR_TARGET, &q))
                q = wc_conv_strict(q, w3mApp::Instance().InnerCharset, buf->document_charset)->ptr;
            char *r = nullptr;
            if (tag->TryGetAttributeValue(ATTR_REFERER, &r))
                r = wc_conv_strict(r, w3mApp::Instance().InnerCharset, buf->document_charset)->ptr;
            char *s = nullptr;
            tag->TryGetAttributeValue(ATTR_TITLE, &s);
            auto t = "";
            tag->TryGetAttributeValue(ATTR_ACCESSKEY, &t);
            auto hseq = 0;
            tag->TryGetAttributeValue(ATTR_HSEQ, &hseq);
            if (hseq > 0)
                buf->putHmarker(currentLn(buf), pos, hseq - 1);
            else if (hseq < 0)
            {
                int h = -hseq - 1;
                if (buf->hmarklist.size() &&
                    h < buf->hmarklist.size() &&
                    buf->hmarklist[h].invalid)
                {
                    buf->hmarklist[h].pos = pos;
                    buf->hmarklist[h].line = currentLn(buf);
                    buf->hmarklist[h].invalid = 0;
                    hseq = -hseq;
                }
            }
            if (id && this->idFrame)
            {
                auto a = Anchor::CreateName(id, currentLn(buf), pos);
                this->idFrame->body->nameList.Put(a);
            }
            if (p)
            {
                this->effect |= PE_ANCHOR;
                this->a_href = buf->href.Put(Anchor::CreateHref(p,
                                                                q ? q : "",
                                                                r ? r : "",
                                                                s ? s : "",
                                                                *t, currentLn(buf), pos));
                this->a_href->hseq = ((hseq > 0) ? hseq : -hseq) - 1;
                this->a_href->slave = (hseq > 0) ? FALSE : TRUE;
            }
            break;
        }
        case HTML_N_A:
            this->effect &= ~PE_ANCHOR;
            if (this->a_href)
            {
                this->a_href->end.line = currentLn(buf);
                this->a_href->end.pos = pos;
                if (this->a_href->start == this->a_href->end)
                {
                    if (buf->hmarklist.size() &&
                        this->a_href->hseq < buf->hmarklist.size())
                        buf->hmarklist[this->a_href->hseq].invalid = 1;
                    this->a_href->hseq = -1;
                }
                this->a_href = nullptr;
            }
            break;

        case HTML_LINK:
            buf->linklist.push_back(Link::create(*tag, buf->document_charset));
            break;

        case HTML_IMG_ALT:
        {
            char *p = nullptr;
            if (tag->TryGetAttributeValue(ATTR_SRC, &p))
            {
                int w = -1, h = -1, iseq = 0, ismap = 0;
                int xoffset = 0, yoffset = 0, top = 0, bottom = 0;
                tag->TryGetAttributeValue(ATTR_HSEQ, &iseq);
                tag->TryGetAttributeValue(ATTR_WIDTH, &w);
                tag->TryGetAttributeValue(ATTR_HEIGHT, &h);
                tag->TryGetAttributeValue(ATTR_XOFFSET, &xoffset);
                tag->TryGetAttributeValue(ATTR_YOFFSET, &yoffset);
                tag->TryGetAttributeValue(ATTR_TOP_MARGIN, &top);
                tag->TryGetAttributeValue(ATTR_BOTTOM_MARGIN, &bottom);
                if (tag->HasAttribute(ATTR_ISMAP))
                    ismap = 1;
                char *q = nullptr;
                tag->TryGetAttributeValue(ATTR_USEMAP, &q);
                if (iseq > 0)
                {
                    buf->putHmarker(currentLn(buf), pos, iseq - 1);
                }

                char *s = nullptr;
                tag->TryGetAttributeValue(ATTR_TITLE, &s);
                p = wc_conv_strict(remove_space(p), w3mApp::Instance().InnerCharset,
                                   buf->document_charset)
                        ->ptr;
                this->a_img = buf->img.Put(Anchor::CreateImage(
                    p,
                    s ? s : "",
                    currentLn(buf), pos));

                this->a_img->hseq = iseq;
                this->a_img->image = nullptr;
                if (iseq > 0)
                {
                    URL u;
                    Image *image;

                    u.Parse2(this->a_img->url, GetCurBaseUrl());
                    this->a_img->image = image = New(Image);
                    image->url = u.ToStr()->ptr;
                    if (!uncompressed_file_type(u.file.c_str(), &image->ext))
                        image->ext = filename_extension(u.file.c_str(), TRUE);
                    image->cache = nullptr;
                    image->width =
                        (w > MAX_IMAGE_SIZE) ? MAX_IMAGE_SIZE : w;
                    image->height =
                        (h > MAX_IMAGE_SIZE) ? MAX_IMAGE_SIZE : h;
                    image->xoffset = xoffset;
                    image->yoffset = yoffset;
                    image->y = currentLn(buf) - top;
                    if (image->xoffset < 0 && pos == 0)
                        image->xoffset = 0;
                    if (image->yoffset < 0 && image->y == 1)
                        image->yoffset = 0;
                    image->rows = 1 + top + bottom;
                    image->map = q;
                    image->ismap = ismap;
                    image->touch = 0;
                    image->cache = getImage(image, GetCurBaseUrl(),
                                            IMG_FLAG_SKIP);
                }
                else if (iseq < 0)
                {
                    BufferPoint *po = &buf->imarklist[-iseq - 1];
                    auto a = buf->img.RetrieveAnchor(po->line, po->pos);
                    if (a)
                    {
                        this->a_img->url = a->url;
                        this->a_img->image = a->image;
                    }
                }
            }
            this->effect |= PE_IMAGE;
            break;
        }
        case HTML_N_IMG_ALT:
            this->effect &= ~PE_IMAGE;
            if (this->a_img)
            {
                this->a_img->end.line = currentLn(buf);
                this->a_img->end.pos = pos;
            }
            this->a_img = nullptr;
            break;
        case HTML_INPUT_ALT:
        {
            auto hseq = 0;
            tag->TryGetAttributeValue(ATTR_HSEQ, &hseq);
            auto form_id = -1;
            tag->TryGetAttributeValue(ATTR_FID, &form_id);
            int top = 0, bottom = 0;
            tag->TryGetAttributeValue(ATTR_TOP_MARGIN, &top);
            tag->TryGetAttributeValue(ATTR_BOTTOM_MARGIN, &bottom);

            auto form = context->FormCurrent(form_id);
            if (!form)
            {
                break;
            }

            if (hseq > 0)
            {
                int hpos = pos;
                if (*str == '[')
                    hpos++;
                buf->putHmarker(currentLn(buf), hpos, hseq - 1);
            }
            if (!form->target)
                form->target = Strnew(buf->baseTarget)->ptr;

            int textareanumber = -1;
            if (tag->TryGetAttributeValue(ATTR_TEXTAREANUMBER, &textareanumber))
            {
                context->TextareaGrow(textareanumber);
            }

            int selectnumber = -1;
            if (tag->TryGetAttributeValue(ATTR_SELECTNUMBER, &selectnumber))
            {
                context->FormSelectGrow(selectnumber);
            }

            auto fi = formList_addInput(form, tag, context);
            if (fi)
            {
                Anchor a;
                a.target = form->target ? form->target : "";
                a.item = fi;
                BufferPoint bp = {
                    line : currentLn(buf),
                    pos : pos
                };
                a.start = bp;
                a.end = bp;
                this->a_form = buf->formitem.Put(a);
            }
            else
            {
                this->a_form = nullptr;
            }

            if (textareanumber >= 0)
            {
                if (a_textarea.size() < textareanumber + 1)
                {
                    a_textarea.resize(textareanumber + 1);
                }
                a_textarea[textareanumber] = this->a_form;
            }

            if (selectnumber >= 0)
            {
                if (a_select.size() < selectnumber + 1)
                {
                    a_select.resize(selectnumber + 1);
                }
                a_select[selectnumber] = this->a_form;
            }

            if (this->a_form)
            {
                this->a_form->hseq = hseq - 1;
                this->a_form->y = currentLn(buf) - top;
                this->a_form->rows = 1 + top + bottom;
                if (!tag->HasAttribute(ATTR_NO_EFFECT))
                    this->effect |= PE_FORM;
                break;
            }
        }
        case HTML_N_INPUT_ALT:
            this->effect &= ~PE_FORM;
            if (this->a_form)
            {
                this->a_form->end.line = currentLn(buf);
                this->a_form->end.pos = pos;
                if (this->a_form->start.line == this->a_form->end.line &&
                    this->a_form->start.pos == this->a_form->end.pos)
                    this->a_form->hseq = -1;
            }
            this->a_form = nullptr;
            break;
        case HTML_MAP:
        {
            char *p = nullptr;
            if (tag->TryGetAttributeValue(ATTR_NAME, &p))
            {
                MapList *m = New(MapList);
                m->name = Strnew(p);
                m->area = newGeneralList();
                m->next = buf->maplist;
                buf->maplist = m;
            }
            break;
        }
        case HTML_N_MAP:
            /* nothing to do */
            break;
        case HTML_AREA:
        {
            if (buf->maplist == nullptr) /* outside of <map>..</map> */
                break;

            char *p = nullptr;
            if (tag->TryGetAttributeValue(ATTR_HREF, &p))
            {
                p = wc_conv_strict(remove_space(p), w3mApp::Instance().InnerCharset,
                                   buf->document_charset)
                        ->ptr;
                char *t = nullptr;
                tag->TryGetAttributeValue(ATTR_TARGET, &t);
                auto q = "";
                tag->TryGetAttributeValue(ATTR_ALT, &q);
                char *r = nullptr;
                tag->TryGetAttributeValue(ATTR_SHAPE, &r);
                char *s = nullptr;
                tag->TryGetAttributeValue(ATTR_COORDS, &s);
                auto a = newMapArea(p, t, q, r, s);
                pushValue(buf->maplist->area, (void *)a);
            }
            break;
        }

        //
        // frame
        //
        case HTML_FRAMESET:
        {
            auto frame = newFrameSet(tag);
            if (frame)
            {
                auto sp = this->frameset_s.size();
                this->frameset_s.push_back(frame);
                if (sp == 0)
                {
                    if (buf->frameset == nullptr)
                    {
                        buf->frameset = frame;
                    }
                    else
                    {
                        pushFrameTree(&(buf->frameQ), frame, nullptr);
                    }
                }
                else
                {
                    addFrameSetElement(this->frameset_s[sp - 1], *(union frameset_element *)frame);
                }
            }
            break;
        }
        case HTML_N_FRAMESET:
            if (!this->frameset_s.empty())
            {
                this->frameset_s.pop_back();
            }
            break;
        case HTML_FRAME:
            if (!this->frameset_s.empty())
            {
                union frameset_element element;
                element.body = newFrame(tag, buf);
                addFrameSetElement(this->frameset_s.back(), element);
            }
            break;

        case HTML_BASE:
        {
            char *p = nullptr;
            if (tag->TryGetAttributeValue(ATTR_HREF, &p))
            {
                p = wc_conv_strict(remove_space(p), w3mApp::Instance().InnerCharset,
                                   buf->document_charset)
                        ->ptr;
                buf->baseURL.Parse(p, nullptr);
            }
            if (tag->TryGetAttributeValue(ATTR_TARGET, &p))
                buf->baseTarget =
                    wc_conv_strict(p, w3mApp::Instance().InnerCharset, buf->document_charset)->ptr;
            break;
        }
        case HTML_META:
        {
            char *p = nullptr;
            tag->TryGetAttributeValue(ATTR_HTTP_EQUIV, &p);
            char *q = nullptr;
            tag->TryGetAttributeValue(ATTR_CONTENT, &q);
            if (p && q && !strcasecmp(p, "refresh") && MetaRefresh)
            {
                Str tmp = nullptr;
                int refresh_interval = getMetaRefreshParam(q, &tmp);

                if (tmp)
                {
                    p = wc_conv_strict(remove_space(tmp->ptr), w3mApp::Instance().InnerCharset,
                                       buf->document_charset)
                            ->ptr;
                    buf->event = setAlarmEvent(buf->event,
                                               refresh_interval,
                                               AL_IMPLICIT_ONCE,
                                               &gorURL, p);
                }
                else if (refresh_interval > 0)
                    buf->event = setAlarmEvent(buf->event,
                                               refresh_interval,
                                               AL_IMPLICIT,
                                               &reload, nullptr);
            }
            break;
        }
        case HTML_INTERNAL:
            this->internal = HTML_INTERNAL;
            break;
        case HTML_N_INTERNAL:
            this->internal = HTML_N_INTERNAL;
            break;
        case HTML_FORM_INT:
        {
            int form_id;
            if (tag->TryGetAttributeValue(ATTR_FID, &form_id))
                context->FormOpen(tag, form_id);
            break;
        }
        case HTML_TEXTAREA_INT:
        {
            int n_textarea = -1;
            if (tag->TryGetAttributeValue(ATTR_TEXTAREANUMBER, &n_textarea))
            {
                context->Textarea(n_textarea, Strnew());
            }
            break;
        }
        case HTML_N_TEXTAREA_INT:
        {
            auto [n, t] = context->TextareaCurrent();
            auto anchor = a_textarea[n];
            if (anchor)
            {
                FormItemList *item = anchor->item;
                item->init_value = item->value = t;
            }
            break;
        }
        case HTML_SELECT_INT:
        {
            int n_select = -1;
            if (tag->TryGetAttributeValue(ATTR_SELECTNUMBER, &n_select))
            {
                context->FormSetSelect(n_select);
            }
            break;
        }
        case HTML_N_SELECT_INT:
        {
            auto [n_select, select] = context->FormSelectCurrent();
            if (select)
            {
                FormItemList *item = a_select[n_select]->item;
                item->select_option = select->first;
                chooseSelectOption(item, item->select_option);
                item->init_selected = item->selected;
                item->init_value = item->value;
                item->init_label = item->label;
            }
            break;
        }
        case HTML_OPTION_INT:
        {
            auto [n_select, select] = context->FormSelectCurrent();
            if (select)
            {
                auto q = "";
                tag->TryGetAttributeValue(ATTR_LABEL, &q);
                auto p = q;
                tag->TryGetAttributeValue(ATTR_VALUE, &p);
                auto selected = tag->HasAttribute(ATTR_SELECTED);
                addSelectOption(select,
                                Strnew(p), Strnew(q),
                                selected);
            }
            break;
        }
        case HTML_TITLE_ALT:
        {
            char *p = nullptr;
            if (tag->TryGetAttributeValue(ATTR_TITLE, &p))
                buf->buffername = html_unquote(p, w3mApp::Instance().InnerCharset);
            break;
        }
        case HTML_SYMBOL:
        {
            this->effect |= PC_SYMBOL;
            char *p = nullptr;
            if (tag->TryGetAttributeValue(ATTR_TYPE, &p))
                this->symbol = (char)atoi(p);
            break;
        }
        case HTML_N_SYMBOL:
            this->effect &= ~PC_SYMBOL;
            break;
        }

        {
            char *id = nullptr;
            if (tag->TryGetAttributeValue(ATTR_ID, &id))
            {
                id = wc_conv_strict(id, w3mApp::Instance().InnerCharset, buf->document_charset)->ptr;
                buf->name.Put(Anchor::CreateName(id, currentLn(buf), pos));
            }
            char *p = nullptr;
            if (renderFrameSet &&
                tag->TryGetAttributeValue(ATTR_FRAMENAME, &p))
            {
                p = wc_conv_strict(p, w3mApp::Instance().InnerCharset, buf->document_charset)->ptr;
                if (!this->idFrame || strcmp(this->idFrame->body->name, p))
                {
                    this->idFrame = search_frame(renderFrameSet, p);
                    if (this->idFrame && this->idFrame->body->attr != F_BODY)
                        this->idFrame = nullptr;
                }
            }
            if (id && this->idFrame)
            {
                auto a = Anchor::CreateName(id, currentLn(buf), pos);
                this->idFrame->body->nameList.Put(a);
            }
        }
    }
};

///
/// 1行ごとに Line の構築と html タグを解釈する
///
using FeedFunc = Str (*)();
static void HTMLlineproc2body(BufferPtr buf, FeedFunc feed, int llimit, HtmlContext *seq)
{
    HtmlProcessor state;

    //
    // each line
    //
    Str line = nullptr;
    for (int nlines = 1; nlines != llimit; ++nlines)
    {
        if (!line)
        {
            // new line
            line = feed();
            if (!line)
            {
                break;
            }

            auto [n, t] = seq->TextareaCurrent();
            if (n >= 0 && *(line->ptr) != '<')
            { /* halfload */
                t->Push(line);
                continue;
            }

            StripRight(line);
        }

        //
        // each char
        //
        const char *str = line->ptr;
        auto endp = str + line->Size();
        PropertiedString out;
        while (str < endp)
        {
            auto mode = get_mctype(*str);
            if ((state.effect | ex_efct(state.ex_effect)) & PC_SYMBOL && *str != '<')
            {
                // symbol
                auto p = get_width_symbol(seq->SymbolWidth0(), state.symbol);
                assert(p.size() > 0);
                int len = get_mclen(p.data());
                mode = get_mctype(p[0]);

                out.push(mode | state.effect | ex_efct(state.ex_effect), p[0]);
                if (--len)
                {
                    mode = (mode & ~PC_WCHAR1) | PC_WCHAR2;
                    for (int i = 1; len--; ++i)
                    {
                        out.push(mode | state.effect | ex_efct(state.ex_effect), p[i]);
                    }
                }
                str += seq->SymbolWidth();
            }
            else if (mode == PC_CTRL || mode == PC_UNDEF)
            {
                // control
                out.push(PC_ASCII | state.effect | ex_efct(state.ex_effect), ' ');
                str++;
            }
            else if (mode & PC_UNKNOWN)
            {
                // unknown
                out.push(PC_ASCII | state.effect | ex_efct(state.ex_effect), ' ');
                str += get_mclen(str);
            }
            else if (*str != '<' && *str != '&')
            {
                // multibyte char ?
                int len = get_mclen(str);
                out.push(mode | state.effect | ex_efct(state.ex_effect), *(str++));
                if (--len)
                {
                    mode = (mode & ~PC_WCHAR1) | PC_WCHAR2;
                    while (len--)
                    {
                        out.push(mode | state.effect | ex_efct(state.ex_effect), *(str++));
                    }
                }
            }
            else if (*str == '&')
            {
                /* 
                 * & escape processing
                 */
                char *p;
                {
                    auto [pos, view] = getescapecmd(str, w3mApp::Instance().InnerCharset);
                    str = const_cast<char *>(pos);
                    p = const_cast<char *>(view.data());
                }

                while (*p)
                {
                    mode = get_mctype(*p);
                    if (mode == PC_CTRL || mode == PC_UNDEF)
                    {
                        out.push(PC_ASCII | state.effect | ex_efct(state.ex_effect), ' ');
                        p++;
                    }
                    else if (mode & PC_UNKNOWN)
                    {
                        out.push(PC_ASCII | state.effect | ex_efct(state.ex_effect), ' ');
                        p += get_mclen(p);
                    }
                    else
                    {
                        int len = get_mclen(p);
                        out.push(mode | state.effect | ex_efct(state.ex_effect), *(p++));
                        if (--len)
                        {
                            mode = (mode & ~PC_WCHAR1) | PC_WCHAR2;
                            while (len--)
                            {
                                out.push(mode | state.effect | ex_efct(state.ex_effect), *(p++));
                            }
                        }
                    }
                }
            }
            else
            {
                /* tag processing */
                auto tag = parse_tag(&str, TRUE);
                if (!tag)
                    continue;

                state.Process(tag, buf, out.len(), str, seq);
            }
        }

        if (state.EndLineAddBuffer())
        {
            buf->AddNewLine(out, nlines);
        }

        if (str != endp)
        {
            // advance line
            line = line->Substr(str - line->ptr, endp - str);
        }
        else
        {
            // clear for next line
            line = nullptr;
        }
    }

    buf->formlist = seq->FormEnd();

    addMultirowsForm(buf, buf->formitem);
    addMultirowsImg(buf, buf->img);
}

static TextLineListItem *_tl_lp2;
static Str
textlist_feed()
{
    TextLine *p;
    if (_tl_lp2 != nullptr)
    {
        p = _tl_lp2->ptr;
        _tl_lp2 = _tl_lp2->next;
        return p->line;
    }
    return nullptr;
}

static InputStream *_file_lp2;
static Str
file_feed()
{
    Str s;
    s = StrISgets(_file_lp2);
    if (s->Size() == 0)
    {
        ISclose(_file_lp2);
        return nullptr;
    }
    return s;
}

///
/// entry
///
void loadHTMLstream(URLFile *f, BufferPtr newBuf, FILE *src, int internal)
{
    struct environment envs[MAX_ENV_LEVEL];
    clen_t linelen = 0;
    clen_t trbyte = 0;
    Str lineBuf2 = Strnew();

    CharacterEncodingScheme charset = WC_CES_US_ASCII;
    CharacterEncodingScheme doc_charset = w3mApp::Instance().DocumentCharset;

    struct html_feed_environ htmlenv1;
    struct readbuffer obuf;

    MySignalHandler prevtrap = nullptr;

    HtmlContext context;

    int image_flag;
    if (newBuf->image_flag)
        image_flag = newBuf->image_flag;
    else if (w3mApp::Instance().activeImage && w3mApp::Instance().displayImage && autoImage)
        image_flag = IMG_FLAG_AUTO;
    else
        image_flag = IMG_FLAG_SKIP;
    if (newBuf->currentURL.file.size())
    {
        *GetCurBaseUrl() = *newBuf->BaseURL();
    }

    if (w3mApp::Instance().w3m_halfload)
    {
        newBuf->buffername = "---";
        newBuf->document_charset = w3mApp::Instance().InnerCharset;

        _file_lp2 = f->stream;
        HTMLlineproc2body(newBuf, file_feed, -1, &context);
        w3mApp::Instance().w3m_halfload = FALSE;
        return;
    }

    init_henv(&htmlenv1, &obuf, envs, MAX_ENV_LEVEL, nullptr, newBuf->width, 0);

    if (w3mApp::Instance().w3m_dump & DUMP_HALFDUMP)
        htmlenv1.f = stdout;
    else
        htmlenv1.buf = newTextLineList();

    auto success = TrapJmp([&]() {

#ifdef USE_M17N
        if (newBuf != nullptr)
        {
            if (newBuf->bufferprop & BP_FRAME)
                charset = w3mApp::Instance().InnerCharset;
            else if (newBuf->document_charset)
                charset = doc_charset = newBuf->document_charset;
        }
        if (content_charset && w3mApp::Instance().UseContentCharset)
            doc_charset = content_charset;
        else if (f->guess_type && !strcasecmp(f->guess_type, "application/xhtml+xml"))
            doc_charset = WC_CES_UTF_8;
        meta_charset = WC_CES_NONE;
#endif
#if 0
    do_blankline(&htmlenv1, &obuf, 0, 0, htmlenv1.limit);
    obuf.flag = RB_IGNORE_P;
#endif
        if (IStype(f->stream) != IST_ENCODED)
            f->stream = newEncodedStream(f->stream, f->encoding);
        while ((lineBuf2 = f->StrmyISgets())->Size())
        {
#ifdef USE_NNTP
            if (f->scheme == SCM_NEWS && lineBuf2->ptr[0] == '.')
            {
                lineBuf2->Delete(0, 1);
                if (lineBuf2->ptr[0] == '\n' || lineBuf2->ptr[0] == '\r' ||
                    lineBuf2->ptr[0] == '\0')
                {
                    /*
                 * iseos(f->stream) = TRUE;
                 */
                    break;
                }
            }
#endif /* USE_NNTP */
            if (src)
                lineBuf2->Puts(src);
            linelen += lineBuf2->Size();
            if (w3mApp::Instance().w3m_dump & DUMP_EXTRA)
                printf("W3m-in-progress: %s\n", convert_size2(linelen, GetCurrentContentLength(), TRUE));
            if (w3mApp::Instance().w3m_dump & DUMP_SOURCE)
                continue;
            showProgress(&linelen, &trbyte);
            /*
         * if (frame_source)
         * continue;
         */
            if (meta_charset)
            { /* <META> */
                if (content_charset == 0 && w3mApp::Instance().UseContentCharset)
                {
                    doc_charset = meta_charset;
                    charset = WC_CES_US_ASCII;
                }
                meta_charset = WC_CES_NONE;
            }

            lineBuf2 = convertLine(f, lineBuf2, HTML_MODE, &charset, doc_charset);

            context.SetCES(charset);

            HTMLlineproc0(lineBuf2->ptr, &htmlenv1, internal, &context);
        }
        if (obuf.status != R_ST_NORMAL)
        {
            obuf.status = R_ST_EOL;
            HTMLlineproc0("\n", &htmlenv1, internal, &context);
        }
        obuf.status = R_ST_NORMAL;
        completeHTMLstream(&htmlenv1, &obuf, &context);
        flushline(&htmlenv1, &obuf, 0, 2, htmlenv1.limit);
        if (htmlenv1.title)
            newBuf->buffername = htmlenv1.title;

        return true;
    });

    if (!success)
    {
        HTMLlineproc1("<br>Transfer Interrupted!<br>", &htmlenv1, &context);
    }
    else
    {

        if (w3mApp::Instance().w3m_dump & DUMP_HALFDUMP)
        {
            print_internal_information(&htmlenv1);
            return;
        }
        if (w3mApp::Instance().w3m_backend)
        {
            print_internal_information(&htmlenv1);
            backend_halfdump_buf = htmlenv1.buf;
            return;
        }
    }

    newBuf->trbyte = trbyte + linelen;

    if (!(newBuf->bufferprop & BP_FRAME))
        newBuf->document_charset = charset;

    newBuf->image_flag = image_flag;

    _tl_lp2 = htmlenv1.buf->first;
    HTMLlineproc2body(newBuf, textlist_feed, -1, &context);
}

/* 
 * loadHTMLBuffer: read file and make new buffer
 */
BufferPtr
loadHTMLBuffer(URLFile *f, BufferPtr newBuf)
{
    FILE *src = nullptr;
    Str tmp;

    if (newBuf == nullptr)
        newBuf = newBuffer(INIT_BUFFER_WIDTH());
    if (newBuf->sourcefile.empty() &&
        (f->scheme != SCM_LOCAL || newBuf->mailcap))
    {
        tmp = tmpfname(TMPF_SRC, ".html");
        src = fopen(tmp->ptr, "w");
        if (src)
            newBuf->sourcefile = tmp->ptr;
    }

    loadHTMLstream(f, newBuf, src, newBuf->bufferprop & BP_FRAME);

    newBuf->CurrentAsLast();

    formResetBuffer(newBuf, newBuf->formitem);
    if (src)
        fclose(src);

    return newBuf;
}

/* 
 * loadHTMLString: read string and make new buffer
 */
BufferPtr loadHTMLString(Str page)
{
    auto newBuf = newBuffer(INIT_BUFFER_WIDTH());

    auto success = TrapJmp([&]() {
        URLFile f(SCM_LOCAL, newStrStream(page));

        newBuf->document_charset = w3mApp::Instance().InnerCharset;
        loadHTMLstream(&f, newBuf, nullptr, TRUE);
        newBuf->document_charset = WC_CES_US_ASCII;

        return true;
    });

    if (!success)
    {
        return nullptr;
    }

    newBuf->CurrentAsLast();
    newBuf->type = "text/html";
    newBuf->real_type = newBuf->type;

    formResetBuffer(newBuf, newBuf->formitem);
    return newBuf;
}
