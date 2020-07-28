#pragma once
#include "buffer.h"
#include "wc_types.h"

long long GetCurrentContentLength();
extern wc_ces content_charset;
extern wc_ces meta_charset;
extern int frame_source;
char *check_accept_charset(char *ac);
char *check_charset(char *p);

struct ParsedURL;
struct FormList;
struct URLFile;
BufferPtr loadFile(char *path);
char *checkContentType(BufferPtr buf);
void readHeader(URLFile *uf, BufferPtr newBuf, int thru, ParsedURL *pu);

BufferPtr loadBuffer(URLFile *uf, BufferPtr newBuf);
BufferPtr loadGeneralFile(char *path, const ParsedURL *current, char *referer, int flag, FormList *request);
int save2tmp(URLFile uf, char *tmpf);
int doFileCopy(const char *tmpf, const char *defstr);
