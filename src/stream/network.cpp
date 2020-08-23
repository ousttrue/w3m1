#include <plog/Log.h>
#include "stream/network.h"
#include "textlist.h"
#include "indep.h"
#include "frontend/event.h"
#include <netdb.h>
#include <arpa/inet.h>

Network::Network()
{
    NO_proxy_domains = newTextList();

    //
    // load env
    //
    if (HTTP_proxy.empty())
    {
        HTTP_proxy = get_non_null_env("HTTP_PROXY", "http_proxy", "HTTP_proxy");
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

// #define set_no_proxy(domains) (Network::Instance().NO_proxy_domains = make_domain_list(domains))
void Network::ParseProxy()
{
    if (Network::Instance().HTTP_proxy.size())
        Network::Instance().HTTP_proxy_parsed = URL::Parse(Network::Instance().HTTP_proxy, nullptr);

    if (Network::Instance().HTTPS_proxy.size())
        Network::Instance().HTTPS_proxy_parsed = URL::Parse(Network::Instance().HTTPS_proxy, nullptr);

    if (non_null(Network::Instance().NO_proxy))
        (Network::Instance().NO_proxy_domains = make_domain_list(Network::Instance().NO_proxy));
        // set_no_proxy(Network::Instance().NO_proxy);
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

bool Network::check_no_proxy(std::string_view domain)
{
    TextListItem *tl;
    int ret = 0;
    MySignalHandler prevtrap = NULL;

    if (this->NO_proxy_domains == NULL || this->NO_proxy_domains->nitem == 0 ||
        domain == NULL)
        return 0;
    for (tl = this->NO_proxy_domains->first; tl != NULL; tl = tl->next)
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
                    for (tl = this->NO_proxy_domains->first; tl != NULL; tl = tl->next)
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

URL Network::GetProxy(URLSchemeTypes scheme)
{
    switch (scheme)
    {
    case SCM_HTTP:
        return Network::Instance().HTTP_proxy_parsed;

    case SCM_HTTPS:
        return Network::Instance().HTTPS_proxy_parsed;
    }

    assert(false);
    return {};
}
