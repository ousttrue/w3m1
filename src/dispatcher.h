#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

    typedef void (*Command)();

#define AL_UNSET 0
#define AL_EXPLICIT 1
#define AL_IMPLICIT 2
#define AL_IMPLICIT_ONCE 3

    typedef struct _AlarmEvent
    {
        int sec;
        short status;
        Command cmd;
        void *data;
    } AlarmEvent;

    int prec_num();
    void set_prec_num(int n);
    int PREC_NUM();
    inline int PREC_LIMIT() { return 10000; }
    int CurrentKey();
    void ClearCurrentKey();
    void ClearCurrentKeyData();
    char *CurrentKeyData();
    void SetCurrentKey(int c);
    void SetMultiKey(int c);
    int CurrentIsMultiKey();
    int MultiKey(int c);
    int PrevKey();
    void SetPrevKey(int key);
    void KeyPressEventProc(int c);
    void escKeyProc(char c);
    void escbKeyProc(char c);
    void MultiKeyProc(char c);
    void DispatchKey(int c);
    void ExecuteCommand(char *data);
    char *GetKeyData(int key);
    void SetKeymap(char *p, int lineno, int verbose);
    void RegisterCommand(const char *name, const char *key, const char *description, void (*command)());
    Command getFuncList(char *name);
    char *getQWord(char **str);
    char *getWord(char **str);
    void initKeymap(int force);
    int getKey(char *s);

#ifdef __cplusplus
}
#endif
