#pragma once
#include <wc.h>
#include "frontend/buffer.h"
#include "stream/input_stream.h"
#include "html/html.h"
#include "html/form.h"
#include "html/readbuffer.h"
#include "html/table.h"

using FormSelectOptionList = std::vector<FormSelectOptionItem>;

class HtmlContext
{
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

    AnchorPtr a_href = nullptr;
    AnchorPtr a_img = nullptr;
    AnchorPtr a_form = nullptr;

    HtmlTags internal = HTML_UNKNOWN;

    std::vector<int> form_stack;
    std::vector<FormPtr> forms;

    std::vector<AnchorPtr> a_select;
    std::vector<FormSelectOptionList> select_option;
    int n_select = -1;
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
    std::vector<AnchorPtr> a_textarea;
    std::vector<Str> textarea_str;
    int n_textarea = -1;
    Str cur_textarea;
    int cur_textarea_size;
    int cur_textarea_rows;
    int cur_textarea_readonly;
    bool ignore_nl_textarea = false;

public:
    Lineprop effect = P_UNKNOWN;
    Lineprop ex_effect = P_UNKNOWN;
    char symbol = '\0';

    HtmlContext(CharacterEncodingScheme content_charset);
    ~HtmlContext();
    const CharacterEncodingScheme &DocCharset() const { return doc_charset; }
    void SetCES(CharacterEncodingScheme ces) { cur_document_charset = ces; }
    Str process_n_select();
    FormSelectOptionList *FormSelect(int n);
    Str Textarea(int n) const;
    Str process_n_textarea();
    using FeedFunc = std::function<Str()>;
    void BufferFromLines(BufferPtr buf, const FeedFunc &feed);
    void completeHTMLstream(struct html_feed_environ *, struct readbuffer *);
    void HTMLlineproc0(const char *istr, html_feed_environ *h_env, bool internal);
    void feed_table1(struct table *tbl, Str tok, struct table_mode *mode, int width);
    FormItemPtr formList_addInput(FormPtr fl, struct parsed_tag *tag);

private:
    void print_internal_information(struct html_feed_environ *henv);

    void print_internal(struct TextLineList *tl);
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
    Str TitleOpen(struct parsed_tag *tag);
    void TitleContent(const char *str);
    Str TitleClose(struct parsed_tag *tag);

    // process <form></form>
    int cur_form_id()
    {
        return form_stack.size() ? form_stack.back() : -1;
    }

    Str FormOpen(struct parsed_tag *tag, int fid = -1);
    Str FormClose(void)
    {
        if (form_stack.size() >= 0)
            form_stack.pop_back();
        return nullptr;
    }
    FormPtr FormCurrent(int form_id)
    {
        if (form_id < 0 || form_id >= forms.size())
            /* outside of <form>..</form> */
            return nullptr;
        return forms[form_id];
    }
    void FormSetSelect(int n);
    std::vector<FormPtr> &FormEnd();
    std::pair<int, FormSelectOptionList *> FormSelectCurrent();
    void process_option();
    Str process_select(struct parsed_tag *tag);
    void feed_select(const char *str);
    Str process_input(struct parsed_tag *tag);
    Str process_img(struct parsed_tag *tag, int width);
    Str process_anchor(struct parsed_tag *tag, const char *tagbuf);
    // void clear(int n);
    void Textarea(int n, Str str);
    std::pair<int, Str> TextareaCurrent() const;

    // push text to current_textarea
    void feed_textarea(const char *str);
    Str process_textarea(struct parsed_tag *tag, int width);

    bool EndLineAddBuffer();
    void Process(parsed_tag *tag, BufferPtr buf, int pos, const char *str);

    void close_anchor(struct html_feed_environ *h_env, struct readbuffer *obuf);
    void CLOSE_P(readbuffer *obuf, html_feed_environ *h_env);
    void CLOSE_A(readbuffer *obuf, html_feed_environ *h_env)
    {
        CLOSE_P(obuf, h_env);
        close_anchor(h_env, obuf);
    }
    void CLOSE_DT(readbuffer *obuf, html_feed_environ *h_env);
    Str process_hr(struct parsed_tag *tag, int width, int indent_width);
    int HTMLtagproc1(struct parsed_tag *tag, struct html_feed_environ *h_env);

    void make_caption(struct table *t, struct html_feed_environ *h_env);
    void do_refill(struct table *tbl, int row, int col, int maxlimit);
    int feed_table(struct table *tbl, const char *line, struct table_mode *mode, int width, int internal);
    TagActions feed_table_tag(struct table *tbl, const char *line, struct table_mode *mode, int width, struct parsed_tag *tag);
    void renderTable(struct table *t, int max_width, struct html_feed_environ *h_env);
    void renderCoTable(struct table *tbl, int maxlimit);
};

std::shared_ptr<struct Buffer> loadHTMLStream(const URL &url, const InputStreamPtr &stream, CharacterEncodingScheme content_charset, bool internal = false);
