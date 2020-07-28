#include "fm.h"
#include "http_request.h"
#include "textlist.h"
#include "url.h"
#include "cookie.h"
#include "html/form.h"

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

Str HRequest::URI(const ParsedURL &url) const
{
    Str tmp = Strnew();
    if (command == HR_COMMAND_CONNECT)
    {
        tmp->Push(url.host);
        tmp->Push(Sprintf(":%d", url.port));
    }
    else if (flag & HR_FLAG_LOCAL)
    {
        tmp->Push(url.file);
        if (url.query.size())
        {
            tmp->Push('?');
            tmp->Push(url.query);
        }
    }
    else
    {
        tmp->Push(url.ToStr(true, false));
    }
    return tmp;
}

Str HRequest::ToStr(const ParsedURL &url, const ParsedURL *current, const TextList *extra) const
{
    TextListItem *i;
    int seen_www_auth = 0;
    Str cookie;

    auto tmp = this->Method();
    tmp->Push(" ");
    tmp->Push(this->URI(url));
    tmp->Push(" HTTP/1.0\r\n");
    if (this->referer == NO_REFERER)
        tmp->Push(otherinfo(&url, NULL, NULL));
    else
        tmp->Push(otherinfo(&url, current, this->referer));
    if (extra != NULL)
        for (i = extra->first; i != NULL; i = i->next)
        {
            if (strncasecmp(i->ptr, "Authorization:",
                            sizeof("Authorization:") - 1) == 0)
            {
                seen_www_auth = 1;
                if (this->command == HR_COMMAND_CONNECT)
                    continue;
            }
            if (strncasecmp(i->ptr, "Proxy-Authorization:",
                            sizeof("Proxy-Authorization:") - 1) == 0)
            {
                if (url.scheme == SCM_HTTPS && this->command != HR_COMMAND_CONNECT)
                    continue;
            }
            tmp->Push(i->ptr);
        }

    if (this->command != HR_COMMAND_CONNECT &&
        use_cookie && (cookie = find_cookie(&url)))
    {
        tmp->Push("Cookie: ");
        tmp->Push(cookie);
        tmp->Push("\r\n");
        /* [DRAFT 12] s. 10.1 */
        if (cookie->ptr[0] != '$')
            tmp->Push("Cookie2: $Version=\"1\"\r\n");
    }

    if (this->command == HR_COMMAND_POST)
    {
        if (this->request->enctype == FORM_ENCTYPE_MULTIPART)
        {
            tmp->Push("Content-type: multipart/form-data; boundary=");
            tmp->Push(this->request->boundary);
            tmp->Push("\r\n");
            tmp->Push(
                Sprintf("Content-length: %ld\r\n", this->request->length));
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
                Sprintf("Content-length: %ld\r\n", this->request->length));
            if (header_string)
                tmp->Push(header_string);
            tmp->Push("\r\n");
            tmp->Push(this->request->body, this->request->length);
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
