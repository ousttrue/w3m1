#include "fm.h"
#include "public.h"

void escKeyProc(int c, int esc, unsigned char *map)
{
    if (CurrentKey >= 0 && CurrentKey & K_MULTI)
    {
        unsigned char **mmap;
        mmap = (unsigned char **)getKeyData(MULTI_KEY(CurrentKey));
        if (!mmap)
            return;
        switch (esc)
        {
        case K_ESCD:
            map = mmap[3];
            break;
        case K_ESCB:
            map = mmap[2];
            break;
        case K_ESC:
            map = mmap[1];
            break;
        default:
            map = mmap[0];
            break;
        }
        esc |= (CurrentKey & ~0xFFFF);
    }
    CurrentKey = esc | c;
    w3mFuncList[(int)map[c]].func();
}
