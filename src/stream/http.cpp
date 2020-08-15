#include "sstream"
#include "fm.h"
#include "stream/http.h"
#include "stream/url.h"
#include "stream/loader.h"
#include "stream/cookie.h"
#include "stream/istream.h"
#include "stream/compression.h"
#include "html/html_processor.h"
#include "html/form.h"
#include "frontend/display.h"
#include "frontend/terms.h"
#include "textlist.h"
#include "w3m.h"
#include "indep.h"
#include "file.h"
#include "mime/mimetypes.h"
#include "mime/mailcap.h"
#include "myctype.h"

void _Push(const HttpRequestPtr &request, std::stringstream &ss)
{
}

template <typename... ARGS>
void _Push(const HttpRequestPtr &request, std::stringstream &ss, Str s, ARGS... args)
{
    ss << s->ptr;
    _Push(request, ss, args...);
}

template <typename... ARGS>
void _Push(const HttpRequestPtr &request, std::stringstream &ss, std::string_view s, ARGS... args)
{
    ss << s;
    _Push(request, ss, args...);
}

template <typename T, typename... ARGS>
void _Push(const HttpRequestPtr &request, std::stringstream &ss, const T &t, ARGS... args)
{
    ss << t;
    _Push(request, ss, args...);
}

template <typename... ARGS>
void Push(const HttpRequestPtr &request, ARGS... args)
{
    std::stringstream ss;
    _Push(request, ss, args...);
    request->lines.push_back(ss.str());
}

///
/// HttpRequest
///
std::shared_ptr<HttpRequest> HttpRequest::Create(const URL &url, struct FormList *form)
{
    auto request = std::make_shared<HttpRequest>();
    request->url = url;
    request->form = form;
    if (form && form->method == FORM_METHOD_POST && form->body)
        request->method = HTTP_METHOD_POST;
    if (form && form->method == FORM_METHOD_HEAD)
        request->method = HTTP_METHOD_HEAD;

    // FIRST LINE => GET / HTTP/1.1
    auto host = url.ToStr(true, false);
    std::stringstream ss;
    ss
        << request->Method()
        << " "
        << host->ptr
        << " HTTP/1.0\r\n";
    request->lines.push_back(ss.str());

    // headers
    Push(request, "User-Agent: ", ((UserAgent == NULL || *UserAgent == '\0') ? w3mApp::w3m_version : UserAgent), "\r\n");
    Push(request, "Accept: ", AcceptMedia, "\r\n");
    Push(request, "Accept-Encoding: ", AcceptEncoding, "\r\n");
    Push(request, "Accept-Language: ", AcceptLang, "\r\n");

    if (url.host.size())
    {
        Push(request, "Host: ", url.host, url.NonDefaultPort(), "\r\n");
    }
    if (url.is_nocache || NoCache)
    {
        Push(request, "Pragma: no-cache\r\n");
        Push(request, "Cache-control: no-cache\r\n");
    }

    Str cookie;
    if (request->method != HTTP_METHOD_CONNECT &&
        w3mApp::Instance().use_cookie && (cookie = find_cookie(&url)))
    {
        Push(request, "Cookie: ", cookie->c_str(), "\r\n");
        /* [DRAFT 12] s. 10.1 */
        if (cookie->ptr[0] != '$')
        {
            Push(request, "Cookie2: $Version=\"1\"\r\n");
        }
    }

    if (request->method == HTTP_METHOD_POST)
    {
        if (request->form->enctype == FORM_ENCTYPE_MULTIPART)
        {
            Push(request, "Content-type: multipart/form-data; boundary=", request->form->boundary, "\r\n");
            Push(request, "Content-length: ", request->form->length, "\r\n");
        }
        else
        {
            if (w3mApp::Instance().override_content_type)
            {
                Push(request, "Content-type: application/x-www-form-urlencoded\r\n");
            }
            Push(request, "Content-length: ", request->form->length, "\r\n");
            if (w3mApp::Instance().header_string.size())
            {
                Push(request, w3mApp::Instance().header_string);
            }
        }
    }
    else
    {
        if (w3mApp::Instance().header_string.size())
        {
            Push(request, w3mApp::Instance().header_string);
        }
    }

    return request;
}

// Str HttpRequest::URI(const URL &url, bool isLocal) const
// {
//     Str tmp = Strnew();
//     if (method == HTTP_METHOD_CONNECT)
//     {
//         tmp->Push(url.host);
//         tmp->Push(Sprintf(":%d", url.port));
//     }
//     else if (isLocal)
//     {
//         tmp->Push(url.path);
//         if (url.query.size())
//         {
//             tmp->Push('?');
//             tmp->Push(url.query);
//         }
//     }
//     else
//     {
//         tmp->Push(url.ToStr(true, false));
//     }
//     return tmp;
// }

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
    tmp->Push(url.ToStr(true, false));
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
        if (this->form->enctype == FORM_ENCTYPE_MULTIPART)
        {
            tmp->Push("Content-type: multipart/form-data; boundary=");
            tmp->Push(this->form->boundary);
            tmp->Push("\r\n");
            tmp->Push(
                Sprintf("Content-length: %ld\r\n", this->form->length));
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
                Sprintf("Content-length: %ld\r\n", this->form->length));
            if (w3mApp::Instance().header_string.size())
                tmp->Push(w3mApp::Instance().header_string);
            tmp->Push("\r\n");
            tmp->Push(this->form->body, this->form->length);
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
static std::tuple<std::string_view, std::string_view> split(std::string_view src, char delemeter)
{
    auto pos = src.find(delemeter);
    if (pos == std::string::npos)
    {
        return {};
    }

    auto key = src.substr(0, pos);
    auto value = src.substr(pos + 1);
    while (value.size() && IS_SPACE(value[0]))
    {
        value.remove_prefix(1);
    }

    return {key, value};
}

static bool ieq(std::string_view l, std::string_view r)
{
    if (l.size() != r.size())
    {
        return false;
    }
    auto rr = r.begin();
    for (auto ll = l.begin(); ll != l.end(); ++ll, ++rr)
    {
        if (tolower(*ll) != tolower(*rr))
        {
            return false;
        }
    }
    return true;
}

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
        auto [key, value] = split(line, ':');
        if (ieq(key, "content-transfer-encoding"))
        {
            auto a = 0;
        }
        else if (ieq(key, "content-encoding"))
        {
            for (auto d = compression_decoders; d->type != CMP_NOCOMPRESS; d++)
            {
                for (auto e = d->encodings; *e != NULL; e++)
                {
                    if (ieq(value, *e))
                    {
                        content_encoding = d->type;
                        break;
                    }
                }
                if (content_encoding != CMP_NOCOMPRESS)
                {
                    break;
                }
            }
        }
    }

    // header is continuous
    return false;
}

// if (strncasecmp(lineBuf2->ptr, "content-transfer-encoding:", 26) == 0)
// {
//     auto p = lineBuf2->ptr + 26;
//     while (IS_SPACE(*p))
//         p++;
//     if (!strncasecmp(p, "base64", 6))
//         uf->encoding = ENC_BASE64;
//     else if (!strncasecmp(p, "quoted-printable", 16))
//         uf->encoding = ENC_QUOTE;
//     else if (!strncasecmp(p, "uuencode", 8) ||
//              !strncasecmp(p, "x-uuencode", 10))
//         uf->encoding = ENC_UUENCODE;
//     else
//         uf->encoding = ENC_7BIT;
// }
// else if (strncasecmp(lineBuf2->ptr, "content-encoding:", 17) == 0)
// {
//     struct compression_decoder *d;
//     auto p = lineBuf2->ptr + 17;
//     while (IS_SPACE(*p))
//         p++;
//     uf->compression = CMP_NOCOMPRESS;
//     for (d = compression_decoders; d->type != CMP_NOCOMPRESS; d++)
//     {
//         const char **e;
//         for (e = d->encodings; *e != NULL; e++)
//         {
//             if (strncasecmp(p, *e, strlen(*e)) == 0)
//             {
//                 uf->compression = d->type;
//                 break;
//             }
//         }
//         if (uf->compression != CMP_NOCOMPRESS)
//             break;
//     }
//     uf->content_encoding = uf->compression;
// }

std::string_view HttpResponse::FindHeader(std::string_view key) const
{
    for (auto &l : lines)
    {
        auto [k, v] = split(l, ':');
        if (ieq(k, key))
        {
            return v;
        }
    }

    return {};
}

///
/// HttpClient
///
BufferPtr HttpClient::Request(const URL &url, const URL *base, HttpReferrerPolicy referer, struct FormList *form)
{
    auto request = HttpRequest::Create(url, form);

    if (!NoSendReferer)
    {
        if (referer == HttpReferrerPolicy::StrictOriginWhenCrossOrigin)
        {
            if (base && base->scheme == url.scheme)
            {
                // strict(same scheme)
                if (url.HasSameOrigin(*base))
                {
                    Push(request, "Referer: ", base->ToReferer(), "\r\n");
                }
                else
                {
                    Push(request, "Referer: ", base->ToRefererOrigin(), "\r\n");
                }
            }
        }
        else
        {
            /* Don't send Referer: if https:// -> http:// */
        }
    }

    auto f = URLFile::OpenHttpAndSendRest(request);

    show_message(Strnew_m_charp(url.host, " contacted. Waiting for reply...")->ptr);

    auto response = HttpResponse::Read(f->stream);
    exchanges.push_back({request, response});

    if (response->HasRedirectionStatus())
    {
        auto location = response->FindHeader("Location");
        if (location.empty())
        {
            return nullptr;
        }

        // redirect
        return Request(URL::Parse(location), &url, referer, nullptr);
    }

    //
    // load buffer
    //
    auto t = response->FindHeader("content-type");
    if (t.size())
    {
        auto [tt, charset] = split(t, ';');
        if (tt.size())
        {
            t = tt;
        }
        if (charset.size())
        {
            auto [_, cs] = split(charset, '=');
            if (cs.size())
            {
                content_charset = wc_guess_charset(cs.data(), WC_CES_NONE);
            }
        }
    }
    if (t.empty())
    {
        t = "text/plain";
    }
    // if (t == NULL && url.path.size())
    // {
    //     if (!((http_response_code >= 400 && http_response_code <= 407) ||
    //           (http_response_code >= 500 && http_response_code <= 505)))
    //         t = guessContentType(url.path);
    // }

    // /* XXX: can we use guess_type to give the type to loadHTMLstream
    //      *      to support default utf8 encoding for XHTML here? */
    // f->guess_type = t;

    // // if (real_type == NULL)
    // //     real_type = t;

    // *GetCurBaseUrl() = pu;

    // current_content_length = 0;
    // if ((p = checkHeader(t_buf, "Content-Length:")) != NULL)
    //     current_content_length = strtoclen(p);
    // if (do_download)
    // {
    //     /* download only */
    //     char *file;
    //     // TRAP_OFF;
    //     if (DecodeCTE && f->stream->type() != IST_ENCODED)
    //         f->stream = newEncodedStream(f->stream, f->encoding);
    //     if (url.scheme == SCM_LOCAL)
    //     {
    //         struct stat st;
    //         if (PreserveTimestamp && !stat(url.real_file.c_str(), &st))
    //             f->modtime = st.st_mtime;
    //         file = conv_from_system(guess_save_name(NULL, url.real_file.c_str()));
    //     }
    //     else
    //         file = guess_save_name(t_buf, url.path);

    //     f->DoFileSave(file, current_content_length);

    //     return nullptr;
    // }

    auto t_buf = newBuffer(INIT_BUFFER_WIDTH());
    if ((f->content_encoding != CMP_NOCOMPRESS) && AutoUncompress && !(w3mApp::Instance().w3m_dump & DUMP_EXTRA))
    {
        // TODO:
        // url.real_file = uncompress_stream(&f, true);
    }
    else if (response->content_encoding != CMP_NOCOMPRESS)
    {
        if (!(w3mApp::Instance().w3m_dump & DUMP_SOURCE) &&
            (w3mApp::Instance().w3m_dump & ~DUMP_FRAME || is_text_type(t) || searchExtViewer(t)))
        {
            // if (t_buf == NULL)
            t_buf->sourcefile = uncompress_stream(f, true);
            uncompressed_file_type(url.path.c_str(), &f->ext);
        }
        else
        {
            t = compress_application_type(f->compression);
            f->compression = CMP_NOCOMPRESS;
        }
    }

    if (image_source)
    {
        BufferPtr b = NULL;
        if (f->stream->type() != IST_ENCODED)
            f->stream = newEncodedStream(f->stream, f->encoding);
        if (save2tmp(f, image_source) == 0)
        {
            b = newBuffer(INIT_BUFFER_WIDTH());
            b->sourcefile = image_source;
            b->real_type = t;
        }
        // f->Close();
        // TRAP_OFF;
        return b;
    }

    auto proc = loadBuffer;
    if (is_html_type(t))
        proc = loadHTMLBuffer;
    else if (is_plain_text_type(t))
        proc = loadBuffer;
    else if (w3mApp::Instance().activeImage && w3mApp::Instance().displayImage && !useExtImageViewer &&
             !(w3mApp::Instance().w3m_dump & ~DUMP_FRAME) && t.starts_with("image/"))
        proc = loadImageBuffer;
    else if (w3mApp::Instance().w3m_backend)
        ;
    // else if (!(w3mApp::Instance().w3m_dump & ~DUMP_FRAME) || is_dump_text_type(t))
    // {
    //     BufferPtr b = NULL;
    //     if (!do_download && doExternal(f,
    //                                    url.real_file.size() ? const_cast<char *>(url.real_file.c_str()) : const_cast<char *>(url.path.c_str()),
    //                                    t, &b, t_buf))
    //     {
    //         if (b)
    //         {
    //             b->real_scheme = f->scheme;
    //             b->real_type = t;
    //             if (b->currentURL.host.empty() && b->currentURL.path.empty())
    //                 b->currentURL = url;
    //         }
    //         // f.Close();
    //         // TRAP_OFF;
    //         return b;
    //     }
    //     else
    //     {
    //         // TRAP_OFF;
    //         if (url.scheme == SCM_LOCAL)
    //         {
    //             // f.Close();
    //             // _doFileCopy(const_cast<char *>(url.real_file.c_str()),
    //             //             conv_from_system(guess_save_name(NULL, url.real_file)), TRUE);
    //         }
    //         else
    //         {
    //             if (DecodeCTE && f->stream->type() != IST_ENCODED)
    //                 f->stream = newEncodedStream(f->stream, f->encoding);
    //             f->DoFileSave(guess_save_name(t_buf, url.path), response->content_length);
    //         }
    //         return nullptr;
    //     }
    // }
    // else if (w3mApp::Instance().w3m_dump & DUMP_FRAME)
    //     return NULL;

    // if (flag & RG_FRAME)
    // {
    //     if (t_buf == NULL)
    //         t_buf = newBuffer(INIT_BUFFER_WIDTH());
    //     t_buf->bufferprop |= BP_FRAME;
    // }

    if (t_buf && f->ssl_certificate)
    {
        t_buf->ssl_certificate = f->ssl_certificate;
    }

    // frame_source = flag & RG_FRAME_SRC;
    auto success = loadSomething(f,
                                 url.real_file.size() ? const_cast<char *>(url.real_file.c_str()) : const_cast<char *>(url.path.c_str()),
                                 proc,
                                 t_buf);
    assert(success);
    auto b = t_buf;
    f->stream = nullptr;
    // f.Close();
    frame_source = 0;
    if (success)
    {
        b->real_scheme = f->scheme;
        b->real_type = t;
        if (b->currentURL.host.empty() && b->currentURL.path.empty())
            b->currentURL = url;
        if (is_html_type(t))
            b->type = "text/html";
        else if (w3mApp::Instance().w3m_backend)
        {
            Str s = Strnew(t);
            b->type = s->ptr;
        }
        else if (proc == loadImageBuffer)
            b->type = "text/html";
        else
            b->type = "text/plain";
        if (url.fragment.size())
        {
            if (proc == loadHTMLBuffer)
            {
                auto a = searchURLLabel(b, const_cast<char *>(url.fragment.c_str()));
                if (a != NULL)
                {
                    b->Goto(a->start, label_topline);
                }
            }
            else
            { /* plain text */
                int l = atoi(url.fragment.c_str());
                b->GotoRealLine(l);
                b->pos = 0;
                b->ArrangeCursor();
            }
        }
    }
    if (w3mApp::Instance().header_string.size())
        w3mApp::Instance().header_string.clear();
    preFormUpdateBuffer(b);
    return b;
}
