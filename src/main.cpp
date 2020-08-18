#include <plog/Log.h> // Step1: include the headers
#include "plog/Initializers/RollingFileInitializer.h"
#include "w3m.h"

int main(int argc, char **argv)
{
    std::u32string a = U"日本語";
    plog::init(plog::debug, "w3m.log"); // Step2: initialize the logger
    PLOGD << "==== utf-8: 開始 ====";
    return w3mApp::Instance().Main(argc, argv);
}
