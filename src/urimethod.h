#pragma once
#include "transport/url.h"
#include "frontend/buffer.h"

void initURIMethods();
Str searchURIMethods(ParsedURL *pu);
void chkExternalURIBuffer(BufferPtr buf);
void chkURLBuffer(BufferPtr buf);