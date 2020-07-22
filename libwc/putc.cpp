
#include "wc.h"
#include "wtf.h"

static wc_status putc_st;
static wc_ces putc_f_ces, putc_t_ces;
static Str putc_str;

void
wc_putc_init(wc_ces f_ces, wc_ces t_ces)
{
    wc_output_init(t_ces, &putc_st);
    putc_str = Strnew_size(8);
    putc_f_ces = f_ces;
    putc_t_ces = t_ces;
}

void
wc_putc(char *c, FILE *f)
{
    uint8_t *p;

    if (putc_f_ces != WC_CES_WTF)
	p = (uint8_t *)wc_conv(c, putc_f_ces, WC_CES_WTF)->ptr;
    else
	p = (uint8_t *)c;

    putc_str->Clear();
    while (*p)
	(*putc_st.ces_info->push_to)(putc_str, wtf_parse(&p), &putc_st);
    fwrite(putc_str->ptr, 1, putc_str->length, f);
}

void
wc_putc_end(FILE *f)
{
    putc_str->Clear();
    wc_push_end(putc_str, &putc_st);
    if (putc_str->length)
	fwrite(putc_str->ptr, 1, putc_str->length, f);
}

void
wc_putc_clear_status(void)
{
    if (putc_st.ces_info->id & WC_CES_T_ISO_2022) {
	putc_st.gl = 0;
	putc_st.gr = 0;
	putc_st.ss = 0;
	putc_st.design[0] = 0;
	putc_st.design[1] = 0;
	putc_st.design[2] = 0;
	putc_st.design[3] = 0;
    }
}

