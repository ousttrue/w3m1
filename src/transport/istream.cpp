
#include "fm.h"
#include "indep.h"
#include "gc_helper.h"
#include "myctype.h"
#include "transport/istream.h"
#include "file.h"

#include "html/html.h"
#include "mime/mimeencoding.h"
#include "frontend/display.h"
#include "frontend/terms.h"
#include <signal.h>
#include <openssl/x509v3.h>

#ifdef __MINGW32_VERSION
#include <winsock.h>
#endif

#define uchar unsigned char

#define STREAM_BUF_SIZE 8192
#define SSL_BUF_SIZE 1536

#define MUST_BE_UPDATED(bs) ((bs)->stream.cur == (bs)->stream.next)

#define POP_CHAR(bs) ((bs)->iseos ? '\0' : (bs)->stream.buf[(bs)->stream.cur++])

//
// BaseStream
//
BaseStream::~BaseStream()
{
#ifdef __MINGW32_VERSION
    closesocket(*(int *)handle);
#else
    close(*(int *)handle);
#endif
}

int BaseStream::ReadFunc(unsigned char *buf, int len)
{
#ifdef __MINGW32_VERSION
    return recv(*(int *)handle, buf, len, 0);
#else
    return read(*(int *)handle, buf, len);
#endif
}

int BaseStream::FD() const
{
    return *(int *)handle;
}

//
// FileStream
//
FileStream::~FileStream()
{
    if(iseos)
    {
        // TODO: ?
        return;
    }
    MySignalHandler prevtrap = NULL;
    prevtrap = mySignal(SIGINT, SIG_IGN);
    handle->close(handle->f);
    mySignal(SIGINT, prevtrap);

}

int FileStream::ReadFunc(unsigned char *buffer, int size)
{
    return fread(buffer, 1, size, handle->f);
}

int FileStream::FD() const
{
    return fileno(handle->f);
}

//
// StrStream
//
StrStream::~StrStream()
{
}

int StrStream::ReadFunc(unsigned char *buf, int len)
{
    return 0;
}

//
// SSLStream
//
SSLStream::~SSLStream()
{
    close(handle->sock);
    if (handle->ssl)
        SSL_free(handle->ssl);
}

int SSLStream::ReadFunc(unsigned char *buf, int len)
{
    int status;
    if (handle->ssl)
    {
#ifdef USE_SSL_VERIFY
        for (;;)
        {
            status = SSL_read(handle->ssl, buf, len);
            if (status > 0)
                break;
            switch (SSL_get_error(handle->ssl, status))
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
        status = SSL_read(handle->ssl, buf, len);
#endif /* !defined(USE_SSL_VERIFY) */
    }
    else
        status = read(handle->sock, buf, len);
    return status;
}

int SSLStream::FD() const
{
    return handle->sock;
}

//
// EncodedStrStream
//
EncodedStrStream::~EncodedStrStream()
{
}

int EncodedStrStream::ReadFunc(unsigned char *buf, int len)
{
    if (handle->s == NULL || handle->pos == handle->s->Size())
    {
        char *p;
        handle->s = StrmyISgets(handle->is);
        if (handle->s->Size() == 0)
            return 0;
        cleanup_line(handle->s, PAGER_MODE);
        if (handle->encoding == ENC_BASE64)
            StripRight(handle->s);
        else if (handle->encoding == ENC_UUENCODE)
        {
            if (!strncmp(handle->s->ptr, "begin", 5))
                handle->s = StrmyISgets(handle->is);
            StripRight(handle->s);
        }
        p = handle->s->ptr;
        if (handle->encoding == ENC_QUOTE)
            handle->s = decodeQP(&p);
        else if (handle->encoding == ENC_BASE64)
            handle->s = decodeB(&p);
        else if (handle->encoding == ENC_UUENCODE)
            handle->s = decodeU(&p);
        handle->pos = 0;
    }

    if (len > handle->s->Size() - handle->pos)
        len = handle->s->Size() - handle->pos;

    bcopy(&handle->s->ptr[handle->pos], buf, len);
    handle->pos += len;
    return len;
}

int EncodedStrStream::FD() const
{
    return handle->is->FD();
}

static void
do_update(InputStreamPtr base)
{
    int len;
    base->stream.cur = base->stream.next = 0;
    len = base->ReadFunc(base->stream.buf, base->stream.size);
    if (len <= 0)
        base->iseos = TRUE;
    else
        base->stream.next += len;
}

static int
buffer_read(StreamBuffer *sb, char *obuf, int count)
{
    int len = sb->next - sb->cur;
    if (len > 0)
    {
        if (len > count)
            len = count;
        bcopy((const void *)&sb->buf[sb->cur], obuf, len);
        sb->cur += len;
    }
    return len;
}

static void
init_buffer(InputStreamPtr base, char *buf, int bufsize)
{
    auto sb = &base->stream;
    sb->size = bufsize;
    sb->cur = 0;
    if (buf)
    {
        sb->buf = (uchar *)buf;
        sb->next = bufsize;
    }
    else
    {
        sb->buf = NewAtom_N(uchar, bufsize);
        sb->next = 0;
    }
    base->iseos = FALSE;
}

static void
init_base_stream(InputStreamPtr base, int bufsize)
{
    init_buffer(base, NULL, bufsize);
}

static void
init_str_stream(InputStreamPtr base, Str s)
{
    init_buffer(base, s->ptr, s->Size());
}

InputStreamPtr
newInputStream(int des)
{
    if (des < 0)
        return NULL;

    auto stream = std::shared_ptr<BaseStream>(new BaseStream());

    init_base_stream(stream, STREAM_BUF_SIZE);
    stream->handle = New(int);
    *(int *)stream->handle = des;
    return stream;
}

InputStreamPtr
newFileStream(FILE *f, FileStreamCloseFunc closep)
{
    if (f == NULL)
        return NULL;

    auto stream = std::shared_ptr<FileStream>(new FileStream());
    init_base_stream(stream, STREAM_BUF_SIZE);
    stream->handle = New(struct filestream_handle);
    stream->handle->f = f;
    if (closep)
        stream->handle->close = closep;
    else
        stream->handle->close = (FileStreamCloseFunc)fclose;
    return stream;
}

InputStreamPtr
newStrStream(Str s)
{
    if (s == NULL)
        return NULL;

    auto stream = std::make_shared<StrStream>();
    init_str_stream(stream, s);
    stream->handle = s;
    return stream;
}

InputStreamPtr
newSSLStream(SSL *ssl, int sock)
{
    if (sock < 0)
        return NULL;

    auto stream = std::make_shared<SSLStream>();
    init_base_stream(stream, SSL_BUF_SIZE);
    stream->handle = New(struct ssl_handle);
    stream->handle->ssl = ssl;
    stream->handle->sock = sock;
    return stream;
}

InputStreamPtr
newEncodedStream(InputStreamPtr is, char encoding)
{
    if (is == NULL || (encoding != ENC_QUOTE && encoding != ENC_BASE64 &&
                       encoding != ENC_UUENCODE))
        return is;

    auto stream = std::make_shared<EncodedStrStream>();
    init_base_stream(stream, STREAM_BUF_SIZE);
    stream->handle = New(struct ens_handle);
    stream->handle->is = is;
    stream->handle->pos = 0;
    stream->handle->encoding = encoding;
    stream->handle->s = NULL;
    return stream;
}

// int ISclose(InputStreamPtr stream)
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

int ISgetc(InputStreamPtr stream)
{
    if (stream == NULL)
        return '\0';
    if (!stream->iseos && MUST_BE_UPDATED(stream))
        do_update(stream);
    return POP_CHAR(stream);
}

int ISundogetc(InputStreamPtr stream)
{
    if (stream == NULL)
        return -1;
    auto sb = &stream->stream;
    if (sb->cur > 0)
    {
        sb->cur--;
        return 0;
    }
    return -1;
}

#define MARGIN_STR_SIZE 10
Str StrISgets(InputStreamPtr stream)
{
    Str s = NULL;
    uchar *p;
    int len;

    if (stream == NULL)
        return NULL;

    auto sb = &stream->stream;

    while (!stream->iseos)
    {
        if (MUST_BE_UPDATED(stream))
        {
            do_update(stream);
        }
        else
        {
            if ((p = (unsigned char *)memchr(&sb->buf[sb->cur], '\n', sb->next - sb->cur)))
            {
                len = p - &sb->buf[sb->cur] + 1;
                if (s == NULL)
                    s = Strnew_size(len);
                s->Push((char *)&sb->buf[sb->cur], len);
                sb->cur += len;
                return s;
            }
            else
            {
                if (s == NULL)
                    s = Strnew_size(sb->next - sb->cur + MARGIN_STR_SIZE);
                s->Push((char *)&sb->buf[sb->cur],
                        sb->next - sb->cur);
                sb->cur = sb->next;
            }
        }
    }

    if (s == NULL)
        return Strnew();
    return s;
}

Str StrmyISgets(InputStreamPtr stream)
{
    Str s = NULL;
    int i, len;

    if (stream == NULL)
        return NULL;
    auto sb = &stream->stream;

    while (!stream->iseos)
    {
        if (MUST_BE_UPDATED(stream))
        {
            do_update(stream);
        }
        else
        {
            if (s && s->Back() == '\r')
            {
                if (sb->buf[sb->cur] == '\n')
                    s->Push((char)sb->buf[sb->cur++]);
                return s;
            }
            for (i = sb->cur;
                 i < sb->next && sb->buf[i] != '\n' && sb->buf[i] != '\r';
                 i++)
                ;
            if (i < sb->next)
            {
                len = i - sb->cur + 1;
                if (s == NULL)
                    s = Strnew_size(len + MARGIN_STR_SIZE);
                s->Push((char *)&sb->buf[sb->cur], len);
                sb->cur = i + 1;
                if (sb->buf[i] == '\n')
                    return s;
            }
            else
            {
                if (s == NULL)
                    s = Strnew_size(sb->next - sb->cur + MARGIN_STR_SIZE);
                s->Push((char *)&sb->buf[sb->cur],
                        sb->next - sb->cur);
                sb->cur = sb->next;
            }
        }
    }

    if (s == NULL)
        return Strnew();
    return s;
}

int ISread(InputStreamPtr stream, Str buf, int count)
{
    if (stream == NULL || stream->iseos)
        return 0;

    auto len = buffer_read(&stream->stream, buf->ptr, count);
    auto rest = count - len;
    if (MUST_BE_UPDATED(stream))
    {
        len = stream->ReadFunc((unsigned char *)&buf->ptr[len], rest);
        if (len <= 0)
        {
            stream->iseos = TRUE;
            len = 0;
        }
        rest -= len;
    }
    buf->Truncate(count - rest);
    if (buf->Size() > 0)
        return 1;
    return 0;
}

int ISeos(InputStreamPtr stream)
{
    if (!stream->iseos && MUST_BE_UPDATED(stream))
        do_update(stream);
    return stream->iseos;
}

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
    char *ans;

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
            free_ssl_ctx();
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
                free_ssl_ctx();
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
            free_ssl_ctx();
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
