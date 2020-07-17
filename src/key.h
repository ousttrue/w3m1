#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

    int CurrentKey();
    void ClearCurrentKey();
    void SetCurrentKey(int c);
    void SetMultiKey(int c);
    int CurrentIsMultiKey();
    int MULTI_KEY(int c);
    void CurrentKeyToPrev();
    int prev_key();
    void set_prev_key(int key);

#ifdef __cplusplus
}
#endif
