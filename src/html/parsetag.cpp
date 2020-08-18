#include "myctype.h"
#include "indep.h"
#include "gc_helper.h"

#include "html/parsetag.h"

const char *tag_get_value(struct parsed_tagarg *t, const char *arg)
{
    for (; t; t = t->next)
    {
        if (!strcasecmp(t->arg, arg))
            return t->value;
    }
    return NULL;
}

int tag_exists(struct parsed_tagarg *t, const char *arg)
{
    for (; t; t = t->next)
    {
        if (!strcasecmp(t->arg, arg))
            return 1;
    }
    return 0;
}

struct parsed_tagarg *
cgistr2tagarg(const char *cgistr)
{
    Str tag;
    Str value;
    struct parsed_tagarg *t0, *t;

    t = t0 = NULL;
    do
    {
        t = New(struct parsed_tagarg);
        t->next = t0;
        t0 = t;
        tag = Strnew();
        while (*cgistr && *cgistr != '=' && *cgistr != '&')
            tag->Push(*cgistr++);
        t->arg = Str_form_unquote(tag)->ptr;
        t->value = NULL;
        if (*cgistr == '\0')
            return t;
        else if (*cgistr == '=')
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
    return t;
}
