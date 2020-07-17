#include "key.h"
#include "func.h"

static int g_CurrentKey = -1;
static int g_prev_key = -1;
static char *g_CurrentKeyData = nullptr;

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

    void ClearCurrentKeyData()
    {
        g_CurrentKeyData = NULL; /* not allowed in w3m-control: */
    }

    char *CurrentKeyData()
    {
        return g_CurrentKeyData;
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
        set_prev_key(g_CurrentKey);
        ClearCurrentKey();
        ClearCurrentKeyData();
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
