#include <plog/Log.h>
#include <string_view_util.h>
#include "command_dispatcher.h"
#include "public.h"
#include "commands.h"
#include "file.h"
#include "rc.h"
#include "ctrlcode.h"
#include "indep.h"
#include "frontend/terminal.h"

#include "frontend/display.h"
#include "frontend/buffer.h"
#include "frontend/tab.h"
#include "frontend/tabbar.h"
#include "charset.h"

static char *g_CurrentKeyData = nullptr;

const int KEYDATA_HASH_SIZE = 16;

#define KEY_HASH_SIZE 127

#define K_ESC 0x100
#define K_ESCB 0x200
#define K_ESCD 0x400

// keybind.c
extern Command GlobalKeymap[];
extern Command EscKeymap[];
extern Command EscBKeymap[];
extern Command EscDKeymap[];

#include <unordered_map>
#include <string>

// static Hash_iv *g_keyData = NULL;
static std::unordered_map<int, std::string> g_keyData;
std::unordered_map<std::string, Command> g_commandMap;

static void DebugPrint(Command map[], int c)
{
    auto callback = map[c];
    std::string_view key;
    for (auto &[k, v] : g_commandMap)
    {
        if (v == callback)
        {
            key = k;
            break;
        }
    }
    if (key.empty())
    {
        LOGD << (char)c << " => " << callback;
    }
    else
    {
        LOGD << (char)c << " => " << key;
    }
}

void ClearCurrentKeyData()
{
    g_CurrentKeyData = NULL; /* not allowed in w3m-control: */
}

char *CurrentKeyData()
{
    return g_CurrentKeyData;
}

CommandContext g_context;

void DispatchKey(int c)
{
    if (IS_ASCII(c))
    { /* Ascii */
        if (('0' <= c) && (c <= '9'))
        {
            g_context.set_prec(c - '0');
        }
        else
        {
            auto prev = g_context.set_key(c);

            DebugPrint(GlobalKeymap, c);

            auto tab = GetCurrentTab();
            auto buf = tab->GetCurrentBuffer();
            set_buffer_environ(buf);
            buf->SavePosition();
            GlobalKeymap[c](&w3mApp::Instance(), g_context);

            g_context.clear();
        }
    }
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
    g_context.set_key(esc | c);
    DebugPrint(map, c);
    map[c](&w3mApp::Instance(), g_context);
}

void escKeyProc(char c)
{
    if (IS_ASCII(c))
    {
        _escKeyProc((int)c, K_ESC, EscKeymap);
    }
}

void MultiKeyProc(char c)
{
    if (IS_ASCII(c))
    {
        g_context.set_multi_key(c);
        _escKeyProc((int)c, 0, NULL);
    }
}

void escdmap(char _c)
{
    auto d = (int)_c - (int)'0';
    auto c = Terminal::getch();
    if (IS_DIGIT(c))
    {
        d = d * 10 + (int)c - (int)'0';
        c = Terminal::getch();
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
void CommandDispatcher::ExecuteCommand(std::string_view data)
{
    while (data.size())
    {
        data = svu::strip_left(data);
        if (data.empty())
        {
            break;
        }

        if (data[0] == ';')
        {
            data.remove_prefix(1);
            continue;
        }

        std::string_view p;
        std::tie(data, p) = getWord(data);
        auto cmd = getFuncList(std::string(p));
        if (cmd < 0)
        {
            break;
        }
        std::tie(data, p) = getQWord(data);

        g_context.clear();

        ClearCurrentKeyData();
        w3mApp::Instance().CurrentCmdData = p;

        Terminal::mouse_on();
        cmd(&w3mApp::Instance(), g_context);
        Terminal::mouse_off();

        w3mApp::Instance().CurrentCmdData.clear();
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

static std::tuple<std::string_view, int> getKey2(std::string_view str)
{
    if (str.empty())
    {
        return {str, -1};
    }

    // int c, esc = 0, ctrl = 0;
    if (svu::ic_eq(str, "UP"))
    { /* ^[[A */
        return {str.substr(2), K_ESCB | 'A'};
    }

    if (svu::ic_eq(str, "DOWN"))
    { /* ^[[B */
        return {str.substr(4), K_ESCB | 'B'};
    }

    if (svu::ic_eq(str, "RIGHT"))
    { /* ^[[C */
        return {str.substr(5), K_ESCB | 'C'};
    }

    if (svu::ic_eq(str, "LEFT"))
    { /* ^[[D */
        return {str.substr(4), K_ESCB | 'D'};
    }

    int esc = 0;
    auto s = str;
    if (svu::ic_starts_with(s, "ESC-") || svu::ic_starts_with(s, "ESC "))
    { /* ^[ */
        s.remove_prefix(4);
        esc = K_ESC;
    }
    else if (svu::ic_starts_with(s, "M-") || svu::ic_starts_with(s, "\\E"))
    { /* ^[ */
        s.remove_prefix(2);
        esc = K_ESC;
    }
    else if (s[0] == ESC_CODE)
    { /* ^[ */
        s.remove_prefix(1);
        esc = K_ESC;
    }

    int ctrl = 0;
    if (svu::ic_starts_with(s, "C-"))
    { /* ^, ^[^ */
        s.remove_prefix(2);
        ctrl = 1;
    }
    else if (s.size() >= 2 && s[0] == '^')
    { /* ^, ^[^ */
        s.remove_prefix(2);
        ctrl = 1;
    }

    if (!esc && ctrl && s[0] == '[')
    { /* ^[ */
        s.remove_prefix(1);
        ctrl = 0;
        esc = K_ESC;
    }
    if (esc && !ctrl)
    {
        if (s[0] == '[' || s[0] == 'O')
        { /* ^[[, ^[O */
            s.remove_prefix(1);
            esc = K_ESCB;
        }
        if (svu::ic_starts_with(s, "C-"))
        { /* ^[^, ^[[^ */
            s.remove_prefix(2);
            ctrl = 1;
        }
        else if (s.size() >= 2 && s[0] == '^')
        { /* ^[^, ^[[^ */
            s.remove_prefix(1);
            ctrl = 1;
        }
    }

    if (ctrl)
    {
        if (s[0] >= '@' && s[0] <= '_') /* ^@ .. ^_ */
            return {s.substr(1), esc | (s[0] - '@')};
        else if (s[0] >= 'a' && s[0] <= 'z') /* ^a .. ^z */
            return {s.substr(1), esc | (s[0] - 'a' + 1)};
        else if (s[0] == '?') /* ^? */
            return {s.substr(1), esc | DEL_CODE};
        else
            return {s.substr(1), -1};
    }

    if (esc == K_ESCB && IS_DIGIT(s[0]))
    {
        auto c = (int)(s[0] - '0');
        s.remove_prefix(1);
        if (IS_DIGIT(s[0]))
        {
            c = c * 10 + (int)(s[0] - '0');
            s.remove_prefix(1);
        }
        if (s[0] == '~')
            return {s.substr(1), K_ESCD | c};
        else
            return {s.substr(1), -1};
    }

    if (svu::ic_starts_with(s, "SPC"))
    { /* ' ' */
        return {s.substr(3), esc | ' '};
    }

    if (svu::ic_starts_with(s, "TAB"))
    { /* ^i */
        return {s.substr(3), esc | '\t'};
    }

    if (svu::ic_starts_with(s, "DEL"))
    { /* ^? */
        return {s.substr(3), esc | DEL_CODE};
    }

    if (s.size() >= 2 && s[0] == '\\')
    {
        s.remove_prefix(1);
        switch (s[0])
        {
        case 'a': /* ^g */
            return {s.substr(1), esc | CTRL_G};
        case 'b': /* ^h */
            return {s.substr(1), esc | CTRL_H};
        case 't': /* ^i */
            return {s.substr(1), esc | CTRL_I};
        case 'n': /* ^j */
            return {s.substr(1), esc | CTRL_J};
        case 'r': /* ^m */
            return {s.substr(1), esc | CTRL_M};
        case 'e': /* ^[ */
            return {s.substr(1), esc | ESC_CODE};
        case '^': /* ^ */
            return {s.substr(1), esc | '^'};
        case '\\': /* \ */
            return {s.substr(1), esc | '\\'};
        default:
            return {s.substr(1), -1};
        }
    }

    if (IS_ASCII(s[0])) /* Ascii */
        return {s.substr(1), esc | s[0]};
    else
        return {s.substr(1), -1};
}

std::tuple<std::string_view, int> getKey(std::string_view s)
{
    int c;
    std::tie(s, c) = getKey2(s);
    if (c < 0)
        return {s, -1};

    if (s.size() && s[0] == ' ' || s[0] == '-')
        s.remove_prefix(1);

    if (s.size())
    {
        int c2;
        std::tie(s, c2) = getKey2(s);
        if (c2 < 0)
            return {s, -1};
        c = K_MULTI | (c << 16) | c2;
    }

    return {s, c};
}

static std::tuple<std::string_view, int> GetKey(std::string_view p, int lineno, int verbose)
{
    std::string s;
    std::tie(p, s) = getQWord(p);
    auto [_, c] = getKey(s);
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
            disp_message_nsec(emsg, false, 1, true, false);
    }
    return {p, c};
}

static std::tuple<std::string_view, Command> GetFunc(std::string_view p, int lineno, int verbose)
{
    std::string s;
    std::tie(p, s) = getWord(p);
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
            disp_message_nsec(emsg, false, 1, true, false);
    }
    return {p, f};
}

void SetKeymap(std::string_view p, int lineno, int verbose)
{
    int c;
    std::tie(p, c) = GetKey(p, lineno, verbose);
    if (c < 0)
    {
        return;
    }

    Command f;
    std::tie(p, f) = GetFunc(p, lineno, verbose);
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
    std::string s;
    std::tie(p, s) = getQWord(p);
    if (s.size())
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

void RegisterCommand(const char *name, const char *key, const char *description, Command command)
{
    g_commandMap[key] = command;
}

Command getFuncList(const std::string &name)
{
    auto found = g_commandMap.find(name);
    if (found == g_commandMap.end())
    {
        return &nulcmd;
    }
    return found->second;
}

std::tuple<std::string_view, std::string_view> getWord(std::string_view str)
{
    int start = 0;
    for (; start < str.size(); ++start)
    {
        if (!IS_SPACE(str[start]))
        {
            break;
        }
    }
    int end = start;
    for (; end < str.size(); ++end)
    {
        if (IS_SPACE(str[end]) || str[end] == ';')
        {
            break;
        }
    }

    return {str.substr(end), str.substr(start, end - start)};
}

std::tuple<std::string_view, std::string> getQWord(std::string_view str)
{
    // char *p;

    std::string tmp;
    int in_q = 0;
    int in_dq = 0;
    int esc = 0;
    auto p = svu::strip_left(str);
    for (; p.size(); p.remove_prefix(1))
    {
        if (esc)
        {
            if (in_q)
            {
                if (p[0] != '\\' && p[0] != '\'') /* '..\\..', '..\'..' */
                    tmp.push_back('\\');
            }
            else if (in_dq)
            {
                if (p[0] != '\\' && p[0] != '"') /* "..\\..", "..\".." */
                    tmp.push_back('\\');
            }
            else
            {
                if (p[0] != '\\' && p[0] != '\'' && /* ..\\.., ..\'.. */
                    p[0] != '"' && !IS_SPACE(p[0])) /* ..\".., ..\.. */
                    tmp.push_back('\\');
            }
            tmp.push_back(p[0]);
            esc = 0;
        }
        else if (p[0] == '\\')
        {
            esc = 1;
        }
        else if (in_q)
        {
            if (p[0] == '\'')
                in_q = 0;
            else
                tmp.push_back(p[0]);
        }
        else if (in_dq)
        {
            if (p[0] == '"')
                in_dq = 0;
            else
                tmp.push_back(p[0]);
        }
        else if (p[0] == '\'')
        {
            in_q = 1;
        }
        else if (p[0] == '"')
        {
            in_dq = 1;
        }
        else if (IS_SPACE(p[0]) || p[0] == ';')
        {
            break;
        }
        else
        {
            tmp.push_back(p[0]);
        }
    }
    return {p, tmp};
}

static char keymap_initialized = false;
static struct stat sys_current_keymap_file;
static struct stat current_keymap_file;

static void interpret_keymap(FILE *kf, struct stat *current, int force)
{
    // Str line;
    // char *p, *s, *emsg;
    // int lineno;

    struct stat kstat;
    int fd;
    if ((fd = fileno(kf)) < 0 || fstat(fd, &kstat) ||
        (!force &&
         kstat.st_mtime == current->st_mtime &&
         kstat.st_dev == current->st_dev &&
         kstat.st_ino == current->st_ino && kstat.st_size == current->st_size))
        return;
    *current = kstat;

    int verbose = 1;
    CharacterEncodingScheme charset = w3mApp::Instance().SystemCharset;
    int lineno = 0;
    while (!feof(kf))
    {
        auto line = Strfgets(kf);
        lineno++;
        Strip(line);
        if (line->Size() == 0)
            continue;

        line = wc_Str_conv(line, charset, w3mApp::Instance().InnerCharset);

        std::string_view p = line->ptr;
        std::string s;
        std::tie(p, s) = getWord(p);
        if (s.size() && s[0] == '#') /* comment */
            continue;
        if (s == "keymap")
            ;
        else if (s == "charset" || s == "encoding")
        {
            std::tie(p, s) = getQWord(p);
            if (s.size())
                charset = wc_guess_charset(s.c_str(), charset);
            continue;
        }
        else if (s == "verbose")
        {
            std::tie(p, s) = getWord(p);
            if (s.size())
                verbose = str_to_bool(s.c_str(), verbose);
            continue;
        }
        else
        { /* error */
            auto emsg = Sprintf("line %d: syntax error '%s'", lineno, s)->ptr;
            record_err_message(emsg);
            if (verbose)
                disp_message_nsec(emsg, false, 1, true, false);
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
    if ((kf = fopen(rcFile(w3mApp::Instance().keymap_file.c_str()), "rt")) != NULL)
    {
        interpret_keymap(kf, &current_keymap_file,
                         force || !keymap_initialized);
        fclose(kf);
    }
    keymap_initialized = true;
}
