#include "urlfile.h"

URLFile::URLFile(SchemaTypes scm, InputStream *strm)
    : scheme(scm), stream(strm)
{
}
