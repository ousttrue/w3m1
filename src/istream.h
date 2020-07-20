/* $Id: istream.h,v 1.12 2003/10/20 16:41:56 ukai Exp $ */
#ifndef IO_STREAM_H
#define IO_STREAM_H

#include <stdio.h>
#include <openssl/bio.h>
#include <openssl/x509.h>
#include <openssl/ssl.h>
#include "Str.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

struct StreamBuffer
{
    unsigned char *buf;
    int size, cur, next;
};

typedef void (*FileStreamCloseFunc)(FILE *);
struct filestream_handle
{
    FILE *f;
    FileStreamCloseFunc close;
};

struct ssl_handle
{
    SSL *ssl;
    int sock;
};

struct ens_handle
{
    union InputStream *is;
    Str s;
    int pos;
    char encoding;
};

typedef int (*ReadFunc)(void *, unsigned char *, int);
typedef void (*CloseFunc)(void *);

struct BaseStream
{
    StreamBuffer stream;
    void *handle;
    char type;
    char iseos;
    ReadFunc read;
    CloseFunc close;
};

struct FileStream
{
    StreamBuffer stream;
    struct filestream_handle *handle;
    char type;
    char iseos;
    ReadFunc read;
    CloseFunc close;
};

struct StrStream
{
    StreamBuffer stream;
    Str handle;
    char type;
    char iseos;
    ReadFunc read;
    CloseFunc close;
};

struct SSLStream
{
    StreamBuffer stream;
    struct ssl_handle *handle;
    char type;
    char iseos;
    ReadFunc read;
    CloseFunc close;
};

struct EncodedStrStream
{
    StreamBuffer stream;
    struct ens_handle *handle;
    char type;
    char iseos;
    ReadFunc read;
    CloseFunc close;
};

union InputStream {
    BaseStream base;
    FileStream file;
    StrStream str;
    SSLStream ssl;
    EncodedStrStream ens;
};

extern InputStream *newInputStream(int des);
extern InputStream *newFileStream(FILE *f, FileStreamCloseFunc closep);
extern InputStream *newStrStream(Str s);
extern InputStream *newSSLStream(SSL *ssl, int sock);
extern InputStream *newEncodedStream(InputStream *is, char encoding);
extern int ISclose(InputStream *stream);
extern int ISgetc(InputStream *stream);
extern int ISundogetc(InputStream *stream);
extern Str StrISgets(InputStream *stream);
extern Str StrmyISgets(InputStream *stream);
extern int ISread(InputStream *stream, Str buf, int count);
extern int ISfileno(InputStream *stream);
extern int ISeos(InputStream *stream);
extern void ssl_accept_this_site(char *hostname);
extern Str ssl_get_certificate(SSL *ssl, char *hostname);

#define IST_BASIC 0
#define IST_FILE 1
#define IST_STR 2
#define IST_SSL 3
#define IST_ENCODED 4
#define IST_UNCLOSE 0x10

#define IStype(stream) ((stream)->base.type)
#define is_eos(stream) ISeos(stream)
#define iseos(stream) ((stream)->base.iseos)
#define file_of(stream) ((stream)->file.handle->f)
#define set_close(stream, closep) ((IStype(stream) == IST_FILE) ? ((stream)->file.handle->close = (closep)) : 0)
#define str_of(stream) ((stream)->str.handle)
#define ssl_socket_of(stream) ((stream)->ssl.handle->sock)
#define ssl_of(stream) ((stream)->ssl.handle->ssl)
#define openIS(path) newInputStream(open((path), O_RDONLY))
#endif
