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

#include "stream/istream.h"
#include <sys/stat.h>
#include <zlib.h>

struct compression_decoder
{
    CompressionTypes type;
    std::string_view ext;
    std::string_view mime_type;
    int auxbin_p;
    const char *cmd;
    const char *name;
    const char *encoding;
    std::vector<std::string_view> encodings;

    const char *expand_cmd() const
    {
        if (auxbin_p)
            return auxbinFile(cmd);
        else
            return cmd;
    }

#define S_IXANY (S_IXUSR | S_IXGRP | S_IXOTH)

    bool check_command() const
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
};

/* *INDENT-OFF* */
static compression_decoder compression_decoders[] = {
    {CMP_COMPRESS, ".gz", "application/x-gzip", 0, GUNZIP_CMDNAME, GUNZIP_NAME, "gzip", {"gzip", "x-gzip"}},
    {CMP_COMPRESS, ".Z", "application/x-compress", 0, GUNZIP_CMDNAME, GUNZIP_NAME, "compress", {"compress", "x-compress"}},
    {CMP_BZIP2, ".bz2", "application/x-bzip", 0, BUNZIP2_CMDNAME, BUNZIP2_NAME, "bzip, bzip2", {"x-bzip", "bzip", "bzip2"}},
    {CMP_DEFLATE, ".deflate", "application/x-deflate", 1, INFLATE_CMDNAME, INFLATE_NAME, "deflate", {"deflate", "x-deflate"}},
};
/* *INDENT-ON* */

CompressionTypes get_compression_type(std::string_view value)
{
    for (auto &d : compression_decoders)
    {
        for (auto &e : d.encodings)
        {
            if (svu::ic_eq(value, e))
            {
                return d.type;
            }
        }
    }
    return CMP_NOCOMPRESS;
}

// void check_compression(std::string_view path, const URLFilePtr &uf)
// {
//     if (path.empty())
//         return;

//     auto len = path.size();
//     uf->compression = CMP_NOCOMPRESS;
//     for (auto &d : compression_decoders)
//     {
//         if (svu::ic_ends_with(path, d.ext))
//         {
//             uf->compression = d.type;
//             uf->guess_type = d.mime_type;
//             break;
//         }
//     }
// }

std::string_view compress_application_type(CompressionTypes compression)
{
    for (auto &d : compression_decoders)
    {
        if (d.type == compression)
        {
            return d.mime_type;
        }
    }
    return "";
}

std::tuple<std::string_view, std::string_view> uncompressed_file_type(std::string_view path)
{
    struct compression_decoder *d = nullptr;
    for (auto &_d : compression_decoders)
    {
        if (svu::ic_ends_with(path, _d.ext))
        {
            d = &_d;
            break;
        }
    }
    if (!d)
        return {};

    auto t0 = guessContentType(path.substr(0, path.size() - d->ext.size()));
    return {t0, d->ext};
}

char *acceptableEncoding()
{
    static Str encodings = NULL;
    if (encodings)
        return encodings->ptr;

    auto l = newTextList();
    for (auto &d : compression_decoders)
    {
        if (d.check_command())
        {
            pushText(l, d.encoding);
        }
    }
    encodings = Strnew();
    char *p;
    while ((p = popText(l)) != NULL)
    {
        if (encodings->Size())
            encodings->Push(", ");
        encodings->Push(p);
    }
    return encodings->ptr;
}

// #define SAVE_BUF_SIZE 1536
// char *uncompress_stream(const URLFilePtr &uf, bool useRealFile)
// {
//     // struct compression_decoder *d;
//     if (uf->stream->type() != IST_ENCODED)
//     {
//         uf->stream = newEncodedStream(uf->stream, uf->encoding);
//         uf->encoding = ENC_7BIT;
//     }

//     // // search decoder
//     const char *ext = nullptr;
//     compression_decoder *d = compression_decoders;
//     for (auto &_d : compression_decoders)
//     {
//         if (uf->compression == _d.type)
//         {
//             d = &_d;
//             ext = _d.ext.data();
//             break;
//         }
//     }
//     uf->compression = CMP_NOCOMPRESS;

//     char *tmpf = NULL;
//     if (!image_source)
//     {
//         tmpf = tmpfname(TMPF_DFL, ext)->ptr;
//     }

//     /* child1 -- stdout|f1=uf -> parent */
//     FILE *f1;
//     auto pid1 = open_pipe_rw(&f1, NULL);
//     if (pid1 < 0)
//     {
//         // uf->Close();
//         return nullptr;
//     }
//     if (pid1 == 0)
//     {
//         /* child */
//         pid_t pid2;
//         FILE *f2 = stdin;

//         /* uf -> child2 -- stdout|stdin -> child1 */
//         pid2 = open_pipe_rw(&f2, NULL);
//         if (pid2 < 0)
//         {
//             // uf->Close();
//             exit(1);
//         }
//         if (pid2 == 0)
//         {
//             /* child2 */
//             Str buf = Strnew_size(SAVE_BUF_SIZE);
//             FILE *f = NULL;

//             setup_child(TRUE, 2, uf->stream->FD());
//             if (tmpf)
//                 f = fopen(tmpf, "wb");
//             while (uf->stream->readto(buf, SAVE_BUF_SIZE))
//             {
//                 if (buf->Puts(stdout) < 0)
//                     break;
//                 if (f)
//                     buf->Puts(f);
//             }
//             // uf->Close();
//             if (f)
//                 fclose(f);
//             exit(0);
//         }
//         /* child1 */
//         dup2(1, 2); /* stderr>&stdout */
//         setup_child(TRUE, -1, -1);
//         execlp(d->expand_cmd(), d->name, NULL);
//         exit(1);
//     }

//     if (tmpf)
//     {
//         if (useRealFile)
//         {
//         }
//         else
//         {
//             tmpf = nullptr;
//             // uf->scheme = SCM_LOCAL;
//         }
//     }
//     uf->stream = newFileStream(f1, fclose);
//     return tmpf;
// }

class ZlibDecompressor
{
    /* decompression stream */
    z_stream d_stream = {0};

public:
    std::vector<uint8_t> decompressed;

    // static voidpf alloc_func(voidpf opaque, uInt items, uInt size)
    // {
    //     auto p = (ZlibDecompressor *)opaque;
    //     auto pos = p->decompressed.size();
    //     p->decompressed.resize(pos + items * size, 0);
    //     auto data = p->decompressed.data();
    //     return data + pos;
    // }

    // static void free_func(voidpf opaque, voidpf address)
    // {
    //     // do nothing
    // }

    int err = Z_OK;

    ZlibDecompressor()
        : decompressed(1024 * 1024)
    {
        // d_stream.zalloc = alloc_func;
        // d_stream.zfree = free_func;
        // d_stream.opaque = this;

        err = inflateInit2(&d_stream, 16 + MAX_WBITS);
        auto data = decompressed.data();
        if (HasError())
        {
            return;
        }
    }

    ~ZlibDecompressor()
    {
        inflateEnd(&d_stream);
    }

    bool HasError() const
    {
        if (err == Z_OK)
        {
            return false;
        }
        if (err == Z_STREAM_END)
        {
            return false;
        }
        return true;
    }

    bool IsEnd() const
    {
        return err == Z_STREAM_END;
    }

    void Decompress(unsigned char *compr, int comprLen)
    {
        d_stream.next_in = compr;
        d_stream.avail_in = comprLen;
        d_stream.next_out = decompressed.data() + d_stream.total_out;
        d_stream.avail_out = decompressed.size() - d_stream.total_out;

        for (int i = 0; i < 4; ++i)
        {
            err = inflate(&d_stream, Z_NO_FLUSH);
            if (!HasError())
            {
                break;
            }
            if (err == Z_BUF_ERROR)
            {
                decompressed.resize(decompressed.size() * 2, 0);
            }
            else if (HasError())
            {
                return;
            }
        }
    }

    Str ToStr() const
    {
        return Strnew_charp_n((const char *)decompressed.data(), d_stream.total_out);
    }
};

std::shared_ptr<class InputStream> decompress(const std::shared_ptr<class InputStream> &stream, CompressionTypes type)
{
    // search decoder
    const char *ext = nullptr;
    compression_decoder *d = compression_decoders;
    for (auto &_d : compression_decoders)
    {
        if (_d.type == type)
        {
            d = &_d;
            ext = _d.ext.data();
            break;
        }
    }

    if (d->type != CMP_COMPRESS)
    {
        assert(false);
        return nullptr;
    }

    ZlibDecompressor decompressor;
    unsigned char buf[1024];
    while (!stream->eos())
    {
        if (decompressor.HasError())
        {
            return nullptr;
        }

        auto size = stream->read(buf, sizeof(buf));
        if (size)
        {
            decompressor.Decompress(buf, size);
        }
        if (decompressor.IsEnd())
        {
            break;
        }
    }

    auto str = decompressor.ToStr();

    return newStrStream(str);
}
