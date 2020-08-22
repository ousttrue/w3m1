#include "indep.h"
#include "regex.h"
#include "open_socket.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "frontend/event.h"
#include "frontend/display.h"
#include "frontend/terminal.h"
#include "frontend/screen.h"

int openSocket4(const char *hostname,
                const char *remoteport_name, unsigned short remoteport_num)
{
    if (w3mApp::Instance().fmInitialized)
    {
        /* FIXME: gettextize? */
        message(Sprintf("Opening socket...")->ptr, 0, 0);
        Screen::Instance().Refresh();
        Terminal::flush();
    }

    if (hostname == NULL)
    {
        return -1;
    }

    int sock = -1;
    struct sockaddr_in hostaddr;
    struct hostent *entry;
    unsigned short s_port;
    int a1, a2, a3, a4;
    unsigned long adr;
    MySignalHandler prevtrap = NULL;
    auto success = TrapJmp([&] {
        s_port = htons(remoteport_num);
        bzero((char *)&hostaddr, sizeof(struct sockaddr_in));
        auto proto = getprotobyname("tcp");
        struct protoent _proto;
        if (!proto)
        {
            /* protocol number of TCP is 6 */
            proto = &_proto;
            bzero(proto, sizeof(struct protoent));
            proto->p_proto = 6;
        }
        if ((sock = socket(AF_INET, SOCK_STREAM, proto->p_proto)) < 0)
        {
            return false;
        }
        regexCompile("^[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+$", 0);
        if (regexMatch(const_cast<char *>(hostname), -1, 1))
        {
            sscanf(hostname, "%d.%d.%d.%d", &a1, &a2, &a3, &a4);
            adr = htonl((a1 << 24) | (a2 << 16) | (a3 << 8) | a4);
            bcopy((void *)&adr, (void *)&hostaddr.sin_addr, sizeof(long));
            hostaddr.sin_family = AF_INET;
            hostaddr.sin_port = s_port;
            if (w3mApp::Instance().fmInitialized)
            {
                message(Sprintf("Connecting to %s", hostname)->ptr, 0, 0);
                Screen::Instance().Refresh();
                Terminal::flush();
            }
            if (connect(sock, (struct sockaddr *)&hostaddr,
                        sizeof(struct sockaddr_in)) < 0)
            {
                return false;
            }
        }
        else
        {
            char **h_addr_list;
            int result = -1;
            if (w3mApp::Instance().fmInitialized)
            {
                message(Sprintf("Performing hostname lookup on %s", hostname)->ptr,
                        0, 0);
                Screen::Instance().Refresh();
                Terminal::flush();
            }
            if ((entry = gethostbyname(hostname)) == NULL)
            {
                return false;
            }
            hostaddr.sin_family = AF_INET;
            hostaddr.sin_port = s_port;
            for (h_addr_list = entry->h_addr_list; *h_addr_list; h_addr_list++)
            {
                bcopy((void *)h_addr_list[0], (void *)&hostaddr.sin_addr,
                      entry->h_length);
                if (w3mApp::Instance().fmInitialized)
                {
                    message(Sprintf("Connecting to %s", hostname)->ptr, 0, 0);
                    Screen::Instance().Refresh();
                    Terminal::flush();
                }
                if ((result = connect(sock, (struct sockaddr *)&hostaddr,
                                      sizeof(struct sockaddr_in))) == 0)
                {
                    break;
                }
            }
            if (result < 0)
            {
                return false;
            }
        }

        return true;
    });

    return success ? sock : -1;
}

int openSocket6(const char *hostname,
                const char *remoteport_name, unsigned short remoteport_num)
{
    int sock = -1;
    int *af;
    struct addrinfo hints, *res0, *res;
    int error;
    char *hname;
    MySignalHandler prevtrap = NULL;

    if (w3mApp::Instance().fmInitialized)
    {
        /* FIXME: gettextize? */
        message(Sprintf("Opening socket...")->ptr, 0, 0);
        Screen::Instance().Refresh();
        Terminal::flush();
    }

    if (hostname == NULL)
    {
        return -1;
    }

    auto success = TrapJmp([&] {
        /* rfc2732 compliance */
        hname = const_cast<char *>(hostname);
        if (hname != NULL && hname[0] == '[' && hname[strlen(hname) - 1] == ']')
        {
            hname = allocStr(hostname + 1, -1);
            hname[strlen(hname) - 1] = '\0';
            if (strspn(hname, "0123456789abcdefABCDEF:.") != strlen(hname))
                return false;
        }
        for (af = ai_family_order_table[w3mApp::Instance().DNS_order];; af++)
        {
            memset(&hints, 0, sizeof(hints));
            hints.ai_family = *af;
            hints.ai_socktype = SOCK_STREAM;
            if (remoteport_num != 0)
            {
                Str portbuf = Sprintf("%d", remoteport_num);
                error = getaddrinfo(hname, portbuf->ptr, &hints, &res0);
            }
            else
            {
                error = -1;
            }
            if (error && remoteport_name && remoteport_name[0] != '\0')
            {
                /* try default port */
                error = getaddrinfo(hname, remoteport_name, &hints, &res0);
            }
            if (error)
            {
                if (*af == PF_UNSPEC)
                {
                    return false;
                }
                /* try next ai family */
                continue;
            }

            sock = -1;
            for (res = res0; res; res = res->ai_next)
            {
                sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
                if (sock < 0)
                {
                    continue;
                }
                if (connect(sock, res->ai_addr, res->ai_addrlen) < 0)
                {
                    close(sock);
                    sock = -1;
                    continue;
                }
                break;
            }
            if (sock < 0)
            {
                freeaddrinfo(res0);
                if (*af == PF_UNSPEC)
                {
                    return false;
                }
                /* try next ai family */
                continue;
            }
            freeaddrinfo(res0);
            break;
        }

        return true;
    });

    return success ? sock : -1;
}

int openSocket(const char *hostname,
               const char *remoteport_name, unsigned short remoteport_num)
{
    return openSocket6(hostname, remoteport_name, remoteport_num);
}

int openSocket(const URL &url)
{
    return openSocket(url.host.c_str(), GetScheme(url.scheme)->name.data(), url.port);
}
