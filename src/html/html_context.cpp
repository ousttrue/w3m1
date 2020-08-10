#include "html/html_context.h"
#include "fm.h"
#include "indep.h"
#include "w3m.h"
#include "frontend/terms.h"

HtmlContext::HtmlContext()
{
    if (fmInitialized && graph_ok())
    {
        symbol_width = symbol_width0 = 1;
    }
    else
    {
        symbol_width0 = 0;
        get_symbol(w3mApp::Instance().DisplayCharset, &symbol_width0);
        symbol_width = WcOption.use_wide ? symbol_width0 : 1;
    }
}

HtmlContext::~HtmlContext()
{
}

Str HtmlContext::GetLinkNumberStr(int correction)
{
    return Sprintf("[%d]", cur_hseq + correction);
}
