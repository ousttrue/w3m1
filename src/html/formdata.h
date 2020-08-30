#pragma once
#include "html/form.h"
#include "html/html_tag.h"
#include <Str.h>
#include <vector>
#include <memory>

using FormSelectOptionList = std::vector<FormSelectOptionItem>;
struct FormData
{
    std::vector<FormPtr> forms;
    std::vector<int> form_stack;

    int cur_form_id()
    {
        return form_stack.size() ? form_stack.back() : -1;
    }

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

    Str FormOpen(HtmlTagPtr tag, int fid = -1);

    std::vector<FormPtr> &FormEnd()
    {
        return forms;
    }

    std::vector<FormSelectOptionList> select_option;
    int n_select = -1;
    FormSelectOptionList *FormSelect(int n)
    {
        return &select_option[n];
    }
    void FormSetSelect(int n)
    {
        if (n >= select_option.size())
        {
            select_option.resize(n + 1);
        }
        select_option[n] = {};
        n_select = n;
    }
    std::pair<int, FormSelectOptionList *> FormSelectCurrent()
    {
        if (n_select < 0)
        {
            return {};
        }
        return {n_select, &select_option[n_select]};
    }

    int n_textarea = -1;
    std::vector<Str> textarea_str;
    Str cur_textarea = nullptr;
    bool ignore_nl_textarea = false;
    int cur_textarea_size;
    int cur_textarea_rows;
    int cur_textarea_readonly;

    void Textarea(int n, Str str)
    {
        n_textarea = n;
        textarea_str[n_textarea] = str;
    }

    Str Textarea(int n) const
    {
        return textarea_str[n];
    }

    std::pair<int, Str> TextareaCurrent() const
    {
        if (n_textarea < 0)
        {
            return {-1, nullptr};
        }
        return {n_textarea, textarea_str[n_textarea]};
    }

    // push text to current_textarea
    void feed_textarea(const char *str);
    Str process_textarea(HtmlTagPtr tag, int width);
};
using FormDataPtr = std::shared_ptr<FormData>;
