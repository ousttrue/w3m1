
#include "fm.h"
#include "indep.h"
#include "myctype.h"
#include "file.h"
#include "stream/istream.h"
#include "stream/open_socket.h"
#include "stream/ssl_socket.h"
#include "html/html.h"
#include "html/form.h"
#include "mime/mimeencoding.h"
#include "frontend/display.h"
#include "frontend/terms.h"
#include <signal.h>
#include <openssl/x509v3.h>

#define STREAM_BUF_SIZE 8192
#define SSL_BUF_SIZE 1536

///
/// InputStream
///
void InputStream::do_update()
{
    auto len = stream.update(std::bind(&InputStream::ReadFunc, this,
                                       std::placeholders::_1, std::placeholders::_2));
    if (len <= 0)
    {
        iseos = TRUE;
    }
}

int InputStream::getc()
{
    if (!iseos && stream.MUST_BE_UPDATED())
        do_update();
    if (iseos)
    {
        return '\0';
    }
    return stream.POP_CHAR();
}

bool InputStream::eos()
{
    if (!iseos && stream.MUST_BE_UPDATED())
        do_update();
    return iseos;
}

#define MARGIN_STR_SIZE 10
Str InputStream::gets()
{
    Str s = Strnew();
    while (!iseos)
    {
        if (stream.MUST_BE_UPDATED())
        {
            do_update();
        }
        else
        {
            if (stream.try_gets(s))
            {
                return s;
            }
        }
    }
    return s;
}

Str InputStream::mygets()
{
    Str s = Strnew();
    while (!iseos)
    {
        if (stream.MUST_BE_UPDATED())
        {
            do_update();
        }
        else
        {
            if (stream.try_mygets(s))
            {
                return s;
            }
        }
    }
    return s;
}

int InputStream::read(unsigned char *buffer, int size)
{
    auto readsize = stream.buffer_read((char *)buffer, size);
    if (stream.MUST_BE_UPDATED())
    {
        auto len = ReadFunc((unsigned char *)&buffer[readsize], size - readsize);
        if (len <= 0)
        {
            iseos = TRUE;
            len = 0;
        }
        readsize += len;
    }
    return readsize;
}

bool InputStream::readto(Str buf, int count)
{
    auto len = stream.buffer_read(buf->ptr, count);
    auto rest = count - len;
    if (stream.MUST_BE_UPDATED())
    {
        len = ReadFunc((unsigned char *)&buf->ptr[len], rest);
        if (len <= 0)
        {
            iseos = TRUE;
            len = 0;
        }
        rest -= len;
    }
    buf->Truncate(count - rest);
    if (buf->Size() > 0)
        return 1;
    return 0;
}

//
// BaseStream
//
InputStreamPtr newInputStream(int des)
{
    if (des < 0)
        return NULL;
    return std::make_shared<BaseStream>(STREAM_BUF_SIZE, des);
}

BaseStream::~BaseStream()
{
#ifdef __MINGW32_VERSION
    closesocket(m_fd);
#else
    close(m_fd);
#endif
}

int BaseStream::ReadFunc(unsigned char *buf, int len)
{
#ifdef __MINGW32_VERSION
    return recv(m_fd, buf, len, 0);
#else
    return ::read(m_fd, buf, len);
#endif
}

int BaseStream::FD() const
{
    return m_fd;
}

//
// FileStream
//
InputStreamPtr newFileStream(FILE *f, const std::function<void(FILE *)> &closep)
{
    if (f == NULL)
        return NULL;
    return std::make_shared<FileStream>(STREAM_BUF_SIZE, f, closep);
}

FileStream::~FileStream()
{
    MySignalHandler prevtrap = NULL;
    prevtrap = mySignal(SIGINT, SIG_IGN);
    if (m_close)
    {
        m_close(m_f);
    }
    else
    {
        fclose(m_f);
    }
    mySignal(SIGINT, prevtrap);
}

int FileStream::ReadFunc(unsigned char *buffer, int size)
{
    auto readsize = fread(buffer, 1, size, m_f);
    m_readsize += readsize;
    return readsize;
}

int FileStream::FD() const
{
    return fileno(m_f);
}

//
// StrStream
//
InputStreamPtr newStrStream(Str s)
{
    if (s == NULL)
        return NULL;
    if (s->Size() == 0)
    {
        // ""
        return nullptr;
    }
    return std::make_shared<StrStream>(STREAM_BUF_SIZE, s);
}

StrStream::~StrStream()
{
}

int StrStream::ReadFunc(unsigned char *buf, int len)
{
    auto strsize = m_str->Size() + 1; // \0 terminate
    if (m_pos + len > strsize)
    {
        len = strsize - m_pos;
    }
    if (len <= 0)
    {
        return 0;
    }

    memcpy(buf, m_str->ptr + m_pos, len);
    m_pos += len;
    return len;
}

//
// SSLStream
//
InputStreamPtr newSSLStream(const std::shared_ptr<SSLSocket> &ssl)
{
    if (!ssl)
        return NULL;
    return std::make_shared<SSLStream>(SSL_BUF_SIZE, ssl);
}

SSLStream::~SSLStream()
{
}

int SSLStream::ReadFunc(unsigned char *buf, int len)
{
    int status;

#ifdef USE_SSL_VERIFY
    for (;;)
    {
        status = m_ssl->Read(buf, len);
        if (status > 0)
            break;
        switch (SSL_get_error((SSL *)m_ssl->Handle(), status))
        {
        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE: /* reads can trigger write errors; see SSL_get_error(3) */
            continue;
        default:
            break;
        }
        break;
    }
#else  /* if !defined(USE_SSL_VERIFY) */
    status = SSL_read(m_ssl, buf, len);
#endif /* !defined(USE_SSL_VERIFY) */

    return status;
}

int SSLStream::FD() const
{
    return m_ssl->Socket();
}

//
// EncodedStrStream
//
InputStreamPtr newEncodedStream(InputStreamPtr is, char encoding)
{
    if (is == NULL || (encoding != ENC_QUOTE && encoding != ENC_BASE64 &&
                       encoding != ENC_UUENCODE))
        return is;

    return std::make_shared<EncodedStrStream>(STREAM_BUF_SIZE, is, encoding);
}

EncodedStrStream::~EncodedStrStream()
{
}

int EncodedStrStream::ReadFunc(unsigned char *buf, int len)
{
    if (m_s == NULL || m_pos == m_s->Size())
    {
        char *p;
        m_s = m_is->mygets();
        if (m_s->Size() == 0)
            return 0;
        cleanup_line(m_s, PAGER_MODE);
        if (m_encoding == ENC_BASE64)
            StripRight(m_s);
        else if (m_encoding == ENC_UUENCODE)
        {
            if (!strncmp(m_s->ptr, "begin", 5))
                m_s = m_is->mygets();
            StripRight(m_s);
        }
        p = m_s->ptr;
        if (m_encoding == ENC_QUOTE)
            m_s = decodeQP(&p);
        else if (m_encoding == ENC_BASE64)
            m_s = decodeB(&p);
        else if (m_encoding == ENC_UUENCODE)
            m_s = decodeU(&p);
        m_pos = 0;
    }

    if (len > m_s->Size() - m_pos)
        len = m_s->Size() - m_pos;

    bcopy(&m_s->ptr[m_pos], buf, len);
    m_pos += len;
    return len;
}

int EncodedStrStream::FD() const
{
    return m_is->FD();
}

///
///
///

// int ISclose(const InputStreamPtr &stream)
// {
//     MySignalHandler prevtrap = NULL;
//     if (stream == NULL || stream->base.close == NULL ||
//         stream->base.type & IST_UNCLOSE)
//         return -1;
//     prevtrap = mySignal(SIGINT, SIG_IGN);
//     stream->base.close(stream->base.handle);
//     mySignal(SIGINT, prevtrap);
//     return 0;
// }

#ifdef USE_SSL
static Str accept_this_site;

void ssl_accept_this_site(char *hostname)
{
    if (hostname)
        accept_this_site = Strnew(hostname);
    else
        accept_this_site = NULL;
}

static int
ssl_match_cert_ident(char *ident, int ilen, char *hostname)
{
    /* RFC2818 3.1.  Server Identity
     * Names may contain the wildcard
     * character * which is considered to match any single domain name
     * component or component fragment. E.g., *.a.com matches foo.a.com but
     * not bar.foo.a.com. f*.com matches foo.com but not bar.com.
     */
    int hlen = strlen(hostname);
    int i, c;

    /* Is this an exact match? */
    if ((ilen == hlen) && strncasecmp(ident, hostname, hlen) == 0)
        return TRUE;

    for (i = 0; i < ilen; i++)
    {
        if (ident[i] == '*' && ident[i + 1] == '.')
        {
            while ((c = *hostname++) != '\0')
                if (c == '.')
                    break;
            i++;
        }
        else
        {
            if (ident[i] != *hostname++)
                return FALSE;
        }
    }
    return *hostname == '\0';
}

static Str
ssl_check_cert_ident(X509 *x, char *hostname)
{
    int i;
    Str ret = NULL;
    int match_ident = FALSE;
    /*
     * All we need to do here is check that the CN matches.
     *
     * From RFC2818 3.1 Server Identity:
     * If a subjectAltName extension of type dNSName is present, that MUST
     * be used as the identity. Otherwise, the (most specific) Common Name
     * field in the Subject field of the certificate MUST be used. Although
     * the use of the Common Name is existing practice, it is deprecated and
     * Certification Authorities are encouraged to use the dNSName instead.
     */
    i = X509_get_ext_by_NID(x, NID_subject_alt_name, -1);
    if (i >= 0)
    {
        X509_EXTENSION *ex;
        STACK_OF(GENERAL_NAME) * alt;

        ex = X509_get_ext(x, i);
        alt = (STACK_OF(GENERAL_NAME) *)X509V3_EXT_d2i(ex);
        if (alt)
        {
            int n;
            GENERAL_NAME *gn;
            Str seen_dnsname = NULL;

            n = sk_GENERAL_NAME_num(alt);
            for (i = 0; i < n; i++)
            {
                gn = sk_GENERAL_NAME_value(alt, i);
                if (gn->type == GEN_DNS)
                {
                    char *sn = (char *)ASN1_STRING_data(gn->d.ia5);
                    int sl = ASN1_STRING_length(gn->d.ia5);

                    if (!seen_dnsname)
                        seen_dnsname = Strnew();
                    /* replace \0 to make full string visible to user */
                    if (sl != strlen(sn))
                    {
                        int i;
                        for (i = 0; i < sl; ++i)
                        {
                            if (!sn[i])
                                sn[i] = '!';
                        }
                    }
                    Strcat_m_charp(seen_dnsname, sn, " ", NULL);
                    if (sl == strlen(sn) /* catch \0 in SAN */
                        && ssl_match_cert_ident(sn, sl, hostname))
                        break;
                }
            }
            auto method = X509V3_EXT_get(ex);
            sk_GENERAL_NAME_free(alt);
            if (i < n) /* Found a match */
                match_ident = TRUE;
            else if (seen_dnsname)
                /* FIXME: gettextize? */
                ret = Sprintf("Bad cert ident from %s: dNSName=%s", hostname,
                              seen_dnsname->ptr);
        }
    }

    if (match_ident == FALSE && ret == NULL)
    {
        X509_NAME *xn;
        char buf[2048];
        int slen;

        xn = X509_get_subject_name(x);

        slen = X509_NAME_get_text_by_NID(xn, NID_commonName, buf, sizeof(buf));
        if (slen == -1)
            /* FIXME: gettextize? */
            ret = Strnew("Unable to get common name from peer cert");
        else if (slen != strlen(buf) || !ssl_match_cert_ident(buf, strlen(buf), hostname))
        {
            /* replace \0 to make full string visible to user */
            if (slen != strlen(buf))
            {
                int i;
                for (i = 0; i < slen; ++i)
                {
                    if (!buf[i])
                        buf[i] = '!';
                }
            }
            /* FIXME: gettextize? */
            ret = Sprintf("Bad cert ident %s from %s", buf, hostname);
        }
        else
            match_ident = TRUE;
    }
    return ret;
}

Str ssl_get_certificate(SSL *ssl, char *hostname)
{
    BIO *bp;
    X509 *x;
    X509_NAME *xn;
    char *p;
    int len;
    Str s;
    char buf[2048];
    Str amsg = NULL;
    Str emsg;
    const char *ans;

    if (ssl == NULL)
        return NULL;
    x = SSL_get_peer_certificate(ssl);
    if (x == NULL)
    {
        if (accept_this_site && strcasecmp(accept_this_site->ptr, hostname) == 0)
            ans = "y";
        else
        {
            /* FIXME: gettextize? */
            emsg = Strnew("No SSL peer certificate: accept? (y/n)");
            ans = inputAnswer(emsg->ptr);
        }
        if (ans && TOLOWER(*ans) == 'y')
            /* FIXME: gettextize? */
            amsg = Strnew("Accept SSL session without any peer certificate");
        else
        {
            /* FIXME: gettextize? */
            char *e = "This SSL session was rejected "
                      "to prevent security violation: no peer certificate";
            disp_err_message(e, FALSE);
            return NULL;
        }
        if (amsg)
            disp_err_message(amsg->ptr, FALSE);
        ssl_accept_this_site(hostname);
        /* FIXME: gettextize? */
        s = amsg ? amsg : Strnew("valid certificate");
        return s;
    }
#ifdef USE_SSL_VERIFY
    /* check the cert chain.
     * The chain length is automatically checked by OpenSSL when we
     * set the verify depth in the ctx.
     */
    if (ssl_verify_server)
    {
        long verr;
        if ((verr = SSL_get_verify_result(ssl)) != X509_V_OK)
        {
            const char *em = X509_verify_cert_error_string(verr);
            if (accept_this_site && strcasecmp(accept_this_site->ptr, hostname) == 0)
                ans = "y";
            else
            {
                /* FIXME: gettextize? */
                emsg = Sprintf("%s: accept? (y/n)", em);
                ans = inputAnswer(emsg->ptr);
            }
            if (ans && TOLOWER(*ans) == 'y')
            {
                /* FIXME: gettextize? */
                amsg = Sprintf("Accept unsecure SSL session: "
                               "unverified: %s",
                               em);
            }
            else
            {
                /* FIXME: gettextize? */
                char *e =
                    Sprintf("This SSL session was rejected: %s", em)->ptr;
                disp_err_message(e, FALSE);
                return NULL;
            }
        }
    }
#endif
    emsg = ssl_check_cert_ident(x, hostname);
    if (emsg != NULL)
    {
        if (accept_this_site && strcasecmp(accept_this_site->ptr, hostname) == 0)
            ans = "y";
        else
        {
            Str ep = emsg->Clone();
            if (ep->Size() > ::COLS - 16)
                ep->Pop(ep->Size() - (::COLS - 16));
            ep->Push(": accept? (y/n)");
            ans = inputAnswer(ep->ptr);
        }
        if (ans && TOLOWER(*ans) == 'y')
        {
            /* FIXME: gettextize? */
            amsg = Strnew("Accept unsecure SSL session:");
            amsg->Push(emsg);
        }
        else
        {
            /* FIXME: gettextize? */
            const char *e = "This SSL session was rejected "
                            "to prevent security violation";
            disp_err_message(e, FALSE);
            return NULL;
        }
    }
    if (amsg)
        disp_err_message(amsg->ptr, FALSE);
    ssl_accept_this_site(hostname);
    /* FIXME: gettextize? */
    s = amsg ? amsg : Strnew("valid certificate");
    s->Push("\n");
    xn = X509_get_subject_name(x);
    if (X509_NAME_get_text_by_NID(xn, NID_commonName, buf, sizeof(buf)) == -1)
        s->Push(" subject=<unknown>");
    else
        Strcat_m_charp(s, " subject=", buf, NULL);
    xn = X509_get_issuer_name(x);
    if (X509_NAME_get_text_by_NID(xn, NID_commonName, buf, sizeof(buf)) == -1)
        s->Push(": issuer=<unknown>");
    else
        Strcat_m_charp(s, ": issuer=", buf, NULL);
    s->Push("\n\n");

    bp = BIO_new(BIO_s_mem());
    X509_print(bp, x);
    len = (int)BIO_ctrl(bp, BIO_CTRL_INFO, 0, (char *)&p);
    s->Push(p, len);
    BIO_free_all(bp);
    X509_free(x);
    return s;
}
#endif

static void write_from_file(int sock, char *file)
{
    FILE *fd;
    int c;
    char buf[1];
    fd = fopen(file, "r");
    if (fd != NULL)
    {
        while ((c = fgetc(fd)) != EOF)
        {
            buf[0] = c;
            write(sock, buf, 1);
        }
        fclose(fd);
    }
}

std::shared_ptr<SSLContext> m_ssl_ctx;

InputStreamPtr OpenHttpAndSendRequest(const std::shared_ptr<HttpRequest> &request)
{
    if (request->url.scheme != SCM_HTTP && request->url.scheme != SCM_HTTPS)
    {
        assert(false);
        return nullptr;
    }

    if (w3mApp::Instance().UseProxy(request->url))
    {
        assert(false);
        return nullptr;
    }

    auto sock = openSocket(request->url);
    if (sock < 0)
    {
        // fail to open socket
        return nullptr;
    }

    if (request->url.scheme == SCM_HTTPS)
    {
        if (!m_ssl_ctx)
        {
            m_ssl_ctx = SSLContext::Create();
        }

        auto ssl = m_ssl_ctx->Open(sock, request->url.host);
        if (!ssl)
        {
            return nullptr;
        }

        // send http request
        for (auto &l : request->lines)
        {
            ssl->Write(l.data(), l.size());
        }
        ssl->Write("\r\n", 2);
        // send post body
        if (request->method == HTTP_METHOD_POST)
        {
            if (request->form->enctype == FORM_ENCTYPE_MULTIPART)
            {
                ssl->WriteFromFile(request->form->body);
            }
            else
            {
                ssl->Write(request->form->body, request->form->length);
            }
        }

        // return stream
        // uf->ssl_certificate = ssl_certificate;
        return newSSLStream(ssl);
    }
    else
    {
        // send http request
        for (auto &l : request->lines)
        {
            write(sock, l.data(), l.size());
        }
        write(sock, "\r\n", 2);
        // send post body
        if (request->method == HTTP_METHOD_POST)
        {
            if (request->form->enctype == FORM_ENCTYPE_MULTIPART)
            {
                write_from_file(sock, request->form->body);
            }
            else
            {
                write(sock, request->form->body, request->form->length);
            }
        }

        // return stream
        return newInputStream(sock);
    }
}

InputStreamPtr StreamFromFile(std::string_view path)
{
    if (path.empty())
    {
        return nullptr;
    }

    struct stat stbuf;
    if (stat(path.data(), &stbuf) == -1)
    {
        return nullptr;
    }
    if (stbuf.st_mode & S_IFMT != S_IFREG)
    {
        return nullptr;
    }

    auto stream = openIS(path.data());
    return stream;

    // if (do_download)
    // {
    //     assert(false);
    //     return stream;
    // }

    // if (use_lessopen && getenv("LESSOPEN"))
    // {
    //     // this->guess_type = guessContentType(path);
    //     // if (this->guess_type.empty())
    //     //     this->guess_type = "text/plain";
    //     // if (is_html_type(this->guess_type))
    //     //     return;
    //     if (auto fp = lessopen_stream(const_cast<char *>(path.data())))
    //     {
    //         // TODO:
    //         // this->Close();
    //         auto stream = newFileStream(fp, pclose);
    //         // this->guess_type = "text/plain";
    //         return stream;
    //     }
    // }

    // check_compression(const_cast<char *>(path.data()), shared_from_this());
    /*
    if (this->compression != CMP_NOCOMPRESS)
    {
        auto [t, ex] = uncompressed_file_type(path);
        if (ex.size())
        {
            ext = ex.data();
        }
        if (t.size())
        {
            this->guess_type = t.data();
        }
        uncompress_stream(shared_from_this(), NULL);
        return;
    }
    */
}
