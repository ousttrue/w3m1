#include "dispatcher.h"
#include "fm.h"
#include "frontend/terms.h"
#include "public.h"
#include "commands.h"
#include "file.h"
#include "rc.h"
#include "ctrlcode.h"
#include "frontend/display.h"
#include "frontend/buffer.h"
#include "frontend/tab.h"

static int g_prec_num = 0;
static int g_CurrentKey = -1;
static char *g_CurrentKeyData = nullptr;
static int g_prev_key = -1;
const int KEYDATA_HASH_SIZE = 16;

#define KEY_HASH_SIZE 127

#define K_ESC 0x100
#define K_ESCB 0x200
#define K_ESCD 0x400
#define K_MULTI 0x10000000

// keybind.c
extern Command GlobalKeymap[];
extern Command EscKeymap[];
extern Command EscBKeymap[];
extern Command EscDKeymap[];

#include <unordered_map>
#include <string>

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
            set_buffer_environ(GetCurrentTab()->GetCurrentBuffer());
            save_buffer_position(GetCurrentTab()->GetCurrentBuffer());
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

static int
getKey2(char **str)
{
    char *s = *str;
    int c, esc = 0, ctrl = 0;

    if (s == NULL || *s == '\0')
        return -1;

    if (strcasecmp(s, "UP") == 0)
    { /* ^[[A */
        *str = s + 2;
        return K_ESCB | 'A';
    }
    else if (strcasecmp(s, "DOWN") == 0)
    { /* ^[[B */
        *str = s + 4;
        return K_ESCB | 'B';
    }
    else if (strcasecmp(s, "RIGHT") == 0)
    { /* ^[[C */
        *str = s + 5;
        return K_ESCB | 'C';
    }
    else if (strcasecmp(s, "LEFT") == 0)
    { /* ^[[D */
        *str = s + 4;
        return K_ESCB | 'D';
    }

    if (strncasecmp(s, "ESC-", 4) == 0 || strncasecmp(s, "ESC ", 4) == 0)
    { /* ^[ */
        s += 4;
        esc = K_ESC;
    }
    else if (strncasecmp(s, "M-", 2) == 0 || strncasecmp(s, "\\E", 2) == 0)
    { /* ^[ */
        s += 2;
        esc = K_ESC;
    }
    else if (*s == ESC_CODE)
    { /* ^[ */
        s++;
        esc = K_ESC;
    }
    if (strncasecmp(s, "C-", 2) == 0)
    { /* ^, ^[^ */
        s += 2;
        ctrl = 1;
    }
    else if (*s == '^' && *(s + 1))
    { /* ^, ^[^ */
        s++;
        ctrl = 1;
    }
    if (!esc && ctrl && *s == '[')
    { /* ^[ */
        s++;
        ctrl = 0;
        esc = K_ESC;
    }
    if (esc && !ctrl)
    {
        if (*s == '[' || *s == 'O')
        { /* ^[[, ^[O */
            s++;
            esc = K_ESCB;
        }
        if (strncasecmp(s, "C-", 2) == 0)
        { /* ^[^, ^[[^ */
            s += 2;
            ctrl = 1;
        }
        else if (*s == '^' && *(s + 1))
        { /* ^[^, ^[[^ */
            s++;
            ctrl = 1;
        }
    }

    if (ctrl)
    {
        *str = s + 1;
        if (*s >= '@' && *s <= '_') /* ^@ .. ^_ */
            return esc | (*s - '@');
        else if (*s >= 'a' && *s <= 'z') /* ^a .. ^z */
            return esc | (*s - 'a' + 1);
        else if (*s == '?') /* ^? */
            return esc | DEL_CODE;
        else
            return -1;
    }

    if (esc == K_ESCB && IS_DIGIT(*s))
    {
        c = (int)(*s - '0');
        s++;
        if (IS_DIGIT(*s))
        {
            c = c * 10 + (int)(*s - '0');
            s++;
        }
        *str = s + 1;
        if (*s == '~')
            return K_ESCD | c;
        else
            return -1;
    }

    if (strncasecmp(s, "SPC", 3) == 0)
    { /* ' ' */
        *str = s + 3;
        return esc | ' ';
    }
    else if (strncasecmp(s, "TAB", 3) == 0)
    { /* ^i */
        *str = s + 3;
        return esc | '\t';
    }
    else if (strncasecmp(s, "DEL", 3) == 0)
    { /* ^? */
        *str = s + 3;
        return esc | DEL_CODE;
    }

    if (*s == '\\' && *(s + 1) != '\0')
    {
        s++;
        *str = s + 1;
        switch (*s)
        {
        case 'a': /* ^g */
            return esc | CTRL_G;
        case 'b': /* ^h */
            return esc | CTRL_H;
        case 't': /* ^i */
            return esc | CTRL_I;
        case 'n': /* ^j */
            return esc | CTRL_J;
        case 'r': /* ^m */
            return esc | CTRL_M;
        case 'e': /* ^[ */
            return esc | ESC_CODE;
        case '^': /* ^ */
            return esc | '^';
        case '\\': /* \ */
            return esc | '\\';
        default:
            return -1;
        }
    }
    *str = s + 1;
    if (IS_ASCII(*s)) /* Ascii */
        return esc | *s;
    else
        return -1;
}

int getKey(char *s)
{
    int c, c2;

    c = getKey2(&s);
    if (c < 0)
        return -1;
    if (*s == ' ' || *s == '-')
        s++;
    if (*s)
    {
        c2 = getKey2(&s);
        if (c2 < 0)
            return -1;
        c = K_MULTI | (c << 16) | c2;
    }
    return c;
}

char *getWord(char **str)
{
    char *p, *s;

    p = *str;
    SKIP_BLANKS(p);
    for (s = p; *p && !IS_SPACE(*p) && *p != ';'; p++)
        ;
    *str = p;
    return Strnew_charp_n(s, p - s)->ptr;
}

char *getQWord(char **str)
{
    Str tmp = Strnew();
    char *p;
    int in_q = 0, in_dq = 0, esc = 0;

    p = *str;
    SKIP_BLANKS(p);
    for (; *p; p++)
    {
        if (esc)
        {
            if (in_q)
            {
                if (*p != '\\' && *p != '\'') /* '..\\..', '..\'..' */
                    tmp->Push('\\');
            }
            else if (in_dq)
            {
                if (*p != '\\' && *p != '"') /* "..\\..", "..\".." */
                    tmp->Push('\\');
            }
            else
            {
                if (*p != '\\' && *p != '\'' && /* ..\\.., ..\'.. */
                    *p != '"' && !IS_SPACE(*p)) /* ..\".., ..\.. */
                    tmp->Push('\\');
            }
            tmp->Push(*p);
            esc = 0;
        }
        else if (*p == '\\')
        {
            esc = 1;
        }
        else if (in_q)
        {
            if (*p == '\'')
                in_q = 0;
            else
                tmp->Push(*p);
        }
        else if (in_dq)
        {
            if (*p == '"')
                in_dq = 0;
            else
                tmp->Push(*p);
        }
        else if (*p == '\'')
        {
            in_q = 1;
        }
        else if (*p == '"')
        {
            in_dq = 1;
        }
        else if (IS_SPACE(*p) || *p == ';')
        {
            break;
        }
        else
        {
            tmp->Push(*p);
        }
    }
    *str = p;
    return tmp->ptr;
}

static char keymap_initialized = FALSE;
static struct stat sys_current_keymap_file;
static struct stat current_keymap_file;

static void
interpret_keymap(FILE *kf, struct stat *current, int force)
{
    int fd;
    struct stat kstat;
    Str line;
    char *p, *s, *emsg;
    int lineno;
#ifdef USE_M17N
    wc_ces charset = SystemCharset;
#endif
    int verbose = 1;

    if ((fd = fileno(kf)) < 0 || fstat(fd, &kstat) ||
        (!force &&
         kstat.st_mtime == current->st_mtime &&
         kstat.st_dev == current->st_dev &&
         kstat.st_ino == current->st_ino && kstat.st_size == current->st_size))
        return;
    *current = kstat;

    lineno = 0;
    while (!feof(kf))
    {
        line = Strfgets(kf);
        lineno++;
        line->Strip();
        if (line->Size() == 0)
            continue;
#ifdef USE_M17N
        line = wc_Str_conv(line, charset, InnerCharset);
#endif
        p = line->ptr;
        s = getWord(&p);
        if (*s == '#') /* comment */
            continue;
        if (!strcmp(s, "keymap"))
            ;
#ifdef USE_M17N
        else if (!strcmp(s, "charset") || !strcmp(s, "encoding"))
        {
            s = getQWord(&p);
            if (*s)
                charset = wc_guess_charset(s, charset);
            continue;
        }
#endif
        else if (!strcmp(s, "verbose"))
        {
            s = getWord(&p);
            if (*s)
                verbose = str_to_bool(s, verbose);
            continue;
        }
        else
        { /* error */
            emsg = Sprintf("line %d: syntax error '%s'", lineno, s)->ptr;
            record_err_message(emsg);
            if (verbose)
                disp_message_nsec(emsg, FALSE, 1, TRUE, FALSE);
            continue;
        }
        SetKeymap(p, lineno, verbose);
    }
}

void initKeymap(int force)
{
    FILE *kf;

    if ((kf = fopen(confFile(KEYMAP_FILE), "rt")) != NULL)
    {
        interpret_keymap(kf, &sys_current_keymap_file,
                         force || !keymap_initialized);
        fclose(kf);
    }
    if ((kf = fopen(rcFile(keymap_file), "rt")) != NULL)
    {
        interpret_keymap(kf, &current_keymap_file,
                         force || !keymap_initialized);
        fclose(kf);
    }
    keymap_initialized = TRUE;
}
