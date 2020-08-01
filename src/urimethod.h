#pragma once
#include "transport/url.h"
#include "frontend/buffer.h"

void initURIMethods();
Str searchURIMethods(URL *pu);
void chkExternalURIBuffer(BufferPtr buf);
void chkURLBuffer(BufferPtr buf);