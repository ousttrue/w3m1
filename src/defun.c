#include "fm.h"

DEFUN(nulcmd, NOTHING NULL @@@, "Do nothing")
{				/* do nothing */
}

#ifdef __EMX__
DEFUN(pcmap, PCMAP, "pcmap")
{
    w3mFuncList[(int)PcKeymap[(int)getch()]].func();
}
#else				/* not __EMX__ */
void
pcmap(void)
{
}
#endif
