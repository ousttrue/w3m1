#pragma once
#include <memory>
#include "html/html.h"
#define MAX_ENV_LEVEL 20

// entry point
struct Buffer;
using BufferPtr = std::shared_ptr<Buffer>;
void loadHTMLstream(struct URLFile *f, BufferPtr newBuf, FILE *src, int internal);
BufferPtr loadHTMLBuffer(URLFile *f, BufferPtr newBuf);
BufferPtr loadHTMLString(Str page);
