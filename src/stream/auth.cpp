#include "stream/auth.h"

#include "myctype.h"
#include "indep.h"
#include "mime/mimeencoding.h"
#include "stream/http.h"
#include "html/form.h"
#include "frontend/display.h"
#include "frontend/terms.h"
#include "frontend/lineinput.h"
#include "public.h"
#include "textlist.h"
#include <unistd.h>

enum
{
    AUTHCHR_NUL,
    AUTHCHR_SEP,
    AUTHCHR_TOKEN,
};

static int
skip_auth_token(char **pp)
{
    char *p;
    int first = AUTHCHR_NUL, typ;

    for (p = *pp;; ++p)
    {
        switch (*p)
        {
        case '\0':
            goto endoftoken;
        default:
            if ((unsigned char)*p > 037)
            {
                typ = AUTHCHR_TOKEN;
                break;
            }
            /* thru */
        case '\177':
        case '[':
        case ']':
        case '(':
        case ')':
        case '<':
        case '>':
        case '@':
        case ';':
        case ':':
        case '\\':
        case '"':
        case '/':
        case '?':
        case '=':
        case ' ':
        case '\t':
        case ',':
            typ = AUTHCHR_SEP;
            break;
        }

        if (!first)
            first = typ;
        else if (first != typ)
            break;
    }
endoftoken:
    *pp = p;
    return first;
}

static Str
extract_auth_val(char **q)
{
    unsigned char *qq = *(unsigned char **)q;
    int quoted = 0;
    Str val = Strnew();

    SKIP_BLANKS(&qq);
    if (*qq == '"')
    {
        quoted = true;
        val->Push(*qq++);
    }
    while (*qq != '\0')
    {
        if (quoted && *qq == '"')
        {
            val->Push(*qq++);
            break;
        }
        if (!quoted)
        {
            switch (*qq)
            {
            case '[':
            case ']':
            case '(':
            case ')':
            case '<':
            case '>':
            case '@':
            case ';':
            case ':':
            case '\\':
            case '"':
            case '/':
            case '?':
            case '=':
            case ' ':
            case '\t':
                qq++;
            case ',':
                goto end_token;
            default:
                if (*qq <= 037 || *qq == 0177)
                {
                    qq++;
                    goto end_token;
                }
            }
        }
        else if (quoted && *qq == '\\')
            val->Push(*qq++);
        val->Push(*qq++);
    }
end_token:
    *q = (char *)qq;
    return val;
}

Str qstr_unquote(Str s)
{
    char *p;

    if (s == NULL)
        return NULL;
    p = s->ptr;
    if (*p == '"')
    {
        Str tmp = Strnew();
        for (p++; *p != '\0'; p++)
        {
            if (*p == '\\')
                p++;
            tmp->Push(*p);
        }
        if (tmp->Back() == '"')
            tmp->Pop(1);
        return tmp;
    }
    else
        return s;
}

static char *
extract_auth_param(char *q, struct auth_param *auth)
{
    struct auth_param *ap;
    char *p;

    for (ap = auth; ap->name != NULL; ap++)
    {
        ap->val = NULL;
    }

    while (*q != '\0')
    {
        SKIP_BLANKS(&q);
        for (ap = auth; ap->name != NULL; ap++)
        {
            size_t len;

            len = strlen(ap->name);
            if (strncasecmp(q, ap->name, len) == 0 &&
                (IS_SPACE(q[len]) || q[len] == '='))
            {
                p = q + len;
                SKIP_BLANKS(&p);
                if (*p != '=')
                    return q;
                q = p + 1;
                ap->val = extract_auth_val(&q);
                break;
            }
        }
        if (ap->name == NULL)
        {
            /* skip unknown param */
            int token_type;
            p = q;
            if ((token_type = skip_auth_token(&q)) == AUTHCHR_TOKEN &&
                (IS_SPACE(*q) || *q == '='))
            {
                SKIP_BLANKS(&q);
                if (*q != '=')
                    return p;
                q++;
                extract_auth_val(&q);
            }
            else
                return p;
        }
        if (*q != '\0')
        {
            SKIP_BLANKS(&q);
            if (*q == ',')
                q++;
            else
                break;
        }
    }
    return q;
}

Str get_auth_param(struct auth_param *auth, char *name)
{
    struct auth_param *ap;
    for (ap = auth; ap->name != NULL; ap++)
    {
        if (strcasecmp(name, ap->name) == 0)
            return ap->val;
    }
    return NULL;
}

static Str
AuthBasicCred(struct http_auth *ha, Str uname, Str pw, URL *pu,
              HttpRequest *hr, FormList *request)
{
    Str s = uname->Clone();
    s->Push(':');
    s->Push(pw);
    return Strnew_m_charp("Basic ", encodeB(s->ptr)->ptr, NULL);
}

#ifdef USE_DIGEST_AUTH
#include <openssl/md5.h>

/* RFC2617: 3.2.2 The Authorization Request Header
 * 
 * credentials      = "Digest" digest-response
 * digest-response  = 1#( username | realm | nonce | digest-uri
 *                    | response | [ algorithm ] | [cnonce] |
 *                     [opaque] | [message-qop] |
 *                         [nonce-count]  | [auth-param] )
 *
 * username         = "username" "=" username-value
 * username-value   = quoted-string
 * digest-uri       = "uri" "=" digest-uri-value
 * digest-uri-value = request-uri   ; As specified by HTTP/1.1
 * message-qop      = "qop" "=" qop-value
 * cnonce           = "cnonce" "=" cnonce-value
 * cnonce-value     = nonce-value
 * nonce-count      = "nc" "=" nc-value
 * nc-value         = 8LHEX
 * response         = "response" "=" request-digest
 * request-digest = <"> 32LHEX <">
 * LHEX             =  "0" | "1" | "2" | "3" |
 *                     "4" | "5" | "6" | "7" |
 *                     "8" | "9" | "a" | "b" |
 *                     "c" | "d" | "e" | "f"
 */

#include <openssl/md5.h>

static Str
digest_hex(unsigned char *p)
{
    auto *h = "0123456789abcdef";
    Str tmp = Strnew_size(MD5_DIGEST_LENGTH * 2 + 1);
    int i;
    for (i = 0; i < MD5_DIGEST_LENGTH; i++, p++)
    {
        tmp->Push(h[(*p >> 4) & 0x0f]);
        tmp->Push(h[*p & 0x0f]);
    }
    return tmp;
}

enum
{
    QOP_NONE,
    QOP_AUTH,
    QOP_AUTH_INT,
};

static Str
AuthDigestCred(struct http_auth *ha, Str uname, Str pw, URL *pu,
               HttpRequest *hr, FormList *request)
{
    Str tmp, a1buf, a2buf, rd, s;
    unsigned char md5[MD5_DIGEST_LENGTH + 1];
    Str uri = pu->ToStr(true, false);
    char nc[] = "00000001";

    Str algorithm = qstr_unquote(get_auth_param(ha->param, "algorithm"));
    Str nonce = qstr_unquote(get_auth_param(ha->param, "nonce"));
    Str cnonce /* = qstr_unquote(get_auth_param(ha->param, "cnonce")) */;
    /* cnonce is what client should generate. */
    Str qop = qstr_unquote(get_auth_param(ha->param, "qop"));

    static union {
        int r[4];
        unsigned char s[sizeof(int) * 4];
    } cnonce_seed;
    int qop_i = QOP_NONE;

    cnonce_seed.r[0] = rand();
    cnonce_seed.r[1] = rand();
    cnonce_seed.r[2] = rand();
    MD5((const unsigned char *)cnonce_seed.s, sizeof(cnonce_seed.s), md5);
    cnonce = digest_hex(md5);
    cnonce_seed.r[3]++;

    if (qop)
    {
        char *p;
        size_t i;

        p = qop->ptr;
        SKIP_BLANKS(&p);

        for (;;)
        {
            if ((i = strcspn(p, " \t,")) > 0)
            {
                if (i == sizeof("auth-int") - sizeof("") && !strncasecmp(p, "auth-int", i))
                {
                    if (qop_i < QOP_AUTH_INT)
                        qop_i = QOP_AUTH_INT;
                }
                else if (i == sizeof("auth") - sizeof("") && !strncasecmp(p, "auth", i))
                {
                    if (qop_i < QOP_AUTH)
                        qop_i = QOP_AUTH;
                }
            }

            if (p[i])
            {
                p += i + 1;
                SKIP_BLANKS(&p);
            }
            else
                break;
        }
    }

    /* A1 = unq(username-value) ":" unq(realm-value) ":" passwd */
    tmp = Strnew_m_charp(uname->ptr, ":",
                         qstr_unquote(get_auth_param(ha->param, "realm"))->ptr,
                         ":", pw->ptr, NULL);
    MD5((const unsigned char *)tmp->ptr, strlen(tmp->ptr), md5);
    a1buf = digest_hex(md5);

    if (algorithm)
    {
        if (strcasecmp(algorithm->ptr, "MD5-sess") == 0)
        {
            /* A1 = H(unq(username-value) ":" unq(realm-value) ":" passwd)
             *      ":" unq(nonce-value) ":" unq(cnonce-value)
             */
            if (nonce == NULL)
                return NULL;
            tmp = Strnew_m_charp(a1buf->ptr, ":",
                                 qstr_unquote(nonce)->ptr,
                                 ":", qstr_unquote(cnonce)->ptr, NULL);
            MD5((const unsigned char *)tmp->ptr, strlen(tmp->ptr), md5);
            a1buf = digest_hex(md5);
        }
        else if (strcasecmp(algorithm->ptr, "MD5") == 0)
            /* ok default */
            ;
        else
            /* unknown algorithm */
            return NULL;
    }

    /* A2 = Method ":" digest-uri-value */
    tmp = Strnew_m_charp(hr->Method(), ":", uri->ptr, NULL);
    if (qop_i == QOP_AUTH_INT)
    {
        /*  A2 = Method ":" digest-uri-value ":" H(entity-body) */
        if (request && request->body)
        {
            if (request->method == FORM_METHOD_POST && request->enctype == FORM_ENCTYPE_MULTIPART)
            {
                FILE *fp = fopen(request->body, "r");
                if (fp != NULL)
                {
                    Str ebody;
                    ebody = Strfgetall(fp);
                    MD5((const unsigned char *)ebody->ptr, strlen(ebody->ptr), md5);
                }
                else
                {
                    MD5((const unsigned char *)"", 0, md5);
                }
            }
            else
            {
                MD5((const unsigned char *)request->body, request->length, md5);
            }
        }
        else
        {
            MD5((const unsigned char *)"", 0, md5);
        }
        tmp->Push(':');
        tmp->Push(digest_hex(md5));
    }
    MD5((const unsigned char *)tmp->ptr, strlen(tmp->ptr), md5);
    a2buf = digest_hex(md5);

    if (qop_i >= QOP_AUTH)
    {
        /* request-digest  = <"> < KD ( H(A1),     unq(nonce-value)
         *                      ":" nc-value
         *                      ":" unq(cnonce-value)
         *                      ":" unq(qop-value)
         *                      ":" H(A2)
         *                      ) <">
         */
        if (nonce == NULL)
            return NULL;
        tmp = Strnew_m_charp(a1buf->ptr, ":", qstr_unquote(nonce)->ptr,
                             ":", nc,
                             ":", qstr_unquote(cnonce)->ptr,
                             ":", qop_i == QOP_AUTH ? "auth" : "auth-int",
                             ":", a2buf->ptr, NULL);
        MD5((const unsigned char *)tmp->ptr, strlen(tmp->ptr), md5);
        rd = digest_hex(md5);
    }
    else
    {
        /* compatibility with RFC 2069
         * request_digest = KD(H(A1),  unq(nonce), H(A2))
         */
        tmp = Strnew_m_charp(a1buf->ptr, ":",
                             qstr_unquote(get_auth_param(ha->param, "nonce"))->ptr, ":", a2buf->ptr, NULL);
        MD5((const unsigned char *)tmp->ptr, strlen(tmp->ptr), md5);
        rd = digest_hex(md5);
    }

    /*
     * digest-response  = 1#( username | realm | nonce | digest-uri
     *                          | response | [ algorithm ] | [cnonce] |
     *                          [opaque] | [message-qop] |
     *                          [nonce-count]  | [auth-param] )
     */

    tmp = Strnew_m_charp("Digest username=\"", uname->ptr, "\"", NULL);
    Strcat_m_charp(tmp, ", realm=",
                   get_auth_param(ha->param, "realm")->ptr, NULL);
    Strcat_m_charp(tmp, ", nonce=",
                   get_auth_param(ha->param, "nonce")->ptr, NULL);
    Strcat_m_charp(tmp, ", uri=\"", uri->ptr, "\"", NULL);
    Strcat_m_charp(tmp, ", response=\"", rd->ptr, "\"", NULL);

    if (algorithm)
        Strcat_m_charp(tmp, ", algorithm=",
                       get_auth_param(ha->param, "algorithm")->ptr, NULL);

    if (cnonce)
        Strcat_m_charp(tmp, ", cnonce=\"", cnonce->ptr, "\"", NULL);

    if ((s = get_auth_param(ha->param, "opaque")) != NULL)
        Strcat_m_charp(tmp, ", opaque=", s->ptr, NULL);

    if (qop_i >= QOP_AUTH)
    {
        Strcat_m_charp(tmp, ", qop=",
                       qop_i == QOP_AUTH ? "auth" : "auth-int",
                       NULL);
        /* XXX how to count? */
        /* Since nonce is unique up to each *-Authenticate and w3m does not re-use *-Authenticate: headers,
           nonce-count should be always "00000001". */
        Strcat_m_charp(tmp, ", nc=", nc, NULL);
    }

    return tmp;
}
#endif

/* *INDENT-OFF* */
struct auth_param none_auth_param[] = {
    {NULL, NULL}};

struct auth_param basic_auth_param[] = {
    {"realm", NULL},
    {NULL, NULL}};

#ifdef USE_DIGEST_AUTH
/* RFC2617: 3.2.1 The WWW-Authenticate Response Header
 * challenge        =  "Digest" digest-challenge
 * 
 * digest-challenge  = 1#( realm | [ domain ] | nonce |
 *                       [ opaque ] |[ stale ] | [ algorithm ] |
 *                        [ qop-options ] | [auth-param] )
 *
 * domain            = "domain" "=" <"> URI ( 1*SP URI ) <">
 * URI               = absoluteURI | abs_path
 * nonce             = "nonce" "=" nonce-value
 * nonce-value       = quoted-string
 * opaque            = "opaque" "=" quoted-string
 * stale             = "stale" "=" ( "true" | "false" )
 * algorithm         = "algorithm" "=" ( "MD5" | "MD5-sess" |
 *                        token )
 * qop-options       = "qop" "=" <"> 1#qop-value <">
 * qop-value         = "auth" | "auth-int" | token
 */
struct auth_param digest_auth_param[] = {
    {"realm", NULL},
    {"domain", NULL},
    {"nonce", NULL},
    {"opaque", NULL},
    {"stale", NULL},
    {"algorithm", NULL},
    {"qop", NULL},
    {NULL, NULL}};
#endif
/* for RFC2617: HTTP Authentication */
struct http_auth www_auth[] = {
    {1, "Basic ", basic_auth_param, AuthBasicCred},
#ifdef USE_DIGEST_AUTH
    {10, "Digest ", digest_auth_param, AuthDigestCred},
#endif
    {
        0,
        NULL,
        NULL,
        NULL,
    }};
/* *INDENT-ON* */

http_auth *findAuthentication(struct http_auth *hauth, BufferPtr buf, char *auth_field)
{
    struct http_auth *ha;
    int len = strlen(auth_field), slen;
    TextListItem *i;
    char *p0, *p;

    bzero(hauth, sizeof(struct http_auth));
    // TODO:
    // for (i = buf->document_header->first; i != NULL; i = i->next)
    // {
    //     if (strncasecmp(i->ptr, auth_field, len) == 0)
    //     {
    //         for (p = i->ptr + len; p != NULL && *p != '\0';)
    //         {
    //             SKIP_BLANKS(&p);
    //             p0 = p;
    //             for (ha = &www_auth[0]; ha->scheme != NULL; ha++)
    //             {
    //                 slen = strlen(ha->scheme);
    //                 if (strncasecmp(p, ha->scheme, slen) == 0)
    //                 {
    //                     p += slen;
    //                     SKIP_BLANKS(&p);
    //                     if (hauth->pri < ha->pri)
    //                     {
    //                         *hauth = *ha;
    //                         p = extract_auth_param(p, hauth->param);
    //                         break;
    //                     }
    //                     else
    //                     {
    //                         /* weak auth */
    //                         p = extract_auth_param(p, none_auth_param);
    //                     }
    //                 }
    //             }
    //             if (p0 == p)
    //             {
    //                 /* all unknown auth failed */
    //                 int token_type;
    //                 if ((token_type = skip_auth_token(&p)) == AUTHCHR_TOKEN && IS_SPACE(*p))
    //                 {
    //                     SKIP_BLANKS(&p);
    //                     p = extract_auth_param(p, none_auth_param);
    //                 }
    //                 else
    //                     break;
    //             }
    //         }
    //     }
    // }
    return hauth->scheme ? hauth : NULL;
}

void
getAuthCookie(struct http_auth *hauth, char *auth_header,
              TextList *extra_header, URL *pu, HttpRequest *hr,
              FormList *request,
              Str *uname, Str *pwd)
{
    Str ss = NULL;
    Str tmp;
    TextListItem *i;
    int a_found;
    int auth_header_len = strlen(auth_header);
    char *realm = NULL;
    int proxy;

    if (hauth)
        realm = qstr_unquote(get_auth_param(hauth->param, "realm"))->ptr;

    if (!realm)
        return;

    a_found = false;
    for (i = extra_header->first; i != NULL; i = i->next)
    {
        if (!strncasecmp(i->ptr, auth_header, auth_header_len))
        {
            a_found = true;
            break;
        }
    }
    proxy = !strncasecmp("Proxy-Authorization:", auth_header,
                         auth_header_len);
    if (a_found)
    {
        /* This means that *-Authenticate: header is received after
         * Authorization: header is sent to the server. 
         */
        if (w3mApp::Instance().fmInitialized)
        {
            message("Wrong username or password", 0, 0);
            refresh();
        }
        else
            fprintf(stderr, "Wrong username or password\n");
        sleep(1);
        /* delete Authenticate: header from extra_header */
        delText(extra_header, (char *)i);
        invalidate_auth_user_passwd(pu, realm, *uname, *pwd, proxy);
    }
    *uname = NULL;
    *pwd = NULL;

    if (!a_found && find_auth_user_passwd(pu, realm, (Str *)uname, (Str *)pwd,
                                          proxy))
    {
        /* found username & password in passwd file */;
    }
    else
    {
        if (w3mApp::Instance().QuietMessage)
            return;
        /* input username and password */
        sleep(2);
        if (w3mApp::Instance().fmInitialized)
        {
            char *pp;
            term_raw();
            /* FIXME: gettextize? */
            if ((pp = inputStr(Sprintf("Username for %s: ", realm)->ptr,
                               NULL)) == NULL)
                return;
            *uname = Str_conv_to_system(Strnew(pp));
            if ((pp = inputLine(Sprintf("Password for %s: ", realm)->ptr, NULL,
                                IN_PASSWORD)) == NULL)
            {
                *uname = NULL;
                return;
            }
            *pwd = Str_conv_to_system(Strnew(pp));
            term_cbreak();
        }
        else
        {
            /*
             * If post file is specified as '-', stdin is closed at this
             * point.
             * In this case, w3m cannot read username from stdin.
             * So exit with error message.
             * (This is same behavior as lwp-request.)
             */
            if (feof(stdin) || ferror(stdin))
            {
                /* FIXME: gettextize? */
                fprintf(stderr, "w3m: Authorization required for %s\n",
                        realm);
                exit(1);
            }

            /* FIXME: gettextize? */
            printf(proxy ? "Proxy Username for %s: " : "Username for %s: ",
                   realm);
            fflush(stdout);
            *uname = Strfgets(stdin);
            StripRight(*uname);
#ifdef HAVE_GETPASSPHRASE
            *pwd = Strnew((char *)
                              getpassphrase(proxy ? "Proxy Password: " : "Password: "));
#else
#ifndef __MINGW32_VERSION
            *pwd = Strnew((char *)
                              getpass(proxy ? "Proxy Password: " : "Password: "));
#else
            term_raw();
            *pwd = Strnew((char *)
                              inputLine(proxy ? "Proxy Password: " : "Password: ", NULL, IN_PASSWORD));
            term_cbreak();
#endif /* __MINGW32_VERSION */
#endif
        }
    }
    ss = hauth->cred(hauth, *uname, *pwd, pu, hr, request);
    if (ss)
    {
        tmp = Strnew(auth_header);
        Strcat_m_charp(tmp, " ", ss->ptr, "\r\n", NULL);
        pushText(extra_header, tmp->ptr);
    }
    else
    {
        *uname = NULL;
        *pwd = NULL;
    }
    return;
}
