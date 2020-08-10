#pragma once
#include <wc.h>
#include <tuple>

/* menu based <select>  */
class HSequence;
struct FormSelectOption;
class FormSelect
{
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
