#pragma once
#include "frontend/buffer.h"
#include "urlfile.h"
#include "http/http_request.h"

long long GetCurrentContentLength();
extern CharacterEncodingScheme content_charset;
extern CharacterEncodingScheme meta_charset;
extern int frame_source;

struct URL;
struct FormList;
struct URLFile;
BufferPtr loadFile(char *path);
char *checkContentType(BufferPtr buf);
void readHeader(const URLFilePtr &uf, BufferPtr newBuf, int thru, const URL *pu);

bool loadBuffer(const URLFilePtr &uf, BufferPtr newBuf);

BufferPtr loadGeneralFile(const URL &url, const URL *current, HttpReferrerPolicy referer, LoadFlags flag, FormList *request);

int doFileCopy(const char *tmpf, const char *defstr);
