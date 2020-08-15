#include "stream/http.h"
#include <string_view>
#include <sys/types.h>
#ifdef HAVE_DIRENT_H
#include <dirent.h>
typedef struct dirent Directory;
#else /* not HAVE_DIRENT_H */
#include <sys/dir.h>
typedef struct direct Directory;
#endif /* not HAVE_DIRENT_H */
#include <sys/stat.h>

#ifndef S_IFMT
#define S_IFMT 0170000
#endif /* not S_IFMT */
#ifndef S_IFREG
#define S_IFREG 0100000
#endif /* not S_IFREG */

#ifndef S_ISDIR
#ifndef S_IFDIR
#define S_IFDIR 0040000
#endif /* not S_IFDIR */
#define S_ISDIR(m) (((m)&S_IFMT) == S_IFDIR)
#endif /* not S_ISDIR */

#ifdef HAVE_READLINK
#ifndef S_IFLNK
#define S_IFLNK 0120000
#endif /* not S_IFLNK */
#ifndef S_ISLNK
#define S_ISLNK(m) (((m)&S_IFMT) == S_IFLNK)
#endif /* not S_ISLNK */
#endif /* not HAVE_READLINK */

void set_environ(std::string_view var, std::string_view value);
// inline void set_environ(const char *var, const char *value)
// {
//     set_environ(
//         var ? std::string_view(var) : "",
//         value ? std::string_view(value) : "");
// }

pid_t open_pipe_rw(FILE **fr, FILE **fw);
Str loadLocalDir(std::string_view dname);

class LocalCGI
{
public:
    std::shared_ptr<struct Buffer> Request(const URL &url, const URL *base, HttpReferrerPolicy referer, struct FormList *form);
};
