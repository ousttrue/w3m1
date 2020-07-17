#include "fm.h"
#include "key.h"
#include "func.h"

static int g_CurrentKey = -1;
static int g_prev_key = -1;

extern "C"
{
    int CurrentKey()
    {
        return g_CurrentKey;
    }

    void ClearCurrentKey()
    {
        g_CurrentKey = -1;
    }

    void SetCurrentKey(int c)
    {
        g_CurrentKey = c;
    }

    void SetMultiKey(int c)
    {
        g_CurrentKey = K_MULTI | (g_CurrentKey << 16) | c;
    }

    int CurrentIsMultiKey()
    {
        // != -1
        return g_CurrentKey >= 0 && g_CurrentKey & K_MULTI;
    }

    int MULTI_KEY(int c)
    {
        return (((c) >> 16) & 0x77F);
    }

    void CurrentKeyToPrev()
    {
        g_prev_key = g_CurrentKey;
        ClearCurrentKey();
        CurrentKeyData = NULL;
    }

    int prev_key()
    {
        return g_prev_key;
    }

    void set_prev_key(int key)
    {
        g_prev_key = key;
    }
}
