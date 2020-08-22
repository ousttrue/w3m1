#include <sstream>
#include "open_socket.h"
#include "regex.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

static int openSocket4(
    const std::string &hostname,
    std::string_view remoteport_name, unsigned short remoteport_num)
{
    // if (w3mApp::Instance().fmInitialized)
    // {
    //     /* FIXME: gettextize? */
    //     message(Sprintf("Opening socket...")->ptr, 0, 0);
    //     Screen::Instance().Refresh();
    //     Terminal::flush();
    // }

    if (hostname.empty())
    {
        return -1;
    }

    // unsigned short s_port;
    // unsigned long adr;
    // MySignalHandler prevtrap = NULL;
    // auto success = TrapJmp([&] {
    auto s_port = htons(remoteport_num);

    auto proto = getprotobyname("tcp");
    struct protoent _proto = {0};
    if (!proto)
    {
        /* protocol number of TCP is 6 */
        proto = &_proto;
        proto->p_proto = 6;
    }

    int sock = -1;
    if ((sock = socket(AF_INET, SOCK_STREAM, proto->p_proto)) < 0)
    {
        return -1;
    }

    regexCompile("^[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+$", 0);
    if (regexMatch(hostname))
    {
        //
        // IP Address. ex: 127.0.0.1 like
        //
        int a1, a2, a3, a4;
        sscanf(hostname.c_str(), "%d.%d.%d.%d", &a1, &a2, &a3, &a4);
        auto adr = htonl((a1 << 24) | (a2 << 16) | (a3 << 8) | a4);
        struct sockaddr_in hostaddr = {0};
        bcopy((void *)&adr, (void *)&hostaddr.sin_addr, sizeof(long));
        hostaddr.sin_family = AF_INET;
        hostaddr.sin_port = s_port;
        // if (w3mApp::Instance().fmInitialized)
        // {
        //     message(Sprintf("Connecting to %s", hostname)->ptr, 0, 0);
        //     Screen::Instance().Refresh();
        //     Terminal::flush();
        // }
        if (connect(sock, (struct sockaddr *)&hostaddr,
                    sizeof(struct sockaddr_in)) != 0)
        {
            return -1;
        }

        return sock;
    }
    else
    {
        // char **h_addr_list;
        // if (w3mApp::Instance().fmInitialized)
        // {
        //     message(Sprintf("Performing hostname lookup on %s", hostname)->ptr,
        //             0, 0);
        //     Screen::Instance().Refresh();
        //     Terminal::flush();
        // }
        auto entry = gethostbyname(hostname.c_str());
        if (!entry)
        {
            return -1;
        }

        for (auto h_addr_list = entry->h_addr_list; *h_addr_list; h_addr_list++)
        {
            struct sockaddr_in hostaddr = {0};
            hostaddr.sin_family = AF_INET;
            hostaddr.sin_port = s_port;
            bcopy((void *)h_addr_list[0], (void *)&hostaddr.sin_addr, entry->h_length);
            // if (w3mApp::Instance().fmInitialized)
            // {
            //     message(Sprintf("Connecting to %s", hostname)->ptr, 0, 0);
            //     Screen::Instance().Refresh();
            //     Terminal::flush();
            // }
            if (connect(sock, (struct sockaddr *)&hostaddr,
                        sizeof(struct sockaddr_in)) == 0)
            {
                return sock;
            }
        }

        return -1;
    }
}

struct Socket6
{
    struct addrinfo *res0 = nullptr;

    ~Socket6()
    {
        freeaddrinfo(res0);
    }

    int open(std::string hostname,
             const std::string &remoteport_name, unsigned short remoteport_num, DnsOrderTypes order)
    {
        if (hostname.empty())
        {
            return -1;
        }

        /* rfc2732 compliance */
        if (hostname.size() && hostname.front() == '[' && hostname.back() == ']')
        {
            hostname = hostname.substr(1, hostname.size() - 2);
            if (strspn(hostname.c_str(), "0123456789abcdefABCDEF:.") != hostname.size())
            {
                return -1;
            }
        }

        for (auto af = ai_family_order_table[order];; af++)
        {
            struct addrinfo hints = {0};
            hints.ai_family = *af;
            hints.ai_socktype = SOCK_STREAM;

            int error = -1;
            if (remoteport_num != 0)
            {
                std::stringstream ss;
                ss << remoteport_num;
                // Str portbuf = Sprintf("%d", remoteport_num);
                error = getaddrinfo(hostname.c_str(), ss.str().c_str(), &hints, &res0);
            }

            if (error && remoteport_name.size() && remoteport_name[0] != '\0')
            {
                /* try default port */
                error = getaddrinfo(hostname.c_str(), remoteport_name.c_str(), &hints, &res0);
            }

            if (error == 0)
            {
                for (auto res = res0; res; res = res->ai_next)
                {
                    auto sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
                    if (sock < 0)
                    {
                        break;
                    }
                    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0)
                    {
                        close(sock);
                        break;
                    }

                    return sock;
                }
            }

            if (*af == PF_UNSPEC)
            {
                break;
            }

            /* try next ai family */
        }

        return -1;
    }
};

int openSocket(const char *hostname,
               const char *remoteport_name, unsigned short remoteport_num, DnsOrderTypes order)
{
    return Socket6().open(hostname, remoteport_name, remoteport_num, order);
}

int openSocket(const URL &url, DnsOrderTypes order)
{
    return openSocket(url.host.c_str(), GetScheme(url.scheme)->name.data(), url.port, order);
}
