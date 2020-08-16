#pragma once
#include <memory>
#include <string>

class SSLSocket
{
    struct SSLSocketImpl *m_impl = nullptr;
    std::string m_ssl_certificate;

public:
    SSLSocket(struct SSLSocketImpl *impl, const char *ssl_certificate);
    ~SSLSocket();
    int Write(const void *buf, int num);
    void WriteFromFile(const char *file);
    int Socket() const;
    int Read(unsigned char *p, int size);
    const void *Handle() const;
};

class SSLContext
{
    struct SSLContextImpl *m_impl = nullptr;

public:
    SSLContext(struct SSLContextImpl *impl);
    ~SSLContext();
    static std::shared_ptr<SSLContext> Create();

    std::shared_ptr<SSLSocket> Open(int sock, std::string_view hostname);
};
