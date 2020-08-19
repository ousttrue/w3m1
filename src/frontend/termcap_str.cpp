#include "config.h"

#include "indep.h"
#include "termcap_str.h"
#include "event.h"
#include "terms.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// termcap
extern "C" int tgetent(const char *, char *);
extern "C" char *tgetstr(const char *, char **);
extern "C" int tgetflag(const char *);

inline void GETSTR(char **v, const char *s)
{
    // v = pt;
    auto pt = v;
    auto suc = tgetstr(s, pt);
    if (!suc)
        *v = "";
    else
        *v = allocStr(suc, -1);
}

static char gcmap[96];
static void
setgraphchar(void)
{
    int c, i, n;

    for (c = 0; c < 96; c++)
        gcmap[c] = (char)(c + ' ');

    if (!T_ac)
        return;

    n = strlen(T_ac);
    for (i = 0; i < n - 1; i += 2)
    {
        c = (unsigned)T_ac[i] - ' ';
        if (c >= 0 && c < 96)
            gcmap[c] = T_ac[i + 1];
    }
}

char graphchar(int c)
{
    return (((unsigned)(c) >= ' ' && (unsigned)(c) < 128) ? gcmap[(c) - ' '] : (c));
}

char *T_cd, *T_ce, *T_kr, *T_kl, *T_cr, *T_bt, *T_ta, *T_sc, *T_rc,
    *T_so, *T_se, *T_us, *T_ue, *T_cl, *T_cm, *T_al, *T_sr, *T_md, *T_me,
    *T_ti, *T_te, *T_nd, *T_as, *T_ae, *T_eA, *T_ac, *T_op;

static char funcstr[256];
static char bp[1024];

void getTCstr(void)
{
    char *ent;
    // char *suc;
    char *pt = funcstr;
    int r;

    ent = getenv("TERM") ? getenv("TERM") : DEFAULT_TERM;
    if (ent == NULL)
    {
        fprintf(stderr, "TERM is not set\n");
        reset_error_exit(SIGNAL_ARGLIST);
    }

    r = tgetent(bp, ent);
    if (r != 1)
    {
        /* Can't find termcap entry */
        fprintf(stderr, "Can't find termcap entry %s\n", ent);
        reset_error_exit(SIGNAL_ARGLIST);
    }

    GETSTR(&T_ce, "ce"); /* clear to the end of line */
    GETSTR(&T_cd, "cd"); /* clear to the end of display */
    GETSTR(&T_kr, "nd"); /* cursor right */
    if (T_kr[0]=='\0')
        GETSTR(&T_kr, "kr");
    if (tgetflag("bs"))
        T_kl = "\b"; /* cursor left */
    else
    {
        GETSTR(&T_kl, "le");
        if (T_kl[0]=='\0')
            GETSTR(&T_kl, "kb");
        if (T_kl[0]=='\0')
            GETSTR(&T_kl, "kl");
    }
    GETSTR(&T_cr, "cr"); /* carriage return */
    GETSTR(&T_ta, "ta"); /* tab */
    GETSTR(&T_sc, "sc"); /* save cursor */
    GETSTR(&T_rc, "rc"); /* restore cursor */
    GETSTR(&T_so, "so"); /* standout mode */
    GETSTR(&T_se, "se"); /* standout mode end */
    GETSTR(&T_us, "us"); /* underline mode */
    GETSTR(&T_ue, "ue"); /* underline mode end */
    GETSTR(&T_md, "md"); /* bold mode */
    GETSTR(&T_me, "me"); /* bold mode end */
    GETSTR(&T_cl, "cl"); /* clear screen */
    GETSTR(&T_cm, "cm"); /* cursor move */
    GETSTR(&T_al, "al"); /* append line */
    GETSTR(&T_sr, "sr"); /* scroll reverse */
    GETSTR(&T_ti, "ti"); /* terminal init */
    GETSTR(&T_te, "te"); /* terminal end */
    GETSTR(&T_nd, "nd"); /* move right one space */
    GETSTR(&T_eA, "eA"); /* enable alternative charset */
    GETSTR(&T_as, "as"); /* alternative (graphic) charset start */
    GETSTR(&T_ae, "ae"); /* alternative (graphic) charset end */
    GETSTR(&T_ac, "ac"); /* graphics charset pairs */
    GETSTR(&T_op, "op"); /* set default color pair to its original value */

    LINES = COLS = 0;
    setlinescols();
    setgraphchar();
}
