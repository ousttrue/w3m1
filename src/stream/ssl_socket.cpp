#include "config.h"
#include "fm.h"
#include "indep.h"
#include "ssl_socket.h"
#include "file.h"
#include "frontend/display.h"
#include "frontend/terms.h"
#include <myctype.h>
#include <string_view>
#include <string>
#include <assert.h>

#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>
#ifndef SSLEAY_VERSION_NUMBER
#include <openssl/crypto.h> /* SSLEAY_VERSION_NUMBER may be here */
#endif

#if SSLEAY_VERSION_NUMBER >= 0x00905100
#include <openssl/rand.h>
static void
init_PRNG()
{
    char buffer[256];
    const char *file;
    long l;
    if (RAND_status())
        return;
    if ((file = RAND_file_name(buffer, sizeof(buffer))))
    {
#ifdef USE_EGD
        if (RAND_egd(file) > 0)
            return;
#endif
        RAND_load_file(file, -1);
    }
    if (RAND_status())
        goto seeded;
    srand48((long)time(NULL));
    while (!RAND_status())
    {
        l = lrand48();
        RAND_seed((unsigned char *)&l, sizeof(long));
    }
seeded:
    if (file)
        RAND_write_file(file);
}
#endif /* SSLEAY_VERSION_NUMBER >= 0x00905100 */

static int ssl_match_cert_ident(const char *ident, int ilen, std::string_view _hostname)
{
    /* RFC2818 3.1.  Server Identity
     * Names may contain the wildcard
     * character * which is considered to match any single domain name
     * component or component fragment. E.g., *.a.com matches foo.a.com but
     * not bar.foo.a.com. f*.com matches foo.com but not bar.com.
     */
    // int hlen = strlen(hostname);
    int i, c;

    /* Is this an exact match? */
    if ((ilen == _hostname.size()) && strncasecmp(ident, _hostname.data(), _hostname.size()) == 0)
        return TRUE;

    auto hostname = _hostname.data();
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

static Str ssl_check_cert_ident(X509 *x, std::string_view hostname)
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

///
/// SSL
///
struct SSLSocketImpl
{
    int m_sock;
    SSL *m_ssl;

    SSLSocketImpl(int sock, SSL *ssl)
        : m_sock(sock), m_ssl(ssl)
    {
    }

    ~SSLSocketImpl()
    {
        close(m_sock);
        SSL_free(m_ssl);
    }
};

SSLSocket::SSLSocket(SSLSocketImpl *impl, const char *ssl_certificate)
    : m_impl(impl), m_ssl_certificate(ssl_certificate)
{
}

SSLSocket::~SSLSocket()
{
    delete m_impl;
}

int SSLSocket::Write(const void *buf, int num)
{
    return SSL_write(m_impl->m_ssl, buf, num);
}

void SSLSocket::WriteFromFile(const char *file)
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
            SSL_write(m_impl->m_ssl, buf, 1);
        }
        fclose(fd);
    }
}

int SSLSocket::Socket() const
{
    return m_impl->m_sock;
}

int SSLSocket::Read(unsigned char *p, int size)
{
    return SSL_read(m_impl->m_ssl, p, size);
}

const void *SSLSocket::Handle() const
{
    return m_impl->m_ssl;
}

///
/// Context
///
struct SSLContextImpl
{
    SSL_CTX *m_ctx = NULL;
    std::string accept_this_site;

    SSLContextImpl(SSL_CTX *ctx)
        : m_ctx(ctx)
    {
    }

    ~SSLContextImpl()
    {
        SSL_CTX_free(m_ctx);
    }

    void ssl_accept_this_site(std::string_view hostname)
    {
        accept_this_site = hostname;
    }

    Str ssl_get_certificate(SSL *ssl, std::string_view hostname)
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
            if (accept_this_site == hostname)
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
                if (accept_this_site == hostname)
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
            if (accept_this_site == hostname)
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
};

SSLContext::SSLContext(SSLContextImpl *impl)
    : m_impl(impl)
{
}

SSLContext::~SSLContext()
{
    delete m_impl;
}

std::shared_ptr<SSLContext> SSLContext::Create()
{
#if SSLEAY_VERSION_NUMBER < 0x0800
    auto ssl_ctx = SSL_CTX_new();
    X509_set_default_verify_paths(ssl_ctx->cert);
    auto impl = std::make_shared<SSLContext>(new SSLContextImpl(ssl_ctx));

#else /* SSLEAY_VERSION_NUMBER >= 0x0800 */
    SSLeay_add_ssl_algorithms();
    SSL_load_error_strings();
    auto ssl_ctx = SSL_CTX_new(SSLv23_client_method());
    if (!ssl_ctx)
    {
        return nullptr;
    }
    auto impl = std::make_shared<SSLContext>(new SSLContextImpl(ssl_ctx));

    int option = SSL_OP_ALL;
    if (ssl_forbid_method)
    {
        if (strchr(ssl_forbid_method, '2'))
            option |= SSL_OP_NO_SSLv2;
        if (strchr(ssl_forbid_method, '3'))
            option |= SSL_OP_NO_SSLv3;
        if (strchr(ssl_forbid_method, 't'))
            option |= SSL_OP_NO_TLSv1;
        if (strchr(ssl_forbid_method, 'T'))
            option |= SSL_OP_NO_TLSv1;
    }
    SSL_CTX_set_options(ssl_ctx, option);
#ifdef USE_SSL_VERIFY
    /* derived from openssl-0.9.5/apps/s_{client,cb}.c */
#if 1 /* use SSL_get_verify_result() to verify cert */
    SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_NONE, NULL);
#else
    SSL_CTX_set_verify(ssl_ctx,
                       ssl_verify_server ? SSL_VERIFY_PEER : SSL_VERIFY_NONE, NULL);
#endif
    if (ssl_cert_file != NULL && *ssl_cert_file != '\0')
    {
        int ng = 1;
        if (SSL_CTX_use_certificate_file(ssl_ctx, ssl_cert_file, SSL_FILETYPE_PEM) > 0)
        {
            char *key_file = (ssl_key_file == NULL || *ssl_key_file ==
                                                          '\0')
                                 ? ssl_cert_file
                                 : ssl_key_file;
            if (SSL_CTX_use_PrivateKey_file(ssl_ctx, key_file, SSL_FILETYPE_PEM) > 0)
                if (SSL_CTX_check_private_key(ssl_ctx))
                    ng = 0;
        }
        if (ng)
        {
            return nullptr;
        }
    }
    if ((!ssl_ca_file && !ssl_ca_path) || SSL_CTX_load_verify_locations(ssl_ctx, ssl_ca_file, ssl_ca_path))
#endif /* defined(USE_SSL_VERIFY) */
        SSL_CTX_set_default_verify_paths(ssl_ctx);
#endif /* SSLEAY_VERSION_NUMBER >= 0x0800 */

    return impl;
}

std::shared_ptr<SSLSocket> SSLContext::Open(int sock, std::string_view hostname)
{
    auto handle = SSL_new(m_impl->m_ctx);
    if (!handle)
    {
        return nullptr;
    }

    SSL_set_fd(handle, sock);
#if SSLEAY_VERSION_NUMBER >= 0x00905100
    init_PRNG();
#endif /* SSLEAY_VERSION_NUMBER >= 0x00905100 */
#if (SSLEAY_VERSION_NUMBER >= 0x00908070) && !defined(OPENSSL_NO_TLSEXT)
    SSL_set_tlsext_host_name(handle, hostname.data());
#endif /* (SSLEAY_VERSION_NUMBER >= 0x00908070) && !defined(OPENSSL_NO_TLSEXT) */
    if (SSL_connect(handle) > 0)
    {
        Str serv_cert = m_impl->ssl_get_certificate(handle, const_cast<char *>(hostname.data()));
        if (serv_cert)
        {
            // *p_cert = serv_cert->ptr;
            // return handle;
            return std::make_shared<SSLSocket>(new SSLSocketImpl(sock, handle), serv_cert->ptr);
        }
    }

    /* FIXME: gettextize? */
    disp_err_message(Sprintf("SSL error: %s",
                             ERR_error_string(ERR_get_error(), NULL))
                         ->ptr,
                     FALSE);
    return NULL;
}
