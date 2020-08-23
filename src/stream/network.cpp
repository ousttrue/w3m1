#include <plog/Log.h>
#include <string_view_util.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "stream/network.h"

Network::Network()
{
    if (HTTP_proxy.empty())
    {
        for (auto &n : {"HTTP_PROXY", "http_proxy", "HTTP_proxy"})
        {
            auto env = getenv(n);
            if (env)
            {
                HTTP_proxy = env;
                break;
            }
        }
    }
}

Network::~Network()
{
    LOGI << "shutdown...";
}

Network &Network::Instance()
{
    static Network n;
    return n;
}

void Network::ParseProxy()
{
    if (this->HTTP_proxy.size())
    {
        this->HTTP_proxy_parsed = URL::Parse(this->HTTP_proxy, nullptr);
    }

    if (this->HTTPS_proxy.size())
    {
        this->HTTPS_proxy_parsed = URL::Parse(this->HTTPS_proxy, nullptr);
    }

    if (this->NO_proxy.size())
    {
        auto splitter = svu::splitter(this->NO_proxy, [](char c) -> bool {
            return IS_SPACE(c) || c == ',';
        });
        for (auto v : splitter)
        {
            this->NO_proxy_domains.push_back(std::string(v));
        }
    }
}

bool Network::UseProxy(const URL &url)
{
    if (!this->use_proxy)
    {
        return false;
    }
    if (url.scheme == SCM_HTTPS)
    {
        if (this->HTTPS_proxy.empty())
        {
            return false;
        }
    }
    else if (url.scheme == SCM_HTTP)
    {
        if (this->HTTP_proxy.empty())
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

static bool domain_match(std::string_view pat, std::string_view domain)
{
    if (domain.empty())
    {
        return false;
    }
    if (pat.empty())
    {
        return false;
    }

    if (pat[0] == '.')
    {
        pat.remove_prefix(1);
    }
    for (;;)
    {
        if (svu::ic_eq(pat, domain))
        {
            return true;
        }
        auto pos = domain.find('.');
        if (pos == std::string::npos)
        {
            break;
        }
        domain.remove_prefix(pos + 1);
    }
    return false;
}

bool Network::check_no_proxy4(const std::string &domain)
{
    auto he = gethostbyname(domain.c_str());
    if (!he)
    {
        return false;
    }

    for (auto h_addr_list = (unsigned char **)he->h_addr_list; *h_addr_list;
         h_addr_list++)
    {
        std::stringstream addr;
        for (int n = 0; n < he->h_length; n++)
        {
            if (n)
            {
                addr << '.';
            }
            addr << h_addr_list[0][n];
        }
        auto str = addr.str();
        for (auto &d : NO_proxy_domains)
        {
            if (str.starts_with(d))
            {
                return true;
            }
        }
    }

    return false;
}

bool Network::check_no_proxy6(const std::string &domain)
{
    for (auto af = ai_family_order_table[DNS_order];; af++)
    {
        struct addrinfo hints = {0};
        hints.ai_family = *af;

        struct addrinfo *res0;
        auto error = getaddrinfo(domain.data(), NULL, &hints, &res0);
        if (error)
        {
            if (*af == PF_UNSPEC)
            {
                break;
            }
            /* try next */
            continue;
        }

        char addr[4 * 16];
        for (auto res = res0; res != NULL; res = res->ai_next)
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

            std::string_view _addr(addr);
            for (auto &d : this->NO_proxy_domains)
            {
                if (_addr.starts_with(d))
                {
                    freeaddrinfo(res0);
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

    return false;
}

bool Network::check_no_proxy(const std::string &domain)
{
    if (domain.empty())
    {
        return false;
    }

    if (this->NO_proxy_domains.empty())
    {
        return false;
    }
    for (auto &d : this->NO_proxy_domains)
    {
        if (domain_match(d, domain.data()))
        {
            return true;
        }
    }

    if (!NOproxy_netaddr)
    {
        return false;
    }

    /* 
     * to check noproxy by network addr
     */
    // return check_no_proxy4(domain);
    return check_no_proxy6(domain);
}

URL Network::GetProxy(URLSchemeTypes scheme)
{
    switch (scheme)
    {
    case SCM_HTTP:
        return this->HTTP_proxy_parsed;

    case SCM_HTTPS:
        return this->HTTPS_proxy_parsed;
    }

    assert(false);
    return {};
}
