#pragma once

#include "html/html.h"
#define MAX_ENV_LEVEL 20

int GetCurHSeq();
void SetCurHSeq(int seq);
Str getLinkNumberStr(int correction);

Str process_img(struct parsed_tag *tag, int width);
Str process_anchor(struct parsed_tag *tag, char *tagbuf);
Str process_input(struct parsed_tag *tag);
Str process_select(struct parsed_tag *tag);
Str process_form_int(struct parsed_tag *tag, int fid);
inline Str process_form(struct parsed_tag *tag)
{
    return process_form_int(tag, -1);
}

int next_status(char c, int *status);
int read_token(Str buf, char **instr, int *status, int pre, int append);
Str correct_irrtag(int status);

int cur_form_id();

struct HtmlTextArea
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
void feed_textarea(char *str);
Str process_n_textarea(void);
Str process_textarea(struct parsed_tag *tag, int width);
