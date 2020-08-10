#include "fm.h"
#include "indep.h"
#include "gc_helper.h"
#include "formselect.h"
#include "html_processor.h"
#include "form.h"
#include "textlist.h"
#include "myctype.h"
#include "w3m.h"
#include "file.h"
#include "html_sequence.h"

void FormSelect::clear(int n)
{
    n_select = n;
    if (!max_select)
    { /* halfload */
        max_select = MAX_SELECT;
        select_option = New_N(FormSelectOption, max_select);
    }
    cur_select = nullptr;
}

void FormSelect::print_internal(TextLineList *tl)
{
    if (n_select > 0)
    {
        FormSelectOptionItem *ip;
        for (int i = 0; i < n_select; i++)
        {
            auto s = Sprintf("<select_int selectnumber=%d>", i);
            pushTextLine(tl, newTextLine(s, 0));
            for (ip = select_option[i].first; ip; ip = ip->next)
            {
                s = Sprintf("<option_int value=\"%s\" label=\"%s\"%s>",
                            html_quote(ip->value ? ip->value->ptr : ip->label->ptr),
                            html_quote(ip->label->ptr),
                            ip->checked ? " selected" : "");
                pushTextLine(tl, newTextLine(s, 0));
            }
            s = Strnew("</select_int>");
            pushTextLine(tl, newTextLine(s, 0));
        }
    }
}

void FormSelect::grow(int selectnumber)
{
    if (selectnumber >= max_select)
    {
        max_select = 2 * selectnumber;
        select_option = New_Reuse(FormSelectOption,
                                  select_option,
                                  max_select);
    }
}

FormSelectOption *FormSelect::get(int n) const
{
    return &select_option[n];
}

void FormSelect::set(int n)
{
    n_select = n;
    select_option[n_select].first = nullptr;
    select_option[n_select].last = nullptr;
}

std::pair<int, FormSelectOption *> FormSelect::getCurrent()
{
    if (n_select < 0)
    {
        return {};
    }
    return {n_select, &select_option[n_select]};
}

Str FormSelect::process_select(struct parsed_tag *tag, HSequence *seq)
{
    Str tmp = nullptr;
    if (cur_form_id() < 0)
    {
        auto s = "<form_int method=internal action=none>";
        tmp = process_form(parse_tag(&s, TRUE));
    }

    auto p = "";
    tag->TryGetAttributeValue(ATTR_NAME, &p);
    cur_select = Strnew(p);
    select_is_multiple = tag->HasAttribute(ATTR_MULTIPLE);

    if (!select_is_multiple)
    {
        select_str = Strnew("<pre_int>");
        if (displayLinkNumber)
            select_str->Push(seq->GetLinkNumberStr(0));
        select_str->Push(Sprintf("[<input_alt hseq=\"%d\" "
                                 "fid=\"%d\" type=select name=\"%s\" selectnumber=%d",
                                 seq->Increment(), cur_form_id(), html_quote(p), n_select));
        select_str->Push(">");
        if (n_select == max_select)
        {
            max_select *= 2;
            select_option =
                New_Reuse(FormSelectOption, select_option, max_select);
        }
        select_option[n_select].first = nullptr;
        select_option[n_select].last = nullptr;
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

void FormSelect::feed_select(char *str, HSequence *seq)
{
    Str tmp = Strnew();
    int prev_status = cur_status;
    static int prev_spaces = -1;

    if (cur_select == nullptr)
        return;
    while (read_token(tmp, &str, &cur_status, 0, 0))
    {
        if (cur_status != R_ST_NORMAL || prev_status != R_ST_NORMAL)
            continue;
        const char *p = tmp->ptr;
        if (tmp->ptr[0] == '<' && tmp->Back() == '>')
        {
            struct parsed_tag *tag;
            char *q;
            if (!(tag = parse_tag(&p, FALSE)))
                continue;
            switch (tag->tagid)
            {
            case HTML_OPTION:
                process_option(seq);
                cur_option = Strnew();
                if (tag->TryGetAttributeValue(ATTR_VALUE, &q))
                    cur_option_value = Strnew(q);
                else
                    cur_option_value = nullptr;
                if (tag->TryGetAttributeValue(ATTR_LABEL, &q))
                    cur_option_label = Strnew(q);
                else
                    cur_option_label = nullptr;
                cur_option_selected = tag->HasAttribute(ATTR_SELECTED);
                prev_spaces = -1;
                break;
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

Str FormSelect::process_n_select(HSequence *seq)
{
    if (cur_select == nullptr)
        return nullptr;
    process_option(seq);

    if (!select_is_multiple)
    {
        if (select_option[n_select].first)
        {
            FormItemList sitem;
            chooseSelectOption(&sitem, select_option[n_select].first);
            select_str->Push(textfieldrep(sitem.label, cur_option_maxwidth));
        }
        select_str->Push("</input_alt>]</pre_int>");
        n_select++;
    }
    else
    {
        select_str->Push("<br>");
    }

    cur_select = nullptr;
    n_selectitem = 0;
    return select_str;
}

void FormSelect::process_option(HSequence *seq)
{
    char begin_char = '[', end_char = ']';
    int len;

    if (cur_select == nullptr || cur_option == nullptr)
        return;
    while (cur_option->Size() > 0 && IS_SPACE(cur_option->Back()))
        cur_option->Pop(1);
    if (cur_option_value == nullptr)
        cur_option_value = cur_option;
    if (cur_option_label == nullptr)
        cur_option_label = cur_option;

    if (!select_is_multiple)
    {
        len = get_Str_strwidth(cur_option_label);
        if (len > cur_option_maxwidth)
            cur_option_maxwidth = len;
        addSelectOption(&select_option[n_select],
                        cur_option_value,
                        cur_option_label, cur_option_selected);
        return;
    }

    if (!select_is_multiple)
    {
        begin_char = '(';
        end_char = ')';
    }
    select_str->Push(Sprintf("<br><pre_int>%c<input_alt hseq=\"%d\" "
                             "fid=\"%d\" type=%s name=\"%s\" value=\"%s\"",
                             begin_char, seq->Increment(), cur_form_id(),
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

static FormSelect g_formSelect;

FormSelect *get_formselect()
{
    return &g_formSelect;
}
