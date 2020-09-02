#include "html_to_buffer.h"
#include "formdata.h"
#include "textlist.h"
#include "indep.h"
#include "frontend/terminal.h"
#include "frontend/event.h"
#include "stream/network.h"
#include "gc_helper.h"
#include "symbol.h"
#include "w3m.h"
#include "file.h"
#include "commands.h"

#define FORM_I_TEXT_DEFAULT_SIZE 40

static int currentLn(const BufferPtr &buf)
{
    if (buf->CurrentLine())
        /*     return buf->currentLine->real_linenumber + 1;      */
        return buf->CurrentLine()->linenumber + 1;
    else
        return 1;
}

BufferPtr HtmlToBuffer::CreateBuffer(const URL &url, std::string_view title, CharacterEncodingScheme charset, TextLineList *list)
{
    auto newBuf = Buffer::Create(url);
    newBuf->type = "text/html";
    if (title.size())
        newBuf->buffername = title;

    newBuf->m_document->document_charset = charset;
    // ImageFlags image_flag;
    // if (newBuf->image_flag)
    //     image_flag = newBuf->image_flag;
    // else if (ImageManager::Instance().activeImage && ImageManager::Instance().displayImage && ImageManager::Instance().autoImage)
    //     image_flag = IMG_FLAG_AUTO;
    // else
    //     image_flag = IMG_FLAG_SKIP;
    // newBuf->image_flag = image_flag;

    BufferFromLines(newBuf, list);

    return newBuf;
}

void HtmlToBuffer::BufferFromLines(const BufferPtr &buf, TextLineList *list)
{
    auto feed = [feeder = TextFeeder{list->first}]() -> Str {
        return feeder();
    };

    int nlines = 1;
    while (true)
    {
        auto line = feed();
        if (!line)
        {
            break;
        }

        auto [n, t] = m_form->TextareaCurrent();
        if (n >= 0 && *(line->ptr) != '<')
        { /* halfload */
            t->Push(line);
            continue;
        }

        StripRight(line);
        ProcessLine(buf, line, nlines++);
    }

    buf->m_document->formlist = m_form->FormEnd();
    addMultirowsForm(buf, buf->m_document->formitem);
    addMultirowsImg(buf, buf->m_document->img);
}

void HtmlToBuffer::ProcessLine(const BufferPtr &buf, Str line, int nlines)
{
    //
    // each char
    //
    std::string_view str = line->ptr;
    // auto endp = str + line->Size();
    PropertiedString out;
    while (str.size())
    {
        auto mode = get_mctype(str[0]);
        if ((effect | ex_efct(ex_effect)) & PC_SYMBOL && str[0] != '<')
        {
            // symbol
            auto p = get_width_symbol(Terminal::SymbolWidth0(), symbol);
            // assert(p.size() > 0);
            int len = get_mclen(p.data());
            mode = get_mctype(p[0]);

            out.push(mode | effect | ex_efct(ex_effect), p[0]);
            if (--len)
            {
                mode = (mode & ~PC_WCHAR1) | PC_WCHAR2;
                for (int i = 1; len--; ++i)
                {
                    out.push(mode | effect | ex_efct(ex_effect), p[i]);
                }
            }
            str.remove_prefix(Terminal::SymbolWidth());
        }
        else if (mode == PC_CTRL || mode == PC_UNDEF)
        {
            // control
            out.push(PC_ASCII | effect | ex_efct(ex_effect), ' ');
            str.remove_prefix(1);
        }
        else if (mode & PC_UNKNOWN)
        {
            // unknown
            out.push(PC_ASCII | effect | ex_efct(ex_effect), ' ');
            str.remove_prefix(get_mclen(str));
        }
        else if (str[0] != '<' && str[0] != '&')
        {
            // multibyte char ?
            int len = get_mclen(str);
            out.push(mode | effect | ex_efct(ex_effect), str[0]);
            str.remove_prefix(1);
            if (--len)
            {
                mode = (mode & ~PC_WCHAR1) | PC_WCHAR2;
                while (len--)
                {
                    out.push(mode | effect | ex_efct(ex_effect), str[0]);
                    str.remove_prefix(1);
                }
            }
        }
        else if (str[0] == '&')
        {
            /* 
                 * & escape processing
                 */
            char *p;
            {
                auto [pos, view] = getescapecmd(str.data(), w3mApp::Instance().InnerCharset);
                str = const_cast<char *>(pos);
                p = const_cast<char *>(view.data());
            }

            while (*p)
            {
                mode = get_mctype(*p);
                if (mode == PC_CTRL || mode == PC_UNDEF)
                {
                    out.push(PC_ASCII | effect | ex_efct(ex_effect), ' ');
                    p++;
                }
                else if (mode & PC_UNKNOWN)
                {
                    out.push(PC_ASCII | effect | ex_efct(ex_effect), ' ');
                    p += get_mclen(p);
                }
                else
                {
                    int len = get_mclen(p);
                    out.push(mode | effect | ex_efct(ex_effect), *(p++));
                    if (--len)
                    {
                        mode = (mode & ~PC_WCHAR1) | PC_WCHAR2;
                        while (len--)
                        {
                            out.push(mode | effect | ex_efct(ex_effect), *(p++));
                        }
                    }
                }
            }
        }
        else
        {
            /* tag processing */
            HtmlTagPtr tag;
            std::tie(str, tag) = HtmlTag::parse(str, true);
            if (tag)
            {
                Process(buf, tag, out.ByteLength(), str.data());
            }
        }
    }

    auto l = buf->m_document->AddLine();
    l->buffer = out;

    assert(str.empty());
}

void HtmlToBuffer::Process(const BufferPtr &buf, HtmlTagPtr tag, int pos, const char *str)
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
        // <a>
        char *p = nullptr;
        auto q = Strnew(buf->baseTarget)->ptr;
        char *id = nullptr;
        if (tag->TryGetAttributeValue(ATTR_NAME, &id))
        {
            id = wc_conv_strict(id, w3mApp::Instance().InnerCharset, buf->m_document->document_charset)->ptr;
            buf->m_document->name.Put(Anchor::CreateName(id, currentLn(buf), pos));
        }
        if (tag->TryGetAttributeValue(ATTR_HREF, &p))
            p = wc_conv_strict(remove_space(p), w3mApp::Instance().InnerCharset,
                               buf->m_document->document_charset)
                    ->ptr;
        if (tag->TryGetAttributeValue(ATTR_TARGET, &q))
            q = wc_conv_strict(q, w3mApp::Instance().InnerCharset, buf->m_document->document_charset)->ptr;
        char *r = nullptr;
        if (tag->TryGetAttributeValue(ATTR_REFERER, &r))
            r = wc_conv_strict(r, w3mApp::Instance().InnerCharset, buf->m_document->document_charset)->ptr;
        char *s = nullptr;
        tag->TryGetAttributeValue(ATTR_TITLE, &s);
        auto t = "";
        tag->TryGetAttributeValue(ATTR_ACCESSKEY, &t);
        auto hseq = 0;
        tag->TryGetAttributeValue(ATTR_HSEQ, &hseq);
        if (hseq > 0)
            buf->m_document->putHmarker(currentLn(buf), pos, hseq - 1);
        else if (hseq < 0)
        {
            int h = -hseq - 1;
            if (buf->m_document->hmarklist.size() &&
                h < buf->m_document->hmarklist.size() &&
                buf->m_document->hmarklist[h].invalid)
            {
                buf->m_document->hmarklist[h].pos = pos;
                buf->m_document->hmarklist[h].line = currentLn(buf);
                buf->m_document->hmarklist[h].invalid = 0;
                hseq = -hseq;
            }
        }
        if (p)
        {
            // TODO:
            // r ? r : ""
            HttpReferrerPolicy referer = HttpReferrerPolicy::StrictOriginWhenCrossOrigin;

            this->effect |= PE_ANCHOR;
            this->a_href = Anchor::CreateHref(p,
                                              q ? q : "",
                                              referer,
                                              s ? s : "",
                                              *t, currentLn(buf), pos);
            buf->m_document->href.Put(this->a_href);
            this->a_href->hseq = ((hseq > 0) ? hseq : -hseq) - 1;
            this->a_href->slave = (hseq > 0) ? false : true;
        }
        break;
    }

    case HTML_N_A:
        // </a>
        this->effect &= ~PE_ANCHOR;
        if (this->a_href)
        {
            this->a_href->end.line = currentLn(buf);
            this->a_href->end.pos = pos;
            if (this->a_href->start == this->a_href->end)
            {
                if (buf->m_document->hmarklist.size() &&
                    this->a_href->hseq < buf->m_document->hmarklist.size())
                    buf->m_document->hmarklist[this->a_href->hseq].invalid = 1;
                this->a_href->hseq = -1;
            }
            this->a_href = nullptr;
        }
        break;

    case HTML_LINK:
        buf->m_document->linklist.push_back(Link::create(tag, buf->m_document->document_charset));
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
                buf->m_document->putHmarker(currentLn(buf), pos, iseq - 1);
            }

            char *s = nullptr;
            tag->TryGetAttributeValue(ATTR_TITLE, &s);
            p = wc_conv_strict(remove_space(p), w3mApp::Instance().InnerCharset,
                               buf->m_document->document_charset)
                    ->ptr;
            this->a_img = Anchor::CreateImage(
                p,
                s ? s : "",
                currentLn(buf), pos);
            buf->m_document->img.Put(this->a_img);

            this->a_img->hseq = iseq;
            this->a_img->image = nullptr;
            if (iseq > 0)
            {
                auto u = URL::Parse(this->a_img->url, nullptr);
                Image *image;
                this->a_img->image = image = New(Image);
                image->url = u.ToStr()->ptr;
                auto [t, ext] = uncompressed_file_type(u.path);
                if (t.size())
                {
                    image->ext = ext.data();
                }
                else
                {
                    image->ext = filename_extension(u.path.c_str(), true);
                }
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
                image->cache = getImage(image, nullptr, IMG_FLAG_SKIP);
            }
            else if (iseq < 0)
            {
                auto a = buf->m_document->img.RetrieveAnchor(buf->m_document->imarklist[-iseq - 1]);
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

        auto form = m_form->FormCurrent(form_id);
        if (!form)
        {
            break;
        }

        if (hseq > 0)
        {
            int hpos = pos;
            if (*str == '[')
                hpos++;
            buf->m_document->putHmarker(currentLn(buf), hpos, hseq - 1);
        }
        if (form->target.empty())
            form->target = buf->baseTarget;

        int textareanumber = -1;
        if (tag->TryGetAttributeValue(ATTR_TEXTAREANUMBER, &textareanumber))
        {
            // TextareaGrow(textareanumber);
        }

        int selectnumber = -1;
        if (tag->TryGetAttributeValue(ATTR_SELECTNUMBER, &selectnumber))
        {
            // FormSelectGrow(selectnumber);
        }

        auto fi = this->formList_addInput(form, tag);
        if (fi)
        {
            auto a = std::make_shared<Anchor>();
            a->target = form->target;
            a->item = fi;
            BufferPoint bp = {
                line : currentLn(buf),
                pos : pos
            };
            a->start = bp;
            a->end = bp;
            this->a_form = a;
            buf->m_document->formitem.Put(a);
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
            auto m = std::make_shared<MapList>();
            buf->m_document->maplist.push_back(m);
            m->name = p;
        }
        break;
    }
    case HTML_N_MAP:
        /* nothing to do */
        break;
    case HTML_AREA:
    {
        if (buf->m_document->maplist.empty()) /* outside of <map>..</map> */
            break;

        char *p = nullptr;
        if (tag->TryGetAttributeValue(ATTR_HREF, &p))
        {
            p = wc_conv_strict(remove_space(p), w3mApp::Instance().InnerCharset,
                               buf->m_document->document_charset)
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
            buf->m_document->maplist.back()->area.push_back(a);
        }
        break;
    }

        //
        // frame
        //
        // case HTML_FRAMESET:
        // {
        //     auto frame = newFrameSet(tag);
        //     if (frame)
        //     {
        //         auto sp = this->frameset_s.size();
        //         this->frameset_s.push_back(frame);
        //         if (sp == 0)
        //         {
        //             if (buf->frameset == nullptr)
        //             {
        //                 buf->frameset = frame;
        //             }
        //             else
        //             {
        //                 pushFrameTree(&(buf->frameQ), frame, nullptr);
        //             }
        //         }
        //         else
        //         {
        //             addFrameSetElement(this->frameset_s[sp - 1], *(union frameset_element *)frame);
        //         }
        //     }
        //     break;
        // }
        // case HTML_N_FRAMESET:
        //     if (!this->frameset_s.empty())
        //     {
        //         this->frameset_s.pop_back();
        //     }
        //     break;
        // case HTML_FRAME:
        //     if (!this->frameset_s.empty())
        //     {
        //         union frameset_element element;
        //         element.body = newFrame(tag, buf);
        //         addFrameSetElement(this->frameset_s.back(), element);
        //     }
        //     break;

    case HTML_BASE:
    {
        char *p = nullptr;
        if (tag->TryGetAttributeValue(ATTR_HREF, &p))
        {
            p = wc_conv_strict(remove_space(p), w3mApp::Instance().InnerCharset, buf->m_document->document_charset)->ptr;
        }
        if (tag->TryGetAttributeValue(ATTR_TARGET, &p))
            buf->baseTarget =
                wc_conv_strict(p, w3mApp::Instance().InnerCharset, buf->m_document->document_charset)->ptr;
        break;
    }
    case HTML_META:
    {
        char *p = nullptr;
        tag->TryGetAttributeValue(ATTR_HTTP_EQUIV, &p);
        char *q = nullptr;
        tag->TryGetAttributeValue(ATTR_CONTENT, &q);
        if (p && q && !strcasecmp(p, "refresh") && Network::Instance().MetaRefresh)
        {
            Str tmp = nullptr;
            int refresh_interval = getMetaRefreshParam(q, &tmp);

            if (tmp)
            {
                p = wc_conv_strict(remove_space(tmp->ptr), w3mApp::Instance().InnerCharset,
                                   buf->m_document->document_charset)
                        ->ptr;
                buf->m_document->event = setAlarmEvent(buf->m_document->event,
                                           refresh_interval,
                                           AL_IMPLICIT_ONCE,
                                           &gorURL, p);
            }
            else if (refresh_interval > 0)
                buf->m_document->event = setAlarmEvent(buf->m_document->event,
                                           refresh_interval,
                                           AL_IMPLICIT,
                                           &reload, nullptr);
        }
        break;
    }

    case HTML_INTERNAL:
        assert(false);
        // this->m_internal = HTML_INTERNAL;
        break;
    case HTML_N_INTERNAL:
        assert(false);
        // this->m_internal = HTML_N_INTERNAL;
        break;

    case HTML_FORM_INT:
    {
        int form_id;
        if (tag->TryGetAttributeValue(ATTR_FID, &form_id))
            m_form->FormOpen(tag, form_id);
        break;
    }
    case HTML_TEXTAREA_INT:
    {
        int n_textarea = -1;
        if (tag->TryGetAttributeValue(ATTR_TEXTAREANUMBER, &n_textarea))
        {
            m_form->Textarea(n_textarea, Strnew());
        }
        break;
    }
    case HTML_N_TEXTAREA_INT:
    {
        auto [n, t] = m_form->TextareaCurrent();
        auto anchor = a_textarea[n];
        if (anchor)
        {
            auto item = anchor->item;
            item->init_value = item->value = t->ptr;
        }
        break;
    }
    case HTML_SELECT_INT:
    {
        int n_select = -1;
        if (tag->TryGetAttributeValue(ATTR_SELECTNUMBER, &n_select))
        {
            m_form->FormSetSelect(n_select);
        }
        break;
    }
    case HTML_N_SELECT_INT:
    {
        auto [n_select, select] = m_form->FormSelectCurrent();
        if (select)
        {
            auto item = a_select[n_select]->item;
            item->select_option = *select;
            item->chooseSelectOption();
            item->init_selected = item->selected;
            item->init_value = item->value;
            item->init_label = item->label;
        }
        break;
    }
    case HTML_OPTION_INT:
    {
        auto [n_select, select] = m_form->FormSelectCurrent();
        if (select)
        {
            auto q = "";
            tag->TryGetAttributeValue(ATTR_LABEL, &q);
            auto p = q;
            tag->TryGetAttributeValue(ATTR_VALUE, &p);
            auto selected = tag->HasAttribute(ATTR_SELECTED);
            select->push_back({
                p,
                q,
                selected,
            });
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
            id = wc_conv_strict(id, w3mApp::Instance().InnerCharset, buf->m_document->document_charset)->ptr;
            buf->m_document->name.Put(Anchor::CreateName(id, currentLn(buf), pos));
        }
    }
}

// /* end of processing for one line */
// bool HtmlToBuffer::EndLineAddBuffer()
// {
//     if (m_internal == HTML_UNKNOWN)
//     {
//         // add non html line
//         return true;
//     }

//     // not add html line
//     if (m_internal == HTML_N_INTERNAL)
//     {
//         // exit internal
//         m_internal = HTML_UNKNOWN;
//     }
//     return false;
// }

/* 
 * add <input> element to form_list
 */
FormItemPtr HtmlToBuffer::formList_addInput(FormPtr fl, HtmlTagPtr tag)
{
    /* if not in <form>..</form> environment, just ignore <input> tag */
    if (fl == NULL)
        return NULL;

    auto item = std::make_shared<FormItem>();
    fl->items.push_back(item);
    item->parent = fl;

    item->type = FORM_UNKNOWN;
    item->size = -1;
    item->rows = 0;
    item->checked = item->init_checked = 0;
    item->accept = 0;
    item->name;
    item->value = item->init_value;
    item->readonly = 0;
    const char *p;
    if (tag->TryGetAttributeValue(ATTR_TYPE, &p))
    {
        item->type = formtype(p);
        if (item->size < 0 &&
            (item->type == FORM_INPUT_TEXT ||
             item->type == FORM_INPUT_FILE ||
             item->type == FORM_INPUT_PASSWORD))
            item->size = FORM_I_TEXT_DEFAULT_SIZE;
    }
    if (tag->TryGetAttributeValue(ATTR_NAME, &p))
        item->name = p;
    if (tag->TryGetAttributeValue(ATTR_VALUE, &p))
        item->value = item->init_value = p;
    item->checked = item->init_checked = tag->HasAttribute(ATTR_CHECKED);
    item->accept = tag->HasAttribute(ATTR_ACCEPT);
    tag->TryGetAttributeValue(ATTR_SIZE, &item->size);
    tag->TryGetAttributeValue(ATTR_MAXLENGTH, &item->maxlength);
    item->readonly = tag->HasAttribute(ATTR_READONLY);
    int i;
    if (tag->TryGetAttributeValue(ATTR_TEXTAREANUMBER, &i))
        item->value = item->init_value = m_form->Textarea(i)->ptr;

    if (tag->TryGetAttributeValue(ATTR_SELECTNUMBER, &i))
        item->select_option = *m_form->FormSelect(i);

    if (tag->TryGetAttributeValue(ATTR_ROWS, &p))
        item->rows = atoi(p);
    if (item->type == FORM_UNKNOWN)
    {
        /* type attribute is missing. Ignore the tag. */
        return NULL;
    }

    if (item->type == FORM_SELECT)
    {
        item->chooseSelectOption();
        item->init_selected = item->selected;
        item->init_value = item->value;
        item->init_label = item->label;
    }

    if (item->type == FORM_INPUT_FILE && item->value.size())
    {
        /* security hole ! */
        return NULL;
    }
    if (item->type == FORM_INPUT_HIDDEN)
        return NULL;
    return item;
}
