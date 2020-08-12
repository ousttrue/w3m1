#include "fm.h"
#include "http/http_request.h"
#include "textlist.h"
#include "transport/url.h"
#include "transport/loader.h"
#include "http/cookie.h"
#include "html/form.h"
#include "w3m.h"
#include "indep.h"

Str HttpRequest::Method() const
{
    switch (method)
    {
    case HTTP_METHOD_CONNECT:
        return Strnew("CONNECT");
    case HTTP_METHOD_POST:
        return Strnew("POST");
        break;
    case HTTP_METHOD_HEAD:
        return Strnew("HEAD");
        break;
    case HTTP_METHOD_GET:
    default:
        return Strnew("GET");
    }
    return NULL;
}

Str HttpRequest::URI(const URL &url, bool isLocal) const
{
    Str tmp = Strnew();
    if (method == HTTP_METHOD_CONNECT)
    {
        tmp->Push(url.host);
        tmp->Push(Sprintf(":%d", url.port));
    }
    else if (isLocal)
    {
        tmp->Push(url.path);
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
        s->Push(w3mApp::w3m_version);
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
        if (target->port != GetScheme(target->scheme)->port)
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
                  (current->userinfo.empty())))
        {
            // char *p = current->label;
            s->Push("Referer: ");
            //current->label = NULL;
            auto withoutLabel = *current;
            withoutLabel.fragment.clear();
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

Str HttpRequest::ToStr(const URL &url, const URL *current, const TextList *extra) const
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
                if (this->method == HTTP_METHOD_CONNECT)
                    continue;
            }
            if (strncasecmp(i->ptr, "Proxy-Authorization:",
                            sizeof("Proxy-Authorization:") - 1) == 0)
            {
                if (url.scheme == SCM_HTTPS && this->method != HTTP_METHOD_CONNECT)
                    continue;
            }
            tmp->Push(i->ptr);
        }

    if (this->method != HTTP_METHOD_CONNECT &&
        w3mApp::Instance().use_cookie && (cookie = find_cookie(&url)))
    {
        tmp->Push("Cookie: ");
        tmp->Push(cookie);
        tmp->Push("\r\n");
        /* [DRAFT 12] s. 10.1 */
        if (cookie->ptr[0] != '$')
            tmp->Push("Cookie2: $Version=\"1\"\r\n");
    }

    if (this->method == HTTP_METHOD_POST)
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
            if (w3mApp::Instance().override_content_type)
            {
                tmp->Push(
                    "Content-type: application/x-www-form-urlencoded\r\n");
            }
            tmp->Push(
                Sprintf("Content-length: %ld\r\n", this->request->length));
            if (w3mApp::Instance().header_string.size())
                tmp->Push(w3mApp::Instance().header_string);
            tmp->Push("\r\n");
            tmp->Push(this->request->body, this->request->length);
            tmp->Push("\r\n");
        }
    }
    else
    {
        if (w3mApp::Instance().header_string.size())
            tmp->Push(w3mApp::Instance().header_string);
        tmp->Push("\r\n");
    }
#ifdef DEBUG
    fprintf(stderr, "HTTPrequest: [ %s ]\n\n", tmp->ptr);
#endif /* DEBUG */
    return tmp;
}
