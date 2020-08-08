#include "w3m.h"
#define MAINPROGRAM
#include "fm.h"

int main(int argc, char **argv)
{
    std::u32string a = U"日本語";

    return w3mApp::Instance().Main(argc, argv);
}
