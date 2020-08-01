#include "fm.h"
#include "http/http_request.h"
#include "textlist.h"
#include "transport/url.h"
#include "http/cookie.h"
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

Str HRequest::URI(const URL &url) const
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

char *
otherinfo(const URL *target, const URL *current, const char *referer)
{
    Str s = Strnew();

    s->Push("User-Agent: ");
    if (UserAgent == NULL || *UserAgent == '\0')
        s->Push(w3m_version);
    else
        s->Push(UserAgent);
    s->Push("\r\n");

    Strcat_m_charp(s, "Accept: ", AcceptMedia, "\r\n", NULL);
    Strcat_m_charp(s, "Accept-Encoding: ", AcceptEncoding, "\r\n", NULL);
    Strcat_m_charp(s, "Accept-Language: ", AcceptLang, "\r\n", NULL);

    if (target->host.size())
    {
        s->Push("Host: ");
        s->Push(target->host);
        if (target->port != GetScheme(target->scheme)->defaultPort)
            s->Push(Sprintf(":%d", target->port));
        s->Push("\r\n");
    }
    if (target->is_nocache || NoCache)
    {
        s->Push("Pragma: no-cache\r\n");
        s->Push("Cache-control: no-cache\r\n");
    }
    if (!NoSendReferer)
    {
        if (current && current->scheme == SCM_HTTPS && target->scheme != SCM_HTTPS)
        {
            /* Don't send Referer: if https:// -> http:// */
        }
        else if (referer == NULL && current && current->scheme != SCM_LOCAL &&
                 (current->scheme != SCM_FTP ||
                  (current->user.empty() && current->pass.empty())))
        {
            // char *p = current->label;
            s->Push("Referer: ");
            //current->label = NULL;
            auto withoutLabel = *current;
            withoutLabel.label.clear();
            s->Push(withoutLabel.ToStr());
            s->Push("\r\n");
        }
        else if (referer != NULL && referer != NO_REFERER)
        {
            char *p = strchr(const_cast<char*>(referer), '#');
            s->Push("Referer: ");
            if (p)
                s->Push(referer, p - referer);
            else
                s->Push(referer);
            s->Push("\r\n");
        }
    }
    return s->ptr;
}

Str HRequest::ToStr(const URL &url, const URL *current, const TextList *extra) const
{
    TextListItem *i;
    int seen_www_auth = 0;
    Str cookie;

    auto tmp = this->Method();
    tmp->Push(" ");
    tmp->Push(this->URI(url));
    tmp->Push(" HTTP/1.0\r\n");
    if (this->referer == NO_REFERER)
        tmp->Push(otherinfo(&url, NULL, ""));
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
