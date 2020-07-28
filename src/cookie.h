#pragma once
#include <string_view>
#include "parsetagx.h"
#include "url.h"

#define COO_USE 1
#define COO_SECURE 2
#define COO_DOMAIN 4
#define COO_PATH 8
#define COO_DISCARD 16
#define COO_OVERRIDE 32 /* user chose to override security checks */

#define COO_OVERRIDE_OK 32                  /* flag to specify that an error is overridable */
                                            /* version 0 refers to the original cookie_spec.html */
                                            /* version 1 refers to RFC 2109 */
                                            /* version 1' refers to the Internet draft to obsolete RFC 2109 */
#define COO_EINTERNAL (1)                   /* unknown error; probably forgot to convert "return 1" in cookie.c */
#define COO_ETAIL (2 | COO_OVERRIDE_OK)     /* tail match failed (version 0) */
#define COO_ESPECIAL (3)                    /* special domain check failed (version 0) */
#define COO_EPATH (4)                       /* Path attribute mismatch (version 1 case 1) */
#define COO_ENODOT (5 | COO_OVERRIDE_OK)    /* no embedded dots in Domain (version 1 case 2.1) */
#define COO_ENOTV1DOM (6 | COO_OVERRIDE_OK) /* Domain does not start with a dot (version 1 case 2.2) */
#define COO_EDOM (7 | COO_OVERRIDE_OK)      /* domain-match failed (version 1 case 3) */
#define COO_EBADHOST (8 | COO_OVERRIDE_OK)  /* dot in matched host name in FQDN (version 1 case 4) */
#define COO_EPORT (9)                       /* Port match failed (version 1' case 5) */
#define COO_EMAX COO_EPORT

void set_cookie_flag(struct parsed_tagarg *arg);
Str find_cookie(const ParsedURL *pu);
int add_cookie(ParsedURL *pu, Str name, Str value, time_t expires,
               Str domain, Str path, int flag, Str comment, int version,
               Str port, Str commentURL);
void save_cookies(void);
void initCookie(void);
int check_cookie_accept_domain(std::string_view domain);
