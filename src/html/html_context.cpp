#include <string_view_util.h>
#include "config.h"
#include "html/html_context.h"
#include "html/html_tag.h"
#include "ctrlcode.h"
#include "html/tagstack.h"
#include "html/image.h"
#include "html/table.h"
#include "html/form.h"
#include "html/maparea.h"
#include "html/html_to_buffer.h"
#include "html/formdata.h"
#include "stream/compression.h"
#include "frontend/terminal.h"
#include "indep.h"
#include "gc_helper.h"
#include "w3m.h"
#include "myctype.h"
#include "file.h"
#include "commands.h"
#include "textlist.h"
#include "stream/network.h"
#include "entity.h"


#define MAX_TABLE 20 /* maximum nest level of table */

#define MAX_SELECT 10 /* max number of <select>..</select> \
                       * within one document */

#define MAX_TEXTAREA 10 /* max number of <textarea>..</textarea> \
                         * within one document */

class HtmlContext
{
    table *m_tables[MAX_TABLE];
    table_mode m_table_modes[MAX_TABLE];
    int table_width(int table_level)
    {
        if (table_level < 0)
            return 0;
        auto width = m_tables[table_level]->total_width;
        if (table_level > 0 || width > 0)
            return width;
        return m_henv.limit - m_henv.envs.back().indent;
    }

    // // HTML <meta>
    // CharacterEncodingScheme meta_charset = WC_CES_NONE;
    // src charset
    CharacterEncodingScheme doc_charset = WC_CES_NONE;
    // detected
    CharacterEncodingScheme cur_document_charset = WC_CES_NONE;
    // inner charset
    // CharacterEncodingScheme charset = WC_CES_US_ASCII;

    // html seq ?
    int cur_hseq = 1;

    // image seq ?
    int cur_iseq = 1;

    Str cur_title = nullptr;

    FormDataPtr m_form;

    int cur_option_maxwidth = 0;
    Str cur_select = nullptr;
    Str select_str = nullptr;
    int select_is_multiple = 0;
    int n_selectitem = 0;
    Str cur_option = nullptr;
    Str cur_option_value = nullptr;
    Str cur_option_label = nullptr;
    bool cur_option_selected = false;
    TokenStatusTypes cur_status = R_ST_NORMAL;

    struct readbuffer m_obuf;
    html_feed_environ m_henv;

public:
    HtmlContext(CharacterEncodingScheme content_charset)
        : doc_charset(content_charset),
          m_henv(&m_obuf, newTextLineList(), Terminal::columns()),
          m_form(new FormData)
    {
    }

    ~HtmlContext()
    {
    }

    std::string_view Title() const
    {
        if (!m_henv.title)
        {
            return "";
        }
        return m_henv.title;
    }

    TextLineList *List() const
    {
        return m_henv.buf;
    }

    FormDataPtr Form() const
    {
        return m_form;
    }

    const CharacterEncodingScheme &DocCharset() const { return doc_charset; }
    void SetCES(CharacterEncodingScheme ces) { cur_document_charset = ces; }
    Str process_n_select();

    Str process_n_textarea();
    using FeedFunc = std::function<Str()>;

    void completeHTMLstream()
    {
        this->close_anchor();
        if (m_obuf.img_alt)
        {
            m_obuf.push_tag("</img_alt>", HTML_N_IMG_ALT);
            m_obuf.img_alt = NULL;
        }
        if (m_obuf.fontstat.in_bold)
        {
            m_obuf.push_tag("</b>", HTML_N_B);
            m_obuf.fontstat.in_bold = 0;
        }
        if (m_obuf.fontstat.in_italic)
        {
            m_obuf.push_tag("</i>", HTML_N_I);
            m_obuf.fontstat.in_italic = 0;
        }
        if (m_obuf.fontstat.in_under)
        {
            m_obuf.push_tag("</u>", HTML_N_U);
            m_obuf.fontstat.in_under = 0;
        }
        if (m_obuf.fontstat.in_strike)
        {
            m_obuf.push_tag("</s>", HTML_N_S);
            m_obuf.fontstat.in_strike = 0;
        }
        if (m_obuf.fontstat.in_ins)
        {
            m_obuf.push_tag("</ins>", HTML_N_INS);
            m_obuf.fontstat.in_ins = 0;
        }
        if (m_obuf.flag & RB_INTXTA)
            this->ProcessLine("</textarea>", true);
        /* for unbalanced select tag */
        if (m_obuf.flag & RB_INSELECT)
            this->ProcessLine("</select>", true);
        if (m_obuf.flag & RB_TITLE)
            this->ProcessLine("</title>", true);

        /* for unbalanced table tag */
        if (m_obuf.table_level >= MAX_TABLE)
            m_obuf.table_level = MAX_TABLE - 1;

        while (m_obuf.table_level >= 0)
        {
            m_table_modes[m_obuf.table_level].pre_mode &= ~(TBLM_SCRIPT | TBLM_STYLE | TBLM_PLAIN);
            this->ProcessLine("</table>", true);
        }
    }

    std::string_view _ProcessLine(std::string_view _line, TableState *state, bool internal)
    {
        auto line = _line.data();
        while (*line != '\0')
        {
            //
            // get token
            //
            auto is_tag = false;
            const char *str = nullptr;
            std::string_view pos;
            std::string token;
            TokenStatusTypes new_status;
            auto front = line[0];
            std::tie(pos, new_status, token) = read_token(line, m_obuf.status, state->pre_mode(m_obuf) & RB_PREMODE);
            line = pos.data();
            if (token.empty())
            {
                continue;
            }
            str = token.data();
            if (front == '<' /*|| m_obuf.status != R_ST_NORMAL*/)
            {
                // Tag processing
                if (new_status != R_ST_NORMAL)
                {
                    // invalid
                    assert(false);
                    return {};
                }
                m_henv.tagbuf->Clear();
                m_henv.tagbuf->Push(token);
                if (*str == '<')
                {
                    if (str[1] && REALLY_THE_BEGINNING_OF_A_TAG(str))
                        is_tag = true;
                    else if (!(state->pre_mode(m_obuf) & (RB_PLAIN | RB_INTXTA | RB_INSELECT |
                                                          RB_SCRIPT | RB_STYLE | RB_TITLE)))
                    {
                        line = Strnew_m_charp(str + 1, line, NULL)->ptr;
                        str = "&lt;";
                    }
                }
            }

            if (state->pre_mode(m_obuf) & (RB_PLAIN | RB_INTXTA | RB_INSELECT | RB_SCRIPT |
                                           RB_STYLE | RB_TITLE))
            {
                bool processed = false;
                if (is_tag)
                {
                    std::string_view p = str;
                    HtmlTagPtr tag;
                    std::tie(p, tag) = HtmlTag::parse(p, internal);
                    if (tag)
                    {
                        if (tag->tagid == state->end_tag(m_obuf) ||
                            (state->pre_mode(m_obuf) & RB_INSELECT && tag->tagid == HTML_N_FORM) || (state->pre_mode(m_obuf) & RB_TITLE && (tag->tagid == HTML_N_HEAD || tag->tagid == HTML_BODY)))
                            processed = true;
                    }
                }

                /* title */
                if (!processed && state->pre_mode(m_obuf) & RB_TITLE)
                {
                    this->TitleContent(str);
                    continue;
                }

                /* select */
                if (!processed && state->pre_mode(m_obuf) & RB_INSELECT)
                {
                    if (m_obuf.table_level >= 0)
                    {
                        processed = true;
                    }
                    else
                    {
                        this->feed_select(str);
                        continue;
                    }
                }

                if (!processed && is_tag)
                {
                    assert(false);
                    // char *p;
                    // if (strncmp(str, "<!--", 4) && (p = strchr(const_cast<char *>(str) + 1, '<')))
                    // {
                    //     str = Strnew_charp_n(str, p - str)->ptr;
                    //     line = Strnew_m_charp(p, line, NULL)->ptr;
                    // }
                    // is_tag = false;
                }

                if (!processed && m_obuf.table_level >= 0)
                {
                    processed = true;
                }

                /* textarea */
                if (!processed && state->pre_mode(m_obuf) & RB_INTXTA)
                {
                    m_form->feed_textarea(str);
                    continue;
                }

                /* script */
                if (!processed && state->pre_mode(m_obuf) & RB_SCRIPT)
                    continue;

                /* style */
                if (!processed && state->pre_mode(m_obuf) & RB_STYLE)
                    continue;
            }

            if (m_obuf.table_level >= 0)
            {
                /* 
                * within table: in <table>..</table>, all input tokens
                * are fed to the table renderer, and then the renderer
                * makes HTML output.
                */
                switch (this->feed_table(state->tbl, str, state->tbl_mode, state->tbl_width, internal))
                {
                case 0:
                    /* </table> tag */
                    m_obuf.table_level--;
                    if (m_obuf.table_level >= MAX_TABLE - 1)
                        continue;
                    if (state->close_table(m_obuf, m_tables, m_table_modes))
                    {
                        state->tbl_width = table_width(m_obuf.table_level);
                        this->feed_table(state->tbl, str, state->tbl_mode, state->tbl_width, true);
                        continue;
                        /* continue to the next */
                    }

                    if (m_obuf.flag & RB_DEL)
                        continue;
                    /* all tables have been read */
                    if (state->tbl->vspace > 0 && !(m_obuf.flag & RB_IGNORE_P))
                    {
                        int indent = m_henv.envs.back().indent;
                        m_henv.flushline(indent, 0, m_henv.limit);
                        m_henv.do_blankline(&m_obuf, indent, 0, m_henv.limit);
                    }
                    m_obuf.save_fonteffect();
                    this->renderTable(state->tbl, state->tbl_width);
                    m_obuf.restore_fonteffect();
                    m_obuf.flag &= ~RB_IGNORE_P;
                    if (state->tbl->vspace > 0)
                    {
                        int indent = m_henv.envs.back().indent;
                        m_henv.do_blankline(&m_obuf, indent, 0, m_henv.limit);
                        m_obuf.flag |= RB_IGNORE_P;
                    }
                    m_obuf.set_space_to_prevchar();
                    continue;
                case 1:
                    /* <table> tag */
                    break;
                default:
                    continue;
                }
            }

            if (is_tag)
            {
                /*** Beginning of a new tag ***/
                auto [pos, tag] = HtmlTag::parse(str, internal);
                str = pos.data();
                if (!tag)
                {
                    continue;
                }
                auto cmd = tag->tagid;

                /* process tags */
                if (this->HTMLtagproc1(tag) == 0)
                {
                    /* preserve the tag for second-stage processing */
                    if (tag->need_reconstruct)
                        m_henv.tagbuf = Strnew(tag->ToStr());
                    m_obuf.push_tag(m_henv.tagbuf->ptr, cmd);
                }
                else
                {
                    m_obuf.process_idattr(cmd, tag);
                }

                m_obuf.bp = {};
                m_obuf.clear_ignore_p_flag(cmd);
                if (cmd == HTML_TABLE)
                    return line;
                else
                    continue;
            }

            if (m_obuf.flag & (RB_DEL | RB_S))
                continue;
            while (*str)
            {
                auto mode = get_mctype(*str);
                auto delta = get_mcwidth(str);
                if (m_obuf.flag & (RB_SPECIAL & ~RB_NOBR))
                {
                    char ch = *str;
                    if (!(m_obuf.flag & RB_PLAIN) && (*str == '&'))
                    {
                        const char *p = str;
                        auto [pos, ech] = ucs4_from_entity(p);
                        p = pos.data();
                        if (ech == '\n' || ech == '\r')
                        {
                            ch = '\n';
                            str = p - 1;
                        }
                        else if (ech == '\t')
                        {
                            ch = '\t';
                            str = p - 1;
                        }
                    }
                    if (ch != '\n')
                        m_obuf.flag &= ~RB_IGNORE_P;
                    if (ch == '\n')
                    {
                        str++;
                        if (m_obuf.flag & RB_IGNORE_P)
                        {
                            m_obuf.flag &= ~RB_IGNORE_P;
                            continue;
                        }
                        if (m_obuf.flag & RB_PRE_INT)
                            m_obuf.PUSH(' ');
                        else
                            m_henv.flushline(m_henv.envs.back().indent, 1, m_henv.limit);
                    }
                    else if (ch == '\t')
                    {
                        do
                        {
                            m_obuf.PUSH(' ');
                        } while ((m_henv.envs.back().indent + m_obuf.pos) % w3mApp::Instance().Tabstop != 0);
                        str++;
                    }
                    else if (m_obuf.flag & RB_PLAIN)
                    {
                        const char *p = html_quote_char(*str);
                        if (p)
                        {
                            m_obuf.push_charp(1, p, PC_ASCII);
                            str++;
                        }
                        else
                        {
                            m_obuf.proc_mchar(1, delta, &str, mode);
                        }
                    }
                    else
                    {
                        if (*str == '&')
                            m_obuf.proc_escape(&str);
                        else
                            m_obuf.proc_mchar(1, delta, &str, mode);
                    }
                    if (m_obuf.flag & (RB_SPECIAL & ~RB_PRE_INT))
                        continue;
                }
                else
                {
                    if (!IS_SPACE(*str))
                        m_obuf.flag &= ~RB_IGNORE_P;
                    if ((mode == PC_ASCII || mode == PC_CTRL) && IS_SPACE(*str))
                    {
                        if (*m_obuf.prevchar->ptr != ' ')
                        {
                            m_obuf.PUSH(' ');
                        }
                        str++;
                    }
                    else
                    {
                        bool is_hangul = false;
                        if (mode == PC_KANJI1)
                            is_hangul = wtf_is_hangul((uint8_t *)str);

                        if (!w3mApp::Instance().SimplePreserveSpace && mode == PC_KANJI1 &&
                            !is_hangul && !state->prev_is_hangul &&
                            m_obuf.pos > m_henv.envs.back().indent &&
                            m_obuf.line->Back() == ' ')
                        {
                            while (m_obuf.line->Size() >= 2 &&
                                   !strncmp(m_obuf.line->ptr + m_obuf.line->Size() -
                                                2,
                                            "  ", 2) &&
                                   m_obuf.pos >= m_henv.envs.back().indent)
                            {
                                m_obuf.line->Pop(1);
                                m_obuf.pos--;
                            }
                            if (m_obuf.line->Size() >= 3 &&
                                m_obuf.prev_ctype == PC_KANJI1 &&
                                m_obuf.line->Back() == ' ' &&
                                m_obuf.pos >= m_henv.envs.back().indent)
                            {
                                m_obuf.line->Pop(1);
                                m_obuf.pos--;
                            }
                        }
                        state->prev_is_hangul = is_hangul;

                        if (*str == '&')
                            m_obuf.proc_escape(&str);
                        else
                            m_obuf.proc_mchar(m_obuf.flag & RB_SPECIAL, delta, &str, mode);
                    }
                }
                if (m_henv.need_flushline(mode))
                {
                    char *bp = m_obuf.line->ptr + m_obuf.bp.len();
                    char *tp = bp - m_obuf.bp.tlen();
                    int i = 0;

                    if (tp > m_obuf.line->ptr && tp[-1] == ' ')
                        i = 1;

                    auto indent = m_henv.envs.back().indent;
                    if (m_obuf.bp.pos() - i > indent)
                    {
                        m_obuf.append_tags();
                        auto line = Strnew(bp);
                        m_obuf.line->Pop(m_obuf.line->Size() - m_obuf.bp.len());
                        m_obuf.bp.back_to(&m_obuf);
                        m_henv.flushline(indent, 0, m_henv.limit);
                        this->ProcessLine(line->ptr, true);
                    }
                }
            }
        }

        return line;
    }

    //
    // HTML processing first pass
    //
    void ProcessLine(std::string_view line, bool internal)
    {
        int i = 0;
        TableState state = {};
        while (line.size())
        {
            if (m_obuf.table_level >= 0)
            {
                int level = std::min((int)m_obuf.table_level, (int)(MAX_TABLE - 1));
                state.tbl = m_tables[level];
                state.tbl_mode = &m_table_modes[level];
                state.tbl_width = table_width(level);
            }

            line = _ProcessLine(line, &state, internal);
        }

        if (!(m_obuf.flag & (RB_SPECIAL | RB_INTXTA | RB_INSELECT)))
        {
            char *tp;
            if (m_obuf.bp.pos() == m_obuf.pos)
            {
                tp = &m_obuf.line->ptr[m_obuf.bp.len() - m_obuf.bp.tlen()];
            }
            else
            {
                tp = &m_obuf.line->ptr[m_obuf.line->Size()];
            }

            int i = 0;
            if (tp > m_obuf.line->ptr && tp[-1] == ' ')
                i = 1;

            if (m_obuf.pos - i > m_henv.limit)
            {
                int indent = m_henv.envs.back().indent;
                m_henv.flushline(indent, 0, m_henv.limit);
            }
        }
    }

    void Finalize(bool internal)
    {
        if (m_obuf.status != R_ST_NORMAL)
        {
            m_obuf.status = R_ST_EOL;
            ProcessLine("\n", internal);
        }
        m_obuf.status = R_ST_NORMAL;
        completeHTMLstream();
        m_henv.flushline(0, 2, m_henv.limit);
    }

    void feed_table1(struct table *tbl, Str tok, struct table_mode *mode, int width);

    BufferPtr CreateBuffer(const URL &url);

private:
    void print_internal_information();

    CharacterEncodingScheme CES() const { return cur_document_charset; }
    void SetMetaCharset(CharacterEncodingScheme ces);

    void SetCurTitle(Str title)
    {
        cur_title = title;
    }

    int Increment()
    {
        return cur_hseq++;
    }
    int Get() const { return cur_hseq; }

    Str GetLinkNumberStr(int correction);
    // process <title>{content}</title> tag
    Str TitleOpen(HtmlTagPtr tag);
    void TitleContent(const char *str);
    Str TitleClose(HtmlTagPtr tag);

    // process <form></form>
    void process_option();
    Str process_select(HtmlTagPtr tag);
    void feed_select(const char *str);
    Str process_input(HtmlTagPtr tag);
    Str process_img(HtmlTagPtr tag, int width);
    Str process_anchor(HtmlTagPtr tag, const char *tagbuf);
    // void clear(int n);

    void close_anchor()
    {
        if (m_obuf.anchor.url.size())
        {
            int i;
            char *p = NULL;
            int is_erased = 0;

            for (i = m_obuf.tag_sp - 1; i >= 0; i--)
            {
                if (m_obuf.tag_stack[i]->cmd == HTML_A)
                    break;
            }
            if (i < 0 && m_obuf.anchor.hseq > 0 && m_obuf.line->Back() == ' ')
            {
                m_obuf.line->Pop(1);
                m_obuf.pos--;
                is_erased = 1;
            }

            if (i >= 0 || (p = m_obuf.has_hidden_link(HTML_A)))
            {
                if (m_obuf.anchor.hseq > 0)
                {
                    this->ProcessLine(ANSP, true);
                    m_obuf.set_space_to_prevchar();
                }
                else
                {
                    if (i >= 0)
                    {
                        m_obuf.tag_sp--;
                        bcopy(&m_obuf.tag_stack[i + 1], &m_obuf.tag_stack[i],
                              (m_obuf.tag_sp - i) * sizeof(struct cmdtable *));
                    }
                    else
                    {
                        m_obuf.passthrough(p, 1);
                    }
                    m_obuf.anchor = {};
                    return;
                }
                is_erased = 0;
            }
            if (is_erased)
            {
                m_obuf.line->Push(' ');
                m_obuf.pos++;
            }

            m_obuf.push_tag("</a>", HTML_N_A);
        }
        m_obuf.anchor = {};
    }

    void CLOSE_A()
    {
        CLOSE_P();
        close_anchor();
    }

    void CLOSE_P()
    {
        if (m_obuf.flag & RB_P)
        {
            m_henv.flushline(m_henv.envs.back().indent, 0, m_henv.limit);
            m_obuf.RB_RESTORE_FLAG();
            m_obuf.flag &= ~RB_P;
        }
    }

    void CLOSE_DT()
    {
        if (m_obuf.flag & RB_IN_DT)
        {
            m_obuf.flag &= ~RB_IN_DT;
            this->ProcessLine("</b>", true);
        }
    }
    Str process_hr(HtmlTagPtr tag, int width, int indent_width);
    int HTMLtagproc1(HtmlTagPtr tag);

    void make_caption(struct table *t);
    void do_refill(struct table *tbl, int row, int col, int maxlimit);
    int feed_table(struct table *tbl, const char *line, struct table_mode *mode, int width, int internal);
    TagActions feed_table_tag(struct table *tbl, const char *line, struct table_mode *mode, int width, HtmlTagPtr tag);
    void renderTable(struct table *t, int max_width);
    void renderCoTable(struct table *tbl, int maxlimit);
};

void HtmlContext::SetMetaCharset(CharacterEncodingScheme ces)
{
    if (cur_document_charset == 0 && w3mApp::Instance().UseContentCharset)
    {
        cur_document_charset = ces;
    }
}

void HtmlContext::print_internal_information()
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

Str HtmlContext::GetLinkNumberStr(int correction)
{
    return Sprintf("[%d]", cur_hseq + correction);
}

Str HtmlContext::TitleOpen(HtmlTagPtr tag)
{
    cur_title = Strnew();
    return NULL;
}

void HtmlContext::TitleContent(const char *str)
{
    if (!cur_title)
    {
        // not open
        assert(cur_title);
        return;
    }

    while (*str)
    {
        if (*str == '&')
        {
            auto [pos, cmd] = getescapecmd(str, w3mApp::Instance().InnerCharset);
            str = pos;
            cur_title->Push(cmd);
        }
        else if (*str == '\n' || *str == '\r')
        {
            cur_title->Push(' ');
            str++;
        }
        else
        {
            cur_title->Push(*(str++));
        }
    }
}

Str HtmlContext::TitleClose(HtmlTagPtr tag)
{
    if (!cur_title)
        return NULL;
    Strip(cur_title);
    auto tmp = Strnew_m_charp("<title_alt title=\"",
                              html_quote(cur_title),
                              "\">");
    cur_title = NULL;
    return tmp;
}

Str HtmlContext::process_select(HtmlTagPtr tag)
{
    Str tmp = nullptr;
    if (m_form->cur_form_id() < 0)
    {
        auto [pos, tag] = HtmlTag::parse("<form_int method=internal action=none>", true);
        tmp = m_form->FormOpen(tag);
    }

    auto p = tag->GetAttributeValue(ATTR_NAME);
    cur_select = Strnew(p);
    select_is_multiple = tag->HasAttribute(ATTR_MULTIPLE);

    if (!select_is_multiple)
    {
        select_str = Strnew("<pre_int>");
        if (w3mApp::Instance().displayLinkNumber)
            select_str->Push(GetLinkNumberStr(0));
        select_str->Push(Sprintf("[<input_alt hseq=\"%d\" "
                                 "fid=\"%d\" type=select name=\"%s\" selectnumber=%d",
                                 Increment(), m_form->cur_form_id(), html_quote(p), m_form->n_select));
        select_str->Push(">");
        m_form->select_option[m_form->n_select] = {};
        cur_option_maxwidth = 0;
    }
    else
    {
        select_str = Strnew();
    }

    cur_option = nullptr;
    cur_status = R_ST_NORMAL;
    n_selectitem = 0;
    return tmp;
}

void HtmlContext::feed_select(const char *str)
{
    Str tmp = Strnew();
    int prev_status = cur_status;
    static int prev_spaces = -1;

    if (cur_select == nullptr)
        return;
    while (*str)
    {
        auto pos = read_token(str, tmp, &cur_status, 0, 0);
        str = pos.data();
        if (cur_status != R_ST_NORMAL || prev_status != R_ST_NORMAL)
            continue;
        const char *p = tmp->ptr;
        if (tmp->ptr[0] == '<' && tmp->Back() == '>')
        {
            auto [pos, tag] = HtmlTag::parse(p, false);
            p = pos.data();
            if (!tag)
                continue;
            switch (tag->tagid)
            {
            case HTML_OPTION:
            {
                process_option();
                cur_option = Strnew();
                {
                    auto q = tag->GetAttributeValue(ATTR_VALUE);
                    if (q.size())
                        cur_option_value = Strnew(q.data());
                    else
                        cur_option_value = nullptr;
                }
                {
                    auto q = tag->GetAttributeValue(ATTR_LABEL);
                    if (q.size())
                        cur_option_label = Strnew(q.data());
                    else
                        cur_option_label = nullptr;
                }
                cur_option_selected = tag->HasAttribute(ATTR_SELECTED);
                prev_spaces = -1;
                break;
            }
            case HTML_N_OPTION:
                /* do nothing */
                break;
            default:
                /* never happen */
                break;
            }
        }
        else if (cur_option)
        {
            while (*p)
            {
                if (IS_SPACE(*p) && prev_spaces != 0)
                {
                    p++;
                    if (prev_spaces > 0)
                        prev_spaces++;
                }
                else
                {
                    if (IS_SPACE(*p))
                        prev_spaces = 1;
                    else
                        prev_spaces = 0;
                    if (*p == '&')
                    {
                        auto [pos, cmd] = getescapecmd(p, w3mApp::Instance().InnerCharset);
                        p = const_cast<char *>(pos);
                        cur_option->Push(cmd);
                    }
                    else
                        cur_option->Push(*(p++));
                }
            }
        }
    }
}

Str HtmlContext::process_n_select()
{
    if (cur_select == nullptr)
        return nullptr;
    process_option();

    if (!select_is_multiple)
    {
        if (m_form->select_option[m_form->n_select].size())
        {
            auto sitem = std::make_shared<FormItem>();
            sitem->select_option = m_form->select_option[m_form->n_select];
            sitem->chooseSelectOption();
            select_str->Push(textfieldrep(Strnew(sitem->label), cur_option_maxwidth));
        }
        select_str->Push("</input_alt>]</pre_int>");
        m_form->n_select++;
    }
    else
    {
        select_str->Push("<br>");
    }

    cur_select = nullptr;
    n_selectitem = 0;
    return select_str;
}

void HtmlContext::process_option()
{
    if (cur_select == nullptr || cur_option == nullptr)
        return;

    char begin_char = '[', end_char = ']';
    while (cur_option->Size() > 0 && IS_SPACE(cur_option->Back()))
        cur_option->Pop(1);
    if (cur_option_value == nullptr)
        cur_option_value = cur_option;
    if (cur_option_label == nullptr)
        cur_option_label = cur_option;

    if (!select_is_multiple)
    {
        int len = get_Str_strwidth(cur_option_label);
        if (len > cur_option_maxwidth)
            cur_option_maxwidth = len;
        m_form->select_option[m_form->n_select].push_back({cur_option_value->ptr,
                                                           cur_option_label->ptr,
                                                           cur_option_selected});
        return;
    }

    if (!select_is_multiple)
    {
        begin_char = '(';
        end_char = ')';
    }
    select_str->Push(Sprintf("<br><pre_int>%c<input_alt hseq=\"%d\" "
                             "fid=\"%d\" type=%s name=\"%s\" value=\"%s\"",
                             begin_char, Increment(), m_form->cur_form_id(),
                             select_is_multiple ? "checkbox" : "radio",
                             html_quote(cur_select->ptr),
                             html_quote(cur_option_value->ptr)));
    if (cur_option_selected)
        select_str->Push(" checked>*</input_alt>");
    else
        select_str->Push("> </input_alt>");
    select_str->Push(end_char);
    select_str->Push(html_quote(cur_option_label->ptr));
    select_str->Push("</pre_int>");
    n_selectitem++;
}

Str HtmlContext::process_input(HtmlTagPtr tag)
{
    Str tmp = nullptr;
    if (m_form->cur_form_id() < 0)
    {
        const char *s = "<form_int method=internal action=none>";
        auto [pos, tag] = HtmlTag::parse(s, true);
        tmp = m_form->FormOpen(tag);
    }
    if (tmp == nullptr)
    {
        tmp = Strnew();
    }

    auto p = tag->GetAttributeValue(ATTR_TYPE, "text");
    auto v = formtype(p);
    if (v == FORM_UNKNOWN)
        return nullptr;

    auto q = tag->GetAttributeValue(ATTR_VALUE);
    if (q.empty())
    {
        switch (v)
        {
        case FORM_INPUT_IMAGE:
        case FORM_INPUT_SUBMIT:
        case FORM_INPUT_BUTTON:
            q = "SUBMIT";
            break;
        case FORM_INPUT_RESET:
            q = "RESET";
            break;
            /* if no VALUE attribute is specified in 
             * <INPUT TYPE=CHECKBOX> tag, then the value "on" is used 
             * as a default value. It is not a part of HTML4.0 
             * specification, but an imitation of Netscape behaviour. 
             */
        case FORM_INPUT_CHECKBOX:
            q = "on";
        }
    }
    /* VALUE attribute is not allowed in <INPUT TYPE=FILE> tag. */
    if (v == FORM_INPUT_FILE)
        q = "";

    const char *qq = "";
    int qlen = 0;
    if (q.size())
    {
        qq = html_quote(q.data());
        qlen = get_strwidth(q.data());
    }

    tmp->Push("<pre_int>");
    switch (v)
    {
    case FORM_INPUT_PASSWORD:
    case FORM_INPUT_TEXT:
    case FORM_INPUT_FILE:
    case FORM_INPUT_CHECKBOX:
        if (w3mApp::Instance().displayLinkNumber)
            tmp->Push(GetLinkNumberStr(0));
        tmp->Push('[');
        break;
    case FORM_INPUT_RADIO:
        if (w3mApp::Instance().displayLinkNumber)
            tmp->Push(GetLinkNumberStr(0));
        tmp->Push('(');
    }

    auto r = tag->GetAttributeValue(ATTR_NAME);
    auto w = tag->GetAttributeValue(ATTR_SIZE, 20);
    auto i = tag->GetAttributeValue(ATTR_MAXLENGTH, 20);
    auto p2 = tag->GetAttributeValue(ATTR_ALT);
    auto x = tag->HasAttribute(ATTR_CHECKED);
    auto y = tag->HasAttribute(ATTR_ACCEPT);
    auto z = tag->HasAttribute(ATTR_READONLY);
    tmp->Push(Sprintf("<input_alt hseq=\"%d\" fid=\"%d\" type=%s "
                      "name=\"%s\" width=%d maxlength=%d value=\"%s\"",
                      Increment(), m_form->cur_form_id(), p.data(), html_quote(r).c_str(), w, i, qq));

    if (x)
        tmp->Push(" checked");
    if (y)
        tmp->Push(" accept");
    if (z)
        tmp->Push(" readonly");
    tmp->Push('>');

    if (v == FORM_INPUT_HIDDEN)
        tmp->Push("</input_alt></pre_int>");
    else
    {
        switch (v)
        {
        case FORM_INPUT_PASSWORD:
        case FORM_INPUT_TEXT:
        case FORM_INPUT_FILE:
            tmp->Push("<u>");
            break;
        case FORM_INPUT_IMAGE:
        {
            auto s = tag->GetAttributeValue(ATTR_SRC);
            if (s.size())
            {
                tmp->Push(Sprintf("<img src=\"%s\"", html_quote(s)));
                if (p2.size())
                    tmp->Push(Sprintf(" alt=\"%s\"", html_quote(p2)));

                auto iw = tag->GetAttributeValue(ATTR_WIDTH, 0);
                if (iw)
                {
                    tmp->Push(Sprintf(" width=\"%d\"", iw));
                }
                auto ih = tag->GetAttributeValue(ATTR_HEIGHT, 0);
                if (ih)
                {
                    tmp->Push(Sprintf(" height=\"%d\"", ih));
                }

                tmp->Push(" pre_int>");
                tmp->Push("</input_alt></pre_int>");
                return tmp;
            }
        }
        case FORM_INPUT_SUBMIT:
        case FORM_INPUT_BUTTON:
        case FORM_INPUT_RESET:
            if (w3mApp::Instance().displayLinkNumber)
                tmp->Push(GetLinkNumberStr(-1));
            tmp->Push("[");
            break;
        }
        switch (v)
        {
        case FORM_INPUT_PASSWORD:
            i = 0;
            if (q.size())
            {
                for (; i < qlen && i < w; i++)
                    tmp->Push('*');
            }
            for (; i < w; i++)
                tmp->Push(' ');
            break;
        case FORM_INPUT_TEXT:
        case FORM_INPUT_FILE:
            if (q.size())
                tmp->Push(textfieldrep(Strnew(q.data()), w));
            else
            {
                for (i = 0; i < w; i++)
                    tmp->Push(' ');
            }
            break;
        case FORM_INPUT_SUBMIT:
        case FORM_INPUT_BUTTON:
            if (p2.size())
                tmp->Push(html_quote(p2));
            else
                tmp->Push(qq);
            break;
        case FORM_INPUT_RESET:
            tmp->Push(qq);
            break;
        case FORM_INPUT_RADIO:
        case FORM_INPUT_CHECKBOX:
            if (x)
                tmp->Push('*');
            else
                tmp->Push(' ');
            break;
        }
        switch (v)
        {
        case FORM_INPUT_PASSWORD:
        case FORM_INPUT_TEXT:
        case FORM_INPUT_FILE:
            tmp->Push("</u>");
            break;
        case FORM_INPUT_IMAGE:
        case FORM_INPUT_SUBMIT:
        case FORM_INPUT_BUTTON:
        case FORM_INPUT_RESET:
            tmp->Push("]");
        }
        tmp->Push("</input_alt>");
        switch (v)
        {
        case FORM_INPUT_PASSWORD:
        case FORM_INPUT_TEXT:
        case FORM_INPUT_FILE:
        case FORM_INPUT_CHECKBOX:
            tmp->Push(']');
            break;
        case FORM_INPUT_RADIO:
            tmp->Push(')');
        }
        tmp->Push("</pre_int>");
    }
    return tmp;
}

#define IMG_SYMBOL UL_SYMBOL(12)
Str HtmlContext::process_img(HtmlTagPtr tag, int width)
{
    char *p, *q, *r, *r2 = nullptr, *t;
    int w, i, nw, ni = 1, n, w0 = -1, i0 = -1;
    int align, xoffset, yoffset, top, bottom, ismap = 0;
    int use_image = ImageManager::Instance().activeImage && ImageManager::Instance().displayImage;
    int pre_int = false, ext_pre_int = false;
    Str tmp = Strnew();

    if (!tag->TryGetAttributeValue(ATTR_SRC, &p))
        return tmp;
    p = remove_space(p);

    q = nullptr;
    tag->TryGetAttributeValue(ATTR_ALT, &q);
    if (!w3mApp::Instance().pseudoInlines && (q == nullptr || (*q == '\0' && w3mApp::Instance().ignore_null_img_alt)))
        return tmp;
    t = q;
    tag->TryGetAttributeValue(ATTR_TITLE, &t);
    w = -1;
    if (tag->TryGetAttributeValue(ATTR_WIDTH, &w))
    {
        if (w < 0)
        {
            if (width > 0)
                w = (int)(-width * ImageManager::Instance().pixel_per_char * w / 100 + 0.5);
            else
                w = -1;
        }
#ifdef USE_IMAGE
        if (use_image)
        {
            if (w > 0)
            {
                w = (int)(w * ImageManager::Instance().image_scale / 100 + 0.5);
                if (w == 0)
                    w = 1;
                else if (w > MAX_IMAGE_SIZE)
                    w = MAX_IMAGE_SIZE;
            }
        }
#endif
    }
#ifdef USE_IMAGE
    if (use_image)
    {
        i = -1;
        if (tag->TryGetAttributeValue(ATTR_HEIGHT, &i))
        {
            if (i > 0)
            {
                i = (int)(i * ImageManager::Instance().image_scale / 100 + 0.5);
                if (i == 0)
                    i = 1;
                else if (i > MAX_IMAGE_SIZE)
                    i = MAX_IMAGE_SIZE;
            }
            else
            {
                i = -1;
            }
        }
        align = -1;
        tag->TryGetAttributeValue(ATTR_ALIGN, &align);
        ismap = 0;
        if (tag->HasAttribute(ATTR_ISMAP))
            ismap = 1;
    }
    else
#endif
        tag->TryGetAttributeValue(ATTR_HEIGHT, &i);
    r = nullptr;
    tag->TryGetAttributeValue(ATTR_USEMAP, &r);
    if (tag->HasAttribute(ATTR_PRE_INT))
        ext_pre_int = true;

    tmp = Strnew_size(128);
#ifdef USE_IMAGE
    if (use_image)
    {
        switch (align)
        {
        case ALIGN_LEFT:
            tmp->Push("<div_int align=left>");
            break;
        case ALIGN_CENTER:
            tmp->Push("<div_int align=center>");
            break;
        case ALIGN_RIGHT:
            tmp->Push("<div_int align=right>");
            break;
        }
    }
#endif
    if (r)
    {
        r2 = strchr(r, '#');
        auto s = "<form_int method=internal action=map>";
        auto [pos, tag] = HtmlTag::parse(s, true);
        auto tmp2 = m_form->FormOpen(tag);
        if (tmp2)
            tmp->Push(tmp2);
        tmp->Push(Sprintf("<input_alt fid=\"%d\" "
                          "type=hidden name=link value=\"",
                          m_form->cur_form_id()));
        tmp->Push(html_quote((r2) ? r2 + 1 : r));
        tmp->Push(Sprintf("\"><input_alt hseq=\"%d\" fid=\"%d\" "
                          "type=submit no_effect=true>",
                          Increment(), m_form->cur_form_id()));
    }

    if (use_image)
    {
        w0 = w;
        i0 = i;
        if (w < 0 || i < 0)
        {
            auto u = URL::Parse(wc_conv(p, w3mApp::Instance().InnerCharset, CES())->ptr, nullptr);
            Image image;
            image.url = u.ToStr()->ptr;
            auto [t, ext] = uncompressed_file_type(u.path);
            if (t.size())
            {
                image.ext = ext.data();
            }
            else
            {
                image.ext = filename_extension(u.path.c_str(), true);
            }
            image.cache = nullptr;
            image.width = w;
            image.height = i;

            image.cache = getImage(&image, nullptr, IMG_FLAG_SKIP);
            if (image.cache && image.cache->width > 0 &&
                image.cache->height > 0)
            {
                w = w0 = image.cache->width;
                i = i0 = image.cache->height;
            }
            if (w < 0)
                w = 8 * ImageManager::Instance().pixel_per_char;
            if (i < 0)
                i = ImageManager::Instance().pixel_per_line;
        }
        nw = (w > 3) ? (int)((w - 3) / ImageManager::Instance().pixel_per_char + 1) : 1;
        ni = (i > 3) ? (int)((i - 3) / ImageManager::Instance().pixel_per_line + 1) : 1;
        tmp->Push(
            Sprintf("<pre_int><img_alt hseq=\"%d\" src=\"", cur_iseq++));
        pre_int = true;
    }
    else
    {
        if (w < 0)
            w = 12 * ImageManager::Instance().pixel_per_char;
        nw = w ? (int)((w - 1) / ImageManager::Instance().pixel_per_char + 1) : 1;
        if (r)
        {
            tmp->Push("<pre_int>");
            pre_int = true;
        }
        tmp->Push("<img_alt src=\"");
    }
    tmp->Push(html_quote(p));
    tmp->Push("\"");
    if (t)
    {
        tmp->Push(" title=\"");
        tmp->Push(html_quote(t));
        tmp->Push("\"");
    }
#ifdef USE_IMAGE
    if (use_image)
    {
        if (w0 >= 0)
            tmp->Push(Sprintf(" width=%d", w0));
        if (i0 >= 0)
            tmp->Push(Sprintf(" height=%d", i0));
        switch (align)
        {
        case ALIGN_TOP:
            top = 0;
            bottom = ni - 1;
            yoffset = 0;
            break;
        case ALIGN_MIDDLE:
            top = ni / 2;
            bottom = top;
            if (top * 2 == ni)
                yoffset = (int)(((ni + 1) * ImageManager::Instance().pixel_per_line - i) / 2);
            else
                yoffset = (int)((ni * ImageManager::Instance().pixel_per_line - i) / 2);
            break;
        case ALIGN_BOTTOM:
            top = ni - 1;
            bottom = 0;
            yoffset = (int)(ni * ImageManager::Instance().pixel_per_line - i);
            break;
        default:
            top = ni - 1;
            bottom = 0;
            if (ni == 1 && ni * ImageManager::Instance().pixel_per_line > i)
                yoffset = 0;
            else
            {
                yoffset = (int)(ni * ImageManager::Instance().pixel_per_line - i);
                if (yoffset <= -2)
                    yoffset++;
            }
            break;
        }
        xoffset = (int)((nw * ImageManager::Instance().pixel_per_char - w) / 2);
        if (xoffset)
            tmp->Push(Sprintf(" xoffset=%d", xoffset));
        if (yoffset)
            tmp->Push(Sprintf(" yoffset=%d", yoffset));
        if (top)
            tmp->Push(Sprintf(" top_margin=%d", top));
        if (bottom)
            tmp->Push(Sprintf(" bottom_margin=%d", bottom));
        if (r)
        {
            tmp->Push(" usemap=\"");
            tmp->Push(html_quote((r2) ? r2 + 1 : r));
            tmp->Push("\"");
        }
        if (ismap)
            tmp->Push(" ismap");
    }
#endif
    tmp->Push(">");
    if (q != nullptr && *q == '\0' && w3mApp::Instance().ignore_null_img_alt)
        q = nullptr;
    if (q != nullptr)
    {
        n = get_strwidth(q);
#ifdef USE_IMAGE
        if (use_image)
        {
            if (n > nw)
            {
                char *r;
                for (r = q, n = 0; r; r += get_mclen(r), n += get_mcwidth(r))
                {
                    if (n + get_mcwidth(r) > nw)
                        break;
                }
                tmp->Push(html_quote(Strnew_charp_n(q, r - q)->ptr));
            }
            else
                tmp->Push(html_quote(q));
        }
        else
#endif
            tmp->Push(html_quote(q));
        goto img_end;
    }
    if (w > 0 && i > 0)
    {
        /* guess what the image is! */
        if (w < 32 && i < 48)
        {
            /* must be an icon or space */
            n = 1;
            if (strcasestr(p, "space") || strcasestr(p, "blank"))
                tmp->Push("_");
            else
            {
                if (w * i < 8 * 16)
                    tmp->Push("*");
                else
                {
                    if (!pre_int)
                    {
                        tmp->Push("<pre_int>");
                        pre_int = true;
                    }
                    push_symbol(tmp, IMG_SYMBOL, Terminal::SymbolWidth(), 1);
                    n = Terminal::SymbolWidth();
                }
            }
            goto img_end;
        }
        if (w > 200 && i < 13)
        {
            /* must be a horizontal line */
            if (!pre_int)
            {
                tmp->Push("<pre_int>");
                pre_int = true;
            }
            w = w / ImageManager::Instance().pixel_per_char / Terminal::SymbolWidth();
            if (w <= 0)
                w = 1;
            push_symbol(tmp, HR_SYMBOL, Terminal::SymbolWidth(), w);
            n = w * Terminal::SymbolWidth();
            goto img_end;
        }
    }
    for (q = p; *q; q++)
        ;
    while (q > p && *q != '/')
        q--;
    if (*q == '/')
        q++;
    tmp->Push('[');
    n = 1;
    p = q;
    for (; *q; q++)
    {
        if (!IS_ALNUM(*q) && *q != '_' && *q != '-')
        {
            break;
        }
        tmp->Push(*q);
        n++;
        if (n + 1 >= nw)
            break;
    }
    tmp->Push(']');
    n++;
img_end:
    if (use_image)
    {
        for (; n < nw; n++)
            tmp->Push(' ');
    }

    tmp->Push("</img_alt>");
    if (pre_int && !ext_pre_int)
        tmp->Push("</pre_int>");
    if (r)
    {
        tmp->Push("</input_alt>");
        m_form->FormClose();
    }

    if (use_image)
    {
        switch (align)
        {
        case ALIGN_RIGHT:
        case ALIGN_CENTER:
        case ALIGN_LEFT:
            tmp->Push("</div_int>");
            break;
        }
    }

    return tmp;
}

Str HtmlContext::process_anchor(HtmlTagPtr tag, const char *tagbuf)
{
    if (tag->need_reconstruct)
    {
        tag->SetAttributeValue(ATTR_HSEQ, Sprintf("%d", Increment())->ptr);
        return Strnew(tag->ToStr());
    }
    else
    {
        Str tmp = Sprintf("<a hseq=\"%d\"", Increment());
        tmp->Push(tagbuf + 2);
        return tmp;
    }
}

Str HtmlContext::process_n_textarea()
{
    if (m_form->cur_textarea == nullptr)
        return nullptr;

    auto tmp = Strnew();
    tmp->Push(Sprintf("<pre_int>[<input_alt hseq=\"%d\" fid=\"%d\" "
                      "type=textarea name=\"%s\" size=%d rows=%d "
                      "top_margin=%d textareanumber=%d",
                      Get(), m_form->cur_form_id(),
                      html_quote(m_form->cur_textarea->ptr),
                      m_form->cur_textarea_size, m_form->cur_textarea_rows,
                      m_form->cur_textarea_rows - 1, m_form->n_textarea));
    if (m_form->cur_textarea_readonly)
        tmp->Push(" readonly");
    tmp->Push("><u>");
    for (int i = 0; i < m_form->cur_textarea_size; i++)
        tmp->Push(' ');
    tmp->Push("</u></input_alt>]</pre_int>\n");
    Increment();
    m_form->n_textarea++;
    m_form->cur_textarea = nullptr;

    return tmp;
}

static int REAL_WIDTH(int w, int limit)
{
    return (((w) >= 0) ? (int)((w) / ImageManager::Instance().pixel_per_char) : -(w) * (limit) / 100);
}

Str HtmlContext::process_hr(HtmlTagPtr tag, int width, int indent_width)
{
    Str tmp = Strnew("<nobr>");
    int w = 0;
    int x = ALIGN_CENTER;
#define HR_ATTR_WIDTH_MAX 65535

    if (width > indent_width)
        width -= indent_width;
    if (tag->TryGetAttributeValue(ATTR_WIDTH, &w))
    {
        if (w > HR_ATTR_WIDTH_MAX)
        {
            w = HR_ATTR_WIDTH_MAX;
        }
        w = REAL_WIDTH(w, width);
    }
    else
    {
        w = width;
    }

    tag->TryGetAttributeValue(ATTR_ALIGN, &x);
    switch (x)
    {
    case ALIGN_CENTER:
        tmp->Push("<div_int align=center>");
        break;
    case ALIGN_RIGHT:
        tmp->Push("<div_int align=right>");
        break;
    case ALIGN_LEFT:
        tmp->Push("<div_int align=left>");
        break;
    }
    w /= Terminal::SymbolWidth();
    if (w <= 0)
        w = 1;
    push_symbol(tmp, HR_SYMBOL, Terminal::SymbolWidth(), w);
    tmp->Push("</div_int></nobr>");
    return tmp;
}

static char roman_num1[] = {
    'i',
    'x',
    'c',
    'm',
    '*',
};

static char roman_num5[] = {
    'v',
    'l',
    'd',
    '*',
};

static Str romanNum2(int l, int n)
{
    Str s = Strnew();

    switch (n)
    {
    case 1:
    case 2:
    case 3:
        for (; n > 0; n--)
            s->Push(roman_num1[l]);
        break;
    case 4:
        s->Push(roman_num1[l]);
        s->Push(roman_num5[l]);
        break;
    case 5:
    case 6:
    case 7:
    case 8:
        s->Push(roman_num5[l]);
        for (n -= 5; n > 0; n--)
            s->Push(roman_num1[l]);
        break;
    case 9:
        s->Push(roman_num1[l]);
        s->Push(roman_num1[l + 1]);
        break;
    }
    return s;
}

static Str romanNumeral(int n)
{
    Str r = Strnew();

    if (n <= 0)
        return r;
    if (n >= 4000)
    {
        r->Push("**");
        return r;
    }
    r->Push(romanNum2(3, n / 1000));
    r->Push(romanNum2(2, (n % 1000) / 100));
    r->Push(romanNum2(1, (n % 100) / 10));
    r->Push(romanNum2(0, n % 10));

    return r;
}

static Str romanAlphabet(int n)
{
    Str r = Strnew();
    int l;
    char buf[14];

    if (n <= 0)
        return r;

    l = 0;
    while (n)
    {
        buf[l++] = 'a' + (n - 1) % 26;
        n = (n - 1) / 26;
    }
    l--;
    for (; l >= 0; l--)
        r->Push(buf[l]);

    return r;
}

int HtmlContext::HTMLtagproc1(HtmlTagPtr tag)
{
    if (m_obuf.flag & RB_PRE)
    {
        switch (tag->tagid)
        {
        case HTML_NOBR:
        case HTML_N_NOBR:
        case HTML_PRE_INT:
        case HTML_N_PRE_INT:
            return 1;
        }
    }

    switch (tag->tagid)
    {
    case HTML_B:
    {
        m_obuf.fontstat.in_bold++;
        if (m_obuf.fontstat.in_bold > 1)
            return 1;
        return 0;
    }
    case HTML_N_B:
    {
        if (m_obuf.fontstat.in_bold == 1 && m_obuf.close_effect0(HTML_B))
            m_obuf.fontstat.in_bold = 0;
        if (m_obuf.fontstat.in_bold > 0)
        {
            m_obuf.fontstat.in_bold--;
            if (m_obuf.fontstat.in_bold == 0)
                return 0;
        }
        return 1;
    }
    case HTML_I:
    {
        m_obuf.fontstat.in_italic++;
        if (m_obuf.fontstat.in_italic > 1)
            return 1;
        return 0;
    }
    case HTML_N_I:
    {
        if (m_obuf.fontstat.in_italic == 1 && m_obuf.close_effect0(HTML_I))
            m_obuf.fontstat.in_italic = 0;
        if (m_obuf.fontstat.in_italic > 0)
        {
            m_obuf.fontstat.in_italic--;
            if (m_obuf.fontstat.in_italic == 0)
                return 0;
        }
        return 1;
    }
    case HTML_U:
    {
        m_obuf.fontstat.in_under++;
        if (m_obuf.fontstat.in_under > 1)
            return 1;
        return 0;
    }
    case HTML_N_U:
    {
        if (m_obuf.fontstat.in_under == 1 && m_obuf.close_effect0(HTML_U))
            m_obuf.fontstat.in_under = 0;
        if (m_obuf.fontstat.in_under > 0)
        {
            m_obuf.fontstat.in_under--;
            if (m_obuf.fontstat.in_under == 0)
                return 0;
        }
        return 1;
    }
    case HTML_EM:
    {
        this->ProcessLine("<i>", true);
        return 1;
    }
    case HTML_N_EM:
    {
        this->ProcessLine("</i>", true);
        return 1;
    }
    case HTML_STRONG:
    {
        this->ProcessLine("<b>", true);
        return 1;
    }
    case HTML_N_STRONG:
    {
        this->ProcessLine("</b>", true);
        return 1;
    }
    case HTML_Q:
    {
        this->ProcessLine("`", true);
        return 1;
    }
    case HTML_N_Q:
    {
        this->ProcessLine("'", true);
        return 1;
    }
    case HTML_P:
    case HTML_N_P:
    {
        this->CLOSE_A();
        if (!(m_obuf.flag & RB_IGNORE_P))
        {
            m_henv.flushline(m_henv.envs.back().indent, 1, m_henv.limit);
            m_henv.do_blankline(m_henv.obuf, m_henv.envs.back().indent, 0,
                                m_henv.limit);
        }
        m_obuf.flag |= RB_IGNORE_P;
        if (tag->tagid == HTML_P)
        {
            m_obuf.set_alignment(tag);
            m_obuf.flag |= RB_P;
        }
        return 1;
    }
    case HTML_BR:
    {
        m_henv.flushline(m_henv.envs.back().indent, 1, m_henv.limit);
        m_henv.blank_lines = 0;
        return 1;
    }
    case HTML_H:
    {
        if (!(m_obuf.flag & (RB_PREMODE | RB_IGNORE_P)))
        {
            m_henv.flushline(m_henv.envs.back().indent, 0, m_henv.limit);
            m_henv.do_blankline(m_henv.obuf, m_henv.envs.back().indent, 0,
                                m_henv.limit);
        }
        this->ProcessLine("<b>", true);
        m_obuf.set_alignment(tag);
        return 1;
    }
    case HTML_N_H:
    {
        this->ProcessLine("</b>", true);
        if (!(m_obuf.flag & RB_PREMODE))
        {
            m_henv.flushline(m_henv.envs.back().indent, 0, m_henv.limit);
        }
        m_henv.do_blankline(m_henv.obuf, m_henv.envs.back().indent, 0, m_henv.limit);
        m_obuf.RB_RESTORE_FLAG();
        this->close_anchor();
        m_obuf.flag |= RB_IGNORE_P;
        return 1;
    }
    case HTML_UL:
    case HTML_OL:
    case HTML_BLQ:
    {
        this->CLOSE_A();
        if (!(m_obuf.flag & RB_IGNORE_P))
        {
            m_henv.flushline(m_henv.envs.back().indent, 0, m_henv.limit);
            if (!(m_obuf.flag & RB_PREMODE) &&
                (m_henv.envs.empty() || tag->tagid == HTML_BLQ))
                m_henv.do_blankline(m_henv.obuf, m_henv.envs.back().indent, 0,
                                    m_henv.limit);
        }
        m_henv.PUSH_ENV(tag->tagid);
        if (tag->tagid == HTML_UL || tag->tagid == HTML_OL)
        {
            int count;
            if (tag->TryGetAttributeValue(ATTR_START, &count))
            {
                m_henv.envs.back().count = count - 1;
            }
        }
        if (tag->tagid == HTML_OL)
        {
            m_henv.envs.back().type = '1';
            char *p;
            if (tag->TryGetAttributeValue(ATTR_TYPE, &p))
            {
                m_henv.envs.back().type = (int)*p;
            }
        }
        if (tag->tagid == HTML_UL)
            m_henv.envs.back().type = tag->ul_type();
        m_henv.flushline(m_henv.envs.back().indent, 0, m_henv.limit);
        return 1;
    }
    case HTML_N_UL:
    case HTML_N_OL:
    case HTML_N_DL:
    case HTML_N_BLQ:
    {
        this->CLOSE_DT();
        this->CLOSE_A();
        if (m_henv.envs.size())
        {
            m_henv.flushline(m_henv.envs.back().indent, 0,
                             m_henv.limit);
            m_henv.POP_ENV();
            if (!(m_obuf.flag & RB_PREMODE) &&
                (m_henv.envs.empty() || tag->tagid == HTML_N_DL || tag->tagid == HTML_N_BLQ))
            {
                m_henv.do_blankline(m_henv.obuf,
                                    m_henv.envs.back().indent,
                                    w3mApp::Instance().IndentIncr, m_henv.limit);
                m_obuf.flag |= RB_IGNORE_P;
            }
        }
        this->close_anchor();
        return 1;
    }
    case HTML_DL:
    {
        this->CLOSE_A();
        if (!(m_obuf.flag & RB_IGNORE_P))
        {
            m_henv.flushline(m_henv.envs.back().indent, 0, m_henv.limit);
            if (!(m_obuf.flag & RB_PREMODE))
                m_henv.do_blankline(m_henv.obuf, m_henv.envs.back().indent, 0,
                                    m_henv.limit);
        }
        m_henv.PUSH_ENV(tag->tagid);
        if (tag->HasAttribute(ATTR_COMPACT))
            m_henv.envs.back().env = HTML_DL_COMPACT;
        m_obuf.flag |= RB_IGNORE_P;
        return 1;
    }
    case HTML_LI:
    {
        this->CLOSE_A();
        this->CLOSE_DT();
        if (m_henv.envs.size())
        {
            Str num;
            m_henv.flushline(
                m_henv.envs.back().indent, 0, m_henv.limit);
            m_henv.envs.back().count++;
            char *p;
            if (tag->TryGetAttributeValue(ATTR_VALUE, &p))
            {
                int count = atoi(p);
                if (count > 0)
                    m_henv.envs.back().count = count;
                else
                    m_henv.envs.back().count = 0;
            }
            switch (m_henv.envs.back().env)
            {
            case HTML_UL:
            {
                m_henv.envs.back().type = tag->ul_type(m_henv.envs.back().type);
                for (int i = 0; i < w3mApp::Instance().IndentIncr - 3; i++)
                    m_obuf.push_charp(1, NBSP, PC_ASCII);
                auto tmp = Strnew();
                switch (m_henv.envs.back().type)
                {
                case 'd':
                    push_symbol(tmp, UL_SYMBOL_DISC, Terminal::SymbolWidth(), 1);
                    break;
                case 'c':
                    push_symbol(tmp, UL_SYMBOL_CIRCLE, Terminal::SymbolWidth(), 1);
                    break;
                case 's':
                    push_symbol(tmp, UL_SYMBOL_SQUARE, Terminal::SymbolWidth(), 1);
                    break;
                default:
                    push_symbol(tmp,
                                UL_SYMBOL((m_henv.envs.size() - 1) % MAX_UL_LEVEL),
                                Terminal::SymbolWidth(),
                                1);
                    break;
                }
                if (Terminal::SymbolWidth() == 1)
                    m_obuf.push_charp(1, NBSP, PC_ASCII);
                m_obuf.push_str(Terminal::SymbolWidth(), tmp, PC_ASCII);
                m_obuf.push_charp(1, NBSP, PC_ASCII);
                m_obuf.set_space_to_prevchar();
                break;
            }
            case HTML_OL:
            {
                if (tag->TryGetAttributeValue(ATTR_TYPE, &p))
                    m_henv.envs.back().type = (int)*p;
                switch ((m_henv.envs.back().count > 0) ? m_henv.envs.back().type : '1')
                {
                case 'i':
                    num = romanNumeral(m_henv.envs.back().count);
                    break;
                case 'I':
                    num = romanNumeral(m_henv.envs.back().count);
                    ToUpper(num);
                    break;
                case 'a':
                    num = romanAlphabet(m_henv.envs.back().count);
                    break;
                case 'A':
                    num = romanAlphabet(m_henv.envs.back().count);
                    ToUpper(num);
                    break;
                default:
                    num = Sprintf("%d", m_henv.envs.back().count);
                    break;
                }
                if (w3mApp::Instance().IndentIncr >= 4)
                    num->Push(". ");
                else
                    num->Push('.');
                m_obuf.push_spaces(1, w3mApp::Instance().IndentIncr - num->Size());
                m_obuf.push_str(num->Size(), num, PC_ASCII);
                if (w3mApp::Instance().IndentIncr >= 4)
                    m_obuf.set_space_to_prevchar();
                break;
            }
            default:
                m_obuf.push_spaces(1, w3mApp::Instance().IndentIncr);
                break;
            }
        }
        else
        {
            m_henv.flushline(0, 0, m_henv.limit);
        }
        m_obuf.flag |= RB_IGNORE_P;
        return 1;
    }
    case HTML_DT:
    {
        this->CLOSE_A();
        if (m_henv.envs.empty() ||
            (m_henv.envs.back().env != HTML_DL &&
             m_henv.envs.back().env != HTML_DL_COMPACT))
        {
            m_henv.PUSH_ENV(HTML_DL);
        }
        if (m_henv.envs.size())
        {
            m_henv.flushline(
                m_henv.envs.back().indent, 0, m_henv.limit);
        }
        if (!(m_obuf.flag & RB_IN_DT))
        {
            this->ProcessLine("<b>", true);
            m_obuf.flag |= RB_IN_DT;
        }
        m_obuf.flag |= RB_IGNORE_P;
        return 1;
    }
    case HTML_DD:
    {
        this->CLOSE_A();
        this->CLOSE_DT();
        if (m_henv.envs.back().env == HTML_DL_COMPACT)
        {
            if (m_obuf.pos > m_henv.envs.back().indent)
                m_henv.flushline(m_henv.envs.back().indent, 0,
                                 m_henv.limit);
            else
                m_obuf.push_spaces(1, m_henv.envs.back().indent - m_obuf.pos);
        }
        else
            m_henv.flushline(m_henv.envs.back().indent, 0, m_henv.limit);
        /* m_obuf.flag |= RB_IGNORE_P; */
        return 1;
    }
    case HTML_TITLE:
    {
        this->close_anchor();
        this->TitleOpen(tag);
        m_obuf.flag |= RB_TITLE;
        m_obuf.end_tag = HTML_N_TITLE;
        return 1;
    }
    case HTML_N_TITLE:
    {
        if (!(m_obuf.flag & RB_TITLE))
            return 1;
        m_obuf.flag &= ~RB_TITLE;
        m_obuf.end_tag = HTML_UNKNOWN;
        auto tmp = this->TitleClose(tag);
        if (tmp)
            this->ProcessLine(tmp->ptr, true);
        return 1;
    }
    case HTML_TITLE_ALT:
    {
        char *p;
        if (tag->TryGetAttributeValue(ATTR_TITLE, &p))
            m_henv.title = html_unquote(p, w3mApp::Instance().InnerCharset);
        return 0;
    }
    case HTML_FRAMESET:
    {
        m_henv.PUSH_ENV(tag->tagid);
        m_obuf.push_charp(9, "--FRAME--", PC_ASCII);
        m_henv.flushline(m_henv.envs.back().indent, 0, m_henv.limit);
        return 0;
    }
    case HTML_N_FRAMESET:
    {
        if (m_henv.envs.size())
        {
            m_henv.POP_ENV();
            m_henv.flushline(m_henv.envs.back().indent, 0, m_henv.limit);
        }
        return 0;
    }
    case HTML_NOFRAMES:
    {
        this->CLOSE_A();
        m_henv.flushline(m_henv.envs.back().indent, 0, m_henv.limit);
        m_obuf.flag |= (RB_NOFRAMES | RB_IGNORE_P);
        /* istr = str; */
        return 1;
    }
    case HTML_N_NOFRAMES:
    {
        this->CLOSE_A();
        m_henv.flushline(m_henv.envs.back().indent, 0, m_henv.limit);
        m_obuf.flag &= ~RB_NOFRAMES;
        return 1;
    }
    case HTML_FRAME:
    {
        char *q;
        tag->TryGetAttributeValue(ATTR_SRC, &q);
        char *r;
        tag->TryGetAttributeValue(ATTR_NAME, &r);
        if (q)
        {
            q = html_quote(q);
            m_obuf.push_tag(Sprintf("<a hseq=\"%d\" href=\"%s\">", this->Increment(), q)->ptr, HTML_A);
            if (r)
                q = html_quote(r);
            m_obuf.push_charp(get_strwidth(q), q, PC_ASCII);
            m_obuf.push_tag("</a>", HTML_N_A);
        }
        m_henv.flushline(m_henv.envs.back().indent, 0, m_henv.limit);
        return 0;
    }
    case HTML_HR:
    {
        this->close_anchor();
        auto tmp = this->process_hr(tag, m_henv.limit, m_henv.envs.back().indent);
        this->ProcessLine(tmp->ptr, true);
        m_obuf.set_space_to_prevchar();
        return 1;
    }
    case HTML_PRE:
    {
        auto x = tag->HasAttribute(ATTR_FOR_TABLE);
        this->CLOSE_A();
        if (!(m_obuf.flag & RB_IGNORE_P))
        {
            m_henv.flushline(m_henv.envs.back().indent, 0, m_henv.limit);
            if (!x)
                m_henv.do_blankline(m_henv.obuf, m_henv.envs.back().indent, 0,
                                    m_henv.limit);
        }
        else
            m_obuf.fillline(m_henv.envs.back().indent);
        m_obuf.flag |= (RB_PRE | RB_IGNORE_P);
        /* istr = str; */
        return 1;
    }
    case HTML_N_PRE:
    {
        m_henv.flushline(m_henv.envs.back().indent, 0, m_henv.limit);
        if (!(m_obuf.flag & RB_IGNORE_P))
        {
            m_henv.do_blankline(m_henv.obuf, m_henv.envs.back().indent, 0,
                                m_henv.limit);
            m_obuf.flag |= RB_IGNORE_P;
            m_henv.blank_lines++;
        }
        m_obuf.flag &= ~RB_PRE;
        this->close_anchor();
        return 1;
    }
    case HTML_PRE_INT:
    {
        auto i = m_obuf.line->Size();
        m_obuf.append_tags();
        if (!(m_obuf.flag & RB_SPECIAL))
        {
            m_obuf.bp.set(m_henv.obuf, m_obuf.line->Size() - i);
        }
        m_obuf.flag |= RB_PRE_INT;
        return 0;
    }
    case HTML_N_PRE_INT:
    {
        m_obuf.push_tag("</pre_int>", HTML_N_PRE_INT);
        m_obuf.flag &= ~RB_PRE_INT;
        if (!(m_obuf.flag & RB_SPECIAL) && m_obuf.pos > m_obuf.bp.pos())
        {
            m_obuf.prevchar->CopyFrom("", 0);
            m_obuf.prev_ctype = PC_CTRL;
        }
        return 1;
    }
    case HTML_NOBR:
    {
        m_obuf.flag |= RB_NOBR;
        m_obuf.nobr_level++;
        return 0;
    }
    case HTML_N_NOBR:
    {
        if (m_obuf.nobr_level > 0)
            m_obuf.nobr_level--;
        if (m_obuf.nobr_level == 0)
            m_obuf.flag &= ~RB_NOBR;
        return 0;
    }
    case HTML_PRE_PLAIN:
    {
        this->CLOSE_A();
        if (!(m_obuf.flag & RB_IGNORE_P))
        {
            m_henv.flushline(m_henv.envs.back().indent, 0, m_henv.limit);
            m_henv.do_blankline(m_henv.obuf, m_henv.envs.back().indent, 0,
                                m_henv.limit);
        }
        m_obuf.flag |= (RB_PRE | RB_IGNORE_P);
        return 1;
    }
    case HTML_N_PRE_PLAIN:
    {
        this->CLOSE_A();
        if (!(m_obuf.flag & RB_IGNORE_P))
        {
            m_henv.flushline(m_henv.envs.back().indent, 0, m_henv.limit);
            m_henv.do_blankline(m_henv.obuf, m_henv.envs.back().indent, 0,
                                m_henv.limit);
            m_obuf.flag |= RB_IGNORE_P;
        }
        m_obuf.flag &= ~RB_PRE;
        return 1;
    }
    case HTML_LISTING:
    case HTML_XMP:
    case HTML_PLAINTEXT:
    {
        this->CLOSE_A();
        if (!(m_obuf.flag & RB_IGNORE_P))
        {
            m_henv.flushline(m_henv.envs.back().indent, 0, m_henv.limit);
            m_henv.do_blankline(m_henv.obuf, m_henv.envs.back().indent, 0,
                                m_henv.limit);
        }
        m_obuf.flag |= (RB_PLAIN | RB_IGNORE_P);
        switch (tag->tagid)
        {
        case HTML_LISTING:
            m_obuf.end_tag = HTML_N_LISTING;
            break;
        case HTML_XMP:
            m_obuf.end_tag = HTML_N_XMP;
            break;
        case HTML_PLAINTEXT:
            m_obuf.end_tag = MAX_HTMLTAG;
            break;
        }
        return 1;
    }
    case HTML_N_LISTING:
    case HTML_N_XMP:
    {
        this->CLOSE_A();
        if (!(m_obuf.flag & RB_IGNORE_P))
        {
            m_henv.flushline(m_henv.envs.back().indent, 0, m_henv.limit);
            m_henv.do_blankline(m_henv.obuf, m_henv.envs.back().indent, 0,
                                m_henv.limit);
            m_obuf.flag |= RB_IGNORE_P;
        }
        m_obuf.flag &= ~RB_PLAIN;
        m_obuf.end_tag = HTML_UNKNOWN;
        return 1;
    }
    case HTML_SCRIPT:
    {
        m_obuf.flag |= RB_SCRIPT;
        m_obuf.end_tag = HTML_N_SCRIPT;
        return 1;
    }
    case HTML_STYLE:
    {
        m_obuf.flag |= RB_STYLE;
        m_obuf.end_tag = HTML_N_STYLE;
        return 1;
    }
    case HTML_N_SCRIPT:
    {
        m_obuf.flag &= ~RB_SCRIPT;
        m_obuf.end_tag = HTML_UNKNOWN;
        return 1;
    }
    case HTML_N_STYLE:
    {
        m_obuf.flag &= ~RB_STYLE;
        m_obuf.end_tag = HTML_UNKNOWN;
        return 1;
    }
    case HTML_A:
    {
        if (m_obuf.anchor.url.size())
            this->close_anchor();
        char *p;
        if (tag->TryGetAttributeValue(ATTR_HREF, &p))
            m_obuf.anchor.url = Strnew(p)->ptr;
        if (tag->TryGetAttributeValue(ATTR_TARGET, &p))
            m_obuf.anchor.target = Strnew(p)->ptr;
        if (tag->TryGetAttributeValue(ATTR_REFERER, &p))
        {
            // TODO: noreferer
            // m_obuf.anchor.referer = Strnew(p)->ptr;
        }
        if (tag->TryGetAttributeValue(ATTR_TITLE, &p))
            m_obuf.anchor.title = Strnew(p)->ptr;
        if (tag->TryGetAttributeValue(ATTR_ACCESSKEY, &p))
            m_obuf.anchor.accesskey = (unsigned char)*p;

        auto hseq = 0;
        if (tag->TryGetAttributeValue(ATTR_HSEQ, &hseq))
            m_obuf.anchor.hseq = hseq;

        if (hseq == 0 && m_obuf.anchor.url.size())
        {
            m_obuf.anchor.hseq = this->Get();
            auto tmp = this->process_anchor(tag, m_henv.tagbuf->ptr);
            m_obuf.push_tag(tmp->ptr, HTML_A);
            if (w3mApp::Instance().displayLinkNumber)
                this->ProcessLine(this->GetLinkNumberStr(-1)->ptr, true);
            return 1;
        }
        return 0;
    }
    case HTML_N_A:
    {
        this->close_anchor();
        return 1;
    }
    case HTML_IMG:
    {
        auto tmp = this->process_img(tag, m_henv.limit);
        this->ProcessLine(tmp->ptr, true);
        return 1;
    }
    case HTML_IMG_ALT:
    {
        char *p;
        if (tag->TryGetAttributeValue(ATTR_SRC, &p))
            m_obuf.img_alt = Strnew(p);

        auto i = 0;
        if (tag->TryGetAttributeValue(ATTR_TOP_MARGIN, &i))
        {
            if (i > m_obuf.top_margin)
                m_obuf.top_margin = i;
        }
        i = 0;
        if (tag->TryGetAttributeValue(ATTR_BOTTOM_MARGIN, &i))
        {
            if (i > m_obuf.bottom_margin)
                m_obuf.bottom_margin = i;
        }

        return 0;
    }
    case HTML_N_IMG_ALT:
    {
        if (m_obuf.img_alt)
        {
            if (!m_obuf.close_effect0(HTML_IMG_ALT))
                m_obuf.push_tag("</img_alt>", HTML_N_IMG_ALT);
            m_obuf.img_alt = NULL;
        }
        return 1;
    }
    case HTML_INPUT_ALT:
    {
        auto i = 0;
        if (tag->TryGetAttributeValue(ATTR_TOP_MARGIN, &i))
        {
            if (i > m_obuf.top_margin)
                m_obuf.top_margin = i;
        }
        i = 0;
        if (tag->TryGetAttributeValue(ATTR_BOTTOM_MARGIN, &i))
        {
            if (i > m_obuf.bottom_margin)
                m_obuf.bottom_margin = i;
        }
        return 0;
    }
    case HTML_TABLE:
    {
        this->close_anchor();
        m_obuf.table_level++;
        if (m_obuf.table_level >= MAX_TABLE)
            break;
        auto w = BORDER_NONE;
        /* x: cellspacing, y: cellpadding */
        auto x = 2;
        auto y = 1;
        auto z = 0;
        auto width = 0;
        if (tag->HasAttribute(ATTR_BORDER))
        {
            if (tag->TryGetAttributeValue(ATTR_BORDER, &w))
            {
                if (w > 2)
                    w = BORDER_THICK;
                else if (w < 0)
                { /* weird */
                    w = BORDER_THIN;
                }
            }
            else
                w = BORDER_THIN;
        }
        int i;
        if (tag->TryGetAttributeValue(ATTR_WIDTH, &i))
        {
            if (m_obuf.table_level == 0)
                width = REAL_WIDTH(i, m_henv.limit - m_henv.envs.back().indent);
            else
                width = RELATIVE_WIDTH(i);
        }
        if (tag->HasAttribute(ATTR_HBORDER))
            w = BORDER_NOWIN;
        tag->TryGetAttributeValue(ATTR_CELLSPACING, &x);
        tag->TryGetAttributeValue(ATTR_CELLPADDING, &y);
        tag->TryGetAttributeValue(ATTR_VSPACE, &z);
        char *id = nullptr;
        tag->TryGetAttributeValue(ATTR_ID, &id);
        m_tables[m_obuf.table_level] = table::begin(w, x, y, z);
        if (id != NULL)
            m_tables[m_obuf.table_level]->id = Strnew(id);

        m_table_modes[m_obuf.table_level].pre_mode = TBLM_NONE;
        m_table_modes[m_obuf.table_level].indent_level = 0;
        m_table_modes[m_obuf.table_level].nobr_level = 0;
        m_table_modes[m_obuf.table_level].caption = 0;
        m_table_modes[m_obuf.table_level].end_tag = HTML_UNKNOWN;
#ifndef TABLE_EXPAND
        m_tables[m_obuf.table_level]->total_width = width;
#else
        tables[m_obuf.table_level]->real_width = width;
        tables[m_obuf.table_level]->total_width = 0;
#endif
        return 1;
    }
    case HTML_N_TABLE:
        /* should be processed in HTMLlineproc() */
        return 1;
    case HTML_CENTER:
    {
        this->CLOSE_A();
        if (!(m_obuf.flag & (RB_PREMODE | RB_IGNORE_P)))
            m_henv.flushline(m_henv.envs.back().indent, 0, m_henv.limit);
        m_obuf.RB_SAVE_FLAG();
        m_obuf.RB_SET_ALIGN(RB_CENTER);
        return 1;
    }
    case HTML_N_CENTER:
    {
        this->CLOSE_A();
        if (!(m_obuf.flag & RB_PREMODE))
            m_henv.flushline(m_henv.envs.back().indent, 0, m_henv.limit);
        m_obuf.RB_RESTORE_FLAG();
        return 1;
    }
    case HTML_DIV:
    {
        this->CLOSE_A();
        if (!(m_obuf.flag & RB_IGNORE_P))
            m_henv.flushline(m_henv.envs.back().indent, 0, m_henv.limit);
        m_obuf.set_alignment(tag);
        return 1;
    }
    case HTML_N_DIV:
    {
        this->CLOSE_A();
        m_henv.flushline(m_henv.envs.back().indent, 0, m_henv.limit);
        m_obuf.RB_RESTORE_FLAG();
        return 1;
    }
    case HTML_DIV_INT:
    {
        this->CLOSE_P();
        if (!(m_obuf.flag & RB_IGNORE_P))
            m_henv.flushline(m_henv.envs.back().indent, 0, m_henv.limit);
        m_obuf.set_alignment(tag);
        return 1;
    }
    case HTML_N_DIV_INT:
    {
        this->CLOSE_P();
        m_henv.flushline(m_henv.envs.back().indent, 0, m_henv.limit);
        m_obuf.RB_RESTORE_FLAG();
        return 1;
    }
    case HTML_FORM:
    {
        this->CLOSE_A();
        if (!(m_obuf.flag & RB_IGNORE_P))
            m_henv.flushline(m_henv.envs.back().indent, 0, m_henv.limit);
        auto tmp = m_form->FormOpen(tag);
        if (tmp)
            this->ProcessLine(tmp->ptr, true);
        return 1;
    }
    case HTML_N_FORM:
    {
        this->CLOSE_A();
        m_henv.flushline(m_henv.envs.back().indent, 0, m_henv.limit);
        m_obuf.flag |= RB_IGNORE_P;
        m_form->FormClose();
        return 1;
    }
    case HTML_INPUT:
    {
        this->close_anchor();
        auto tmp = this->process_input(tag);
        if (tmp)
            this->ProcessLine(tmp->ptr, true);
        return 1;
    }
    case HTML_SELECT:
    {
        this->close_anchor();
        auto tmp = this->process_select(tag);
        if (tmp)
            this->ProcessLine(tmp->ptr, true);
        m_obuf.flag |= RB_INSELECT;
        m_obuf.end_tag = HTML_N_SELECT;
        return 1;
    }
    case HTML_N_SELECT:
    {
        m_obuf.flag &= ~RB_INSELECT;
        m_obuf.end_tag = HTML_UNKNOWN;
        auto tmp = this->process_n_select();
        if (tmp)
            this->ProcessLine(tmp->ptr, true);
        return 1;
    }
    case HTML_OPTION:
        /* nothing */
        return 1;
    case HTML_TEXTAREA:
    {
        this->close_anchor();
        auto tmp = m_form->process_textarea(tag, m_henv.limit);
        if (tmp)
            this->ProcessLine(tmp->ptr, true);
        m_obuf.flag |= RB_INTXTA;
        m_obuf.end_tag = HTML_N_TEXTAREA;
        return 1;
    }
    case HTML_N_TEXTAREA:
    {
        m_obuf.flag &= ~RB_INTXTA;
        m_obuf.end_tag = HTML_UNKNOWN;
        auto tmp = this->process_n_textarea();
        if (tmp)
            this->ProcessLine(tmp->ptr, true);
        return 1;
    }
    case HTML_ISINDEX:
    {
        auto p = "";
        tag->TryGetAttributeValue(ATTR_PROMPT, &p);
        auto q = "!CURRENT_URL!";
        tag->TryGetAttributeValue(ATTR_ACTION, &q);
        auto tmp = Strnew_m_charp("<form method=get action=\"",
                                  html_quote(q),
                                  "\">",
                                  html_quote(p),
                                  "<input type=text name=\"\" accept></form>",
                                  NULL);
        this->ProcessLine(tmp->ptr, true);
        return 1;
    }
    case HTML_META:
    {
        char *p;
        tag->TryGetAttributeValue(ATTR_HTTP_EQUIV, &p);
        char *q;
        tag->TryGetAttributeValue(ATTR_CONTENT, &q);

        if (p && q && !strcasecmp(p, "Content-Type") &&
            (q = strcasestr(q, "charset")) != NULL)
        {
            q += 7;
            SKIP_BLANKS(&q);
            if (*q == '=')
            {
                q++;
                SKIP_BLANKS(&q);
                this->SetMetaCharset(wc_guess_charset(q, WC_CES_NONE));
            }
        }
        else if (p && q && !strcasecmp(p, "refresh"))
        {
            int refresh_interval;
            Str tmp = NULL;
            refresh_interval = getMetaRefreshParam(q, &tmp);
            if (tmp)
            {
                q = html_quote(tmp->ptr);
                tmp = Sprintf("Refresh (%d sec) <a href=\"%s\">%s</a>",
                              refresh_interval, q, q);
            }
            else if (refresh_interval > 0)
                tmp = Sprintf("Refresh (%d sec)", refresh_interval);
            if (tmp)
            {
                this->ProcessLine(tmp->ptr, true);
                m_henv.do_blankline(m_henv.obuf, m_henv.envs.back().indent, 0,
                                    m_henv.limit);
                if (!w3mApp::Instance().is_redisplay &&
                    !((m_obuf.flag & RB_NOFRAMES) && w3mApp::Instance().RenderFrame))
                {
                    tag->need_reconstruct = true;
                    return 0;
                }
            }
        }
        return 1;
    }
    case HTML_BASE:
    {
        char *p = NULL;
        if (tag->TryGetAttributeValue(ATTR_HREF, &p))
        {
            // *GetCurBaseUrl() = URL::Parse(p, NULL);
        }
    }
    case HTML_MAP:
    case HTML_N_MAP:
    case HTML_AREA:
        return 0;
    case HTML_DEL:
    {
        switch (w3mApp::Instance().displayInsDel)
        {
        case DISPLAY_INS_DEL_SIMPLE:
            m_obuf.flag |= RB_DEL;
            break;
        case DISPLAY_INS_DEL_NORMAL:
            this->ProcessLine("<U>[DEL:</U>", true);
            break;
        case DISPLAY_INS_DEL_FONTIFY:
            m_obuf.fontstat.in_strike++;
            if (m_obuf.fontstat.in_strike == 1)
            {
                m_obuf.push_tag("<s>", HTML_S);
            }
            break;
        }
        return 1;
    }
    case HTML_N_DEL:
    {
        switch (w3mApp::Instance().displayInsDel)
        {
        case DISPLAY_INS_DEL_SIMPLE:
            m_obuf.flag &= ~RB_DEL;
            break;
        case DISPLAY_INS_DEL_NORMAL:
            this->ProcessLine("<U>:DEL]</U>", true);
        case DISPLAY_INS_DEL_FONTIFY:
            if (m_obuf.fontstat.in_strike == 0)
                return 1;
            if (m_obuf.fontstat.in_strike == 1 && m_obuf.close_effect0(HTML_S))
                m_obuf.fontstat.in_strike = 0;
            if (m_obuf.fontstat.in_strike > 0)
            {
                m_obuf.fontstat.in_strike--;
                if (m_obuf.fontstat.in_strike == 0)
                {
                    m_obuf.push_tag("</s>", HTML_N_S);
                }
            }
            break;
        }
        return 1;
    }
    case HTML_S:
    {
        switch (w3mApp::Instance().displayInsDel)
        {
        case DISPLAY_INS_DEL_SIMPLE:
            m_obuf.flag |= RB_S;
            break;
        case DISPLAY_INS_DEL_NORMAL:
            this->ProcessLine("<U>[S:</U>", true);
            break;
        case DISPLAY_INS_DEL_FONTIFY:
            m_obuf.fontstat.in_strike++;
            if (m_obuf.fontstat.in_strike == 1)
            {
                m_obuf.push_tag("<s>", HTML_S);
            }
            break;
        }
        return 1;
    }
    case HTML_N_S:
    {
        switch (w3mApp::Instance().displayInsDel)
        {
        case DISPLAY_INS_DEL_SIMPLE:
            m_obuf.flag &= ~RB_S;
            break;
        case DISPLAY_INS_DEL_NORMAL:
            this->ProcessLine("<U>:S]</U>", true);
            break;
        case DISPLAY_INS_DEL_FONTIFY:
            if (m_obuf.fontstat.in_strike == 0)
                return 1;
            if (m_obuf.fontstat.in_strike == 1 && m_obuf.close_effect0(HTML_S))
                m_obuf.fontstat.in_strike = 0;
            if (m_obuf.fontstat.in_strike > 0)
            {
                m_obuf.fontstat.in_strike--;
                if (m_obuf.fontstat.in_strike == 0)
                {
                    m_obuf.push_tag("</s>", HTML_N_S);
                }
            }
        }
        return 1;
    }
    case HTML_INS:
    {
        switch (w3mApp::Instance().displayInsDel)
        {
        case DISPLAY_INS_DEL_SIMPLE:
            break;
        case DISPLAY_INS_DEL_NORMAL:
            this->ProcessLine("<U>[INS:</U>", true);
            break;
        case DISPLAY_INS_DEL_FONTIFY:
            m_obuf.fontstat.in_ins++;
            if (m_obuf.fontstat.in_ins == 1)
            {
                m_obuf.push_tag("<ins>", HTML_INS);
            }
            break;
        }
        return 1;
    }
    case HTML_N_INS:
    {
        switch (w3mApp::Instance().displayInsDel)
        {
        case DISPLAY_INS_DEL_SIMPLE:
            break;
        case DISPLAY_INS_DEL_NORMAL:
            this->ProcessLine("<U>:INS]</U>", true);
            break;
        case DISPLAY_INS_DEL_FONTIFY:
            if (m_obuf.fontstat.in_ins == 0)
                return 1;
            if (m_obuf.fontstat.in_ins == 1 && m_obuf.close_effect0(HTML_INS))
                m_obuf.fontstat.in_ins = 0;
            if (m_obuf.fontstat.in_ins > 0)
            {
                m_obuf.fontstat.in_ins--;
                if (m_obuf.fontstat.in_ins == 0)
                {
                    m_obuf.push_tag("</ins>", HTML_N_INS);
                }
            }
            break;
        }
        return 1;
    }
    case HTML_SUP:
    {
        if (!(m_obuf.flag & (RB_DEL | RB_S)))
            this->ProcessLine("^", true);
        return 1;
    }
    case HTML_N_SUP:
        return 1;
    case HTML_SUB:
    {
        if (!(m_obuf.flag & (RB_DEL | RB_S)))
            this->ProcessLine("[", true);
        return 1;
    }
    case HTML_N_SUB:
    {
        if (!(m_obuf.flag & (RB_DEL | RB_S)))
            this->ProcessLine("]", true);
        return 1;
    }
    case HTML_FONT:
    case HTML_N_FONT:
    case HTML_NOP:
        return 1;
    case HTML_BGSOUND:
    {
        if (w3mApp::Instance().view_unseenobject)
        {
            char *p;
            if (tag->TryGetAttributeValue(ATTR_SRC, &p))
            {
                auto q = html_quote(p);
                Str s = Sprintf("<A HREF=\"%s\">bgsound(%s)</A>", q, q);
                this->ProcessLine(s->ptr, true);
            }
        }
        return 1;
    }
    case HTML_EMBED:
    {
        if (w3mApp::Instance().view_unseenobject)
        {
            char *p;
            if (tag->TryGetAttributeValue(ATTR_SRC, &p))
            {
                auto q = html_quote(p);
                Str s = Sprintf("<A HREF=\"%s\">embed(%s)</A>", q, q);
                this->ProcessLine(s->ptr, true);
            }
        }
        return 1;
    }
    case HTML_APPLET:
    {
        if (w3mApp::Instance().view_unseenobject)
        {
            char *p;
            if (tag->TryGetAttributeValue(ATTR_ARCHIVE, &p))
            {
                auto q = html_quote(p);
                auto s = Sprintf("<A HREF=\"%s\">applet archive(%s)</A>", q, q);
                this->ProcessLine(s->ptr, true);
            }
        }
        return 1;
    }
    case HTML_BODY:
    {
        if (w3mApp::Instance().view_unseenobject)
        {
            char *p;
            if (tag->TryGetAttributeValue(ATTR_BACKGROUND, &p))
            {
                auto q = html_quote(p);
                auto s = Sprintf("<IMG SRC=\"%s\" ALT=\"bg image(%s)\"><BR>", q, q);
                this->ProcessLine(s->ptr, true);
            }
        }
        return 1;
    }
    case HTML_N_HEAD:
    {
        if (m_obuf.flag & RB_TITLE)
            this->ProcessLine("</title>", true);
        return 1;
    }
    case HTML_HEAD:
    case HTML_N_BODY:
        return 1;
    default:
        /* m_obuf.prevchar = '\0'; */
        return 0;
    }
    /* not reached */
    return 0;
}

bool html_feed_environ::need_flushline(Lineprop mode)
{
    if (this->obuf->flag & RB_PRE_INT)
    {
        if (this->obuf->pos > this->limit)
            return 1;
        else
            return 0;
    }

    auto ch = this->obuf->line->Back();
    /* if (ch == ' ' && this->obuf->tag_sp > 0) */
    if (ch == ' ')
        return 0;

    if (this->obuf->pos > this->limit)
        return 1;

    return 0;
}

void HtmlContext::make_caption(struct table *t)
{
    if (t->caption->Size() <= 0)
        return;

    int limit;
    if (t->total_width > 0)
        limit = t->total_width;
    else
        limit = m_henv.limit;

    // TODO:
    // struct readbuffer obuf;
    // html_feed_environ henv(&obuf, newTextLineList(), limit, h_env->envs.back().indent);
    this->ProcessLine("<center>", true);
    this->ProcessLine(t->caption->ptr, false);
    this->ProcessLine("</center>", true);

    // if (t->total_width < henv.maxlimit)
    //     t->total_width = henv.maxlimit;
    limit = m_henv.limit;
    m_henv.limit = t->total_width;
    this->ProcessLine("<center>", true);
    this->ProcessLine(t->caption->ptr, false);
    this->ProcessLine("</center>", true);
    m_henv.limit = limit;
}

#define TAG_IS(s, tag, len) (strncasecmp(s, tag, len) == 0 && (s[len] == '>' || IS_SPACE((int)s[len])))

void HtmlContext::do_refill(struct table *tbl, int row, int col, int maxlimit)
{
    TextList *orgdata;
    TextListItem *l;
    struct readbuffer obuf;
    int colspan, icell;

    if (tbl->tabdata[row] == NULL || tbl->tabdata[row][col] == NULL)
        return;
    orgdata = (TextList *)tbl->tabdata[row][col];
    tbl->tabdata[row][col] = newGeneralList();

    html_feed_environ h_env(&obuf,
                            (TextLineList *)tbl->tabdata[row][col],
                            tbl->get_spec_cell_width(row, col));
    obuf.flag |= RB_INTABLE;
    if (h_env.limit > maxlimit)
        h_env.limit = maxlimit;
    if (tbl->border_mode != BORDER_NONE && tbl->vcellpadding > 0)
        h_env.do_blankline(&obuf, 0, 0, h_env.limit);
    for (l = orgdata->first; l != NULL; l = l->next)
    {
        if (TAG_IS(l->ptr, "<table_alt", 10))
        {
            int id = -1;
            const char *p = l->ptr;
            auto [pos, tag] = HtmlTag::parse(p, true);
            p = pos.data();
            if (tag)
                tag->TryGetAttributeValue(ATTR_TID, &id);
            if (id >= 0 && id < tbl->ntable)
            {
                TextLineListItem *ti;
                struct table *t = tbl->tables[id].ptr;
                int limit = tbl->tables[id].indent + t->total_width;
                tbl->tables[id].ptr = NULL;
                h_env.obuf->save_fonteffect();
                h_env.flushline(0, 2, h_env.limit);
                if (t->vspace > 0 && !(obuf.flag & RB_IGNORE_P))
                    h_env.do_blankline(&obuf, 0, 0, h_env.limit);

                AlignTypes alignment;
                if (h_env.obuf->RB_GET_ALIGN() == RB_CENTER)
                    alignment = ALIGN_CENTER;
                else if (h_env.obuf->RB_GET_ALIGN() == RB_RIGHT)
                    alignment = ALIGN_RIGHT;
                else
                    alignment = ALIGN_LEFT;

                if (alignment != ALIGN_LEFT)
                {
                    for (ti = tbl->tables[id].buf->first;
                         ti != NULL; ti = ti->next)
                        align(ti->ptr, h_env.limit, alignment);
                }
                appendTextLineList(h_env.buf, tbl->tables[id].buf);
                if (h_env.maxlimit < limit)
                    h_env.maxlimit = limit;
                h_env.obuf->restore_fonteffect();
                obuf.flag &= ~RB_IGNORE_P;
                h_env.blank_lines = 0;
                if (t->vspace > 0)
                {
                    h_env.do_blankline(&obuf, 0, 0, h_env.limit);
                    obuf.flag |= RB_IGNORE_P;
                }
            }
        }
        else
            this->ProcessLine(l->ptr, true);
    }
    if (obuf.status != R_ST_NORMAL)
    {
        obuf.status = R_ST_EOL;
        this->ProcessLine("\n", true);
    }
    this->completeHTMLstream();
    h_env.flushline(0, 2, h_env.limit);
    if (tbl->border_mode == BORDER_NONE)
    {
        int rowspan = tbl->table_rowspan(row, col);
        if (row + rowspan <= tbl->maxrow)
        {
            if (tbl->vcellpadding > 0 && !(obuf.flag & RB_IGNORE_P))
                h_env.do_blankline(&obuf, 0, 0, h_env.limit);
        }
        else
        {
            if (tbl->vspace > 0)
                h_env.purgeline();
        }
    }
    else
    {
        if (tbl->vcellpadding > 0)
        {
            if (!(obuf.flag & RB_IGNORE_P))
                h_env.do_blankline(&obuf, 0, 0, h_env.limit);
        }
        else
            h_env.purgeline();
    }
    if ((colspan = tbl->table_colspan(row, col)) > 1)
    {
        struct table_cell *cell = &tbl->cell;
        int k;
        k = bsearch_2short(colspan, cell->colspan, col, cell->col, MAXCOL,
                           cell->index, cell->maxcell + 1);
        icell = cell->index[k];
        if (cell->minimum_width[icell] < h_env.maxlimit)
            cell->minimum_width[icell] = h_env.maxlimit;
    }
    else
    {
        if (tbl->minimum_width[col] < h_env.maxlimit)
            tbl->minimum_width[col] = h_env.maxlimit;
    }
}

#define CASE_TABLE_TAG    \
    case HTML_TABLE:      \
    case HTML_N_TABLE:    \
    case HTML_TR:         \
    case HTML_N_TR:       \
    case HTML_TD:         \
    case HTML_N_TD:       \
    case HTML_TH:         \
    case HTML_N_TH:       \
    case HTML_THEAD:      \
    case HTML_N_THEAD:    \
    case HTML_TBODY:      \
    case HTML_N_TBODY:    \
    case HTML_TFOOT:      \
    case HTML_N_TFOOT:    \
    case HTML_COLGROUP:   \
    case HTML_N_COLGROUP: \
    case HTML_COL

#define ATTR_ROWSPAN_MAX 32766

static void
table_close_select(struct table *tbl, struct table_mode *mode, int width, HtmlContext *seq)
{
    Str tmp = seq->process_n_select();
    mode->pre_mode &= ~TBLM_INSELECT;
    mode->end_tag = HTML_UNKNOWN;
    seq->feed_table1(tbl, tmp, mode, width);
}

static void
table_close_textarea(struct table *tbl, struct table_mode *mode, int width, HtmlContext *seq)
{
    Str tmp = seq->process_n_textarea();
    mode->pre_mode &= ~TBLM_INTXTA;
    mode->end_tag = HTML_UNKNOWN;
    seq->feed_table1(tbl, tmp, mode, width);
}

TagActions HtmlContext::feed_table_tag(struct table *tbl, const char *line, struct table_mode *mode, int width, HtmlTagPtr tag)
{
    int cmd;
#ifdef ID_EXT
    char *p;
#endif
    struct table_cell *cell = &tbl->cell;
    int colspan, rowspan;
    int col, prev_col;
    int i, j, k, v, v0, w, id;
    Str tok, tmp, anchor;
    TableAttributes align, valign;

    cmd = tag->tagid;

    if (mode->pre_mode & TBLM_PLAIN)
    {
        if (mode->end_tag == cmd)
        {
            mode->pre_mode &= ~TBLM_PLAIN;
            mode->end_tag = HTML_UNKNOWN;
            tbl->feed_table_block_tag(line, mode, 0, cmd);
            return TAG_ACTION_NONE;
        }
        return TAG_ACTION_PLAIN;
    }
    if (mode->pre_mode & TBLM_INTXTA)
    {
        switch (cmd)
        {
        CASE_TABLE_TAG:
        case HTML_N_TEXTAREA:
            table_close_textarea(tbl, mode, width, this);
            if (cmd == HTML_N_TEXTAREA)
                return TAG_ACTION_NONE;
            break;
        default:
            return TAG_ACTION_FEED;
        }
    }
    if (mode->pre_mode & TBLM_SCRIPT)
    {
        if (mode->end_tag == cmd)
        {
            mode->pre_mode &= ~TBLM_SCRIPT;
            mode->end_tag = HTML_UNKNOWN;
            return TAG_ACTION_NONE;
        }
        return TAG_ACTION_PLAIN;
    }
    if (mode->pre_mode & TBLM_STYLE)
    {
        if (mode->end_tag == cmd)
        {
            mode->pre_mode &= ~TBLM_STYLE;
            mode->end_tag = HTML_UNKNOWN;
            return TAG_ACTION_NONE;
        }
        return TAG_ACTION_PLAIN;
    }
    /* failsafe: a tag other than <option></option>and </select> in *
     * <select> environment is regarded as the end of <select>. */
    if (mode->pre_mode & TBLM_INSELECT)
    {
        switch (cmd)
        {
        CASE_TABLE_TAG:
        case HTML_N_FORM:
        case HTML_N_SELECT: /* mode->end_tag */
            table_close_select(tbl, mode, width, this);
            if (cmd == HTML_N_SELECT)
                return TAG_ACTION_NONE;
            break;
        default:
            return TAG_ACTION_FEED;
        }
    }
    if (mode->caption)
    {
        switch (cmd)
        {
        CASE_TABLE_TAG:
        case HTML_N_CAPTION:
            mode->caption = 0;
            if (cmd == HTML_N_CAPTION)
                return TAG_ACTION_NONE;
            break;
        default:
            return TAG_ACTION_FEED;
        }
    }

    if (mode->pre_mode & TBLM_PRE)
    {
        switch (cmd)
        {
        case HTML_NOBR:
        case HTML_N_NOBR:
        case HTML_PRE_INT:
        case HTML_N_PRE_INT:
            return TAG_ACTION_NONE;
        }
    }

    switch (cmd)
    {
    case HTML_TABLE:
        tbl->check_rowcol(mode);
        return TAG_ACTION_TABLE;
    case HTML_N_TABLE:
        if (tbl->suspended_data)
            tbl->check_rowcol(mode);
        return TAG_ACTION_N_TABLE;
    case HTML_TR:
        if (tbl->col >= 0 && tbl->tabcontentssize > 0)
            tbl->setwidth(mode);
        tbl->col = -1;
        tbl->row++;
        tbl->flag |= TBL_IN_ROW;
        tbl->flag &= ~TBL_IN_COL;
        align = {};
        valign = {};
        if (tag->TryGetAttributeValue(ATTR_ALIGN, &i))
        {
            switch (i)
            {
            case ALIGN_LEFT:
                align = (HTT_LEFT | HTT_TRSET);
                break;
            case ALIGN_RIGHT:
                align = (HTT_RIGHT | HTT_TRSET);
                break;
            case ALIGN_CENTER:
                align = (HTT_CENTER | HTT_TRSET);
                break;
            }
        }
        if (tag->TryGetAttributeValue(ATTR_VALIGN, &i))
        {
            switch (i)
            {
            case VALIGN_TOP:
                valign = (HTT_TOP | HTT_VTRSET);
                break;
            case VALIGN_MIDDLE:
                valign = (HTT_MIDDLE | HTT_VTRSET);
                break;
            case VALIGN_BOTTOM:
                valign = (HTT_BOTTOM | HTT_VTRSET);
                break;
            }
        }
#ifdef ID_EXT
        if (tag->TryGetAttributeValue(ATTR_ID, &p))
            tbl->tridvalue[tbl->row] = Strnew(p);
#endif /* ID_EXT */
        tbl->trattr = align | valign;
        break;
    case HTML_TH:
    case HTML_TD:
        prev_col = tbl->col;
        if (tbl->col >= 0 && tbl->tabcontentssize > 0)
            tbl->setwidth(mode);
        if (tbl->row == -1)
        {
            /* for broken HTML... */
            tbl->row = -1;
            tbl->col = -1;
            tbl->maxrow = tbl->row;
        }
        if (tbl->col == -1)
        {
            if (!(tbl->flag & TBL_IN_ROW))
            {
                tbl->row++;
                tbl->flag |= TBL_IN_ROW;
            }
            if (tbl->row > tbl->maxrow)
                tbl->maxrow = tbl->row;
        }
        tbl->col++;
        tbl->check_row(tbl->row);
        while (tbl->tabattr[tbl->row][tbl->col])
        {
            tbl->col++;
        }
        if (tbl->col > MAXCOL - 1)
        {
            tbl->col = prev_col;
            return TAG_ACTION_NONE;
        }
        if (tbl->col > tbl->maxcol)
        {
            tbl->maxcol = tbl->col;
        }
        colspan = rowspan = 1;
        if (tbl->trattr & HTT_TRSET)
            align = (tbl->trattr & HTT_ALIGN);
        else if (cmd == HTML_TH)
            align = HTT_CENTER;
        else
            align = HTT_LEFT;
        if (tbl->trattr & HTT_VTRSET)
            valign = (tbl->trattr & HTT_VALIGN);
        else
            valign = HTT_MIDDLE;
        if (tag->TryGetAttributeValue(ATTR_ROWSPAN, &rowspan))
        {
            if (rowspan > ATTR_ROWSPAN_MAX)
            {
                rowspan = ATTR_ROWSPAN_MAX;
            }
            if ((tbl->row + rowspan) >= tbl->max_rowsize)
                tbl->check_row(tbl->row + rowspan);
        }
        if (tag->TryGetAttributeValue(ATTR_COLSPAN, &colspan))
        {
            if ((tbl->col + colspan) >= MAXCOL)
            {
                /* Can't expand column */
                colspan = MAXCOL - tbl->col;
            }
        }
        if (tag->TryGetAttributeValue(ATTR_ALIGN, &i))
        {
            switch (i)
            {
            case ALIGN_LEFT:
                align = HTT_LEFT;
                break;
            case ALIGN_RIGHT:
                align = HTT_RIGHT;
                break;
            case ALIGN_CENTER:
                align = HTT_CENTER;
                break;
            }
        }
        if (tag->TryGetAttributeValue(ATTR_VALIGN, &i))
        {
            switch (i)
            {
            case VALIGN_TOP:
                valign = HTT_TOP;
                break;
            case VALIGN_MIDDLE:
                valign = HTT_MIDDLE;
                break;
            case VALIGN_BOTTOM:
                valign = HTT_BOTTOM;
                break;
            }
        }
#ifdef NOWRAP
        if (tag->HasAttribute(ATTR_NOWRAP))
            tbl->tabattr[tbl->row][tbl->col] |= HTT_NOWRAP;
#endif /* NOWRAP */
        v = 0;
        if (tag->TryGetAttributeValue(ATTR_WIDTH, &v))
        {
#ifdef TABLE_EXPAND
            if (v > 0)
            {
                if (tbl->real_width > 0)
                    v = -(v * 100) / (tbl->real_width * ImageManager::Instance().pixel_per_char);
                else
                    v = (int)(v / ImageManager::Instance().pixel_per_char);
            }
#else
            v = RELATIVE_WIDTH(v);
#endif /* not TABLE_EXPAND */
        }
#ifdef ID_EXT
        if (tag->TryGetAttributeValue(ATTR_ID, &p))
            tbl->tabidvalue[tbl->row][tbl->col] = Strnew(p);
#endif /* ID_EXT */
#ifdef NOWRAP
        if (v != 0)
        {
            /* NOWRAP and WIDTH= conflicts each other */
            tbl->tabattr[tbl->row][tbl->col] &= ~HTT_NOWRAP;
        }
#endif /* NOWRAP */
        tbl->tabattr[tbl->row][tbl->col] &= ~(HTT_ALIGN | HTT_VALIGN);
        tbl->tabattr[tbl->row][tbl->col] |= (align | valign);
        if (colspan > 1)
        {
            col = tbl->col;

            cell->icell = cell->maxcell + 1;
            k = bsearch_2short(colspan, cell->colspan, col, cell->col, MAXCOL,
                               cell->index, cell->icell);
            if (k <= cell->maxcell)
            {
                i = cell->index[k];
                if (cell->col[i] == col && cell->colspan[i] == colspan)
                    cell->icell = i;
            }
            if (cell->icell > cell->maxcell && cell->icell < MAXCELL)
            {
                cell->maxcell++;
                cell->col[cell->maxcell] = col;
                cell->colspan[cell->maxcell] = colspan;
                cell->width[cell->maxcell] = 0;
                cell->minimum_width[cell->maxcell] = 0;
                cell->fixed_width[cell->maxcell] = 0;
                if (cell->maxcell > k)
                {
                    int ii;
                    for (ii = cell->maxcell; ii > k; ii--)
                        cell->index[ii] = cell->index[ii - 1];
                }
                cell->index[k] = cell->maxcell;
            }
            if (cell->icell > cell->maxcell)
                cell->icell = -1;
        }
        if (v != 0)
        {
            if (colspan == 1)
            {
                v0 = tbl->fixed_width[tbl->col];
                if (v0 == 0 || (v0 > 0 && v > v0) || (v0 < 0 && v < v0))
                {
#ifdef FEED_TABLE_DEBUG
                    fprintf(stderr, "width(%d) = %d\n", tbl->col, v);
#endif /* TABLE_DEBUG */
                    tbl->fixed_width[tbl->col] = v;
                }
            }
            else if (cell->icell >= 0)
            {
                v0 = cell->fixed_width[cell->icell];
                if (v0 == 0 || (v0 > 0 && v > v0) || (v0 < 0 && v < v0))
                    cell->fixed_width[cell->icell] = v;
            }
        }
        for (i = 0; i < rowspan; i++)
        {
            tbl->check_row(tbl->row + i);
            for (j = 0; j < colspan; j++)
            {
#if 0
		tbl->tabattr[tbl->row + i][tbl->col + j] &= ~(HTT_X | HTT_Y);
#endif
                if (!(tbl->tabattr[tbl->row + i][tbl->col + j] &
                      (HTT_X | HTT_Y)))
                {
                    tbl->tabattr[tbl->row + i][tbl->col + j] |=
                        ((i > 0) ? HTT_Y : HTT_NONE) | ((j > 0) ? HTT_X : HTT_NONE);
                }
                if (tbl->col + j > tbl->maxcol)
                {
                    tbl->maxcol = tbl->col + j;
                }
            }
            if (tbl->row + i > tbl->maxrow)
            {
                tbl->maxrow = tbl->row + i;
            }
        }
        tbl->begin_cell(mode);
        break;
    case HTML_N_TR:
        tbl->setwidth(mode);
        tbl->col = -1;
        tbl->flag &= ~(TBL_IN_ROW | TBL_IN_COL);
        return TAG_ACTION_NONE;
    case HTML_N_TH:
    case HTML_N_TD:
        tbl->setwidth(mode);
        tbl->flag &= ~TBL_IN_COL;
#ifdef FEED_TABLE_DEBUG
        {
            TextListItem *it;
            int i = tbl->col, j = tbl->row;
            fprintf(stderr, "(a) row,col: %d, %d\n", j, i);
            if (tbl->tabdata[j] && tbl->tabdata[j][i])
            {
                for (it = ((TextList *)tbl->tabdata[j][i])->first;
                     it; it = it->next)
                    fprintf(stderr, "  [%s] \n", it->ptr);
            }
        }
#endif
        return TAG_ACTION_NONE;
    case HTML_P:
    case HTML_BR:
    case HTML_CENTER:
    case HTML_N_CENTER:
    case HTML_DIV:
    case HTML_N_DIV:
        if (!(tbl->flag & TBL_IN_ROW))
            break;
    case HTML_DT:
    case HTML_DD:
    case HTML_H:
    case HTML_N_H:
    case HTML_LI:
    case HTML_PRE:
    case HTML_N_PRE:
    case HTML_HR:
    case HTML_LISTING:
    case HTML_XMP:
    case HTML_PLAINTEXT:
    case HTML_PRE_PLAIN:
    case HTML_N_PRE_PLAIN:
        tbl->feed_table_block_tag(line, mode, 0, cmd);
        switch (cmd)
        {
        case HTML_PRE:
        case HTML_PRE_PLAIN:
            mode->pre_mode |= TBLM_PRE;
            break;
        case HTML_N_PRE:
        case HTML_N_PRE_PLAIN:
            mode->pre_mode &= ~TBLM_PRE;
            break;
        case HTML_LISTING:
            mode->pre_mode |= TBLM_PLAIN;
            mode->end_tag = HTML_N_LISTING;
            break;
        case HTML_XMP:
            mode->pre_mode |= TBLM_PLAIN;
            mode->end_tag = HTML_N_XMP;
            break;
        case HTML_PLAINTEXT:
            mode->pre_mode |= TBLM_PLAIN;
            mode->end_tag = MAX_HTMLTAG;
            break;
        }
        break;
    case HTML_DL:
    case HTML_BLQ:
    case HTML_OL:
    case HTML_UL:
        tbl->feed_table_block_tag(line, mode, 1, cmd);
        break;
    case HTML_N_DL:
    case HTML_N_BLQ:
    case HTML_N_OL:
    case HTML_N_UL:
        tbl->feed_table_block_tag(line, mode, -1, cmd);
        break;
    case HTML_NOBR:
    case HTML_WBR:
        if (!(tbl->flag & TBL_IN_ROW))
            break;
    case HTML_PRE_INT:
        tbl->feed_table_inline_tag(line, mode, -1);
        switch (cmd)
        {
        case HTML_NOBR:
            mode->nobr_level++;
            if (mode->pre_mode & TBLM_NOBR)
                return TAG_ACTION_NONE;
            mode->pre_mode |= TBLM_NOBR;
            break;
        case HTML_PRE_INT:
            if (mode->pre_mode & TBLM_PRE_INT)
                return TAG_ACTION_NONE;
            mode->pre_mode |= TBLM_PRE_INT;
            tbl->linfo.prev_spaces = 0;
            break;
        }
        mode->nobr_offset = -1;
        if (tbl->linfo.length > 0)
        {
            tbl->check_minimum0(tbl->linfo.length);
            tbl->linfo.length = 0;
        }
        break;
    case HTML_N_NOBR:
        if (!(tbl->flag & TBL_IN_ROW))
            break;
        tbl->feed_table_inline_tag(line, mode, -1);
        if (mode->nobr_level > 0)
            mode->nobr_level--;
        if (mode->nobr_level == 0)
            mode->pre_mode &= ~TBLM_NOBR;
        break;
    case HTML_N_PRE_INT:
        tbl->feed_table_inline_tag(line, mode, -1);
        mode->pre_mode &= ~TBLM_PRE_INT;
        break;
    case HTML_IMG:
        tbl->check_rowcol(mode);
        w = tbl->fixed_width[tbl->col];
        if (w < 0)
        {
            if (tbl->total_width > 0)
                w = -tbl->total_width * w / 100;
            else if (width > 0)
                w = -width * w / 100;
            else
                w = 0;
        }
        else if (w == 0)
        {
            if (tbl->total_width > 0)
                w = tbl->total_width;
            else if (width > 0)
                w = width;
        }
        tok = this->process_img(tag, w);
        this->feed_table1(tbl, tok, mode, width);
        break;
    case HTML_FORM:
        tbl->feed_table_block_tag("", mode, 0, cmd);
        tmp = m_form->FormOpen(tag);
        if (tmp)
            this->feed_table1(tbl, tmp, mode, width);
        break;
    case HTML_N_FORM:
        tbl->feed_table_block_tag("", mode, 0, cmd);
        m_form->FormClose();
        break;
    case HTML_INPUT:
        tmp = this->process_input(tag);
        this->feed_table1(tbl, tmp, mode, width);
        break;
    case HTML_SELECT:
        tmp = this->process_select(tag);
        if (tmp)
            this->feed_table1(tbl, tmp, mode, width);
        mode->pre_mode |= TBLM_INSELECT;
        mode->end_tag = HTML_N_SELECT;
        break;
    case HTML_N_SELECT:
    case HTML_OPTION:
        /* nothing */
        break;
    case HTML_TEXTAREA:
        w = 0;
        tbl->check_rowcol(mode);
        if (tbl->col + 1 <= tbl->maxcol &&
            tbl->tabattr[tbl->row][tbl->col + 1] & HTT_X)
        {
            if (cell->icell >= 0 && cell->fixed_width[cell->icell] > 0)
                w = cell->fixed_width[cell->icell];
        }
        else
        {
            if (tbl->fixed_width[tbl->col] > 0)
                w = tbl->fixed_width[tbl->col];
        }
        tmp = m_form->process_textarea(tag, w);
        if (tmp)
            this->feed_table1(tbl, tmp, mode, width);
        mode->pre_mode |= TBLM_INTXTA;
        mode->end_tag = HTML_N_TEXTAREA;
        break;
    case HTML_A:
        tbl->table_close_anchor0(mode);
        anchor = NULL;
        i = 0;
        tag->TryGetAttributeValue(ATTR_HREF, &anchor);
        tag->TryGetAttributeValue(ATTR_HSEQ, &i);
        if (anchor)
        {
            tbl->check_rowcol(mode);
            if (i == 0)
            {
                Str tmp = this->process_anchor(tag, line);
                if (w3mApp::Instance().displayLinkNumber)
                {
                    Str t = this->GetLinkNumberStr(-1);
                    tbl->feed_table_inline_tag(NULL, mode, t->Size());
                    tmp->Push(t);
                }
                tbl->pushdata(tbl->row, tbl->col, tmp->ptr);
            }
            else
                tbl->pushdata(tbl->row, tbl->col, line);
            if (i >= 0)
            {
                mode->pre_mode |= TBLM_ANCHOR;
                mode->anchor_offset = tbl->tabcontentssize;
            }
        }
        else
            tbl->suspend_or_pushdata(line);
        break;
    case HTML_DEL:
        switch (w3mApp::Instance().displayInsDel)
        {
        case DISPLAY_INS_DEL_SIMPLE:
            mode->pre_mode |= TBLM_DEL;
            break;
        case DISPLAY_INS_DEL_NORMAL:
            tbl->feed_table_inline_tag(line, mode, 5); /* [DEL: */
            break;
        case DISPLAY_INS_DEL_FONTIFY:
            tbl->feed_table_inline_tag(line, mode, -1);
            break;
        }
        break;
    case HTML_N_DEL:
        switch (w3mApp::Instance().displayInsDel)
        {
        case DISPLAY_INS_DEL_SIMPLE:
            mode->pre_mode &= ~TBLM_DEL;
            break;
        case DISPLAY_INS_DEL_NORMAL:
            tbl->feed_table_inline_tag(line, mode, 5); /* :DEL] */
            break;
        case DISPLAY_INS_DEL_FONTIFY:
            tbl->feed_table_inline_tag(line, mode, -1);
            break;
        }
        break;
    case HTML_S:
        switch (w3mApp::Instance().displayInsDel)
        {
        case DISPLAY_INS_DEL_SIMPLE:
            mode->pre_mode |= TBLM_S;
            break;
        case DISPLAY_INS_DEL_NORMAL:
            tbl->feed_table_inline_tag(line, mode, 3); /* [S: */
            break;
        case DISPLAY_INS_DEL_FONTIFY:
            tbl->feed_table_inline_tag(line, mode, -1);
            break;
        }
        break;
    case HTML_N_S:
        switch (w3mApp::Instance().displayInsDel)
        {
        case DISPLAY_INS_DEL_SIMPLE:
            mode->pre_mode &= ~TBLM_S;
            break;
        case DISPLAY_INS_DEL_NORMAL:
            tbl->feed_table_inline_tag(line, mode, 3); /* :S] */
            break;
        case DISPLAY_INS_DEL_FONTIFY:
            tbl->feed_table_inline_tag(line, mode, -1);
            break;
        }
        break;
    case HTML_INS:
    case HTML_N_INS:
        switch (w3mApp::Instance().displayInsDel)
        {
        case DISPLAY_INS_DEL_SIMPLE:
            break;
        case DISPLAY_INS_DEL_NORMAL:
            tbl->feed_table_inline_tag(line, mode, 5); /* [INS:, :INS] */
            break;
        case DISPLAY_INS_DEL_FONTIFY:
            tbl->feed_table_inline_tag(line, mode, -1);
            break;
        }
        break;
    case HTML_SUP:
    case HTML_SUB:
    case HTML_N_SUB:
        if (!(mode->pre_mode & (TBLM_DEL | TBLM_S)))
            tbl->feed_table_inline_tag(line, mode, 1); /* ^, [, ] */
        break;
    case HTML_N_SUP:
        break;
    case HTML_TABLE_ALT:
        id = -1;
        w = 0;
        tag->TryGetAttributeValue(ATTR_TID, &id);
        if (id >= 0 && id < tbl->ntable)
        {
            struct table *tbl1 = tbl->tables[id].ptr;
            tbl->feed_table_block_tag(line, mode, 0, cmd);
            tbl->addcontentssize(tbl1->maximum_table_width());
            tbl->check_minimum0(tbl1->sloppy_width);
#ifdef TABLE_EXPAND
            w = tbl1->total_width;
            v = 0;
            colspan = table_colspan(tbl, tbl->row, tbl->col);
            if (colspan > 1)
            {
                if (cell->icell >= 0)
                    v = cell->fixed_width[cell->icell];
            }
            else
                v = tbl->fixed_width[tbl->col];
            if (v < 0 && tbl->real_width > 0 && tbl1->real_width > 0)
                w = -(tbl1->real_width * 100) / tbl->real_width;
            else
                w = tbl1->real_width;
            if (w > 0)
                tbl->check_minimum0(w);
            else if (w < 0 && v < w)
            {
                if (colspan > 1)
                {
                    if (cell->icell >= 0)
                        cell->fixed_width[cell->icell] = w;
                }
                else
                    tbl->fixed_width[tbl->col] = w;
            }
#endif
            tbl->setwidth0(mode);
            tbl->clearcontentssize(mode);
        }
        break;
    case HTML_CAPTION:
        mode->caption = 1;
        break;
    case HTML_N_CAPTION:
    case HTML_THEAD:
    case HTML_N_THEAD:
    case HTML_TBODY:
    case HTML_N_TBODY:
    case HTML_TFOOT:
    case HTML_N_TFOOT:
    case HTML_COLGROUP:
    case HTML_N_COLGROUP:
    case HTML_COL:
        break;
    case HTML_SCRIPT:
        mode->pre_mode |= TBLM_SCRIPT;
        mode->end_tag = HTML_N_SCRIPT;
        break;
    case HTML_STYLE:
        mode->pre_mode |= TBLM_STYLE;
        mode->end_tag = HTML_N_STYLE;
        break;
    case HTML_N_A:
        tbl->table_close_anchor0(mode);
    case HTML_FONT:
    case HTML_N_FONT:
    case HTML_NOP:
        tbl->suspend_or_pushdata(line);
        break;
    case HTML_INTERNAL:
    case HTML_N_INTERNAL:
    case HTML_FORM_INT:
    case HTML_N_FORM_INT:
    case HTML_INPUT_ALT:
    case HTML_N_INPUT_ALT:
    case HTML_SELECT_INT:
    case HTML_N_SELECT_INT:
    case HTML_OPTION_INT:
    case HTML_TEXTAREA_INT:
    case HTML_N_TEXTAREA_INT:
    case HTML_IMG_ALT:
    case HTML_SYMBOL:
    case HTML_N_SYMBOL:
    default:
        /* unknown tag: put into table */
        return TAG_ACTION_FEED;
    }
    return TAG_ACTION_NONE;
}

int HtmlContext::feed_table(struct table *tbl, const char *line, struct table_mode *mode, int width, int internal)
{
    std::string x;
    int i;
    Str tmp;
    struct table_linfo *linfo = &tbl->linfo;

    if (*line == '<' && line[1] && REALLY_THE_BEGINNING_OF_A_TAG(line))
    {
        auto [pos, tag] = HtmlTag::parse(line, internal);
        if (tag)
        {
            switch (this->feed_table_tag(tbl, line, mode, width, tag))
            {
            case TAG_ACTION_NONE:
                return -1;
            case TAG_ACTION_N_TABLE:
                return 0;
            case TAG_ACTION_TABLE:
                return 1;
            case TAG_ACTION_PLAIN:
                break;
            case TAG_ACTION_FEED:
            default:
                if (tag->need_reconstruct)
                {
                    x = tag->ToStr();
                    line = x.c_str();
                }
            }
        }
        else
        {
            if (!(mode->pre_mode & (TBLM_PLAIN | TBLM_INTXTA | TBLM_INSELECT |
                                    TBLM_SCRIPT | TBLM_STYLE)))
                return -1;
        }
    }
    else
    {
        if (mode->pre_mode & (TBLM_DEL | TBLM_S))
            return -1;
    }
    if (mode->caption)
    {
        tbl->caption->Push(line);
        return -1;
    }
    if (mode->pre_mode & TBLM_SCRIPT)
        return -1;
    if (mode->pre_mode & TBLM_STYLE)
        return -1;
    if (mode->pre_mode & TBLM_INTXTA)
    {
        m_form->feed_textarea(line);
        return -1;
    }
    if (mode->pre_mode & TBLM_INSELECT)
    {
        this->feed_select(line);
        return -1;
    }
    if (!(mode->pre_mode & TBLM_PLAIN) &&
        !(*line == '<' && line[strlen(line) - 1] == '>') &&
        strchr(line, '&') != NULL)
    {
        tmp = Strnew();
        for (auto p = line; *p;)
        {
            const char *q, *r;
            if (*p == '&')
            {
                if (!strncasecmp(p, "&amp;", 5) ||
                    !strncasecmp(p, "&gt;", 4) || !strncasecmp(p, "&lt;", 4))
                {
                    /* do not convert */
                    tmp->Push(*p);
                    p++;
                }
                else
                {
                    q = p;
                    auto [pos, ec] = ucs4_from_entity(p);
                    p = pos.data();
                    switch (ec)
                    {
                    case '<':
                        tmp->Push("&lt;");
                        break;
                    case '>':
                        tmp->Push("&gt;");
                        break;
                    case '&':
                        tmp->Push("&amp;");
                        break;
                    case '\r':
                        tmp->Push('\n');
                        break;
                    default:
                        r = (char *)from_unicode(ec, w3mApp::Instance().InnerCharset);
                        if (r != NULL && strlen(r) == 1 &&
                            ec == (unsigned char)*r)
                        {
                            tmp->Push(*r);
                            break;
                        }
                    case -1:
                        tmp->Push(*q);
                        p = q + 1;
                        break;
                    }
                }
            }
            else
            {
                tmp->Push(*p);
                p++;
            }
        }
        line = tmp->ptr;
    }
    if (!(mode->pre_mode & (TBLM_SPECIAL & ~TBLM_NOBR)))
    {
        if (!(tbl->flag & TBL_IN_COL) || linfo->prev_spaces != 0)
            while (IS_SPACE(*line))
                line++;
        if (*line == '\0')
            return -1;
        tbl->check_rowcol(mode);
        if (mode->pre_mode & TBLM_NOBR && mode->nobr_offset < 0)
            mode->nobr_offset = tbl->tabcontentssize;

        /* count of number of spaces skipped in normal mode */
        i = tbl->skip_space(line, linfo, !(mode->pre_mode & TBLM_NOBR));
        tbl->addcontentssize(visible_length(line) - i);
        tbl->setwidth(mode);
        tbl->pushdata(tbl->row, tbl->col, line);
    }
    else if (mode->pre_mode & TBLM_PRE_INT)
    {
        tbl->check_rowcol(mode);
        if (mode->nobr_offset < 0)
            mode->nobr_offset = tbl->tabcontentssize;
        tbl->addcontentssize(maximum_visible_length(line, tbl->tabcontentssize));
        tbl->setwidth(mode);
        tbl->pushdata(tbl->row, tbl->col, line);
    }
    else
    {
        /* <pre> mode or something like it */
        tbl->check_rowcol(mode);
        while (*line)
        {
            int nl = false;
            const char *p;
            if ((p = strchr(const_cast<char *>(line), '\r')) || (p = strchr(const_cast<char *>(line), '\n')))
            {
                if (*p == '\r' && p[1] == '\n')
                    p++;
                if (p[1])
                {
                    p++;
                    tmp = Strnew_charp_n(line, p - line);
                    line = p;
                    p = tmp->ptr;
                }
                else
                {
                    p = line;
                    line = "";
                }
                nl = true;
            }
            else
            {
                p = line;
                line = "";
            }
            if (mode->pre_mode & TBLM_PLAIN)
                i = maximum_visible_length_plain(p, tbl->tabcontentssize);
            else
                i = maximum_visible_length(p, tbl->tabcontentssize);
            tbl->addcontentssize(i);
            tbl->setwidth(mode);
            if (nl)
                tbl->clearcontentssize(mode);
            tbl->pushdata(tbl->row, tbl->col, p);
        }
    }
    return -1;
}

void HtmlContext::feed_table1(struct table *tbl, Str tok, struct table_mode *mode, int width)
{
    if (!tok)
        return;

    auto tokbuf = Strnew();
    auto status = R_ST_NORMAL;
    std::string_view line = tok->ptr;
    while (line.size())
    {
        line = read_token(line, tokbuf, &status, mode->pre_mode & TBLM_PREMODE, 0);
        this->feed_table(tbl, tokbuf->ptr, mode, width, true);
    }
}

static int
floor_at_intervals(int x, int step)
{
    int mo = x % step;
    if (mo > 0)
        x -= mo;
    else if (mo < 0)
        x += step - mo;
    return x;
}

#define RULE(mode, n) (((mode) == BORDER_THICK) ? ((n) + 16) : (n))
#define TK_VERTICALBAR(mode) RULE(mode, 5)

void HtmlContext::renderTable(struct table *t, int max_width)
{
    t->total_height = 0;
    if (t->maxcol < 0)
    {
        this->make_caption(t);
        return;
    }

    if (t->sloppy_width > max_width)
        max_width = t->sloppy_width;

    int rulewidth = t->table_rule_width(Terminal::SymbolWidth());

    max_width -= t->table_border_width(Terminal::SymbolWidth());

    if (rulewidth > 1)
        max_width = floor_at_intervals(max_width, rulewidth);

    if (max_width < rulewidth)
        max_width = rulewidth;

    t->check_maximum_width();

#ifdef MATRIX
    if (t->maxcol == 0)
    {
        if (t->tabwidth[0] > max_width)
            t->tabwidth[0] = max_width;
        if (t->total_width > 0)
            t->tabwidth[0] = max_width;
        else if (t->fixed_width[0] > 0)
            t->tabwidth[0] = t->fixed_width[0];
        if (t->tabwidth[0] < t->minimum_width[0])
            t->tabwidth[0] = t->minimum_width[0];
    }
    else
    {
        t->set_table_matrix(max_width);

        int itr = 0;
        auto mat = m_get(t->maxcol + 1, t->maxcol + 1);
        auto pivot = px_get(t->maxcol + 1);
        auto newwidth = v_get(t->maxcol + 1);
        auto minv = m_get(t->maxcol + 1, t->maxcol + 1);
        do
        {
            m_copy(t->matrix, mat);
            LUfactor(mat, pivot);
            LUsolve(mat, pivot, t->vector, newwidth);
            LUinverse(mat, pivot, minv);
#ifdef TABLE_DEBUG
            set_integered_width(t, newwidth->ve, new_tabwidth);
            fprintf(stderr, "itr=%d\n", itr);
            fprintf(stderr, "max_width=%d\n", max_width);
            fprintf(stderr, "minimum : ");
            for (i = 0; i <= t->maxcol; i++)
                fprintf(stderr, "%2d ", t->minimum_width[i]);
            fprintf(stderr, "\nfixed : ");
            for (i = 0; i <= t->maxcol; i++)
                fprintf(stderr, "%2d ", t->fixed_width[i]);
            fprintf(stderr, "\ndecided : ");
            for (i = 0; i <= t->maxcol; i++)
                fprintf(stderr, "%2d ", new_tabwidth[i]);
            fprintf(stderr, "\n");
#endif /* TABLE_DEBUG */
            itr++;

        } while (t->check_table_width(newwidth->ve, minv, itr));
        short new_tabwidth[MAXCOL];
        t->set_integered_width(newwidth->ve, new_tabwidth, Terminal::SymbolWidth());
        t->check_minimum_width(new_tabwidth);
        v_free(newwidth);
        px_free(pivot);
        m_free(mat);
        m_free(minv);
        m_free(t->matrix);
        v_free(t->vector);
        for (int i = 0; i <= t->maxcol; i++)
        {
            t->tabwidth[i] = new_tabwidth[i];
        }
    }
#else  /* not MATRIX */
    set_table_width(t, new_tabwidth, max_width);
    for (i = 0; i <= t->maxcol; i++)
    {
        t->tabwidth[i] = new_tabwidth[i];
    }
#endif /* not MATRIX */

    t->check_minimum_width(t->tabwidth);
    for (int i = 0; i <= t->maxcol; i++)
        t->tabwidth[i] = ceil_at_intervals(t->tabwidth[i], rulewidth);

    this->renderCoTable(t, m_henv.limit);

    for (int i = 0; i <= t->maxcol; i++)
    {
        for (int j = 0; j <= t->maxrow; j++)
        {
            t->check_row(j);
            if (t->tabattr[j][i] & HTT_Y)
                continue;
            this->do_refill(t, j, i, m_henv.limit);
        }
    }

    t->check_minimum_width(t->tabwidth);
    t->total_width = 0;
    for (int i = 0; i <= t->maxcol; i++)
    {
        t->tabwidth[i] = ceil_at_intervals(t->tabwidth[i], rulewidth);
        t->total_width += t->tabwidth[i];
    }

    t->total_width += t->table_border_width(Terminal::SymbolWidth());

    t->check_table_height();

    for (int i = 0; i <= t->maxcol; i++)
    {
        for (int j = 0; j <= t->maxrow; j++)
        {
            if ((t->tabattr[j][i] & HTT_Y) ||
                (t->tabattr[j][i] & HTT_TOP) || (t->tabdata[j][i] == NULL))
                continue;
            auto h = t->tabheight[j];
            for (auto k = j + 1; k <= t->maxrow; k++)
            {
                if (!(t->tabattr[k][i] & HTT_Y))
                    break;
                h += t->tabheight[k];
                switch (t->border_mode)
                {
                case BORDER_THIN:
                case BORDER_THICK:
                case BORDER_NOWIN:
                    h += 1;
                    break;
                }
            }
            h -= t->tabdata[j][i]->nitem;
            if (t->tabattr[j][i] & HTT_MIDDLE)
                h /= 2;
            if (h <= 0)
                continue;
            auto l = newTextLineList();
            for (auto k = 0; k < h; k++)
                pushTextLine(l, newTextLine(NULL, 0));
            t->tabdata[j][i] = appendGeneralList((GeneralList *)l,
                                                 t->tabdata[j][i]);
        }
    }

    /* table output */
    auto width = t->total_width;

    this->make_caption(t);

    this->ProcessLine("<pre for_table>", true);

    if (t->id != NULL)
    {
        auto idtag = Sprintf("<_id id=\"%s\">", html_quote((t->id)->ptr));
        this->ProcessLine(idtag->ptr, true);
    }

    switch (t->border_mode)
    {
    case BORDER_THIN:
    case BORDER_THICK:
    {
        auto renderbuf = Strnew();
        t->print_sep(-1, VALIGN_TOP, t->maxcol, renderbuf, Terminal::SymbolWidth());
        m_henv.push_render_image(renderbuf, width, t->total_width);
        t->total_height += 1;
        break;
    }
    }

    Str vrulea = nullptr;
    Str vruleb = Strnew();
    Str vrulec = nullptr;
    switch (t->border_mode)
    {
    case BORDER_THIN:
    case BORDER_THICK:
        vrulea = Strnew();
        vrulec = Strnew();
        push_symbol(vrulea, TK_VERTICALBAR(t->border_mode), Terminal::SymbolWidth(), 1);
        for (int i = 0; i < t->cellpadding; i++)
        {
            vrulea->Push(' ');
            vruleb->Push(' ');
            vrulec->Push(' ');
        }
        push_symbol(vrulec, TK_VERTICALBAR(t->border_mode), Terminal::SymbolWidth(), 1);
    case BORDER_NOWIN:
        push_symbol(vruleb, TK_VERTICALBAR(BORDER_THIN), Terminal::SymbolWidth(), 1);
        for (int i = 0; i < t->cellpadding; i++)
            vruleb->Push(' ');
        break;
    case BORDER_NONE:
        for (int i = 0; i < t->cellspacing; i++)
            vruleb->Push(' ');
    }

    for (int r = 0; r <= t->maxrow; r++)
    {
        for (int h = 0; h < t->tabheight[r]; h++)
        {
            auto renderbuf = Strnew();
            if (t->border_mode == BORDER_THIN || t->border_mode == BORDER_THICK)
                renderbuf->Push(vrulea);

            if (t->tridvalue[r] != NULL && h == 0)
            {
                auto idtag = Sprintf("<_id id=\"%s\">",
                                     html_quote((t->tridvalue[r])->ptr));
                renderbuf->Push(idtag);
            }

            for (int i = 0; i <= t->maxcol; i++)
            {
                t->check_row(r);

                if (t->tabidvalue[r][i] != NULL && h == 0)
                {
                    auto idtag = Sprintf("<_id id=\"%s\">",
                                         html_quote((t->tabidvalue[r][i])->ptr));
                    renderbuf->Push(idtag);
                }

                if (!(t->tabattr[r][i] & HTT_X))
                {
                    int w = t->tabwidth[i];
                    for (int j = i + 1;
                         j <= t->maxcol && (t->tabattr[r][j] & HTT_X); j++)
                        w += t->tabwidth[j] + t->cellspacing;
                    if (t->tabattr[r][i] & HTT_Y)
                    {
                        int j = r - 1;
                        for (; j >= 0 && t->tabattr[j] && (t->tabattr[j][i] & HTT_Y); j--)
                            ;
                        t->print_item(j, i, w, renderbuf);
                    }
                    else
                        t->print_item(r, i, w, renderbuf);
                }
                if (i < t->maxcol && !(t->tabattr[r][i + 1] & HTT_X))
                    renderbuf->Push(vruleb);
            }
            switch (t->border_mode)
            {
            case BORDER_THIN:
            case BORDER_THICK:
                renderbuf->Push(vrulec);
                t->total_height += 1;
                break;
            }
            m_henv.push_render_image(renderbuf, width, t->total_width);
        }
        if (r < t->maxrow && t->border_mode != BORDER_NONE)
        {
            auto renderbuf = Strnew();
            t->print_sep(r, VALIGN_MIDDLE, t->maxcol, renderbuf, Terminal::SymbolWidth());
            m_henv.push_render_image(renderbuf, width, t->total_width);
        }
        t->total_height += t->tabheight[r];
    }
    switch (t->border_mode)
    {
    case BORDER_THIN:
    case BORDER_THICK:
    {
        auto renderbuf = Strnew();
        t->print_sep(t->maxrow, VALIGN_BOTTOM, t->maxcol, renderbuf, Terminal::SymbolWidth());
        m_henv.push_render_image(renderbuf, width, t->total_width);
        t->total_height += 1;
        break;
    }
    }
    if (t->total_height == 0)
    {
        auto renderbuf = Strnew(" ");
        t->total_height++;
        t->total_width = 1;
        m_henv.push_render_image(renderbuf, 1, t->total_width);
    }
    this->ProcessLine("</pre>", true);
}

void HtmlContext::renderCoTable(struct table *tbl, int maxlimit)
{
    struct readbuffer obuf;
    struct table *t;
    int i, col, row;
    int indent, maxwidth;

    for (i = 0; i < tbl->ntable; i++)
    {
        t = tbl->tables[i].ptr;
        col = tbl->tables[i].col;
        row = tbl->tables[i].row;
        indent = tbl->tables[i].indent;

        html_feed_environ h_env(&obuf, tbl->tables[i].buf,
                                tbl->get_spec_cell_width(row, col), indent);
        tbl->check_row(row);
        if (h_env.limit > maxlimit)
            h_env.limit = maxlimit;
        if (t->total_width == 0)
            maxwidth = h_env.limit - indent;
        else if (t->total_width > 0)
            maxwidth = t->total_width;
        else
            maxwidth = t->total_width = -t->total_width * h_env.limit / 100;
        this->renderTable(t, maxwidth);
    }
}

///
/// public
///
BufferPtr loadHTMLStream(const URL &url, const InputStreamPtr &stream, CharacterEncodingScheme content_charset, bool internal)
{
    ///
    /// parse
    ///
    HtmlContext context(content_charset);
    while (true)
    {
        auto lineBuf2 = stream->mygets();
        if (lineBuf2->Size() == 0)
        {
            break;
        }
        CharacterEncodingScheme detected = {};
        auto converted = wc_Str_conv_with_detect(lineBuf2, &detected, context.DocCharset(), w3mApp::Instance().InnerCharset);
        context.SetCES(detected);
        context.ProcessLine(converted->ptr, internal);
    }
    context.Finalize(internal);

    ///
    /// create buffer
    ///
    HtmlToBuffer generator(context.Form());
    return generator.CreateBuffer(url, context.Title(), context.DocCharset(), context.List());
}
