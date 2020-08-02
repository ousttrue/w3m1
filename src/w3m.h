#pragma once

class w3mApp
{
public:
    w3mApp();
    ~w3mApp();
    
    // 使いわないのが目標
    static w3mApp &Instance()
    {
        static w3mApp w3m;
        return w3m;
    }
};

// keymap などで起動される関数。
// この関数内で、global 変数 Currentbuf 等へのアクセスを避ける( w3m から取れるようにする)
using Command = void (*)(w3mApp *w3m);
