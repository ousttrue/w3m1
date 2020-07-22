#include "fm.h"
#include "indep.h"
#include "mimetypes.h"
#include "textlist.h"
#include "url.h"

static TextList *mimetypes_list;

struct ExtensionWithMime
{
    const char *item1;
    const char *item2;
};

static struct ExtensionWithMime **UserMimeTypes;

static struct ExtensionWithMime DefaultGuess[] = {
    {"html", "text/html"},
    {"htm", "text/html"},
    {"shtml", "text/html"},
    {"xhtml", "application/xhtml+xml"},
    {"gif", "image/gif"},
    {"jpeg", "image/jpeg"},
    {"jpg", "image/jpeg"},
    {"png", "image/png"},
    {"xbm", "image/xbm"},
    {"au", "audio/basic"},
    {"gz", "application/x-gzip"},
    {"Z", "application/x-compress"},
    {"bz2", "application/x-bzip"},
    {"tar", "application/x-tar"},
    {"zip", "application/x-zip"},
    {"lha", "application/x-lha"},
    {"lzh", "application/x-lha"},
    {"ps", "application/postscript"},
    {"pdf", "application/pdf"},
    {NULL, NULL}};

static struct ExtensionWithMime *
loadMimeTypes(char *filename)
{
    FILE *f;
    char *d, *type;
    int i, n;
    Str tmp;
    struct ExtensionWithMime *mtypes;

    f = fopen(expandPath(filename), "r");
    if (f == NULL)
        return NULL;
    n = 0;
    while (tmp = Strfgets(f), tmp->Size() > 0)
    {
        d = tmp->ptr;
        if (d[0] != '#')
        {
            d = strtok(d, " \t\n\r");
            if (d != NULL)
            {
                d = strtok(NULL, " \t\n\r");
                for (i = 0; d != NULL; i++)
                    d = strtok(NULL, " \t\n\r");
                n += i;
            }
        }
    }
    fseek(f, 0, 0);
    mtypes = New_N(struct ExtensionWithMime, n + 1);
    i = 0;
    while (tmp = Strfgets(f), tmp->Size() > 0)
    {
        d = tmp->ptr;
        if (d[0] == '#')
            continue;
        type = strtok(d, " \t\n\r");
        if (type == NULL)
            continue;
        while (1)
        {
            d = strtok(NULL, " \t\n\r");
            if (d == NULL)
                break;
            mtypes[i].item1 = Strnew_charp(d)->ptr;
            mtypes[i].item2 = Strnew_charp(type)->ptr;
            i++;
        }
    }
    mtypes[i].item1 = NULL;
    mtypes[i].item2 = NULL;
    fclose(f);
    return mtypes;
}

void initMimeTypes()
{
    if (non_null(mimetypes_files))
        mimetypes_list = make_domain_list(mimetypes_files);
    else
        mimetypes_list = NULL;
    if (mimetypes_list == NULL)
        return;
    UserMimeTypes = New_N(struct ExtensionWithMime *, mimetypes_list->nitem);

    int i = 0;
    for (auto tl = mimetypes_list->first; tl; tl = tl->next)
    {
        UserMimeTypes[i++] = loadMimeTypes(tl->ptr);
    }
}

static const char *
guessContentTypeFromTable(struct ExtensionWithMime *table, const char *filename)
{
    if (table == NULL)
        return NULL;
    auto p = &filename[strlen(filename) - 1];
    while (filename < p && *p != '.')
        p--;
    if (p == filename)
        return NULL;
    p++;
    for (auto t = table; t->item1; t++)
    {
        if (!strcmp(p, t->item1))
            return t->item2;
    }
    for (auto t = table; t->item1; t++)
    {
        if (!strcasecmp(p, t->item1))
            return t->item2;
    }
    return NULL;
}

const char *guessContentType(const char *filename)
{
    if (!filename)
    {
        return nullptr;
    }

    if (mimetypes_list)
    {

        for (int i = 0; i < mimetypes_list->nitem; i++)
        {
            auto type = guessContentTypeFromTable(UserMimeTypes[i], filename);
            if (type)
            {
                return type;
            }
        }
    }

    return guessContentTypeFromTable(DefaultGuess, filename);
}
