#pragma once
#include "buffer.h"

struct ParsedURL;
struct FormList;
BufferPtr loadGeneralFile(char *path, const ParsedURL *current, char *referer, int flag, FormList *request);
