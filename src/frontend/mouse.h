#pragma once

enum class MouseBtnAction
{
    BTN1_DOWN = 0,
    BTN2_DOWN = 1,
    BTN3_DOWN = 2,
    BTN4_DOWN_RXVT = 3,
    BTN5_DOWN_RXVT = 4,
    BTN4_DOWN_XTERM = 64,
    BTN5_DOWN_XTERM = 65,
    BTN_UP = 3,
    BTN_RESET = -1,
};

void initMouseAction();
void DisableMouseAction();
bool TryGetMouseActionPosition(int *x, int *y);
const char *GetMouseActionMenuStr();
int GetMouseActionMenuWidth();
const char *GetMouseActionLastlineStr();
void do_mouse_action(MouseBtnAction btn, int x, int y);
void process_mouse(MouseBtnAction btn, int x, int y);
