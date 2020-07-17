#pragma once

#ifdef __cplusplus
extern "C"
{
#endif
#include "func.h"

    // keybind.c
    extern unsigned char GlobalKeymap[];

    // funcname.c
    extern FuncList w3mFuncList[];

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
    void DispatchKey(int c);
    void ExecuteCommand(char *data);
    char *GetKeyData(int key);
    void SetKeymap(char *p, int lineno, int verbose);

#ifdef __cplusplus
}
#endif
