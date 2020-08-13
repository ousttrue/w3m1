#pragma once
#include <stdio.h>
#include <openssl/bio.h>
#include <openssl/x509.h>
#include <openssl/ssl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <wc.h>
#include <memory>

struct StreamBuffer
{
    unsigned char *buf;
    int size, cur, next;
};

typedef void (*FileStreamCloseFunc)(FILE *);
struct filestream_handle
{
    FILE *f;
    FileStreamCloseFunc close;
};

struct ssl_handle
{
    SSL *ssl;
    int sock;
};

struct ens_handle
{
    std::shared_ptr<class InputStream> is;
    Str s;
    int pos;
    char encoding;
};

enum InputStreamTypes
{
    IST_BASIC = 0,
    IST_FILE = 1,
    IST_STR = 2,
    IST_SSL = 3,
    IST_ENCODED = 4,
};

class InputStream
{
public:
    StreamBuffer stream;
    char iseos;
    virtual ~InputStream() {}
    virtual InputStreamTypes type() const = 0;
    virtual int ReadFunc(unsigned char *buffer, int size) = 0;
    virtual int FD() const
    {
        return -1;
    }
};
using InputStreamPtr = std::shared_ptr<InputStream>;

// FileDescriptor
class BaseStream : public InputStream
{
public:
    void *handle;

    ~BaseStream();
    InputStreamTypes type() const override { return IST_BASIC; }
    int ReadFunc(unsigned char *buffer, int size) override;
    int FD() const override;
};

// FILE*
class FileStream : public InputStream
{
public:
    struct filestream_handle *handle;

    ~FileStream();
    InputStreamTypes type() const override { return IST_FILE; }
    int ReadFunc(unsigned char *buffer, int size) override;
    int FD() const override;
};

class StrStream : public InputStream
{
public:
    Str handle;

    ~StrStream();
    InputStreamTypes type() const override { return IST_STR; }
    int ReadFunc(unsigned char *buffer, int size) override;
};

class SSLStream : public InputStream
{
public:
    struct ssl_handle *handle;

    ~SSLStream();
    InputStreamTypes type() const override { return IST_SSL; }
    int ReadFunc(unsigned char *buffer, int size) override;
    int FD() const override;
};

struct EncodedStrStream : public InputStream
{
public:
    struct ens_handle *handle;

    ~EncodedStrStream();
    InputStreamTypes type() const override { return IST_ENCODED; }
    int ReadFunc(unsigned char *buffer, int size) override;
    int FD() const override;
};

InputStreamPtr newInputStream(int des);
InputStreamPtr newFileStream(FILE *f, FileStreamCloseFunc closep);
InputStreamPtr newStrStream(Str s);
InputStreamPtr newSSLStream(SSL *ssl, int sock);
InputStreamPtr newEncodedStream(InputStreamPtr is, char encoding);
// int ISclose(InputStreamPtr stream);
int ISgetc(InputStreamPtr stream);
int ISundogetc(InputStreamPtr stream);
Str StrISgets(InputStreamPtr stream);
Str StrmyISgets(InputStreamPtr stream);
int ISread(InputStreamPtr stream, Str buf, int count);
int ISeos(InputStreamPtr stream);
void ssl_accept_this_site(char *hostname);
Str ssl_get_certificate(SSL *ssl, char *hostname);

// #define IST_UNCLOSE 0x10

#define IStype(stream) ((stream)->type())
#define is_eos(stream) ISeos(stream)
#define iseos(stream) ((stream)->iseos)
// #define file_of(stream) ((stream)->file.handle->f)
// #define set_close(stream, closep) ((IStype(stream) == IST_FILE) ? ((stream)->file.handle->close = (closep)) : 0)
// #define str_of(stream) ((stream)->str.handle)
// #define ssl_socket_of(stream) ((stream)->ssl.handle->sock)
// #define ssl_of(stream) ((stream)->ssl.handle->ssl)

inline InputStreamPtr openIS(const char *path)
{
    return newInputStream(open(path, O_RDONLY));
}
