#include "urlfile.h"

void init_stream(URLFile *uf, SchemaTypes scheme, InputStream *stream)
{
    memset(uf, 0, sizeof(URLFile));
    uf->stream = stream;
    uf->scheme = scheme;
    uf->encoding = ENC_7BIT;
    uf->is_cgi = 0;
    uf->compression = CMP_NOCOMPRESS;
    uf->content_encoding = CMP_NOCOMPRESS;
    uf->guess_type = NULL;
    uf->ext = NULL;
    uf->modtime = -1;
}
