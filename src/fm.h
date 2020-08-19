/* 
 * w3m: WWW wo Miru utility
 * 
 * by A.ITO  Feb. 1995
 * 
 * You can use,copy,modify and distribute this program without any permission.
 */
#pragma once
#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* strcasestr() */
#endif
#include <string_view>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "config.h"
#include "history.h"
#include "stream/url.h"

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#if !HAVE_SETLOCALE
#define setlocale(category, locale) /* empty */
#endif

#ifdef ENABLE_NLS
#include <libintl.h>
#define _(String) gettext(String)
#define N_(String) (String)
#else
#undef bindtextdomain
#define bindtextdomain(Domain, Directory) /* empty */
#undef textdomain
#define textdomain(Domain) /* empty */
#define _(Text) Text
#define N_(Text) Text
#define gettext(Text) Text
#endif

#ifdef FALSE
#undef FALSE
#endif
#ifdef TRUE
#undef TRUE
#endif
#define FALSE 0
#define TRUE 1
