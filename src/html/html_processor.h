#pragma once
#include <memory>
#include "html/html.h"
#include "stream/urlfile.h"
#define MAX_ENV_LEVEL 20

// entry point
struct Buffer;
using BufferPtr = std::shared_ptr<Buffer>;
void loadHTMLstream(const URLFilePtr &f, BufferPtr newBuf, FILE *src, int internal);
BufferPtr loadHTMLBuffer(const URLFilePtr &f);
BufferPtr loadHTMLString(Str page);
