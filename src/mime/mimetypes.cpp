#include "mime/mimetypes.h"
#include "mime/mailcap.h"

#include "indep.h"
#include "textlist.h"
#include "frontend/buffer.h"
#include "w3m.h"
#include <vector>
#include <string_view>

struct ExtensionWithMime
{
    std::string extension;
    std::string mime;
};

static std::vector<ExtensionWithMime> UserMimeTypes;

static std::vector<ExtensionWithMime> DefaultGuess = {
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
};

static std::vector<ExtensionWithMime> loadMimeTypes(std::string_view filename)
{
    auto f = fopen(expandPath(filename.data()), "r");
    if (f == NULL)
        return {};

    char *d, *type;
    int i;
    Str tmp;
    int n = 0;
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

    std::vector<ExtensionWithMime> mtypes(n);
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
            mtypes[i].extension = Strnew(d)->ptr;
            mtypes[i].mime = Strnew(type)->ptr;
            i++;
        }
    }
    fclose(f);
    return mtypes;
}

void initMimeTypes()
{
    TextList *mimetypes_list;
    if (w3mApp::Instance().mimetypes_files.size())
        mimetypes_list = make_domain_list(w3mApp::Instance().mimetypes_files.c_str());
    else
        mimetypes_list = NULL;
    if (mimetypes_list == NULL)
        return;

    for (auto tl = mimetypes_list->first; tl; tl = tl->next)
    {
        for (auto &mime : loadMimeTypes(tl->ptr))
        {
            UserMimeTypes.push_back(mime);
        }
    }
}

static bool iequals(std::string_view l, std::string_view r)
{
    if (l.size() != r.size())
    {
        return false;
    }
    for (int i = 0; i < l.size(); ++i)
    {
        if (tolower(l[i]) != tolower(r[i]))
        {
            return false;
        }
    }
    return true;
}

static const char *
guessContentTypeFromTable(const std::vector<ExtensionWithMime> &table, std::string_view filename)
{
    auto p = filename.rfind('.');
    if (p == std::string::npos)
        return NULL;

    auto ext = filename.substr(p + 1);
    for (auto &em : table)
    {
        if (iequals(ext, em.extension))
        {
            return em.mime.c_str();
        }
    }
    return NULL;
}

const char *guessContentType(std::string_view filename)
{
    if (filename.empty())
    {
        return nullptr;
    }

    auto type = guessContentTypeFromTable(UserMimeTypes, filename);
    if (type)
    {
        return type;
    }

    return guessContentTypeFromTable(DefaultGuess, filename);
}

bool is_html_type(std::string_view type)
{
    return type == "text/html" || type == "application/xhtml+xml";
}

bool is_text_type(std::string_view type)
{
    return type.empty() ||
           type.starts_with("text/") ||
           (type.starts_with("application/") && type.find("xhtml") != std::string::npos) ||
           type.starts_with("message/");
}

bool is_dump_text_type(std::string_view type)
{
    auto mcap = searchExtViewer(type);
    if (!mcap)
    {
        return false;
    }
    return mcap->flags & (MAILCAP_HTMLOUTPUT | MAILCAP_COPIOUSOUTPUT);
}

bool is_plain_text_type(std::string_view type)
{
    if (type == "text/plain")
    {
        return true;
    }

    return is_text_type(type) && !is_dump_text_type(type);
}
