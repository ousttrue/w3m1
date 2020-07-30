#include "fm.h"
#include "indep.h"
#include "gc_helper.h"
#include "file.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include "transport/local.h"
#include "html/form.h"
#include "frontend/terms.h"

#ifdef __MINGW32_VERSION
#include <winsock.h>
#endif

#define CGIFN_NORMAL     0
#define CGIFN_LIBDIR     1
#define CGIFN_CGIBIN     2

static Str Local_cookie = NULL;
static char *Local_cookie_file = NULL;

static void
writeLocalCookie()
{
    FILE *f;

    if (no_rc_dir)
	return;
    if (Local_cookie_file)
	return;
    Local_cookie_file = tmpfname(TMPF_COOKIE, NULL)->ptr;
    set_environ("LOCAL_COOKIE_FILE", Local_cookie_file);
    f = fopen(Local_cookie_file, "wb");
    if (!f)
	return;
    localCookie();
    fwrite(Local_cookie->ptr, sizeof(char), Local_cookie->Size(), f);
    fclose(f);
    chmod(Local_cookie_file, S_IRUSR | S_IWUSR);
}

/* setup cookie for local CGI */
Str
localCookie()
{
    char hostname[256];

    if (Local_cookie)
	return Local_cookie;
    gethostname(hostname, 256);
    srand48((long)New(char) + (long)time(NULL));
    Local_cookie = Sprintf("%ld@%s", lrand48(), hostname);
    return Local_cookie;
}

Str
loadLocalDir(char *dname)
{
    Str tmp;
    DIR *d;
    Directory *dir;
    struct stat st;
    char **flist;
    char *p, *qdir;
    Str fbuf = Strnew();
#ifdef HAVE_LSTAT
    struct stat lst;
#ifdef HAVE_READLINK
    char lbuf[1024];
#endif				/* HAVE_READLINK */
#endif				/* HAVE_LSTAT */
    int i, l, nrow = 0, n = 0, maxlen = 0;
    int nfile, nfile_max = 100;
    Str dirname;

    d = opendir(dname);
    if (d == NULL)
	return NULL;
    dirname = Strnew(dname);
    if (dirname->Back() != '/')
	dirname->Push( '/');
    qdir = html_quote(Str_conv_from_system(dirname)->ptr);
    /* FIXME: gettextize? */
    tmp = Strnew_m_charp("<HTML>\n<HEAD>\n<BASE HREF=\"file://",
			html_quote(file_quote(dirname->ptr)),
			 "\">\n<TITLE>Directory list of ", qdir,
			 "</TITLE>\n</HEAD>\n<BODY>\n<H1>Directory list of ",
			 qdir, "</H1>\n", NULL);
    flist = New_N(char *, nfile_max);
    nfile = 0;
    while ((dir = readdir(d)) != NULL) {
	flist[nfile++] = allocStr(dir->d_name, -1);
	if (nfile == nfile_max) {
	    nfile_max *= 2;
	    flist = New_Reuse(char *, flist, nfile_max);
	}
	if (multicolList) {
	    l = strlen(dir->d_name);
	    if (l > maxlen)
		maxlen = l;
	    n++;
	}
    }

    if (multicolList) {
	l = COLS / (maxlen + 2);
	if (!l)
	    l = 1;
	nrow = (n + l - 1) / l;
	n = 1;
	tmp->Push("<TABLE CELLPADDING=0>\n<TR VALIGN=TOP>\n");
    }
    qsort((void *)flist, nfile, sizeof(char *), strCmp);
    for (i = 0; i < nfile; i++) {
	p = flist[i];
	if (strcmp(p, ".") == 0)
	    continue;
	fbuf->CopyFrom(dirname);
	if (fbuf->Back() != '/')
	    fbuf->Push( '/');
	fbuf->Push( p);
#ifdef HAVE_LSTAT
	if (lstat(fbuf->ptr, &lst) < 0)
	    continue;
#endif				/* HAVE_LSTAT */
	if (stat(fbuf->ptr, &st) < 0)
	    continue;
	if (multicolList) {
	    if (n == 1)
		tmp->Push("<TD><NOBR>");
	}
	else {
#ifdef HAVE_LSTAT
	    if (S_ISLNK(lst.st_mode))
		tmp->Push("[LINK] ");
	    else
#endif				/* HAVE_LSTAT */
	    if (S_ISDIR(st.st_mode))
		tmp->Push("[DIR]&nbsp; ");
	    else
		tmp->Push("[FILE] ");
	}
	Strcat_m_charp(tmp, "<A HREF=\"", html_quote(file_quote(p)), NULL);
	if (S_ISDIR(st.st_mode))
	    tmp->Push('/');
	Strcat_m_charp(tmp, "\">", html_quote(conv_from_system(p)), NULL);
	if (S_ISDIR(st.st_mode))
	    tmp->Push('/');
	tmp->Push("</A>");
	if (multicolList) {
	    if (n++ == nrow) {
		tmp->Push("</NOBR></TD>\n");
		n = 1;
	    }
	    else {
		tmp->Push("<BR>\n");
	    }
	}
	else {
#if defined(HAVE_LSTAT) && defined(HAVE_READLINK)
	    if (S_ISLNK(lst.st_mode)) {
		if ((l = readlink(fbuf->ptr, lbuf, sizeof(lbuf))) > 0) {
		    lbuf[l] = '\0';
		    Strcat_m_charp(tmp, " -> ",
				   html_quote(conv_from_system(lbuf)), NULL);
		    if (S_ISDIR(st.st_mode))
			tmp->Push('/');
		}
	    }
#endif				/* HAVE_LSTAT && HAVE_READLINK */
	    tmp->Push("<br>\n");
	}
    }
    if (multicolList) {
	tmp->Push("</TR>\n</TABLE>\n");
    }
    tmp->Push("</BODY>\n</HTML>\n");

    return tmp;
}

static int
check_local_cgi(char *file, int status)
{
    struct stat st;

    if (status != CGIFN_LIBDIR && status != CGIFN_CGIBIN)
	return -1;
    if (stat(file, &st) < 0)
	return -1;
    if (S_ISDIR(st.st_mode))
	return -1;
#ifndef __MINGW32_VERSION
    if ((st.st_uid == geteuid() && (st.st_mode & S_IXUSR)) || (st.st_gid == getegid() && (st.st_mode & S_IXGRP)) || (st.st_mode & S_IXOTH))	/* executable */
	return 0;
#endif
    return -1;
}

void
set_environ(std::string_view var, std::string_view value)
{
#ifdef HAVE_SETENV
    if (var.size() && value.size())
	    setenv(var.data(), value.data(), 1);
#else				/* not HAVE_SETENV */
#ifdef HAVE_PUTENV
    static Hash_sv *env_hash = NULL;
    Str tmp = Strnew_m_charp(var, "=", value, NULL);

    if (env_hash == NULL)
	env_hash = newHash_sv(20);
    putHash_sv(env_hash, var, (void *)tmp->ptr);
    putenv(tmp->ptr);
#else				/* not HAVE_PUTENV */
    extern char **environ;
    char **ne;
    char *p;
    int i, l, el;
    char **e, **newenv;

    /* I have no setenv() nor putenv() */
    /* This part is taken from terms.c of skkfep */
    l = strlen(var);
    for (e = environ, i = 0; *e != NULL; e++, i++) {
	if (strncmp(e, var, l) == 0 && (*e)[l] == '=') {
	    el = strlen(*e) - l - 1;
	    if (el >= strlen(value)) {
		strcpy(*e + l + 1, value);
		return 0;
	    }
	    else {
		for (; *e != NULL; e++, i++) {
		    *e = *(e + 1);
		}
		i--;
		break;
	    }
	}
    }
    newenv = (char **)GC_malloc((i + 2) * sizeof(char *));
    if (newenv == NULL)
	return;
    for (e = environ, ne = newenv; *e != NULL; *(ne++) = *(e++)) ;
    *(ne++) = p;
    *ne = NULL;
    environ = newenv;
#endif				/* not HAVE_PUTENV */
#endif				/* not HAVE_SETENV */
}

static void
set_cgi_environ(char *name, char *fn, char *req_uri)
{
    set_environ("SERVER_SOFTWARE", w3m_version);
    set_environ("SERVER_PROTOCOL", "HTTP/1.0");
    set_environ("SERVER_NAME", "localhost");
    set_environ("SERVER_PORT", "80");	/* dummy */
    set_environ("REMOTE_HOST", "localhost");
    set_environ("REMOTE_ADDR", "127.0.0.1");
    set_environ("GATEWAY_INTERFACE", "CGI/1.1");

    set_environ("SCRIPT_NAME", name);
    set_environ("SCRIPT_FILENAME", fn);
    set_environ("REQUEST_URI", req_uri);
}

static Str
checkPath(char *fn, char *path)
{
    char *p;
    Str tmp;
    struct stat st;
    while (*path) {
	p = strchr(path, ':');
	tmp = Strnew(expandPath(p ? allocStr(path, p - path) : path));
	if (tmp->Back() != '/')
	    tmp->Push('/');
	tmp->Push(fn);
	if (stat(tmp->ptr, &st) == 0)
	    return tmp;
	if (!p)
	    break;
	path = p + 1;
	while (*path == ':')
	    path++;
    }
    return NULL;
}

static int
cgi_filename(char *uri, char **fn, char **name, char **path_info)
{
    Str tmp;
    int offset;

    *fn = uri;
    *name = uri;
    *path_info = NULL;

    if (cgi_bin != NULL && strncmp(uri, "/cgi-bin/", 9) == 0) {
	offset = 9;
	if ((*path_info = strchr(uri + offset, '/')))
	    *name = allocStr(uri, *path_info - uri);
	tmp = checkPath(*name + offset, cgi_bin);
	if (tmp == NULL)
	    return CGIFN_NORMAL;
	*fn = tmp->ptr;
	return CGIFN_CGIBIN;
    }

#ifdef __EMX__
    {
	char lib[_MAX_PATH];
	_abspath(lib, w3m_lib_dir(), _MAX_PATH);	/* Translate '\\' to '/' */
	tmp = Strnew(lib);
    }
#else
    tmp = Strnew(w3m_lib_dir());
#endif
    if (tmp->Back() != '/')
	tmp->Push('/');
    if (strncmp(uri, "/$LIB/", 6) == 0)
	offset = 6;
    else if (strncmp(uri, tmp->ptr, tmp->Size()) == 0)
	offset = tmp->Size();
    else if (*uri == '/' && document_root != NULL) {
	Str tmp2 = Strnew(document_root);
	if (tmp2->Back() != '/')
	    tmp2->Push( '/');
	tmp2->Push( uri + 1);
	if (strncmp(tmp2->ptr, tmp->ptr, tmp->Size()) != 0)
	    return CGIFN_NORMAL;
	uri = tmp2->ptr;
	*name = uri;
	offset = tmp->Size();
    }
    else
	return CGIFN_NORMAL;
    if ((*path_info = strchr(uri + offset, '/')))
	*name = allocStr(uri, *path_info - uri);
    tmp->Push(*name + offset);
    *fn = tmp->ptr;
    return CGIFN_LIBDIR;
}

FILE *
localcgi_post(char *uri, char *qstr, FormList *request, char *referer)
{
    FILE *fr = NULL, *fw = NULL;
    int status;
    pid_t pid;
    char *file = uri, *name = uri, *path_info = NULL, *tmpf = NULL;

#ifdef __MINGW32_VERSION
    return NULL;
#else
    status = cgi_filename(uri, &file, &name, &path_info);
    if (check_local_cgi(file, status) < 0)
	return NULL;
    writeLocalCookie();
    if (request && request->enctype != FORM_ENCTYPE_MULTIPART) {
	tmpf = tmpfname(TMPF_DFL, NULL)->ptr;
	fw = fopen(tmpf, "w");
	if (!fw)
	    return NULL;
    }
    pid = open_pipe_rw(&fr, NULL);
    if (pid < 0)
	return NULL;
    else if (pid) {
	if (fw)
	    fclose(fw);
	return fr;
    }
    setup_child(TRUE, 2, fw ? fileno(fw) : -1);

    if (qstr)
	uri = Strnew_m_charp(uri, "?", qstr, NULL)->ptr;
    set_cgi_environ(name, file, uri);
    if (path_info)
	set_environ("PATH_INFO", path_info);
    if (referer && referer != NO_REFERER)
	set_environ("HTTP_REFERER", referer);
    if (request) {
	set_environ("REQUEST_METHOD", "POST");
	if (qstr)
	    set_environ("QUERY_STRING", qstr);
	set_environ("CONTENT_LENGTH", Sprintf("%d", request->length)->ptr);
	if (request->enctype == FORM_ENCTYPE_MULTIPART) {
	    set_environ("CONTENT_TYPE",
			Sprintf("multipart/form-data; boundary=%s",
				request->boundary)->ptr);
	    freopen(request->body, "r", stdin);
	}
	else {
	    set_environ("CONTENT_TYPE", "application/x-www-form-urlencoded");
	    fwrite(request->body, sizeof(char), request->length, fw);
	    fclose(fw);
	    freopen(tmpf, "r", stdin);
	}
    }
    else {
	set_environ("REQUEST_METHOD", "GET");
	set_environ("QUERY_STRING", qstr ? qstr : (char*)"");
	freopen(DEV_NULL_PATH, "r", stdin);
    }

#ifdef HAVE_CHDIR		/* ifndef __EMX__ ? */
    chdir(mydirname(file));
#endif
    execl(file, mybasename(file), NULL);
    fprintf(stderr, "execl(\"%s\", \"%s\", NULL): %s\n",
	    file, mybasename(file), strerror(errno));
    exit(1);
    return NULL;
#endif
}

#ifndef __MINGW32_VERSION
pid_t open_pipe_rw(FILE **fr, FILE **fw)
{
    int fdr[2];
    int fdw[2];
    pid_t pid;

    if (fr && pipe(fdr) < 0)
        goto err0;
    if (fw && pipe(fdw) < 0)
        goto err1;

    flush_tty();
    pid = fork();
    if (pid < 0)
        goto err2;
    if (pid == 0)
    {
        /* child */
        if (fr)
        {
            close(fdr[0]);
            dup2(fdr[1], 1);
        }
        if (fw)
        {
            close(fdw[1]);
            dup2(fdw[0], 0);
        }
    }
    else
    {
        if (fr)
        {
            close(fdr[1]);
            if (*fr == stdin)
                dup2(fdr[0], 0);
            else
                *fr = fdopen(fdr[0], "r");
        }
        if (fw)
        {
            close(fdw[0]);
            if (*fw == stdout)
                dup2(fdw[1], 1);
            else
                *fw = fdopen(fdw[1], "w");
        }
    }
    return pid;
err2:
    if (fw)
    {
        close(fdw[0]);
        close(fdw[1]);
    }
err1:
    if (fr)
    {
        close(fdr[0]);
        close(fdr[1]);
    }
err0:
    return (pid_t)-1;
}
#endif /* __MINGW32_VERSION */
