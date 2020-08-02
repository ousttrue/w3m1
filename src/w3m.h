#pragma once

class w3mApp
{
    w3mApp();
    ~w3mApp();

    w3mApp(const w3mApp &) = delete;
    w3mApp &operator=(const w3mApp &) = delete;

public:
    // 使いわないのが目標
    static w3mApp &Instance()
    {
        static w3mApp w3m;
        return w3m;
    }

    int Main(int argc, char **argv);
};

// keymap などで起動される関数。
// この関数内で、global 変数 Currentbuf 等へのアクセスを避ける( w3m から取れるようにする)
using Command = void (*)(w3mApp *w3m);
