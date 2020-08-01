#pragma once
#include "frontend/buffer.h"
#include "urlfile.h"

long long GetCurrentContentLength();
extern CharacterEncodingScheme content_charset;
extern CharacterEncodingScheme meta_charset;
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

BufferPtr loadGeneralFile(std::string_view path, const ParsedURL *current, char *referer, LoadFlags flag, FormList *request);

int save2tmp(URLFile uf, char *tmpf);
int doFileCopy(const char *tmpf, const char *defstr);
