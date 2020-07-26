#include "fm.h"
#include "http_request.h"
#include "textlist.h"
#include "url.h"
#include "cookie.h"
#include "form.h"

Str HRequest::Method() const
{
    switch (command)
    {
    case HR_COMMAND_CONNECT:
        return Strnew("CONNECT");
    case HR_COMMAND_POST:
        return Strnew("POST");
        break;
    case HR_COMMAND_HEAD:
        return Strnew("HEAD");
        break;
    case HR_COMMAND_GET:
    default:
        return Strnew("GET");
    }
    return NULL;
}

Str HRequest::URI(ParsedURL *pu) const
{
    Str tmp = Strnew();
    if (command == HR_COMMAND_CONNECT)
    {
        tmp->Push(pu->host);
        tmp->Push(Sprintf(":%d", pu->port));
    }
    else if (flag & HR_FLAG_LOCAL)
    {
        tmp->Push(pu->file);
        if (pu->query)
        {
            tmp->Push('?');
            tmp->Push(pu->query);
        }
    }
    else
    {
        char *save_label = pu->label;
        pu->label = NULL;
        tmp->Push(parsedURL2Str(pu, true));
        pu->label = save_label;
    }
    return tmp;
}

Str HTTPrequest(ParsedURL *pu, const ParsedURL *current, HRequest *hr, TextList *extra)
{
    TextListItem *i;
    int seen_www_auth = 0;
    Str cookie;

    auto tmp = hr->Method();
    tmp->Push(" ");
    tmp->Push(hr->URI(pu)->ptr);
    tmp->Push(" HTTP/1.0\r\n");
    if (hr->referer == NO_REFERER)
        tmp->Push(otherinfo(pu, NULL, NULL));
    else
        tmp->Push(otherinfo(pu, current, hr->referer));
    if (extra != NULL)
        for (i = extra->first; i != NULL; i = i->next)
        {
            if (strncasecmp(i->ptr, "Authorization:",
                            sizeof("Authorization:") - 1) == 0)
            {
                seen_www_auth = 1;
#ifdef USE_SSL
                if (hr->command == HR_COMMAND_CONNECT)
                    continue;
#endif
            }
            if (strncasecmp(i->ptr, "Proxy-Authorization:",
                            sizeof("Proxy-Authorization:") - 1) == 0)
            {
#ifdef USE_SSL
                if (pu->scheme == SCM_HTTPS && hr->command != HR_COMMAND_CONNECT)
                    continue;
#endif
            }
            tmp->Push(i->ptr);
        }

#ifdef USE_COOKIE
    if (hr->command != HR_COMMAND_CONNECT &&
        use_cookie && (cookie = find_cookie(pu)))
    {
        tmp->Push("Cookie: ");
        tmp->Push(cookie);
        tmp->Push("\r\n");
        /* [DRAFT 12] s. 10.1 */
        if (cookie->ptr[0] != '$')
            tmp->Push("Cookie2: $Version=\"1\"\r\n");
    }
#endif /* USE_COOKIE */
    if (hr->command == HR_COMMAND_POST)
    {
        if (hr->request->enctype == FORM_ENCTYPE_MULTIPART)
        {
            tmp->Push("Content-type: multipart/form-data; boundary=");
            tmp->Push(hr->request->boundary);
            tmp->Push("\r\n");
            tmp->Push(
                Sprintf("Content-length: %ld\r\n", hr->request->length));
            tmp->Push("\r\n");
        }
        else
        {
            if (!override_content_type)
            {
                tmp->Push(
                    "Content-type: application/x-www-form-urlencoded\r\n");
            }
            tmp->Push(
                Sprintf("Content-length: %ld\r\n", hr->request->length));
            if (header_string)
                tmp->Push(header_string);
            tmp->Push("\r\n");
            tmp->Push(hr->request->body, hr->request->length);
            tmp->Push("\r\n");
        }
    }
    else
    {
        if (header_string)
            tmp->Push(header_string);
        tmp->Push("\r\n");
    }
#ifdef DEBUG
    fprintf(stderr, "HTTPrequest: [ %s ]\n\n", tmp->ptr);
#endif /* DEBUG */
    return tmp;
}
