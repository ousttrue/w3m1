#include "transport/urlfile.h"
#include "transport/url.h"
#include "transport/local_cgi.h"
#include "transport/istream.h"
#include "http/http_request.h"
#include "fm.h"
#include "indep.h"
#include "file.h"
#include "html/form.h"
#include "frontend/display.h"
#include "public.h"
#include "frontend/terms.h"
#include "http/compression.h"
#include "myctype.h"
#include "transport/loader.h"
#include "mime/mimetypes.h"
#include "frontend/lineinput.h"
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

/* add index_file if exists */
static void
add_index_file(const URL *pu, const URLFilePtr &uf)
{
    assert(false);
    // char *p, *q;
    // TextList *index_file_list = NULL;
    // TextListItem *ti;

    // if (non_null(index_file))
    //     index_file_list = make_domain_list(index_file);
    // if (index_file_list == NULL)
    // {
    //     uf->stream = NULL;
    //     return;
    // }
    // for (ti = index_file_list->first; ti; ti = ti->next)
    // {
    //     p = Strnew_m_charp(pu->path, "/", file_quote(ti->ptr), NULL)->ptr;
    //     p = cleanupName(p);
    //     q = cleanupName(file_unquote(p));
    //     uf->examineFile(q);
    //     if (uf->stream != NULL)
    //     {
    //         pu->path = p;
    //         pu->real_file = q;
    //         return;
    //     }
    // }
}

static void
write_from_file(int sock, char *file)
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

SSL_CTX *ssl_ctx = NULL;

void free_ssl_ctx()
{
    if (ssl_ctx != NULL)
        SSL_CTX_free(ssl_ctx);
    ssl_ctx = NULL;
    ssl_accept_this_site(NULL);
}

#ifdef USE_SSL
#ifndef SSLEAY_VERSION_NUMBER
#include <openssl/crypto.h> /* SSLEAY_VERSION_NUMBER may be here */
#endif
#include <openssl/err.h>
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

static SSL *
openSSLHandle(int sock, char *hostname, char **p_cert)
{
    SSL *handle = NULL;
    static char *old_ssl_forbid_method = NULL;
#ifdef USE_SSL_VERIFY
    static int old_ssl_verify_server = -1;
#endif

    if (old_ssl_forbid_method != ssl_forbid_method && (!old_ssl_forbid_method || !ssl_forbid_method ||
                                                       strcmp(old_ssl_forbid_method, ssl_forbid_method)))
    {
        old_ssl_forbid_method = ssl_forbid_method;
#ifdef USE_SSL_VERIFY
        ssl_path_modified = 1;
#else
        free_ssl_ctx();
#endif
    }
#ifdef USE_SSL_VERIFY
    if (old_ssl_verify_server != ssl_verify_server)
    {
        old_ssl_verify_server = ssl_verify_server;
        ssl_path_modified = 1;
    }
    if (ssl_path_modified)
    {
        free_ssl_ctx();
        ssl_path_modified = 0;
    }
#endif /* defined(USE_SSL_VERIFY) */
    if (ssl_ctx == NULL)
    {
        int option;
#if SSLEAY_VERSION_NUMBER < 0x0800
        ssl_ctx = SSL_CTX_new();
        X509_set_default_verify_paths(ssl_ctx->cert);
#else /* SSLEAY_VERSION_NUMBER >= 0x0800 */
        SSLeay_add_ssl_algorithms();
        SSL_load_error_strings();
        if (!(ssl_ctx = SSL_CTX_new(SSLv23_client_method())))
            goto eend;
        option = SSL_OP_ALL;
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
                free_ssl_ctx();
                goto eend;
            }
        }
        if ((!ssl_ca_file && !ssl_ca_path) || SSL_CTX_load_verify_locations(ssl_ctx, ssl_ca_file, ssl_ca_path))
#endif /* defined(USE_SSL_VERIFY) */
            SSL_CTX_set_default_verify_paths(ssl_ctx);
#endif /* SSLEAY_VERSION_NUMBER >= 0x0800 */
    }
    handle = SSL_new(ssl_ctx);
    SSL_set_fd(handle, sock);
#if SSLEAY_VERSION_NUMBER >= 0x00905100
    init_PRNG();
#endif /* SSLEAY_VERSION_NUMBER >= 0x00905100 */
#if (SSLEAY_VERSION_NUMBER >= 0x00908070) && !defined(OPENSSL_NO_TLSEXT)
    SSL_set_tlsext_host_name(handle, hostname);
#endif /* (SSLEAY_VERSION_NUMBER >= 0x00908070) && !defined(OPENSSL_NO_TLSEXT) */
    if (SSL_connect(handle) > 0)
    {
        Str serv_cert = ssl_get_certificate(handle, hostname);
        if (serv_cert)
        {
            *p_cert = serv_cert->ptr;
            return handle;
        }
        close(sock);
        SSL_free(handle);
        return NULL;
    }
eend:
    close(sock);
    if (handle)
        SSL_free(handle);
    /* FIXME: gettextize? */
    disp_err_message(Sprintf("SSL error: %s",
                             ERR_error_string(ERR_get_error(), NULL))
                         ->ptr,
                     FALSE);
    return NULL;
}

static void
SSL_write_from_file(SSL *ssl, char *file)
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
            SSL_write(ssl, buf, 1);
        }
        fclose(fd);
    }
}

static bool domain_match(const char *pat, const char *domain)
{
    if (domain == NULL)
        return 0;
    if (*pat == '.')
        pat++;
    for (;;)
    {
        if (!strcasecmp(pat, domain))
            return 1;
        domain = strchr(domain, '.');
        if (domain == NULL)
            return 0;
        domain++;
    }
}

bool check_no_proxy(std::string_view domain)
{
    TextListItem *tl;
    int ret = 0;
    MySignalHandler prevtrap = NULL;

    if (w3mApp::Instance().NO_proxy_domains == NULL || w3mApp::Instance().NO_proxy_domains->nitem == 0 ||
        domain == NULL)
        return 0;
    for (tl = w3mApp::Instance().NO_proxy_domains->first; tl != NULL; tl = tl->next)
    {
        if (domain_match(tl->ptr, domain.data()))
            return 1;
    }
    if (!NOproxy_netaddr)
    {
        return 0;
    }
    /* 
     * to check noproxy by network addr
     */

    auto success = TrapJmp([&]() {
        {
#ifndef INET6
            struct hostent *he;
            int n;
            unsigned char **h_addr_list;
            char addr[4 * 16], buf[5];

            he = gethostbyname(domain);
            if (!he)
            {
                ret = 0;
                goto end;
            }
            for (h_addr_list = (unsigned char **)he->h_addr_list; *h_addr_list;
                 h_addr_list++)
            {
                sprintf(addr, "%d", h_addr_list[0][0]);
                for (n = 1; n < he->h_length; n++)
                {
                    sprintf(buf, ".%d", h_addr_list[0][n]);
                    addr->Push(buf);
                }
                for (tl = NO_proxy_domains->first; tl != NULL; tl = tl->next)
                {
                    if (strncmp(tl->ptr, addr, strlen(tl->ptr)) == 0)
                    {
                        ret = 1;
                        goto end;
                    }
                }
            }
#else  /* INET6 */
            int error;
            struct addrinfo hints;
            struct addrinfo *res, *res0;
            char addr[4 * 16];
            int *af;

            for (af = ai_family_order_table[DNS_order];; af++)
            {
                memset(&hints, 0, sizeof(hints));
                hints.ai_family = *af;
                error = getaddrinfo(domain.data(), NULL, &hints, &res0);
                if (error)
                {
                    if (*af == PF_UNSPEC)
                    {
                        break;
                    }
                    /* try next */
                    continue;
                }
                for (res = res0; res != NULL; res = res->ai_next)
                {
                    switch (res->ai_family)
                    {
                    case AF_INET:
                        inet_ntop(AF_INET,
                                  &((struct sockaddr_in *)res->ai_addr)->sin_addr,
                                  addr, sizeof(addr));
                        break;
                    case AF_INET6:
                        inet_ntop(AF_INET6,
                                  &((struct sockaddr_in6 *)res->ai_addr)->sin6_addr, addr, sizeof(addr));
                        break;
                    default:
                        /* unknown */
                        continue;
                    }
                    for (tl = w3mApp::Instance().NO_proxy_domains->first; tl != NULL; tl = tl->next)
                    {
                        if (strncmp(tl->ptr, addr, strlen(tl->ptr)) == 0)
                        {
                            freeaddrinfo(res0);
                            ret = 1;
                            return true;
                        }
                    }
                }
                freeaddrinfo(res0);
                if (*af == PF_UNSPEC)
                {
                    break;
                }
            }
#endif /* INET6 */
        }

        return true;
    });

    if (!success)
    {
        ret = 0;
    }

    return ret;
}

static bool UseProxy(const URL &url)
{
    if (!w3mApp::Instance().use_proxy)
    {
        return false;
    }
    if (url.scheme == SCM_HTTPS)
    {
        if (w3mApp::Instance().HTTPS_proxy.empty())
        {
            return false;
        }
    }
    else if (url.scheme == SCM_HTTP)
    {
        if (w3mApp::Instance().HTTP_proxy.empty())
        {
            return false;
        }
    }
    else
    {
        assert(false);
        return false;
    }

    if (url.host.empty())
    {
        return false;
    }
    if (check_no_proxy(url.host))
    {
        return false;
    }

    return true;
}

///
/// URLFile
///
URLFile::URLFile(URLSchemeTypes scm, InputStreamPtr strm)
    : scheme(scm), stream(strm)
{
}

URLFile::~URLFile()
{
}

#define IS_DIRECTORY(m) (((m)&S_IFMT) == S_IFDIR)
int dir_exist(const char *path)
{
    if (path == NULL || *path == '\0')
        return 0;

    struct stat stbuf;
    if (stat(path, &stbuf) == -1)
        return 0;

    return IS_DIRECTORY(stbuf.st_mode);
}

URLFilePtr URLFile::OpenHttp(const URL &url, const URL *current,
                             HttpReferrerPolicy referer, LoadFlags flag, FormList *request, TextList *extra_header,
                             HttpRequest *hr)
{
    if (url.scheme != SCM_HTTP && url.scheme != SCM_HTTPS)
    {
        assert(false);
        return {};
    }

    if (request && request->method == FORM_METHOD_POST && request->body)
        hr->method = HTTP_METHOD_POST;
    if (request && request->method == FORM_METHOD_HEAD)
        hr->method = HTTP_METHOD_HEAD;

    int sock = 0;
    SSL *sslh = nullptr;
    Str tmp = nullptr;
    char *ssl_certificate = nullptr;

    if (UseProxy(url))
    {
        // // hr->flag |= HR_FLAG_PROXY;
        // if (url.scheme == SCM_HTTPS /*&& *status == HTST_CONNECT*/)
        // {
        //     // https proxy の時に通る？
        //     assert(false);
        //     return {};
        //     // sock = ssl_socket_of(ouf->stream);
        //     // if (!(sslh = openSSLHandle(sock, url.host,
        //     //                            &this->ssl_certificate)))
        //     // {
        //     //     *status = HTST_MISSING;
        //     //     return;
        //     // }
        // }
        // else if (url.scheme == SCM_HTTPS)
        // {
        //     sock = openSocket(w3mApp::Instance().HTTPS_proxy_parsed.host.c_str(),
        //                       GetScheme(w3mApp::Instance().HTTPS_proxy_parsed.scheme)->name.data(), w3mApp::Instance().HTTPS_proxy_parsed.port);
        //     sslh = NULL;
        // }
        // else
        // {
        //     sock = openSocket(w3mApp::Instance().HTTP_proxy_parsed.host.c_str(),
        //                       GetScheme(w3mApp::Instance().HTTP_proxy_parsed.scheme)->name.data(), w3mApp::Instance().HTTP_proxy_parsed.port);
        //     sslh = NULL;
        // }

        // if (sock < 0)
        // {
        //     return {};
        // }

        // if (url.scheme == SCM_HTTPS)
        // {
        //     if (*status == HTST_NORMAL)
        //     {
        //         hr->method = HTTP_METHOD_CONNECT;
        //         tmp = hr->ToStr(url, current, extra_header);
        //         *status = HTST_CONNECT;
        //     }
        //     else
        //     {
        //         // hr->flag |= HR_FLAG_LOCAL;
        //         tmp = hr->ToStr(url, current, extra_header);
        //         *status = HTST_NORMAL;
        //     }
        // }
        // else
        // {
        //     tmp = hr->ToStr(url, current, extra_header);
        //     *status = HTST_NORMAL;
        // }

        assert(false);
    }
    else
    {
        sock = openSocket(url.host.c_str(),
                          GetScheme(url.scheme)->name.data(), url.port);
        if (sock < 0)
        {
            // *status = HTST_MISSING;
            return {};
        }
        if (url.scheme == SCM_HTTPS)
        {
            if (!(sslh = openSSLHandle(sock, const_cast<char *>(url.host.c_str()), &ssl_certificate)))
            {
                // *status = HTST_MISSING;
                return {};
            }
        }
        // hr->flag |= HR_FLAG_LOCAL;
        tmp = hr->ToStr(url, current, extra_header);
        // *status = HTST_NORMAL;
    }

    if (url.scheme == SCM_HTTPS)
    {
        auto uf = std::shared_ptr<URLFile>(new URLFile(SCM_HTTPS, newSSLStream(sslh, sock)));
        uf->ssl_certificate = ssl_certificate;
        if (sslh)
            SSL_write(sslh, tmp->ptr, tmp->Size());
        else
            write(sock, tmp->ptr, tmp->Size());
        if (w3mApp::Instance().w3m_reqlog.size())
        {
            FILE *ff = fopen(w3mApp::Instance().w3m_reqlog.c_str(), "a");
            if (sslh)
                fputs("HTTPS: request via SSL\n", ff);
            else
                fputs("HTTPS: request without SSL\n", ff);
            fwrite(tmp->ptr, sizeof(char), tmp->Size(), ff);
            fclose(ff);
        }
        if (hr->method == HTTP_METHOD_POST &&
            request->enctype == FORM_ENCTYPE_MULTIPART)
        {
            if (sslh)
                SSL_write_from_file(sslh, request->body);
            else
                write_from_file(sock, request->body);
        }
        return uf;
    }
    else
    {
        write(sock, tmp->ptr, tmp->Size());
        if (w3mApp::Instance().w3m_reqlog.size())
        {
            FILE *ff = fopen(w3mApp::Instance().w3m_reqlog.c_str(), "a");
            fwrite(tmp->ptr, sizeof(char), tmp->Size(), ff);
            fclose(ff);
        }
        if (hr->method == HTTP_METHOD_POST &&
            request->enctype == FORM_ENCTYPE_MULTIPART)
            write_from_file(sock, request->body);

        auto uf = std::shared_ptr<URLFile>(new URLFile(SCM_HTTP, newInputStream(sock)));
        return uf;
    }
}

// Str tmp;
// int sock;
// char *p, *q;
// SSL *sslh = NULL;

// auto u = url.data();
// auto [pos, scheme] = getURLScheme(u);
// // u = pos;
// if (current == NULL && scheme == SCM_MISSING && !ArgvIsURL)
//     u = file_to_url(url.data()); /* force to local file */
// else
//     u = const_cast<char *>(url.data());
// retry:
// *pu = URL::Parse(u, current);

// if (url.scheme == SCM_LOCAL && url.path.empty())
// {
//     // TODO:
//     assert(false);
//     // if (url.fragment.size())
//     // {
//     //     /* #hogege is not a label but a filename */
//     //     Str tmp2 = Strnew("#");
//     //     tmp2->Push(url.fragment);
//     //     url.path = tmp2->ptr;
//     //     url.real_file = cleanupName(file_unquote(url.path));
//     //     url.fragment.clear();
//     // }
//     // else
//     // {
//     //     /* given URL must be null string */
//     //     return;
//     // }
// }

std::shared_ptr<URLFile> URLFile::OpenFile(std::string_view path)
{
    auto uf = std::shared_ptr<URLFile>(new URLFile(SCM_LOCAL, NULL));
    uf->examineFile(path);
    return uf;
}

std::shared_ptr<URLFile> URLFile::OpenStream(URLSchemeTypes scheme, InputStreamPtr stream)
{
    auto uf = std::shared_ptr<URLFile>(new URLFile(scheme, stream));
    return uf;
}

int URLFile::DoFileSave(const char *defstr, long long content_length)
{
#ifndef __MINGW32_VERSION
    Str msg;
    Str filen;
    char *p, *q;
    pid_t pid;
    char *lock;
    char *tmpf = NULL;
#if !(defined(HAVE_SYMLINK) && defined(HAVE_LSTAT))
    FILE *f;
#endif

    if (fmInitialized)
    {
        p = searchKeyData();
        if (p == NULL || *p == '\0')
        {
            /* FIXME: gettextize? */
            p = inputLineHist("(Download)Save file to: ",
                              defstr, IN_FILENAME, w3mApp::Instance().SaveHist);
            if (p == NULL || *p == '\0')
                return -1;
            p = conv_to_system(p);
        }
        if (checkOverWrite(p) < 0)
            return -1;
        if (checkSaveFile(this->stream, p) < 0)
        {
            /* FIXME: gettextize? */
            msg = Sprintf("Can't save. Load file and %s are identical.",
                          conv_from_system(p));
            disp_err_message(msg->ptr, FALSE);
            return -1;
        }
        lock = tmpfname(TMPF_DFL, ".lock")->ptr;
#if defined(HAVE_SYMLINK) && defined(HAVE_LSTAT)
        symlink(p, lock);
#else
        f = fopen(lock, "w");
        if (f)
        {
            fclose(f);
        }
#endif
        flush_tty();
        pid = fork();
        if (!pid)
        {
            int err;
            if ((this->content_encoding != CMP_NOCOMPRESS) && AutoUncompress)
            {
                tmpf = uncompress_stream(shared_from_this(), true);
                if (tmpf)
                    unlink(tmpf);
            }
            setup_child(FALSE, 0, stream->FD());
            err = save2tmp(shared_from_this(), p);
            if (err == 0 && PreserveTimestamp && this->modtime != -1)
                setModtime(p, this->modtime);

            unlink(lock);
            if (err != 0)
                exit(-err);
            exit(0);
        }
        addDownloadList(pid, this->url, p, lock, content_length);
    }
    else
    {
        q = searchKeyData();
        if (q == NULL || *q == '\0')
        {
            /* FIXME: gettextize? */
            printf("(Download)Save file to: ");
            fflush(stdout);
            filen = Strfgets(stdin);
            if (filen->Size() == 0)
                return -1;
            q = filen->ptr;
        }
        for (p = q + strlen(q) - 1; IS_SPACE(*p); p--)
            ;
        *(p + 1) = '\0';
        if (*q == '\0')
            return -1;
        p = expandPath(q);
        if (checkOverWrite(p) < 0)
            return -1;
        if (checkSaveFile(this->stream, p) < 0)
        {
            /* FIXME: gettextize? */
            printf("Can't save. Load file and %s are identical.", p);
            return -1;
        }
        if (this->content_encoding != CMP_NOCOMPRESS && AutoUncompress)
        {
            tmpf = uncompress_stream(shared_from_this(), true);
            if (tmpf)
                unlink(tmpf);
        }
        if (save2tmp(shared_from_this(), p) < 0)
        {
            /* FIXME: gettextize? */
            printf("Can't save to %s\n", p);
            return -1;
        }
        if (PreserveTimestamp && this->modtime != -1)
            setModtime(p, this->modtime);
    }
#endif /* __MINGW32_VERSION */
    return 0;
}

static FILE *
lessopen_stream(char *path)
{
    char *lessopen;
    FILE *fp;

    lessopen = getenv("LESSOPEN");
    if (lessopen == NULL)
    {
        return NULL;
    }
    if (lessopen[0] == '\0')
    {
        return NULL;
    }

    if (lessopen[0] == '|')
    {
        /* pipe mode */
        Str tmpf;
        int c;

        ++lessopen;
        tmpf = Sprintf(lessopen, shell_quote(path));
        fp = popen(tmpf->ptr, "r");
        if (fp == NULL)
        {
            return NULL;
        }
        c = getc(fp);
        if (c == EOF)
        {
            fclose(fp);
            return NULL;
        }
        ungetc(c, fp);
    }
    else
    {
        /* filename mode */
        /* not supported m(__)m */
        fp = NULL;
    }
    return fp;
}

void URLFile::examineFile(std::string_view path)
{
    this->guess_type = NULL;
    if (path.empty())
    {
        return;
    }

    struct stat stbuf;
    if (stat(path.data(), &stbuf) == -1)
    {
        return;
    }

    if (stbuf.st_mode & S_IFMT != S_IFREG)
    {
        this->stream = NULL;
        return;
    }

    this->stream = openIS(path.data());
    if (do_download)
    {
        return;
    }

    if (use_lessopen && getenv("LESSOPEN"))
    {
        this->guess_type = guessContentType(path);
        if (this->guess_type == NULL)
            this->guess_type = "text/plain";
        if (is_html_type(this->guess_type))
            return;
        FILE *fp;
        if ((fp = lessopen_stream(const_cast<char *>(path.data()))))
        {
            // TODO:
            // this->Close();
            this->stream = newFileStream(fp, (FileStreamCloseFunc)pclose);
            this->guess_type = "text/plain";
            return;
        }
    }

    check_compression(const_cast<char *>(path.data()), shared_from_this());
    if (this->compression != CMP_NOCOMPRESS)
    {
        const char *ext = this->ext;
        auto t0 = uncompressed_file_type(path.data(), &ext);
        this->guess_type = t0;
        this->ext = ext;
        uncompress_stream(shared_from_this(), NULL);
        return;
    }
}

int save2tmp(const URLFilePtr &uf, char *tmpf)
{
    auto ff = fopen(tmpf, "wb");
    if (ff == NULL)
    {
        /* fclose(f); */
        return -1;
    }

    auto success = TrapJmp([&]() -> bool {
        Str buf = Strnew_size(SAVE_BUF_SIZE);
        clen_t linelen = 0;
        while (ISread(uf->stream, buf, SAVE_BUF_SIZE))
        {
            if (buf->Puts(ff) != buf->Size())
            {
                // bcopy(env_bak, AbortLoading, sizeof(JMP_BUF));
                // TRAP_OFF;
                // fclose(ff);
                return false;
            }
            linelen += buf->Size();
            clen_t trbyte = 0;
            showProgress(&linelen, &trbyte);
        }

        return true;
    });

    fclose(ff);
    // current_content_length = 0;
    return 0;
}
