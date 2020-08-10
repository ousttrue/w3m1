#pragma once
#include <wc.h>
#include <tuple>

/* menu based <select>  */
class HSequence;
struct FormSelectOption;
class FormSelect
{
    FormSelectOption *select_option;
#define MAX_SELECT 10 /* max number of <select>..</select> \
                       * within one document */
    int max_select = MAX_SELECT;
    int n_select;
    int cur_option_maxwidth;
    Str cur_select;
    Str select_str;
    int select_is_multiple;
    int n_selectitem;
    Str cur_option;
    Str cur_option_value;
    Str cur_option_label;
    int cur_option_selected;
    int cur_status;

public:
    void clear(int n);
    void print_internal(struct TextLineList *tl);
    void grow(int selectnumber);
    void set(int n);
    FormSelectOption *get(int n) const;
    std::pair<int, FormSelectOption *> getCurrent();

    Str process_n_select(HSequence *seq);
    void process_option(HSequence *seq);
    Str process_select(struct parsed_tag *tag, HSequence *seq);

    void feed_select(char *str, HSequence *seq);
};
FormSelect *get_formselect();

inline Str process_select(struct parsed_tag *tag, HSequence *seq)
{
    return get_formselect()->process_select(tag, seq);
}
inline Str process_n_select(HSequence *seq)
{
    return get_formselect()->process_n_select(seq);
}
inline void feed_select(char *str, HSequence *seq)
{
    get_formselect()->feed_select(str, seq);
}
inline void process_option(HSequence *seq)
{
    get_formselect()->process_option(seq);
}
