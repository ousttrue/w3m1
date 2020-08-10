#pragma once
#include <wc.h>
#include <tuple>

class HtmlTextArea
{
    int n_textarea;
    Str *textarea_str;
    int max_textarea = 0;

    Str cur_textarea;
    int cur_textarea_size;
    int cur_textarea_rows;
    int cur_textarea_readonly;

    bool ignore_nl_textarea = false;

public:
    void clear(int n);
    void grow(int textareanumber);
    void set(int n, Str str);
    Str get(int n) const;
    std::pair<int, Str> getCurrent() const;
    void print_internal(struct TextLineList *tl);

    // push text to current_textarea
    void feed_textarea(const char *str);
    Str process_textarea(struct parsed_tag *tag, int width);
    Str process_n_textarea(class HtmlContext *seq);
};
HtmlTextArea *get_textarea();
inline Str process_textarea(struct parsed_tag *tag, int width)
{
    return get_textarea()->process_textarea(tag, width);
}
inline Str process_n_textarea(HtmlContext *seq)
{
    return get_textarea()->process_n_textarea(seq);
}
inline void feed_textarea(char *str)
{
    return get_textarea()->feed_textarea(str);
}
