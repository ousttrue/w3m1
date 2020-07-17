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

    int CurrentKey();
    void ClearCurrentKey();
    void ClearCurrentKeyData();
    char *CurrentKeyData();
    void SetCurrentKey(int c);
    void SetMultiKey(int c);
    int CurrentIsMultiKey();
    int MultiKey(int c);
    void CurrentKeyToPrev();
    int PrevKey();
    void SetPrevKey(int key);
    void KeyPressEventProc(int c);
    void ExecuteCommand(char *data);

#ifdef __cplusplus
}
#endif
