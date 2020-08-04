#include "w3m.h"
#define MAINPROGRAM
#include "fm.h"
#include "charset.h"
#include "rc.h"
#include "indep.h"
#include "gc_helper.h"
#include "frontend/display.h"
#include "frontend/tabbar.h"
#include "frontend/mouse.h"
#include "file.h"
#include "html/form.h"
#include "public.h"
#include "commands.h"
#include "http/cookie.h"
#include "transport/loader.h"
#include <signal.h>
#include <setjmp.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#if defined(HAVE_WAITPID) || defined(HAVE_WAIT3)
#include <sys/wait.h>
#endif
#include <time.h>
#include "frontend/terms.h"
#include "myctype.h"
#include "regex.h"
#include "ucs.h"
#include "dispatcher.h"
#include "transport/url.h"
#include <assert.h>

#ifdef __MINGW32_VERSION
#include <winsock.h>
WSADATA WSAData;
#endif

#define DSTR_LEN 256













#include "register_commands.h"

#ifndef HAVE_SYS_ERRLIST
char **sys_errlist;

#ifndef HAVE_STRERROR
char *
strerror(int errno)
{
    extern char *sys_errlist[];
    return sys_errlist[errno];
}
#endif /* not HAVE_STRERROR */

prepare_sys_errlist()
{
    int i, n;

    i = 1;
    while (strerror(i) != NULL)
        i++;
    n = i;
    sys_errlist = New_N(char *, n);
    sys_errlist[0] = "";
    for (i = 1; i < n; i++)
        sys_errlist[i] = strerror(i);
}
#endif /* not HAVE_SYS_ERRLIST */

int main(int argc, char **argv)
{
    return w3mApp::Instance().Main(argc, argv);
}
