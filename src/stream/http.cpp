#include "fm.h"
#include "stream/http.h"
#include "stream/url.h"
#include "stream/loader.h"
#include "stream/cookie.h"
#include "stream/istream.h"
#include "html/form.h"
#include "textlist.h"
#include "w3m.h"
#include "indep.h"

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
otherinfo(const URL &target, const URL *current, HttpReferrerPolicy referer)
{
    if (target.scheme != SCM_HTTP && target.scheme != SCM_HTTPS)
    {
        // IS NOT HTTP
        assert(false);
        return nullptr;
    }

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

    if (target.host.size())
    {
        s->Push("Host: ");
        s->Push(target.host);
        if (target.port != GetScheme(target.scheme)->port)
            s->Push(Sprintf(":%d", target.port));
        s->Push("\r\n");
    }
    if (target.is_nocache || NoCache)
    {
        s->Push("Pragma: no-cache\r\n");
        s->Push("Cache-control: no-cache\r\n");
    }

    if (!NoSendReferer)
    {
        if (referer == HttpReferrerPolicy::StrictOriginWhenCrossOrigin)
        {
            if (current && current->scheme == target.scheme)
            {
                // strict(same scheme)
                s->Push("Referer: ");
                if (target.HasSameOrigin(*current))
                {
                    s->Push(current->ToReferer());
                }
                else
                {
                    s->Push(current->ToRefererOrigin());
                }
                s->Push("\r\n");
            }
        }
        else
        {
            /* Don't send Referer: if https:// -> http:// */
        }
    }

    return s->ptr;
}

Str HttpRequest::ToStr(const URL &url, const URL *current, const TextList *extra) const
{
    TextListItem *i;
    int seen_www_auth = 0;
    Str cookie;

    auto tmp = Strnew(this->Method());
    tmp->Push(" ");
    tmp->Push(this->URI(url));
    tmp->Push(" HTTP/1.0\r\n");
    tmp->Push(otherinfo(url, current, this->referer));
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

///
/// HttpResponse
///
std::shared_ptr<HttpResponse> HttpResponse::Read(const std::shared_ptr<InputStream> &stream)
{
    auto res = std::make_shared<HttpResponse>();

    while (!stream->eos())
    {
        auto line = stream->mygets();
        if (res->PushIsEndHeader(line->ptr))
        {
            break;
        }
    }

    return res;
}

bool HttpResponse::PushIsEndHeader(std::string_view line)
{
    if (line.ends_with("\r\n"))
    {
        line.remove_suffix(2);
    }

    if (line.empty())
    {
        // empty line finish HTTP response header
        return true;
    }

    if (lines.empty())
    {
        lines.push_back(std::string(line));
        // first line
        // HTTP/1.1 200 OK
        if (!line.starts_with("HTTP/"))
        {
            // invalid http response. abort
            return true;
        }

        if (line[6] != '.')
        {
            return true;
        }

        version_major = line[5] - '0';
        if (version_major != 1)
        {
            // unknown http version
            return true;
        }
        version_minor = line[7] - '0';
        if (version_minor != 0 && version_minor != 1)
        {
            // unknown http version
            return true;
        }

        status_code = (HttpResponseStatusCode)atoi(line.data() + 9);
    }
    else
    {
        lines.push_back(std::string(line));
        // headers. ex.
        // Content-Type: text/html
        // KEY: VALUE
    }

    // header is continuous
    return false;
}
