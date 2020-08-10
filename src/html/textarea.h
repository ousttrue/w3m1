#pragma once
#include <wc.h>
#include <tuple>

class HtmlTextArea
{
    int n_textarea;
    Str *textarea_str;
#define MAX_TEXTAREA 10 /* max number of <textarea>..</textarea> \
                         * within one document */
    int max_textarea = MAX_TEXTAREA;

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
    Str process_n_textarea(void);
};
HtmlTextArea* get_textarea();
void feed_textarea(char *str);
Str process_n_textarea(void);
Str process_textarea(struct parsed_tag *tag, int width);
