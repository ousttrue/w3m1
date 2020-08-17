#pragma once
#include <wc.h>
#include "frontend/propstring.h"
#include "frontend/buffer.h"
#include "html/html.h"

struct FormSelectOption;
struct Anchor;
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

    int symbol_width = 0;
    int symbol_width0 = 0;

    Str cur_title = nullptr;

    Anchor *a_href = nullptr;
    Anchor *a_img = nullptr;
    Anchor *a_form = nullptr;

    HtmlTags internal = HTML_UNKNOWN;

    union frameset_element *idFrame = nullptr;
    std::vector<struct frameset *> frameset_s;

#define FORMSTACK_SIZE 10
#define INITIAL_FORM_SIZE 10
    struct FormList **forms;
    int *form_stack;
    int form_max = -1;
    int form_sp = 0;
    int forms_size = 0;
    std::vector<Anchor *> a_select;
    FormSelectOption *select_option;
    int max_select = 0;
    int n_select = 0;
    int cur_option_maxwidth = 0;
    Str cur_select = nullptr;
    Str select_str = nullptr;
    int select_is_multiple = 0;
    int n_selectitem = 0;
    Str cur_option = nullptr;
    Str cur_option_value = nullptr;
    Str cur_option_label = nullptr;
    int cur_option_selected = 0;
    int cur_status = 0;
    std::vector<Anchor *> a_textarea;
    int max_textarea = 0;
    int n_textarea;
    Str *textarea_str;
    Str cur_textarea;
    int cur_textarea_size;
    int cur_textarea_rows;
    int cur_textarea_readonly;
    bool ignore_nl_textarea = false;

public:
    Lineprop effect = P_UNKNOWN;
    Lineprop ex_effect = P_UNKNOWN;
    char symbol = '\0';

    HtmlContext();
    ~HtmlContext();

    void Initialize(const BufferPtr &newBuf, CharacterEncodingScheme content_charset);
    void print_internal_information(struct html_feed_environ *henv);

private:
    void print_internal(struct TextLineList *tl);

public:
    const CharacterEncodingScheme &DocCharset() const { return doc_charset; }
    CharacterEncodingScheme CES() const { return cur_document_charset; }
    void SetCES(CharacterEncodingScheme ces) { cur_document_charset = ces; }
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
    int SymbolWidth() const { return symbol_width; }
    int SymbolWidth0() const { return symbol_width0; }

    // process <title>{content}</title> tag
    Str TitleOpen(struct parsed_tag *tag);
    void TitleContent(const char *str);
    Str TitleClose(struct parsed_tag *tag);

    // process <form></form>
    int cur_form_id()
    {
        return form_sp >= 0 ? form_stack[form_sp] : -1;
    }
    Str FormOpen(struct parsed_tag *tag, int fid = -1);
    Str FormClose(void)
    {
        if (form_sp >= 0)
            form_sp--;
        return nullptr;
    }
    FormList *FormCurrent(int form_id)
    {
        if (form_id < 0 || form_id > form_max || forms == nullptr)
            /* outside of <form>..</form> */
            return nullptr;
        return forms[form_id];
    }
    FormList *FormEnd();
    void FormSelectGrow(int selectnumber);
    void FormSetSelect(int n);
    FormSelectOption *FormSelect(int n);
    std::pair<int, FormSelectOption *> FormSelectCurrent();
    Str process_n_select();
    void process_option();
    Str process_select(struct parsed_tag *tag);
    void feed_select(const char *str);
    Str process_input(struct parsed_tag *tag);
    Str process_img(struct parsed_tag *tag, int width);
    Str process_anchor(struct parsed_tag *tag, char *tagbuf);
    // void clear(int n);
    void TextareaGrow(int textareanumber);
    void Textarea(int n, Str str);
    Str Textarea(int n) const;
    std::pair<int, Str> TextareaCurrent() const;
    // push text to current_textarea
    void feed_textarea(const char *str);
    Str process_textarea(struct parsed_tag *tag, int width);
    Str process_n_textarea();

private:
    bool EndLineAddBuffer();
    void Process(parsed_tag *tag, BufferPtr buf, int pos, const char *str);

public:
    using FeedFunc = std::function<Str()>;
    void BufferFromLines(BufferPtr buf, const FeedFunc &feed);
};
