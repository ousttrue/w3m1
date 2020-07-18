#include "dispatcher.h"
extern "C"
{
#include "fm.h"
#include "func.h"
#include "public.h"
}

static int g_prec_num = 0;
static int g_CurrentKey = -1;
static char *g_CurrentKeyData = nullptr;
static int g_prev_key = -1;
const int KEYDATA_HASH_SIZE = 16;

// keybind.c
extern Command GlobalKeymap[];
extern Command EscKeymap[];
extern Command EscBKeymap[];
extern Command EscDKeymap[];

#include <unordered_map>
#include <string>

extern "C"
{
    // static Hash_iv *g_keyData = NULL;
    static std::unordered_map<int, std::string> g_keyData;

    int prec_num()
    {
        return g_prec_num;
    }

    void set_prec_num(int n)
    {
        g_prec_num = n;
    }

    int PREC_NUM()
    {
        return prec_num() ? prec_num() : 1;
    }

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
        GlobalKeymap[c]();
        // w3mFuncList[index].func();
    }

    void DispatchKey(int c)
    {
        if (IS_ASCII(c))
        { /* Ascii */
            if (('0' <= c) && (c <= '9') &&
                (prec_num() || (GlobalKeymap[c] == &nulcmd)))
            {
                set_prec_num(prec_num() * 10 + (int)(c - '0'));
                if (prec_num() > PREC_LIMIT())
                    set_prec_num(PREC_LIMIT());
            }
            else
            {
                set_buffer_environ(Currentbuf);
                save_buffer_position(Currentbuf);
                KeyPressEventProc((int)c);
                set_prec_num(0);
            }
        }
        SetPrevKey(g_CurrentKey);
        ClearCurrentKey();
        ClearCurrentKeyData();
    }

    static void _escKeyProc(int c, int esc, Command map[])
    {
        // if (CurrentIsMultiKey())
        // {
        //     unsigned char **mmap;
        //     mmap = (unsigned char **)GetKeyData(MultiKey(CurrentKey()));
        //     if (!mmap)
        //         return;
        //     switch (esc)
        //     {
        //     case K_ESCD:
        //         map = mmap[3];
        //         break;
        //     case K_ESCB:
        //         map = mmap[2];
        //         break;
        //     case K_ESC:
        //         map = mmap[1];
        //         break;
        //     default:
        //         map = mmap[0];
        //         break;
        //     }
        //     esc |= (CurrentKey() & ~0xFFFF);
        // }
        SetCurrentKey(esc | c);
        map[c]();
    }

    void escKeyProc(char c)
    {
        if (IS_ASCII(c))
            _escKeyProc((int)c, K_ESC, EscKeymap);
    }

    void MultiKeyProc(char c)
    {
        if (IS_ASCII(c))
        {
            SetMultiKey(c);
            _escKeyProc((int)c, 0, NULL);
        }
    }

    void escdmap(char c)
    {
        int d;
        d = (int)c - (int)'0';
        c = getch();
        if (IS_DIGIT(c))
        {
            d = d * 10 + (int)c - (int)'0';
            c = getch();
        }
        if (c == '~')
            _escKeyProc((int)d, K_ESCD, EscDKeymap);
    }

    void escbKeyProc(char c)
    {
        if (IS_DIGIT(c))
        {
            escdmap(c);
            return;
        }
        if (IS_ASCII(c))
        {
            _escKeyProc((int)c, K_ESCB, EscBKeymap);
        }
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
            cmd();
            if (use_mouse)
            {
                mouse_active();
            }

            CurrentCmdData = NULL;
        }
    }

    char *GetKeyData(int key)
    {
        auto found = g_keyData.find(key);
        if (found == g_keyData.end())
        {
            return nullptr;
        }
        return (char *)found->second.c_str();
    }

    static int GetKey(char **p, int lineno, int verbose)
    {
        auto s = getQWord(p);
        auto c = getKey(s);
        if (c < 0)
        { /* error */
            char *emsg = nullptr;
            if (lineno > 0)
                /* FIXME: gettextize? */
                emsg = Sprintf("line %d: unknown key '%s'", lineno, s)->ptr;
            else
                /* FIXME: gettextize? */
                emsg = Sprintf("defkey: unknown key '%s'", s)->ptr;
            record_err_message(emsg);
            if (verbose)
                disp_message_nsec(emsg, FALSE, 1, TRUE, FALSE);
        }
        return c;
    }

    static Command GetFunc(char **p, int lineno, int verbose)
    {
        auto s = getWord(p);
        auto f = getFuncList(s);
        if (!f)
        {
            char *emsg = nullptr;
            if (lineno > 0)
                /* FIXME: gettextize? */
                emsg = Sprintf("line %d: invalid command '%s'", lineno, s)->ptr;
            else
                /* FIXME: gettextize? */
                emsg = Sprintf("defkey: invalid command '%s'", s)->ptr;
            record_err_message(emsg);
            if (verbose)
                disp_message_nsec(emsg, FALSE, 1, TRUE, FALSE);
        }
        return f;
    }

    void SetKeymap(char *p, int lineno, int verbose)
    {
        auto c = GetKey(&p, lineno, verbose);
        if (c < 0)
        {
            return;
        }
        auto f = GetFunc(&p, lineno, verbose);
        if (!f)
        {
            return;
        }
        Command *map = nullptr;
        // if (c & K_MULTI)
        // {
        //     // Kanji Multibyte ? SJIS とか EUC ?
        //     int m = MultiKey(c);

        //     if (m & K_ESCD)
        //         map = EscDKeymap;
        //     else if (m & K_ESCB)
        //         map = EscBKeymap;
        //     else if (m & K_ESC)
        //         map = EscKeymap;
        //     else
        //         map = GlobalKeymap;

        //     unsigned char **mmap = NULL;
        //     if (map[m & 0x7F] == FUNCNAME_multimap)
        //         mmap = (unsigned char **)GetKeyData(m);
        //     else
        //         map[m & 0x7F] = FUNCNAME_multimap;
        //     if (!mmap)
        //     {
        //         mmap = New_N(unsigned char *, 4);
        //         for (int i = 0; i < 4; i++)
        //         {
        //             mmap[i] = New_N(unsigned char, 128);
        //             for (int j = 0; j < 128; j++)
        //                 mmap[i][j] = FUNCNAME_nulcmd;
        //         }
        //         mmap[0][ESC_CODE] = FUNCNAME_escmap;
        //         mmap[1]['['] = FUNCNAME_escbmap;
        //         mmap[1]['O'] = FUNCNAME_escbmap;
        //     }
        //     if (keyData == NULL)
        //         keyData = newHash_iv(KEYDATA_HASH_SIZE);
        //     putHash_iv(keyData, m, (void *)mmap);
        //     if (c & K_ESCD)
        //         map = mmap[3];
        //     else if (c & K_ESCB)
        //         map = mmap[2];
        //     else if (c & K_ESC)
        //         map = mmap[1];
        //     else
        //         map = mmap[0];
        // }
        // else
        {
            if (c & K_ESCD)
                map = EscDKeymap;
            else if (c & K_ESCB)
                map = EscBKeymap;
            else if (c & K_ESC)
                map = EscKeymap;
            else
                map = GlobalKeymap;
        }

        // push c: key = f: function
        map[c & 0x7F] = f;

        // data
        auto s = getQWord(&p);
        if (*s)
        {
            g_keyData[c] = s;
        }
        else
        {
            auto found = g_keyData.find(c);
            if (found != g_keyData.end())
            {
                g_keyData.erase(found);
            }
        }
    }

    std::unordered_map<std::string, Command> g_commandMap;

    void RegisterCommand(const char *name, const char *key, const char *description, void (*command)())
    {
        g_commandMap[key] = command;
    }

    Command getFuncList(char *name)
    {
        auto found = g_commandMap.find(name);
        if (found == g_commandMap.end())
        {
            return &nulcmd;
        }
        return found->second;
    }
}
