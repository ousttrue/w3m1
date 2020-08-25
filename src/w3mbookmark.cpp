/* $Id: w3mbookmark.c,v 1.12 2007/05/31 01:19:50 inu Exp $ */
#include <stdlib.h>
#include <stdio.h>
#include "config.h"

#include "indep.h"
#include "textlist.h"
#include "html/parsetag.h"

#define BKMARK_TITLE "Register to my bookmark"
#define BKMARK_ADD "ADD"
auto DEFAULT_SECTION = "Miscellaneous";

static auto bkmark_src1 =
    "<html>\n\
<head>\n\
<title>" BKMARK_TITLE "</title>\n\
</head>\n\
<body>\n\
<h1>" BKMARK_TITLE "</h1>\n\
<form method=post action=\"file:///$LIB/" W3MBOOKMARK_CMDNAME "\">\n\
<input type=hidden name=mode value=register>\n\
<input type=hidden name=bmark value=\"%s\">\n\
<input type=hidden name=cookie value=\"%s\">\n\
<table cellpadding=0>\n";

static auto bkmark_src2 =
    "<tr><td>New&nbsp;Section:<td><input type=text name=newsection size=60>\n\
<tr><td>URL:<td><input type=text name=url value=\"%s\" size=60>\n\
<tr><td>Title:<td><input type=text name=title value=\"%s\" size=60>\n\
<tr><td><input type=submit value=\"" BKMARK_ADD "\">\n\
</table>\n\
</form>\n\
</body>\n\
</html>\n";

#undef false
#define false 0
#undef true
#define true 1

static const char end_section[] =
    "<!--End of section (do not delete this comment)-->\n";

static const char *Local_cookie = NULL;

void print_bookmark_panel(const char *bmark, const char *url, const char *title, const char *charset)
{
    Str tmp, tmp2;
    FILE *f;

    if (charset == NULL)
    {
        printf("Content-Type: text/html\n\n");
    }
    else
    {
        printf("Content-Type: text/html; charset=%s\n\n", charset);
    }
    printf(bkmark_src1, html_quote(bmark), html_quote(Local_cookie));
    if ((f = fopen(bmark, "r")) != NULL)
    {
        printf("<tr><td>Section:<td><select name=\"section\">\n");
        while (tmp = Strfgets(f), tmp->Size() > 0)
        {
            StripLeft(tmp);
            if (tmp->ICaseCmp("<h2>", 4) == 0)
            {
                auto p = tmp->c_str() + 4;
                tmp2 = Strnew();
                while (*p && *p != '<')
                    tmp2->Push(*p++);
                printf("<option value=\"%s\">%s\n",
                       tmp2->c_str(),
                       tmp2->c_str());
            }
        }
        printf("</select>\n");
    }
    printf(bkmark_src2, html_quote(url), html_quote(title));
}

/* create new bookmark */
static int
create_new_bookmark(const char *bmark, const char *section, const char *title, const char *url,
                    const char *mode)
{
    FILE *f;
    f = fopen(bmark, mode);
    if (f == NULL)
    {
        printf("\nCan't open bookmark %s\n", bmark);
        return false;
    }
    else
    {
        fprintf(f, "<html><head><title>Bookmarks</title></head>\n");
        fprintf(f, "<body>\n<h1>Bookmarks</h1>\n");
        fprintf(f, "<h2>%s</h2>\n<ul>\n", section);
        fprintf(f, "<li><a href=\"%s\">%s</a>\n", url, title);
        fprintf(f, end_section);
        fprintf(f, "</ul>\n</body>\n</html>\n");
        fclose(f);
    }
    return true;
}

int insert_bookmark(char *bmark, tcb::span<parsed_tagarg> data)
{
    const char *url, *title, *section;
    FILE *f;
    TextList *tl = newTextList();
    int section_found = 0;
    int bmark_added = 0;
    Str tmp, section_tmp;

    url = tag_get_value(data, "url");
    title = tag_get_value(data, "title");
    section = tag_get_value(data, "newsection");
    if (section == NULL || *section == '\0')
        section = tag_get_value(data, "section");
    if (section == NULL || *section == '\0')
        section = DEFAULT_SECTION;

    if (url == NULL || *url == '\0' || title == NULL || *title == '\0')
    {
        /* Bookmark not added */
        return false;
    }
    url = html_quote(url);
    title = html_quote(title);
    section = html_quote(section);

    f = fopen(bmark, "r");
    if (f == NULL)
        return create_new_bookmark(bmark, section, title, url, "w");

    section_tmp = Sprintf("<h2>%s</h2>\n", section);
    for (;;)
    {
        tmp = Strfgets(f);
        if (tmp->Size() == 0)
            break;
        if (tmp->ICaseCmp(section_tmp) == 0)
            section_found = 1;
        if (section_found && !bmark_added)
        {
            StripLeft(tmp);
            if (tmp->Cmp(end_section) == 0)
            {
                pushText(tl,
                         Sprintf("<li><a href=\"%s\">%s</a>\n", url,
                                 title)
                             ->c_str());
                bmark_added = 1;
            }
        }
        if (!bmark_added && tmp->ICaseCmp("</body>\n") == 0)
        {
            pushText(tl, Sprintf("<h2>%s</h2>\n<ul>\n", section)->c_str());
            pushText(tl,
                     Sprintf("<li><a href=\"%s\">%s</a>\n", url, title)->c_str());
            pushText(tl, end_section);
            pushText(tl, "</ul>\n");
            bmark_added = 1;
        }
        pushText(tl, tmp->c_str());
    }
    fclose(f);
    if (!bmark_added)
    {
        /* Bookmark not added; perhaps the bookmark file is ill-formed */
        /* In this case, a new bookmark is appeneded after the bookmark file */
        return create_new_bookmark(bmark, section, title, url, "a");
    }
    f = fopen(bmark, "w");
    while (tl->nitem)
    {
        fputs(popText(tl), f);
    }
    fclose(f);
    return true;
}

// extern "C" char *getenv(const char *);

int main(int argc, char *argv[], char **envp)
{
    char *p;
    int length;
    Str qs = NULL;
    std::vector<parsed_tagarg> cgiarg;
    char *bmark;
    const char *mode;
    const char *url;
    const char *title;
    const char *charset;
    const char *sent_cookie;

    GC_INIT();
    p = getenv("REQUEST_METHOD");
    if (p == NULL || strcasecmp(p, "post"))
        goto request_err;
    p = getenv("CONTENT_LENGTH");
    if (p == NULL || (length = atoi(p)) <= 0)
        goto request_err;

    qs = Strfgets(stdin);
    StripRight(qs);
    if (qs->Size() != length)
        goto request_err;
    cgiarg = cgistr2tagarg(qs->c_str());

    p = getenv("LOCAL_COOKIE_FILE");
    if (p)
    {
        FILE *f = fopen(p, "r");
        if (f)
        {
            Local_cookie = Strfgets(f)->c_str();
            fclose(f);
        }
    }
    sent_cookie = tag_get_value(cgiarg, "cookie");
    if (sent_cookie == NULL || Local_cookie == NULL ||
        strcmp(sent_cookie, Local_cookie) != 0)
    {
        /* local cookie doesn't match: It may be an illegal invocation */
        printf("Content-Type: text/plain\n\n");
        printf("Local cookie doesn't match: It may be an illegal invocation\n");
        exit(1);
    }

    mode = tag_get_value(cgiarg, "mode");
    bmark = expandPath(tag_get_value(cgiarg, "bmark"));
    url = tag_get_value(cgiarg, "url");
    title = tag_get_value(cgiarg, "title");
    charset = tag_get_value(cgiarg, "charset");
    if (bmark == NULL || url == NULL)
        goto request_err;
    if (mode && !strcmp(mode, "panel"))
    {
        if (title == NULL)
            title = "";
        print_bookmark_panel(bmark, url, title, charset);
    }
    else if (mode && !strcmp(mode, "register"))
    {
        printf("Content-Type: text/plain\n");
        if (insert_bookmark(bmark, cgiarg))
        {
            printf("w3m-control: BACK\n");
            printf("w3m-control: BACK\n");
        }
        printf("\n");
    }
    return 0;

request_err:
    printf("Content-Type: text/plain\n\n");
    printf("Incomplete Request: %s\n", qs ? qs->c_str() : "(null)");
    exit(1);
}
