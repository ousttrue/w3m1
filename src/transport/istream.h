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
#include <functional>

struct StreamBuffer
{
    unsigned char *buf;
    int size, cur, next;

    void initialize(char *buf, int bufsize);

    int buffer_read(char *obuf, int count)
    {
        int len = next - cur;
        if (len > 0)
        {
            if (len > count)
                len = count;
            bcopy((const void *)&buf[cur], obuf, len);
            cur += len;
        }
        return len;
    }

    bool MUST_BE_UPDATED() const
    {
        return cur == next;
    }
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
    bool iseos = false;

protected:
    int m_readsize = 0;
    void do_update();

public:
    int getc();
    Str gets();
    int read(Str buf, int count);
    Str mygets();
    StreamBuffer stream = {};
    bool eos();
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
    FILE *m_f = nullptr;
    std::function<void(FILE *)> m_close;

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
InputStreamPtr newFileStream(FILE *f, const std::function<void(FILE *)> &closep);
InputStreamPtr newStrStream(Str s);
InputStreamPtr newSSLStream(SSL *ssl, int sock);
InputStreamPtr newEncodedStream(InputStreamPtr is, char encoding);
// int ISclose(InputStreamPtr stream);
// int ISgetc(InputStreamPtr stream);
// int ISundogetc(InputStreamPtr stream);
// Str StrISgets(InputStreamPtr stream);
// Str StrmyISgets(InputStreamPtr stream);
// int ISread(InputStreamPtr stream, Str buf, int count);
// int ISeos(InputStreamPtr stream);
void ssl_accept_this_site(char *hostname);
Str ssl_get_certificate(SSL *ssl, char *hostname);

// #define IST_UNCLOSE 0x10

#define IStype(stream) ((stream)->type())
#define is_eos(stream) ISeos(stream)
// #define iseos(stream) ((stream)->iseos)
// #define file_of(stream) ((stream)->file.handle->f)
// #define set_close(stream, closep) ((IStype(stream) == IST_FILE) ? ((stream)->file.handle->close = (closep)) : 0)
// #define str_of(stream) ((stream)->str.handle)
// #define ssl_socket_of(stream) ((stream)->ssl.handle->sock)
// #define ssl_of(stream) ((stream)->ssl.handle->ssl)

inline InputStreamPtr openIS(const char *path)
{
    return newInputStream(open(path, O_RDONLY));
}
