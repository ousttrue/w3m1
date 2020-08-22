#pragma once
#include "stream/url.h"

enum DnsOrderTypes
{
    DNS_ORDER_UNSPEC = 0,
    DNS_ORDER_INET_INET6 = 1,
    DNS_ORDER_INET6_INET = 2,
    DNS_ORDER_INET_ONLY = 4,
    DNS_ORDER_INET6_ONLY = 6,
};

int openSocket(const char *hostname,
               const char *remoteport_name, unsigned short remoteport_num, DnsOrderTypes order = DNS_ORDER_UNSPEC);
int openSocket(const URL &url, DnsOrderTypes order = DNS_ORDER_UNSPEC);
