#pragma once
#include "frontend/buffer.h"
#include "urlfile.h"
#include "stream/http.h"
#include <memory>

extern CharacterEncodingScheme content_charset;
extern CharacterEncodingScheme meta_charset;

struct URL;
struct FormList;
struct URLFile;

struct BufferLoader
{
    int length = 0;
    BufferPtr Load(const URLFilePtr &uf, CharacterEncodingScheme charset);
};
using BufferLoaderPtr = std::shared_ptr<BufferLoader>;

using LoaderFunc = BufferPtr (*)(const URLFilePtr &uf);

BufferPtr loadFile(char *path);
BufferPtr loadBuffer(const URLFilePtr &uf);
BufferPtr loadGeneralFile(const URL &url, const URL *current, HttpReferrerPolicy referer, LoadFlags flag, FormList *request);
int doFileCopy(const char *tmpf, const char *defstr);
BufferPtr loadSomething(const URLFilePtr &f, const char *path, LoaderFunc loadproc);
BufferPtr loadcmdout(char *cmd, LoaderFunc loadproc);
BufferPtr LoadStream(const URLFilePtr &f, const URL &pu, LoadFlags flag);
