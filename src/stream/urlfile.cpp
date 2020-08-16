#include "stream/urlfile.h"
#include "stream/url.h"
#include "stream/local_cgi.h"
#include "stream/istream.h"
#include "stream/http.h"
#include "fm.h"
#include "indep.h"
#include "file.h"
#include "gc_helper.h"
#include "html/form.h"
#include "frontend/display.h"
#include "public.h"
#include "frontend/terms.h"
#include "stream/compression.h"
#include "myctype.h"
#include "stream/loader.h"
#include "mime/mimetypes.h"
#include "frontend/lineinput.h"
#include "regex.h"
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "open_socket.h"

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
openSSLHandle(int sock, std::string_view hostname, char **p_cert)
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
    SSL_set_tlsext_host_name(handle, hostname.data());
#endif /* (SSLEAY_VERSION_NUMBER >= 0x00908070) && !defined(OPENSSL_NO_TLSEXT) */
    if (SSL_connect(handle) > 0)
    {
        Str serv_cert = ssl_get_certificate(handle, const_cast<char *>(hostname.data()));
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


///
/// URLFile
///
// #define IS_DIRECTORY(m) (((m)&S_IFMT) == S_IFDIR)
// int dir_exist(const char *path)
// {
//     if (path == NULL || *path == '\0')
//         return 0;

//     struct stat stbuf;
//     if (stat(path, &stbuf) == -1)
//         return 0;

//     return IS_DIRECTORY(stbuf.st_mode);
// }

InputStreamPtr URLFile::OpenHttpAndSendRest(const std::shared_ptr<HttpRequest> &request)
{
    if (request->url.scheme != SCM_HTTP && request->url.scheme != SCM_HTTPS)
    {
        assert(false);
        return {};
    }

    if (w3mApp::Instance().UseProxy(request->url))
    {
        assert(false);
        return {};
    }

    auto sock = openSocket(request->url);
    if (sock < 0)
    {
        // fail to open socket
        return {};
    }

    if (request->url.scheme == SCM_HTTPS)
    {
        char *ssl_certificate = nullptr;
        SSL *ssl = openSSLHandle(sock, request->url.host, &ssl_certificate);
        if (!ssl)
        {
            // *status = HTST_MISSING;
            return {};
        }

        // send http request
        for (auto &l : request->lines)
        {
            SSL_write(ssl, l.data(), l.size());
        }
        SSL_write(ssl, "\r\n", 2);
        // send post body
        if (request->method == HTTP_METHOD_POST)
        {
            if (request->form->enctype == FORM_ENCTYPE_MULTIPART)
            {
                SSL_write_from_file(ssl, request->form->body);
            }
            else
            {
                SSL_write(ssl, request->form->body, request->form->length);
            }
        }

        // return stream
        // uf->ssl_certificate = ssl_certificate;
        return newSSLStream(ssl, sock);
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
    auto uf = std::make_shared<URLFile>();
    uf->examineFile(path);
    return uf;
}

std::shared_ptr<URLFile> URLFile::FromStream(URLSchemeTypes scheme, const InputStreamPtr &stream)
{
    auto uf = std::make_shared<URLFile>();
    uf->stream = stream;
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
            // if ((this->content_encoding != CMP_NOCOMPRESS) && AutoUncompress)
            // {
            //     tmpf = uncompress_stream(shared_from_this(), true);
            //     if (tmpf)
            //         unlink(tmpf);
            // }
            setup_child(FALSE, 0, stream->FD());
            err = save2tmp(shared_from_this(), p);
            // if (err == 0 && PreserveTimestamp && this->modtime != -1)
            //     setModtime(p, this->modtime);

            unlink(lock);
            if (err != 0)
                exit(-err);
            exit(0);
        }
        addDownloadList(pid, {}, p, lock, content_length);
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
        // if (this->content_encoding != CMP_NOCOMPRESS && AutoUncompress)
        // {
        //     tmpf = uncompress_stream(shared_from_this(), true);
        //     if (tmpf)
        //         unlink(tmpf);
        // }
        if (save2tmp(shared_from_this(), p) < 0)
        {
            /* FIXME: gettextize? */
            printf("Can't save to %s\n", p);
            return -1;
        }
        // if (PreserveTimestamp && this->modtime != -1)
        //     setModtime(p, this->modtime);
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
    // this->guess_type.clear();
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
        // this->guess_type = guessContentType(path);
        // if (this->guess_type.empty())
        //     this->guess_type = "text/plain";
        // if (is_html_type(this->guess_type))
        //     return;
        FILE *fp;
        if ((fp = lessopen_stream(const_cast<char *>(path.data()))))
        {
            // TODO:
            // this->Close();
            this->stream = newFileStream(fp, pclose);
            // this->guess_type = "text/plain";
            return;
        }
    }

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
        while (uf->stream->readto(buf, SAVE_BUF_SIZE))
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
            showProgress(&linelen, &trbyte, 0);
        }

        return true;
    });

    fclose(ff);
    return 0;
}
