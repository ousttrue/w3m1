#pragma once
#include "frontend/buffer.h"
#include "urlfile.h"

#define NO_REFERER ((char *)-1)

long long GetCurrentContentLength();
extern CharacterEncodingScheme content_charset;
extern CharacterEncodingScheme meta_charset;
extern int frame_source;

struct URL;
struct FormList;
struct URLFile;
BufferPtr loadFile(char *path);
char *checkContentType(BufferPtr buf);
void readHeader(URLFile *uf, BufferPtr newBuf, int thru, URL *pu);

BufferPtr loadBuffer(URLFile *uf, BufferPtr newBuf);

BufferPtr loadGeneralFile(std::string_view path, const URL *current, char *referer, LoadFlags flag, FormList *request);

int save2tmp(URLFile uf, char *tmpf);
int doFileCopy(const char *tmpf, const char *defstr);
