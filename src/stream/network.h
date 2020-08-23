#pragma once
#include "url.h"
#include <vector>
#ifndef RC_DIR
#define RC_DIR "~/.w3m"
#endif

enum DefaultUrlTypes
{
    DEFAULT_URL_EMPTY = 0,
    DEFAULT_URL_CURRENT = 1,
    DEFAULT_URL_LINK = 2,
};

//
// singleton for network utility
//
class Network
{
    Network();
    ~Network();

    Network(const Network &) = delete;
    Network &operator=(const Network &) = delete;

    // proxy
    std::vector<std::string> NO_proxy_domains;
    URL HTTP_proxy_parsed;
    URL HTTPS_proxy_parsed;
    URL FTP_proxy_parsed;
    bool check_no_proxy(const std::string &domain);
    bool check_no_proxy4(const std::string &domain);
    bool check_no_proxy6(const std::string &domain);

public:
    //
    // Network
    //
    std::string passwd_file = RC_DIR "/passwd";
    bool disable_secret_security_check = false;
    std::string ftppasswd;
    bool ftppass_hostnamegen = true;
    std::string pre_form_file = RC_DIR "/pre_form";
    std::string UserAgent;
    bool NoSendReferer = false;
    std::string AcceptLang;
    std::string AcceptEncoding;
    std::string AcceptMedia;
    bool ArgvIsURL = false;
    bool retryAsHttp = true;
    DefaultUrlTypes DefaultURLString = DEFAULT_URL_EMPTY;
    int FollowRedirection = 10;
    bool MetaRefresh = false;
    int DNS_order = 0;

    //
    // Proxy
    //
    bool use_proxy = true;
    std::string HTTP_proxy;
    std::string HTTPS_proxy;
    std::string FTP_proxy;
    std::string NO_proxy;
    bool NOproxy_netaddr = true;
    bool NoCache = false;

    //
    // SSL
    //
    std::string ssl_forbid_method;
    bool ssl_verify_server = false;
    std::string ssl_cert_file;
    std::string ssl_key_file;
    std::string ssl_ca_path;
    std::string ssl_ca_file;

    static Network &Instance();
    void ParseProxy();
    bool UseProxy(const URL &url);
    URL GetProxy(URLSchemeTypes scheme);
    std::string FQDN(const std::string &host);
};
