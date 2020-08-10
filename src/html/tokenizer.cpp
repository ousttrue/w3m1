#include "tokenizer.h"
#include "myctype.h"

int next_status(char c, int *status)
{
    switch (*status)
    {
    case R_ST_NORMAL:
        if (c == '<')
        {
            *status = R_ST_TAG0;
            return 0;
        }
        else if (c == '&')
        {
            *status = R_ST_AMP;
            return 1;
        }
        else
            return 1;
        break;
    case R_ST_TAG0:
        if (c == '!')
        {
            *status = R_ST_CMNT1;
            return 0;
        }
        *status = R_ST_TAG;
        /* continues to next case */
    case R_ST_TAG:
        if (c == '>')
            *status = R_ST_NORMAL;
        else if (c == '=')
            *status = R_ST_EQL;
        return 0;
    case R_ST_EQL:
        if (c == '"')
            *status = R_ST_DQUOTE;
        else if (c == '\'')
            *status = R_ST_QUOTE;
        else if (IS_SPACE(c))
            *status = R_ST_EQL;
        else if (c == '>')
            *status = R_ST_NORMAL;
        else
            *status = R_ST_VALUE;
        return 0;
    case R_ST_QUOTE:
        if (c == '\'')
            *status = R_ST_TAG;
        return 0;
    case R_ST_DQUOTE:
        if (c == '"')
            *status = R_ST_TAG;
        return 0;
    case R_ST_VALUE:
        if (c == '>')
            *status = R_ST_NORMAL;
        else if (IS_SPACE(c))
            *status = R_ST_TAG;
        return 0;
    case R_ST_AMP:
        if (c == ';')
        {
            *status = R_ST_NORMAL;
            return 0;
        }
        else if (c != '#' && !IS_ALNUM(c) && c != '_')
        {
            /* something's wrong! */
            *status = R_ST_NORMAL;
            return 0;
        }
        else
            return 0;
    case R_ST_CMNT1:
        switch (c)
        {
        case '-':
            *status = R_ST_CMNT2;
            break;
        case '>':
            *status = R_ST_NORMAL;
            break;
        default:
            *status = R_ST_IRRTAG;
        }
        return 0;
    case R_ST_CMNT2:
        switch (c)
        {
        case '-':
            *status = R_ST_CMNT;
            break;
        case '>':
            *status = R_ST_NORMAL;
            break;
        default:
            *status = R_ST_IRRTAG;
        }
        return 0;
    case R_ST_CMNT:
        if (c == '-')
            *status = R_ST_NCMNT1;
        return 0;
    case R_ST_NCMNT1:
        if (c == '-')
            *status = R_ST_NCMNT2;
        else
            *status = R_ST_CMNT;
        return 0;
    case R_ST_NCMNT2:
        switch (c)
        {
        case '>':
            *status = R_ST_NORMAL;
            break;
        case '-':
            *status = R_ST_NCMNT2;
            break;
        default:
            if (IS_SPACE(c))
                *status = R_ST_NCMNT3;
            else
                *status = R_ST_CMNT;
            break;
        }
        break;
    case R_ST_NCMNT3:
        switch (c)
        {
        case '>':
            *status = R_ST_NORMAL;
            break;
        case '-':
            *status = R_ST_NCMNT1;
            break;
        default:
            if (IS_SPACE(c))
                *status = R_ST_NCMNT3;
            else
                *status = R_ST_CMNT;
            break;
        }
        return 0;
    case R_ST_IRRTAG:
        if (c == '>')
            *status = R_ST_NORMAL;
        return 0;
    }
    /* notreached */
    return 0;
}

int read_token(Str buf, char **instr, int *status, int pre, int append)
{
    if (!append)
        buf->Clear();
    if (**instr == '\0')
        return 0;

    int prev_status;
    auto p = *instr;
    for (; *p; p++)
    {
        prev_status = *status;
        next_status(*p, status);
        switch (*status)
        {
        case R_ST_NORMAL:
            if (prev_status == R_ST_AMP && *p != ';')
            {
                p--;
                break;
            }
            if (prev_status == R_ST_NCMNT2 || prev_status == R_ST_NCMNT3 ||
                prev_status == R_ST_IRRTAG || prev_status == R_ST_CMNT1)
            {
                if (prev_status == R_ST_CMNT1 && !append && !pre)
                    buf->Clear();
                if (pre)
                    buf->Push(*p);
                p++;
                goto proc_end;
            }
            buf->Push((!pre && IS_SPACE(*p)) ? ' ' : *p);
            if (ST_IS_REAL_TAG(prev_status))
            {
                *instr = p + 1;
                if (buf->Size() < 2 ||
                    buf->ptr[buf->Size() - 2] != '<' ||
                    buf->ptr[buf->Size() - 1] != '>')
                    return 1;
                buf->Pop(2);
            }
            break;
        case R_ST_TAG0:
        case R_ST_TAG:
            if (prev_status == R_ST_NORMAL && p != *instr)
            {
                *instr = p;
                *status = prev_status;
                return 1;
            }
            if (*status == R_ST_TAG0 && !REALLY_THE_BEGINNING_OF_A_TAG(p))
            {
                /* it seems that this '<' is not a beginning of a tag */
                /*
                 * buf->Push( "&lt;");
                 */
                buf->Push('<');
                *status = R_ST_NORMAL;
            }
            else
                buf->Push(*p);
            break;
        case R_ST_EQL:
        case R_ST_QUOTE:
        case R_ST_DQUOTE:
        case R_ST_VALUE:
        case R_ST_AMP:
            buf->Push(*p);
            break;
        case R_ST_CMNT:
        case R_ST_IRRTAG:
            if (pre)
                buf->Push(*p);
            else if (!append)
                buf->Clear();
            break;
        case R_ST_CMNT1:
        case R_ST_CMNT2:
        case R_ST_NCMNT1:
        case R_ST_NCMNT2:
        case R_ST_NCMNT3:
            /* do nothing */
            if (pre)
                buf->Push(*p);
            break;
        }
    }
proc_end:
    *instr = p;
    return 1;
}
