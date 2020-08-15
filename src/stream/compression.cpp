#include "stream/compression.h"
#include "mime/mimetypes.h"
#include "textlist.h"
#include "indep.h"
#include "stream/istream.h"
#include "rc.h"
#include "file.h"
#include <string_view_util.h>
#include "fm.h"
#include "html/html.h"
#include "stream/local_cgi.h"
#include <sys/stat.h>

struct compression_decoder
{
    CompressionTypes type;
    const char *ext;
    const char *mime_type;
    int auxbin_p;
    const char *cmd;
    const char *name;
    const char *encoding;
    const char *encodings[4];
};

/* *INDENT-OFF* */
static compression_decoder compression_decoders[] = {
    {CMP_COMPRESS, ".gz", "application/x-gzip", 0, GUNZIP_CMDNAME, GUNZIP_NAME, "gzip", {"gzip", "x-gzip", NULL}},
    {CMP_COMPRESS, ".Z", "application/x-compress", 0, GUNZIP_CMDNAME, GUNZIP_NAME, "compress", {"compress", "x-compress", NULL}},
    {CMP_BZIP2, ".bz2", "application/x-bzip", 0, BUNZIP2_CMDNAME, BUNZIP2_NAME, "bzip, bzip2", {"x-bzip", "bzip", "bzip2", NULL}},
    {CMP_DEFLATE, ".deflate", "application/x-deflate", 1, INFLATE_CMDNAME, INFLATE_NAME, "deflate", {"deflate", "x-deflate", NULL}},
    {CMP_NOCOMPRESS, NULL, NULL, 0, NULL, NULL, NULL, {NULL}},
};
/* *INDENT-ON* */

CompressionTypes get_compression_type(std::string_view value)
{
    auto content_encoding = CMP_NOCOMPRESS;
    for (auto d = compression_decoders; d->type != CMP_NOCOMPRESS; d++)
    {
        for (auto e = d->encodings; *e != NULL; e++)
        {
            if (svu::iceq(value, *e))
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
    return content_encoding;
}

void check_compression(std::string_view path, const URLFilePtr &uf)
{
    if (path.empty())
        return;

    auto len = path.size();
    uf->compression = CMP_NOCOMPRESS;
    for (auto d = compression_decoders; d->type != CMP_NOCOMPRESS; d++)
    {
        int elen;
        if (d->ext == NULL)
            continue;
        elen = strlen(d->ext);
        if (len > elen && strcasecmp(&path[len - elen], d->ext) == 0)
        {
            uf->compression = d->type;
            uf->guess_type = d->mime_type;
            break;
        }
    }
}

const char *compress_application_type(CompressionTypes compression)
{
    struct compression_decoder *d;

    for (d = compression_decoders; d->type != CMP_NOCOMPRESS; d++)
    {
        if (d->type == compression)
            return d->mime_type;
    }
    return NULL;
}

const char *uncompressed_file_type(const char *path, const char **ext)
{
    int len, slen;
    Str fn;
    struct compression_decoder *d;

    if (path == NULL)
        return NULL;

    slen = 0;
    len = strlen(path);
    for (d = compression_decoders; d->type != CMP_NOCOMPRESS; d++)
    {
        if (d->ext == NULL)
            continue;
        slen = strlen(d->ext);
        if (len > slen && strcasecmp(&path[len - slen], d->ext) == 0)
            break;
    }
    if (d->type == CMP_NOCOMPRESS)
        return NULL;

    fn = Strnew(path);
    fn->Pop(slen);
    if (ext)
        *ext = filename_extension(fn->ptr, 0);
    auto t0 = guessContentType(fn->ptr);
    if (t0 == NULL)
        t0 = "text/plain";
    return t0;
}

#define S_IXANY (S_IXUSR | S_IXGRP | S_IXOTH)

static int check_command(const char *cmd, int auxbin_p)
{
    static char *path = NULL;
    Str dirs;
    char *p, *np;
    Str pathname;
    struct stat st;

    if (path == NULL)
        path = getenv("PATH");
    if (auxbin_p)
        dirs = Strnew(w3m_auxbin_dir());
    else
        dirs = Strnew(path);
    for (p = dirs->ptr; p != NULL; p = np)
    {
        np = strchr(p, PATH_SEPARATOR);
        if (np)
            *np++ = '\0';
        pathname = Strnew();
        pathname->Push(p);
        pathname->Push('/');
        pathname->Push(cmd);
        if (stat(pathname->ptr, &st) == 0 && S_ISREG(st.st_mode) && (st.st_mode & S_IXANY) != 0)
            return 1;
    }
    return 0;
}

char *acceptableEncoding()
{
    static Str encodings = NULL;
    struct compression_decoder *d;
    TextList *l;
    char *p;

    if (encodings != NULL)
        return encodings->ptr;
    l = newTextList();
    for (d = compression_decoders; d->type != CMP_NOCOMPRESS; d++)
    {
        if (check_command(d->cmd, d->auxbin_p))
        {
            pushText(l, d->encoding);
        }
    }
    encodings = Strnew();
    while ((p = popText(l)) != NULL)
    {
        if (encodings->Size())
            encodings->Push(", ");
        encodings->Push(p);
    }
    return encodings->ptr;
}

#define SAVE_BUF_SIZE 1536
char *uncompress_stream(const URLFilePtr &uf, bool useRealFile)
{
    // struct compression_decoder *d;
    if (uf->stream->type() != IST_ENCODED)
    {
        uf->stream = newEncodedStream(uf->stream, uf->encoding);
        uf->encoding = ENC_7BIT;
    }

    // search decoder
    const char *expand_cmd = GUNZIP_CMDNAME;
    const char *expand_name = GUNZIP_NAME;
    const char *ext = NULL;
    for (auto d = compression_decoders; d->type != CMP_NOCOMPRESS; d++)
    {
        if (uf->compression == d->type)
        {
            if (d->auxbin_p)
                expand_cmd = auxbinFile(d->cmd);
            else
                expand_cmd = d->cmd;
            expand_name = d->name;
            ext = d->ext;
            break;
        }
    }
    uf->compression = CMP_NOCOMPRESS;

    char *tmpf = NULL;
    if (uf->scheme != SCM_LOCAL && !image_source)
    {
        tmpf = tmpfname(TMPF_DFL, ext)->ptr;
    }

    /* child1 -- stdout|f1=uf -> parent */
    FILE *f1;
    auto pid1 = open_pipe_rw(&f1, NULL);
    if (pid1 < 0)
    {
        // uf->Close();
        return nullptr;
    }
    if (pid1 == 0)
    {
        /* child */
        pid_t pid2;
        FILE *f2 = stdin;

        /* uf -> child2 -- stdout|stdin -> child1 */
        pid2 = open_pipe_rw(&f2, NULL);
        if (pid2 < 0)
        {
            // uf->Close();
            exit(1);
        }
        if (pid2 == 0)
        {
            /* child2 */
            Str buf = Strnew_size(SAVE_BUF_SIZE);
            FILE *f = NULL;

            setup_child(TRUE, 2, uf->stream->FD());
            if (tmpf)
                f = fopen(tmpf, "wb");
            while (uf->stream->readto(buf, SAVE_BUF_SIZE))
            {
                if (buf->Puts(stdout) < 0)
                    break;
                if (f)
                    buf->Puts(f);
            }
            // uf->Close();
            if (f)
                fclose(f);
            exit(0);
        }
        /* child1 */
        dup2(1, 2); /* stderr>&stdout */
        setup_child(TRUE, -1, -1);
        execlp(expand_cmd, expand_name, NULL);
        exit(1);
    }

    if (tmpf)
    {
        if (useRealFile)
        {
        }
        else
        {
            tmpf = nullptr;
            uf->scheme = SCM_LOCAL;
        }
    }
    // uf->Close();
    uf->stream = newFileStream(f1, fclose);
    return tmpf;
}
