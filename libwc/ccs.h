#pragma once
#include <stdint.h>
// #include "iso2022.h"
#define WC_C_ESC	0x1B	/* '\033' */
#define WC_C_SS2	0x4E	/* ESC 'N' */
#define WC_C_SS3	0x4F	/* ESC 'O' */
#define WC_C_LS2	0x6E	/* ESC 'n' */
#define WC_C_LS3	0x6F	/* ESC 'o' */
#define WC_C_LS1R	0x7E	/* ESC '~' */
#define WC_C_LS2R	0x7D	/* ESC '}' */
#define WC_C_LS3R	0x7C	/* ESC '|' */
#define WC_C_G0_CS94	0x28	/* ESC '(' F */
#define WC_C_G1_CS94	0x29	/* ESC ')' F */
#define WC_C_G2_CS94	0x2A	/* ESC '*' F */
#define WC_C_G3_CS94	0x2B	/* ESC '+' F */
#define WC_C_G0_CS96	0x2C	/* ESC ',' F */ /* ISO 2022 does not permit */
#define WC_C_G1_CS96	0x2D	/* ESC '-' F */
#define WC_C_G2_CS96	0x2E	/* ESC '.' F */
#define WC_C_G3_CS96	0x2F	/* ESC '/' F */
#define WC_C_MBCS	0x24	/* ESC '$' G F */
#define WC_C_CS942	0x21	/* ESC G '!' F */
#define WC_C_C0		0x21	/* ESC '!' F */
#define WC_C_C1		0x22	/* ESC '"' F */
#define WC_C_REP	0x26	/* ESC '&' F ESC '"' F */
#define WC_C_CSWSR	0x25	/* ESC '%' F */
#define WC_C_CSWOSR	0x2F	/* ESC '%' '/' F */

#define WC_C_SO		0x0E	/* '\016' */
#define WC_C_SI		0x0F	/* '\017' */
#define WC_C_SS2R	0x8E
#define WC_C_SS3R	0x8F

#define WC_F_ISO_646_US		0x42	/* 'B' */
#define WC_F_ISO_646_IRV	WC_F_ISO_646_US
#define WC_F_US_ASCII		WC_F_ISO_646_US
#define WC_F_JIS_X_0201K	0x49	/* 'I' */
#define WC_F_JIS_X_0201		0x4A	/* 'J' */
#define WC_F_GB_1988		0x54	/* 'T' */

#define WC_F_ISO_8859_1		0x41	/* 'A' */
#define WC_F_ISO_8859_2		0x42	/* 'B' */
#define WC_F_ISO_8859_3		0x43	/* 'C' */
#define WC_F_ISO_8859_4		0x44	/* 'D' */
#define WC_F_ISO_8859_5		0x4C	/* 'L' */
#define WC_F_ISO_8859_6		0x47	/* 'G' */
#define WC_F_ISO_8859_7		0x46	/* 'F' */
#define WC_F_ISO_8859_8		0x48	/* 'H' */
#define WC_F_ISO_8859_9		0x4D	/* 'M' */
#define WC_F_ISO_8859_10	0x56	/* 'V' */
#define WC_F_ISO_8859_11	0x54	/* 'T' */
#define WC_F_TIS_620		WC_F_ISO_8859_11
#define WC_F_ISO_8859_13	0x59	/* 'Y' */
#define WC_F_ISO_8859_14	0x5F	/* '_' */
#define WC_F_ISO_8859_15	0x62	/* 'b' */
#define WC_F_ISO_8859_16	0x66	/* 'f' */

#define WC_F_JIS_C_6226		0x40	/* '@' */
#define WC_F_GB_2312		0x41	/* 'A' */
#define WC_F_JIS_X_0208		0x42	/* 'B' */
#define WC_F_KS_X_1001		0x43	/* 'C' */
#define WC_F_KS_C_5601		WC_F_KS_X_1001
#define WC_F_JIS_X_0212		0x44	/* 'D' */
#define WC_F_ISO_IR_165		0x45	/* 'E' */
#define WC_F_CCITT_GB		WC_F_ISO_IR_165
#define WC_F_CNS_11643_1	0x47	/* 'G' */
#define WC_F_CNS_11643_2	0x48	/* 'H' */
#define WC_F_CNS_11643_3	0x49	/* 'I' */
#define WC_F_CNS_11643_4	0x4A	/* 'J' */
#define WC_F_CNS_11643_5	0x4B	/* 'K' */
#define WC_F_CNS_11643_6	0x4C	/* 'L' */
#define WC_F_CNS_11643_7	0x4D	/* 'M' */
#define WC_F_KPS_9566		0x4E	/* 'N' */
#define WC_F_JIS_X_0213_1	0x4F	/* 'O' */
#define WC_F_JIS_X_0213_2	0x50	/* 'P' */

#define WC_ISO_NOSTATE		0
#define WC_ISO_MBYTE1		1
#define WC_EUC_NOSTATE		0
#define WC_EUC_MBYTE1		2	/* for EUC (G1) */
#define WC_EUC_TW_SS2		3	/* for EUC_TW (G2) */
#define WC_EUC_TW_MBYTE1	4	/* for EUC_TW (G2) */
#define WC_EUC_TW_MBYTE2	5	/* for EUC_TW (G2) */
#define WC_ISO_ESC		6
#define WC_ISO_CSWSR		0x10
#define WC_ISO_CSWOSR		0x20

#define WC_ISO_MAP_CG   0xF0
#define WC_ISO_MAP_C0	0x10			/* 0x00 - 0x1F */
#define WC_ISO_MAP_GL	0x00			/* 0x21 - 0x7E */
#define WC_ISO_MAP_GL96	0x20			/* 0x20,  0x7F */
#define WC_ISO_MAP_C1	0x50			/* 0x80 - 0x9F */
#define WC_ISO_MAP_GR	0x40			/* 0xA1 - 0xFE */
#define WC_ISO_MAP_GR96	0x60			/* 0xA0,  0xFF */
#define WC_ISO_MAP_SO	(0x1 | WC_ISO_MAP_C0)	/* 0x0E */
#define WC_ISO_MAP_SI	(0x2 | WC_ISO_MAP_C0)	/* 0x0F */
#define WC_ISO_MAP_ESC	(0x3 | WC_ISO_MAP_C0)	/* 0x1B */
#define WC_ISO_MAP_SS2	(0x4 | WC_ISO_MAP_C1)	/* 0x8E */
#define WC_ISO_MAP_SS3	(0x5 | WC_ISO_MAP_C1)	/* 0x8F */
#define WC_ISO_MAP_DETECT	0x4F

#define WC_CS94WUL_N(U,L)	(((U) - 0x21) * 0x5E + (L) - 0x21)
#define WC_CS94W_N(c)	WC_CS94WUL_N(((c) >> 8) & 0x7F, (c) & 0x7F)     
#define WC_CS96WUL_N(U,L)	(((U) - 0x20) * 0x60 + (L) - 0x20)
#define WC_CS96W_N(c)	WC_CS96WUL_N(((c) >> 8) & 0x7F, (c) & 0x7F)     
#define WC_N_CS94WU(c)	((c) / 0x5E + 0x21)
#define WC_N_CS94WL(c)	((c) % 0x5E + 0x21)
#define WC_N_CS94W(c)	((WC_N_CS94WU(c) << 8) + WC_N_CS94WL(c))
#define WC_N_CS96WU(c)	((c) / 0x60 + 0x20)
#define WC_N_CS96WL(c)	((c) % 0x60 + 0x20)
#define WC_N_CS96W(c)	((WC_N_CS96WU(c) << 8) + WC_N_CS96WL(c))

// #include "priv.h"
#define WC_F_SPECIAL 0x00
#define WC_F_CP437 0x01
#define WC_F_CP737 0x02
#define WC_F_CP775 0x03
#define WC_F_CP850 0x04
#define WC_F_CP852 0x05
#define WC_F_CP855 0x06
#define WC_F_CP856 0x07
#define WC_F_CP857 0x08
#define WC_F_CP860 0x09
#define WC_F_CP861 0x0A
#define WC_F_CP862 0x0B
#define WC_F_CP863 0x0C
#define WC_F_CP864 0x0D
#define WC_F_CP865 0x0E
#define WC_F_CP866 0x0F
#define WC_F_CP869 0x10
#define WC_F_CP874 0x11
#define WC_F_CP1006 0x12
#define WC_F_CP1250 0x13
#define WC_F_CP1251 0x14
#define WC_F_CP1252 0x15
#define WC_F_CP1253 0x16
#define WC_F_CP1254 0x17
#define WC_F_CP1255 0x18
#define WC_F_CP1256 0x19
#define WC_F_CP1257 0x1A
#define WC_F_CP1258_1 0x1B
#define WC_F_CP1258_2 0x1C
#define WC_F_TCVN_5712_1 0x1D
#define WC_F_TCVN_5712_2 0x1E
#define WC_F_TCVN_5712_3 0x1F
#define WC_F_VISCII_11_1 0x20
#define WC_F_VISCII_11_2 0x21
#define WC_F_VPS_1 0x22
#define WC_F_VPS_2 0x23
#define WC_F_KOI8_R 0x24
#define WC_F_KOI8_U 0x25
#define WC_F_NEXTSTEP 0x26
#define WC_F_GBK_80 0x27
#define WC_F_RAW 0x28

#define WC_F_SPECIAL_W 0x00
#define WC_F_BIG5 0x01
#define WC_F_BIG5_1 0x02
#define WC_F_BIG5_2 0x03
#define WC_F_CNS_11643_8 0x04
#define WC_F_CNS_11643_9 0x05
#define WC_F_CNS_11643_10 0x06
#define WC_F_CNS_11643_11 0x07
#define WC_F_CNS_11643_12 0x08
#define WC_F_CNS_11643_13 0x09
#define WC_F_CNS_11643_14 0x0A
#define WC_F_CNS_11643_15 0x0B
#define WC_F_CNS_11643_16 0x0C
#define WC_F_CNS_11643_X 0x0D
#define WC_F_GB_12345 0x0E
#define WC_F_JOHAB 0x0F
#define WC_F_JOHAB_1 0x10
#define WC_F_JOHAB_2 0x11
#define WC_F_JOHAB_3 0x12
#define WC_F_SJIS_EXT 0x13
#define WC_F_SJIS_EXT_1 0x14
#define WC_F_SJIS_EXT_2 0x15
#define WC_F_GBK 0x16
#define WC_F_GBK_1 0x17
#define WC_F_GBK_2 0x18
#define WC_F_GBK_EXT 0x19
#define WC_F_GBK_EXT_1 0x1A
#define WC_F_GBK_EXT_2 0x1B
#define WC_F_UHC 0x1C
#define WC_F_UHC_1 0x1D
#define WC_F_UHC_2 0x1E
#define WC_F_HKSCS 0x1F
#define WC_F_HKSCS_1 0x20
#define WC_F_HKSCS_2 0x21

#define WC_F_UCS2 0x00
#define WC_F_UCS4 0x00
#define WC_F_UCS_TAG 0x01
#define WC_F_GB18030 0x02

#define WC_F_C1 0x01

//
#define WC_CCS_SET_CS94(c) ((CodedCharacterSet)(c) | WC_CCS_A_CS94)
#define WC_CCS_SET_CS94W(c) ((CodedCharacterSet)(c) | WC_CCS_A_CS94W)
#define WC_CCS_SET_CS96(c) ((CodedCharacterSet)(c) | WC_CCS_A_CS96)
#define WC_CCS_SET_CS96W(c) ((CodedCharacterSet)(c) | WC_CCS_A_CS96W)
#define WC_CCS_SET_CS942(c) ((CodedCharacterSet)(c) | WC_CCS_A_CS942)
#define WC_CCS_SET_PCS(c) ((c) | WC_CCS_A_PCS)
#define WC_CCS_SET_PCSW(c) ((c) | WC_CCS_A_PCSW)
#define WC_CCS_SET_WCS16(c) ((c) | WC_CCS_A_WCS16)
#define WC_CCS_SET_WCS16W(c) ((c) | WC_CCS_A_WCS16W)
#define WC_CCS_SET_WCS32(c) ((c) | WC_CCS_A_WCS32)
#define WC_CCS_SET_WCS32W(c) ((c) | WC_CCS_A_WCS32W)

#define WC_CCS_IS_WIDE(c) ((c) & (WC_CCS_A_MBYTE | WC_CCS_A_WIDE))
#define WC_CCS_IS_COMB(c) ((c)&WC_CCS_A_COMB)
#define WC_CCS_IS_ISO_2022(c) ((c)&WC_CCS_A_ISO_2022)
#define WC_CCS_IS_UNKNOWN(c) ((c)&WC_CCS_A_UNKNOWN)

#define WC_CCS_SET(c) ((c)&WC_CCS_A_SET)
#define WC_CCS_TYPE(c) ((c)&WC_CCS_A_TYPE)
#define WC_CCS_INDEX(c) ((c)&WC_CCS_A_INDEX)
#define WC_CCS_GET_F(c) WC_CCS_INDEX(c)

#define WC_CCS_IS_UNICODE(c) (WC_CCS_SET(c) == WC_CCS_UCS2 || WC_CCS_SET(c) == WC_CCS_UCS4 || WC_CCS_SET(c) == WC_CCS_UCS_TAG)

enum CodedCharacterSet: uint32_t
{
    WC_CCS_NONE = 0,

    WC_F_ISO_BASE = 0x40,
    WC_F_PCS_BASE = 0x01,
    WC_F_WCS_BASE = 0x00,

    WC_F_CS94_END = WC_F_GB_1988,
    WC_F_CS96_END = WC_F_ISO_8859_16,
    WC_F_CS94W_END = WC_F_JIS_X_0213_2,
    WC_F_CS96W_END = 0,
    WC_F_CS942_END = 0,
    WC_F_PCS_END = WC_F_RAW,
    WC_F_PCSW_END = WC_F_HKSCS_2,
    WC_F_WCS16_END = WC_F_UCS2,
    WC_F_WCS16W_END = WC_F_UCS2,
    WC_F_WCS32_END = WC_F_GB18030,
    WC_F_WCS32W_END = WC_F_GB18030,

    WC_CCS_A_SET = 0x0FFFF,
    WC_CCS_A_TYPE = 0x0FF00,
    WC_CCS_A_INDEX = 0x000FF,
    WC_CCS_A_MBYTE = 0x08000,
    WC_CCS_A_WIDE = 0x10000,
    WC_CCS_A_COMB = 0x20000,
    WC_CCS_A_ISO_2022 = 0x00700,

    WC_CCS_A_CS94 = 0x00100,
    WC_CCS_A_CS96 = 0x00200,
    WC_CCS_A_CS942 = 0x00400,
    WC_CCS_A_PCS = 0x00800,
    WC_CCS_A_WCS16 = 0x01000,
    WC_CCS_A_WCS32 = 0x02000,
    WC_CCS_A_UNKNOWN = 0x04000,
    WC_CCS_A_CS94W = (WC_CCS_A_CS94 | WC_CCS_A_MBYTE),
    WC_CCS_A_CS96W = (WC_CCS_A_CS96 | WC_CCS_A_MBYTE),
    WC_CCS_A_PCSW = (WC_CCS_A_PCS | WC_CCS_A_MBYTE),
    WC_CCS_A_WCS16W = (WC_CCS_A_WCS16 | WC_CCS_A_WIDE),
    WC_CCS_A_WCS32W = (WC_CCS_A_WCS32 | WC_CCS_A_WIDE),
    WC_CCS_A_UNKNOWN_W = (WC_CCS_A_UNKNOWN | WC_CCS_A_MBYTE),
    WC_CCS_A_CS94_C = (WC_CCS_A_CS94 | WC_CCS_A_COMB),
    WC_CCS_A_CS96_C = (WC_CCS_A_CS96 | WC_CCS_A_COMB),
    WC_CCS_A_CS942_C = (WC_CCS_A_CS942 | WC_CCS_A_COMB),
    WC_CCS_A_PCS_C = (WC_CCS_A_PCS | WC_CCS_A_COMB),
    WC_CCS_A_WCS16_C = (WC_CCS_A_WCS16 | WC_CCS_A_COMB),
    WC_CCS_A_WCS32_C = (WC_CCS_A_WCS32 | WC_CCS_A_COMB),
    WC_CCS_A_CS94W_C = (WC_CCS_A_CS94W | WC_CCS_A_COMB),
    WC_CCS_A_CS96W_C = (WC_CCS_A_CS96W | WC_CCS_A_COMB),
    WC_CCS_A_PCSW_C = (WC_CCS_A_PCSW | WC_CCS_A_COMB),
    WC_CCS_A_WCS16W_C = (WC_CCS_A_WCS16W | WC_CCS_A_COMB),
    WC_CCS_A_WCS32W_C = (WC_CCS_A_WCS32W | WC_CCS_A_COMB),

    WC_CCS_US_ASCII = WC_CCS_SET_CS94(WC_F_US_ASCII),
    WC_CCS_JIS_X_0201 = WC_CCS_SET_CS94(WC_F_JIS_X_0201),
    WC_CCS_JIS_X_0201K = WC_CCS_SET_CS94(WC_F_JIS_X_0201K),
    WC_CCS_GB_1988 = WC_CCS_SET_CS94(WC_F_GB_1988),

    WC_CCS_ISO_8859_1 = WC_CCS_SET_CS96(WC_F_ISO_8859_1),
    WC_CCS_ISO_8859_2 = WC_CCS_SET_CS96(WC_F_ISO_8859_2),
    WC_CCS_ISO_8859_3 = WC_CCS_SET_CS96(WC_F_ISO_8859_3),
    WC_CCS_ISO_8859_4 = WC_CCS_SET_CS96(WC_F_ISO_8859_4),
    WC_CCS_ISO_8859_5 = WC_CCS_SET_CS96(WC_F_ISO_8859_5),
    WC_CCS_ISO_8859_6 = WC_CCS_SET_CS96(WC_F_ISO_8859_6),
    WC_CCS_ISO_8859_7 = WC_CCS_SET_CS96(WC_F_ISO_8859_7),
    WC_CCS_ISO_8859_8 = WC_CCS_SET_CS96(WC_F_ISO_8859_8),
    WC_CCS_ISO_8859_9 = WC_CCS_SET_CS96(WC_F_ISO_8859_9),
    WC_CCS_ISO_8859_10 = WC_CCS_SET_CS96(WC_F_ISO_8859_10),
    WC_CCS_ISO_8859_11 = WC_CCS_SET_CS96(WC_F_ISO_8859_11),
    WC_CCS_TIS_620 = WC_CCS_ISO_8859_11,
    WC_CCS_ISO_8859_13 = WC_CCS_SET_CS96(WC_F_ISO_8859_13),
    WC_CCS_ISO_8859_14 = WC_CCS_SET_CS96(WC_F_ISO_8859_14),
    WC_CCS_ISO_8859_15 = WC_CCS_SET_CS96(WC_F_ISO_8859_15),
    WC_CCS_ISO_8859_16 = WC_CCS_SET_CS96(WC_F_ISO_8859_16),

    WC_CCS_SPECIAL = WC_CCS_SET_PCS(WC_F_SPECIAL),
    WC_CCS_CP437 = WC_CCS_SET_PCS(WC_F_CP437),
    WC_CCS_CP737 = WC_CCS_SET_PCS(WC_F_CP737),
    WC_CCS_CP775 = WC_CCS_SET_PCS(WC_F_CP775),
    WC_CCS_CP850 = WC_CCS_SET_PCS(WC_F_CP850),
    WC_CCS_CP852 = WC_CCS_SET_PCS(WC_F_CP852),
    WC_CCS_CP855 = WC_CCS_SET_PCS(WC_F_CP855),
    WC_CCS_CP856 = WC_CCS_SET_PCS(WC_F_CP856),
    WC_CCS_CP857 = WC_CCS_SET_PCS(WC_F_CP857),
    WC_CCS_CP860 = WC_CCS_SET_PCS(WC_F_CP860),
    WC_CCS_CP861 = WC_CCS_SET_PCS(WC_F_CP861),
    WC_CCS_CP862 = WC_CCS_SET_PCS(WC_F_CP862),
    WC_CCS_CP863 = WC_CCS_SET_PCS(WC_F_CP863),
    WC_CCS_CP864 = WC_CCS_SET_PCS(WC_F_CP864),
    WC_CCS_CP865 = WC_CCS_SET_PCS(WC_F_CP865),
    WC_CCS_CP866 = WC_CCS_SET_PCS(WC_F_CP866),
    WC_CCS_CP869 = WC_CCS_SET_PCS(WC_F_CP869),
    WC_CCS_CP874 = WC_CCS_SET_PCS(WC_F_CP874),
    WC_CCS_CP1006 = WC_CCS_SET_PCS(WC_F_CP1006),
    WC_CCS_CP1250 = WC_CCS_SET_PCS(WC_F_CP1250),
    WC_CCS_CP1251 = WC_CCS_SET_PCS(WC_F_CP1251),
    WC_CCS_CP1252 = WC_CCS_SET_PCS(WC_F_CP1252),
    WC_CCS_CP1253 = WC_CCS_SET_PCS(WC_F_CP1253),
    WC_CCS_CP1254 = WC_CCS_SET_PCS(WC_F_CP1254),
    WC_CCS_CP1255 = WC_CCS_SET_PCS(WC_F_CP1255),
    WC_CCS_CP1256 = WC_CCS_SET_PCS(WC_F_CP1256),
    WC_CCS_CP1257 = WC_CCS_SET_PCS(WC_F_CP1257),
    WC_CCS_CP1258_1 = WC_CCS_SET_PCS(WC_F_CP1258_1),
    WC_CCS_CP1258_2 = WC_CCS_SET_PCS(WC_F_CP1258_2),
    WC_CCS_TCVN_5712_1 = WC_CCS_SET_PCS(WC_F_TCVN_5712_1),
    WC_CCS_TCVN_5712_2 = WC_CCS_SET_PCS(WC_F_TCVN_5712_2),
    WC_CCS_TCVN_5712_3 = WC_CCS_SET_PCS(WC_F_TCVN_5712_3),
    WC_CCS_VISCII_11_1 = WC_CCS_SET_PCS(WC_F_VISCII_11_1),
    WC_CCS_VISCII_11_2 = WC_CCS_SET_PCS(WC_F_VISCII_11_2),
    WC_CCS_VPS_1 = WC_CCS_SET_PCS(WC_F_VPS_1),
    WC_CCS_VPS_2 = WC_CCS_SET_PCS(WC_F_VPS_2),
    WC_CCS_KOI8_R = WC_CCS_SET_PCS(WC_F_KOI8_R),
    WC_CCS_KOI8_U = WC_CCS_SET_PCS(WC_F_KOI8_U),
    WC_CCS_NEXTSTEP = WC_CCS_SET_PCS(WC_F_NEXTSTEP),
    WC_CCS_GBK_80 = WC_CCS_SET_PCS(WC_F_GBK_80),
    WC_CCS_RAW = WC_CCS_SET_PCS(WC_F_RAW),

    WC_CCS_JIS_C_6226 = WC_CCS_SET_CS94W(WC_F_JIS_C_6226),
    WC_CCS_JIS_X_0208 = WC_CCS_SET_CS94W(WC_F_JIS_X_0208),
    WC_CCS_JIS_X_0212 = WC_CCS_SET_CS94W(WC_F_JIS_X_0212),
    WC_CCS_GB_2312 = WC_CCS_SET_CS94W(WC_F_GB_2312),
    WC_CCS_ISO_IR_165 = WC_CCS_SET_CS94W(WC_F_ISO_IR_165),
    WC_CCS_CNS_11643_1 = WC_CCS_SET_CS94W(WC_F_CNS_11643_1),
    WC_CCS_CNS_11643_2 = WC_CCS_SET_CS94W(WC_F_CNS_11643_2),
    WC_CCS_CNS_11643_3 = WC_CCS_SET_CS94W(WC_F_CNS_11643_3),
    WC_CCS_CNS_11643_4 = WC_CCS_SET_CS94W(WC_F_CNS_11643_4),
    WC_CCS_CNS_11643_5 = WC_CCS_SET_CS94W(WC_F_CNS_11643_5),
    WC_CCS_CNS_11643_6 = WC_CCS_SET_CS94W(WC_F_CNS_11643_6),
    WC_CCS_CNS_11643_7 = WC_CCS_SET_CS94W(WC_F_CNS_11643_7),
    WC_CCS_KS_X_1001 = WC_CCS_SET_CS94W(WC_F_KS_X_1001),
    WC_CCS_KPS_9566 = WC_CCS_SET_CS94W(WC_F_KPS_9566),
    WC_CCS_JIS_X_0213_1 = WC_CCS_SET_CS94W(WC_F_JIS_X_0213_1),
    WC_CCS_JIS_X_0213_2 = WC_CCS_SET_CS94W(WC_F_JIS_X_0213_2),

    WC_CCS_SPECIAL_W = WC_CCS_SET_PCSW(WC_F_SPECIAL_W),
    WC_CCS_BIG5 = WC_CCS_SET_PCSW(WC_F_BIG5),
    WC_CCS_BIG5_1 = WC_CCS_SET_PCSW(WC_F_BIG5_1),
    WC_CCS_BIG5_2 = WC_CCS_SET_PCSW(WC_F_BIG5_2),
    WC_CCS_CNS_11643_8 = WC_CCS_SET_PCSW(WC_F_CNS_11643_8),
    WC_CCS_CNS_11643_9 = WC_CCS_SET_PCSW(WC_F_CNS_11643_9),
    WC_CCS_CNS_11643_10 = WC_CCS_SET_PCSW(WC_F_CNS_11643_10),
    WC_CCS_CNS_11643_11 = WC_CCS_SET_PCSW(WC_F_CNS_11643_11),
    WC_CCS_CNS_11643_12 = WC_CCS_SET_PCSW(WC_F_CNS_11643_12),
    WC_CCS_CNS_11643_13 = WC_CCS_SET_PCSW(WC_F_CNS_11643_13),
    WC_CCS_CNS_11643_14 = WC_CCS_SET_PCSW(WC_F_CNS_11643_14),
    WC_CCS_CNS_11643_15 = WC_CCS_SET_PCSW(WC_F_CNS_11643_15),
    WC_CCS_CNS_11643_16 = WC_CCS_SET_PCSW(WC_F_CNS_11643_16),
    WC_CCS_CNS_11643_X = WC_CCS_SET_PCSW(WC_F_CNS_11643_X),
    WC_CCS_GB_12345 = WC_CCS_SET_PCSW(WC_F_GB_12345),
    WC_CCS_JOHAB = WC_CCS_SET_PCSW(WC_F_JOHAB),
    WC_CCS_JOHAB_1 = WC_CCS_SET_PCSW(WC_F_JOHAB_1),
    WC_CCS_JOHAB_2 = WC_CCS_SET_PCSW(WC_F_JOHAB_2),
    WC_CCS_JOHAB_3 = WC_CCS_SET_PCSW(WC_F_JOHAB_3),
    WC_CCS_SJIS_EXT = WC_CCS_SET_PCSW(WC_F_SJIS_EXT),
    WC_CCS_SJIS_EXT_1 = WC_CCS_SET_PCSW(WC_F_SJIS_EXT_1),
    WC_CCS_SJIS_EXT_2 = WC_CCS_SET_PCSW(WC_F_SJIS_EXT_2),
    WC_CCS_GBK = WC_CCS_SET_PCSW(WC_F_GBK),
    WC_CCS_GBK_1 = WC_CCS_SET_PCSW(WC_F_GBK_1),
    WC_CCS_GBK_2 = WC_CCS_SET_PCSW(WC_F_GBK_2),
    WC_CCS_GBK_EXT = WC_CCS_SET_PCSW(WC_F_GBK_EXT),
    WC_CCS_GBK_EXT_1 = WC_CCS_SET_PCSW(WC_F_GBK_EXT_1),
    WC_CCS_GBK_EXT_2 = WC_CCS_SET_PCSW(WC_F_GBK_EXT_2),
    WC_CCS_UHC = WC_CCS_SET_PCSW(WC_F_UHC),
    WC_CCS_UHC_1 = WC_CCS_SET_PCSW(WC_F_UHC_1),
    WC_CCS_UHC_2 = WC_CCS_SET_PCSW(WC_F_UHC_2),
    WC_CCS_HKSCS = WC_CCS_SET_PCSW(WC_F_HKSCS),
    WC_CCS_HKSCS_1 = WC_CCS_SET_PCSW(WC_F_HKSCS_1),
    WC_CCS_HKSCS_2 = WC_CCS_SET_PCSW(WC_F_HKSCS_2),

    WC_CCS_UCS2 = WC_CCS_SET_WCS16(WC_F_UCS2),
    WC_CCS_UCS4 = WC_CCS_SET_WCS32(WC_F_UCS4),
    WC_CCS_UCS_TAG = WC_CCS_SET_WCS32(WC_F_UCS_TAG),
    WC_CCS_GB18030 = WC_CCS_SET_WCS32(WC_F_GB18030),
    WC_CCS_UCS2_W = WC_CCS_SET_WCS16W(WC_F_UCS2),
    WC_CCS_UCS4_W = WC_CCS_SET_WCS32W(WC_F_UCS4),
    WC_CCS_UCS_TAG_W = WC_CCS_SET_WCS32W(WC_F_UCS_TAG),
    WC_CCS_GB18030_W = WC_CCS_SET_WCS32W(WC_F_GB18030),

    WC_CCS_UNKNOWN = WC_CCS_A_UNKNOWN,
    WC_CCS_C1 = (WC_CCS_A_UNKNOWN | WC_F_C1),
    WC_CCS_UNKNOWN_W = WC_CCS_A_UNKNOWN_W,
};
template<class CodedCharacterSet> inline CodedCharacterSet operator~ (CodedCharacterSet a) { return (CodedCharacterSet)~(int)a; }
template<class CodedCharacterSet> inline CodedCharacterSet operator| (CodedCharacterSet a, CodedCharacterSet b) { return (CodedCharacterSet)((int)a | (int)b); }
template<class CodedCharacterSet> inline CodedCharacterSet operator& (CodedCharacterSet a, CodedCharacterSet b) { return (CodedCharacterSet)((int)a & (int)b); }
template<class CodedCharacterSet> inline CodedCharacterSet operator^ (CodedCharacterSet a, CodedCharacterSet b) { return (CodedCharacterSet)((int)a ^ (int)b); }
template<class CodedCharacterSet> inline CodedCharacterSet& operator|= (CodedCharacterSet& a, CodedCharacterSet b) { return (CodedCharacterSet&)((int&)a |= (int)b); }
template<class CodedCharacterSet> inline CodedCharacterSet& operator&= (CodedCharacterSet& a, CodedCharacterSet b) { return (CodedCharacterSet&)((int&)a &= (int)b); }
template<class CodedCharacterSet> inline CodedCharacterSet& operator^= (CodedCharacterSet& a, CodedCharacterSet b) { return (CodedCharacterSet&)((int&)a ^= (int)b); }


struct wc_wchar_t
{
    CodedCharacterSet ccs;
    uint32_t code;
};

struct wc_gset
{
    CodedCharacterSet ccs;
    uint8_t g;
    bool init;
};
