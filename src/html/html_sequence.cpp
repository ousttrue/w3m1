#include "html_sequence.h"
#include "indep.h"

Str HSequence::GetLinkNumberStr(int correction)
{
    return Sprintf("[%d]", cur_hseq + correction);
}
