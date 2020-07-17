#include "dispatcher.h"

static int g_CurrentKey = -1;
static char *g_CurrentKeyData = nullptr;
static int g_prev_key = -1;

extern "C"
{
#include "fm.h"
#include "func.h"

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

    int MultiKey(int c)
    {
        return (((c) >> 16) & 0x77F);
    }

    void CurrentKeyToPrev()
    {
        SetPrevKey(g_CurrentKey);
        ClearCurrentKey();
        ClearCurrentKeyData();
    }

    int PrevKey()
    {
        return g_prev_key;
    }

    void SetPrevKey(int key)
    {
        g_prev_key = key;
    }

    void KeyPressEventProc(int c)
    {
        SetCurrentKey(c);
        auto index = (int)GlobalKeymap[c];
        w3mFuncList[index].func();
    }

    /* data: FUNC [DATA] [; FUNC [DATA] ...] */
    void ExecuteCommand(char *data)
    {
        while (*data)
        {
            SKIP_BLANKS(data);
            if (*data == ';')
            {
                data++;
                continue;
            }
            auto p = getWord(&data);
            auto cmd = getFuncList(p);
            if (cmd < 0)
            {
                break;
            }
            p = getQWord(&data);
            ClearCurrentKey();
            ClearCurrentKeyData();
            CurrentCmdData = *p ? p : NULL;

            if (use_mouse)
            {
                mouse_inactive();
            }
            w3mFuncList[cmd].func();
            if (use_mouse)
            {
                mouse_active();
            }

            CurrentCmdData = NULL;
        }
    }
}
