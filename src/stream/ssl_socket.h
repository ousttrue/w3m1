#pragma once
#include <memory>
#include <string>

class SSLSocket
{
    struct SSLSocketImpl *m_impl = nullptr;
    std::string m_ssl_certificate;

public:
    SSLSocket(SSLSocketImpl *impl, const char *ssl_certificate);
    ~SSLSocket();
};

class SSLContext
{
    struct SSLContextImpl *m_impl = nullptr;

public:
    SSLContext(SSLContextImpl *impl);
    ~SSLContext();
    static std::shared_ptr<SSLContext> Create();

    std::shared_ptr<SSLSocket> Open(int sock, std::string_view hostname);
};
