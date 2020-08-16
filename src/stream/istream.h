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
private:
    std::vector<unsigned char> buf;
    int cur = 0;
    int next = 0;

public:
    StreamBuffer(int bufsize) : buf(bufsize) {}

    int update(const std::function<int(unsigned char *, int)> &readfunc)
    {
        cur = next = 0;
        int len = readfunc(buf.data(), buf.size());
        next += len;
        return len;
    }

    int undogetc()
    {
        if (cur > 0)
        {
            cur--;
            return 0;
        }
        return -1;
    }

    // LF
    bool try_gets(Str s)
    {
        assert(s);
        if (auto p = (unsigned char *)memchr(&buf[cur], '\n', next - cur))
        {
            // found new line
            int len = p - &buf[cur] + 1;
            s->Push((char *)&buf[cur], len);
            cur += len;
            return true;
        }
        else
        {
            s->Push((char *)&buf[cur], next - cur);
            cur = next;
            return false;
        }
    }

    // CRLF
    bool try_mygets(Str s)
    {
        assert(s);

        if (s && s->Back() == '\r')
        {
            if (buf[cur] == '\n')
                s->Push((char)buf[cur++]);
            return true;
        }
        int i = cur;
        for (;
             i < next && buf[i] != '\n' && buf[i] != '\r';
             i++)
            ;
        if (i < next)
        {
            int len = i - cur + 1;
            s->Push((char *)&buf[cur], len);
            cur = i + 1;
            if (buf[i] == '\n')
                return true;
        }
        else
        {
            s->Push((char *)&buf[cur],
                    next - cur);
            cur = next;
        }

        return false;
    }

    unsigned char POP_CHAR()
    {
        return buf[cur++];
    }

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
    StreamBuffer stream;

protected:
    int m_readsize = 0;
    void do_update();
    InputStream(int size) : stream(size) {}
    virtual ~InputStream() {}
    virtual int ReadFunc(unsigned char *buffer, int size) = 0;

public:
    virtual InputStreamTypes type() const = 0;
    int getc();
    Str gets();
    int read(unsigned char *buffer, int size);
    bool readto(Str buf, int count);
    Str mygets();
    bool eos();

    virtual int FD() const
    {
        return -1;
    }
};
using InputStreamPtr = std::shared_ptr<InputStream>;

// FileDescriptor
class BaseStream : public InputStream
{
    int m_fd = -1;

public:
    BaseStream(int size, int fd) : InputStream(size), m_fd(fd) {}
    ~BaseStream();
    InputStreamTypes type() const override { return IST_BASIC; }
    int ReadFunc(unsigned char *buffer, int size) override;
    int FD() const override;
};

// FILE*
class FileStream : public InputStream
{
    FILE *m_f = nullptr;
    std::function<void(FILE *)> m_close;

public:
    FileStream(int size, FILE *f, const std::function<void(FILE *)> &close)
        : InputStream(size), m_f(f), m_close(close)
    {
    }
    ~FileStream();
    InputStreamTypes type() const override { return IST_FILE; }
    int ReadFunc(unsigned char *buffer, int size) override;
    int FD() const override;
};

class StrStream : public InputStream
{
    Str m_str = nullptr;
    int m_pos = 0;

public:
    StrStream(int size, Str str) : InputStream(size), m_str(str) {}
    ~StrStream();
    InputStreamTypes type() const override { return IST_STR; }
    int ReadFunc(unsigned char *buffer, int size) override;
};

class SSLStream : public InputStream
{
    SSL *m_ssl = nullptr;
    int m_sock = -1;

public:
    SSLStream(int size, SSL *ssl, int sock) : InputStream(size), m_ssl(ssl), m_sock(sock) {}
    ~SSLStream();
    InputStreamTypes type() const override { return IST_SSL; }
    int ReadFunc(unsigned char *buffer, int size) override;
    int FD() const override;
};

struct EncodedStrStream : public InputStream
{
    InputStreamPtr m_is;
    Str m_s = nullptr;
    int m_pos = 0;
    char m_encoding = 0;

public:
    EncodedStrStream(int size, const InputStreamPtr &is, char encoding)
        : InputStream(size), m_is(is), m_encoding(encoding)
    {
    }
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

void ssl_accept_this_site(char *hostname);
Str ssl_get_certificate(SSL *ssl, char *hostname);

inline InputStreamPtr openIS(const char *path)
{
    return newInputStream(open(path, O_RDONLY));
}
