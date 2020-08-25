#include "myctype.h"
#include "indep.h"
#include "html/parsetag.h"

const char *tag_get_value(tcb::span<parsed_tagarg> list, const char *arg)
{
    for (auto &t : list)
    {
        if (!strcasecmp(t.arg, arg))
            return t.value;
    }
    return NULL;
}

struct std::vector<parsed_tagarg> cgistr2tagarg(const char *cgistr)
{
    Str tag;
    Str value;
    // struct parsed_tagarg *t0, *t;

    std::vector<parsed_tagarg> list;

    // t = t0 = NULL;
    do
    {
        // t = New(struct parsed_tagarg);
        // t->next = t0;
        // t0 = t;

        auto tag = Strnew();
        while (*cgistr && *cgistr != '=' && *cgistr != '&')
            tag->Push(*cgistr++);

        list.push_back({});
        auto t = &list.back();

        t->arg = Str_form_unquote(tag)->ptr;
        t->value = NULL;
        if (*cgistr == '\0')
        {
            break;
        }

        if (*cgistr == '=')
        {
            cgistr++;
            value = Strnew();
            while (*cgistr && *cgistr != '&')
                value->Push(*cgistr++);
            t->value = Str_form_unquote(value)->ptr;
        }
        else if (*cgistr == '&')
            cgistr++;
    } while (*cgistr);

    return list;
}
