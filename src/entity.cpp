#ifdef DUMMY
#include "Str.h"
#define NBSP " "
#define UseAltEntity 1
#undef USE_M17N
#else /* DUMMY */
#include "fm.h"
#include "ctrlcode.h"
#endif /* DUMMY */
#include "entity.h"
#include "ucs.h"
#include "utf8.h"
#include "conv.h"
#include "myctype.h"
#include "indep.h"
#include <assert.h>

/* *INDENT-OFF* */
static const char *alt_latin1[96] = {
    " ", "!", "-c-", "-L-", "CUR", "=Y=", "|", "S:",
    "\"", "(C)", "-a", "<<", "NOT", "-", "(R)", "-",
    "DEG", "+-", "^2", "^3", "'", "u", "P:", ".",
    ",", "^1", "-o", ">>", "1/4", "1/2", "3/4", "?",
    "A`", "A'", "A^", "A~", "A:", "AA", "AE", "C,",
    "E`", "E'", "E^", "E:", "I`", "I'", "I^", "I:",
    "D-", "N~", "O`", "O'", "O^", "O~", "O:", "x",
    "O/", "U`", "U'", "U^", "U:", "Y'", "TH", "ss",
    "a`", "a'", "a^", "a~", "a:", "aa", "ae", "c,",
    "e`", "e'", "e^", "e:", "i`", "i'", "i^", "i:",
    "d-", "n~", "o`", "o'", "o^", "o~", "o:", "-:",
    "o/", "u`", "u'", "u^", "u:", "y'", "th", "y:"};
/* *INDENT-ON* */

const char *
conv_entity(unsigned int c, CharacterEncodingScheme ces)
{
    if (c < 0x20) /* C0 */

        return " ";

    char b = c & 0xff;
    if (c < 0x7f) /* ASCII */
        return Strnew_charp_n(&b, 1)->ptr;

    if (c < 0xa0) /* DEL, C1 */
        return " ";

    if (c == 0xa0)
        return NBSP;

    if (c < 0x100)
    {
        /* Latin1 (ISO 8859-1) */
        if (UseAltEntity)
            return alt_latin1[c - 0xa0];
        return wc_conv_n(&b, 1, WC_CES_ISO_8859_1, ces)->ptr;
    }

    if (c <= WC_C_UCS4_END)
    {
        /* Unicode */
        uint8_t utf8[7];
        wc_ucs_to_utf8(c, utf8);
        return wc_conv((char *)utf8, WC_CES_UTF_8, ces)->ptr;
    }

    return "?";
}

#include <unordered_map>
#include <string>

static std::unordered_map<std::string_view, int> g_entityMap = {
    /* 0 */ {"otimes", 0x2297},
    /* 1 */ {"laquo", 0xAB},
    /* 2 */ {"cap", 0x2229},
    /* 3 */ {"dArr", 0x21D3},
    /* 4 */ {"euml", 0xEB},
    /* 5 */ {"sum", 0x2211},
    /* 6 */ {"Ocirc", 0xD4},
    /* 7 */ {"dagger", 0x2020},
    /* 8 */ {"Scaron", 0x0160},
    /* 9 */ {"Omicron", 0x039F},
    /* 10 */ {"brvbar", 0xA6},
    /* 11 */ {"Eta", 0x0397},
    /* 12 */ {"iacute", 0xED},
    /* 13 */ {"aelig", 0xE6},
    /* 14 */ {"Ugrave", 0xD9},
    /* 15 */ {"deg", 0xB0},
    /* 16 */ {"Yuml", 0x0178},
    /* 17 */ {"sup", 0x2283},
    /* 18 */ {"middot", 0xB7},
    /* 19 */ {"ge", 0x2265},
    /* 20 */ {"alefsym", 0x2135},
    /* 21 */ {"sigma", 0x03C3},
    /* 22 */ {"aring", 0xE5},
    /* 23 */ {"Icirc", 0xCE},
    /* 24 */ {"and", 0x2227},
    /* 25 */ {"weierp", 0x2118},
    /* 26 */ {"frac12", 0xBD},
    /* 27 */ {"radic", 0x221A},
    /* 28 */ {"chi", 0x03C7},
    /* 29 */ {"zeta", 0x03B6},
    /* 30 */ {"Theta", 0x0398},
    /* 31 */ {"Atilde", 0xC3},
    /* 32 */ {"para", 0xB6},
    /* 33 */ {"frac14", 0xBC},
    /* 34 */ {"cedil", 0xB8},
    /* 35 */ {"quot", 0x22},
    /* 36 */ {"ang", 0x2220},
    /* 37 */ {"ucirc", 0xFB},
    /* 38 */ {"supe", 0x2287},
    /* 39 */ {"iota", 0x03B9},
    /* 40 */ {"Ograve", 0xD2},
    /* 41 */ {"rArr", 0x21D2},
    /* 42 */ {"Auml", 0xC4},
    /* 43 */ {"frac34", 0xBE},
    /* 44 */ {"nbsp", 0xA0},
    /* 45 */ {"euro", 0x20AC},
    /* 46 */ {"ocirc", 0xF4},
    /* 47 */ {"equiv", 0x2261},
    /* 48 */ {"upsilon", 0x03C5},
    /* 49 */ {"sigmaf", 0x03C2},
    /* 50 */ {"ETH", 0xD0},
    /* 51 */ {"le", 0x2264},
    /* 52 */ {"beta", 0x03B2},
    /* 53 */ {"yacute", 0xFD},
    /* 54 */ {"egrave", 0xE8},
    /* 55 */ {"lowast", 0x2217},
    /* 56 */ {"real", 0x211C},
    /* 57 */ {"amp", 0x26},
    /* 58 */ {"icirc", 0xEE},
    /* 59 */ {"micro", 0xB5},
    /* 60 */ {"isin", 0x2208},
    /* 61 */ {"curren", 0xA4},
    /* 62 */ {"rdquo", 0x201D},
    /* 63 */ {"sbquo", 0x201A},
    /* 64 */ {"ne", 0x2260},
    /* 65 */ {"theta", 0x03B8},
    /* 66 */ {"Igrave", 0xCC},
    /* 67 */ {"gt", 0x3E},
    /* 68 */ {"hearts", 0x2665},
    /* 69 */ {"rang", 0x232A},
    /* 70 */ {"rfloor", 0x230B},
    /* 71 */ {"ldquo", 0x201C},
    /* 72 */ {"ni", 0x220B},
    /* 73 */ {"Ntilde", 0xD1},
    /* 74 */ {"Aacute", 0xC1},
    /* 75 */ {"crarr", 0x21B5},
    /* 76 */ {"Ouml", 0xD6},
    /* 77 */ {"GT", 0x3E},
    /* 78 */ {"clubs", 0x2663},
    /* 79 */ {"scaron", 0x0161},
    /* 80 */ {"part", 0x2202},
    /* 81 */ {"tilde", 0x02DC},
    /* 82 */ {"oelig", 0x0153},
    /* 83 */ {"pi", 0x03C0},
    /* 84 */ {"ugrave", 0xF9},
    /* 85 */ {"darr", 0x2193},
    /* 86 */ {"uuml", 0xFC},
    /* 87 */ {"QUOT", 0x22},
    /* 88 */ {"Prime", 0x2033},
    /* 89 */ {"zwj", 0x200D},
    /* 90 */ {"lfloor", 0x230A},
    /* 91 */ {"notin", 0x2209},
    /* 92 */ {"cent", 0xA2},
    /* 93 */ {"lt", 0x3C},
    /* 94 */ {"eta", 0x03B7},
    /* 95 */ {"Phi", 0x03A6},
    /* 96 */ {"atilde", 0xE3},
    /* 97 */ {"hArr", 0x21D4},
    /* 98 */ {"iuml", 0xEF},
    /* 99 */ {"NBSP", 0xA0},
    /* 100 */ {"mu", 0x03BC},
    /* 101 */ {"or", 0x2228},
    /* 102 */ {"plusmn", 0xB1},
    /* 103 */ {"LT", 0x3C},
    /* 104 */ {"nu", 0x03BD},
    /* 105 */ {"ograve", 0xF2},
    /* 106 */ {"AElig", 0xC6},
    /* 107 */ {"rceil", 0x2309},
    /* 108 */ {"uArr", 0x21D1},
    /* 109 */ {"sect", 0xA7},
    /* 110 */ {"circ", 0x02C6},
    /* 111 */ {"perp", 0x22A5},
    /* 112 */ {"eth", 0xF0},
    /* 113 */ {"rsquo", 0x2019},
    /* 114 */ {"nabla", 0x2207},
    /* 115 */ {"lceil", 0x2308},
    /* 116 */ {"cup", 0x222A},
    /* 117 */ {"exist", 0x2203},
    /* 118 */ {"rarr", 0x2192},
    /* 119 */ {"upsih", 0x03D2},
    /* 120 */ {"prime", 0x2032},
    /* 121 */ {"Omega", 0x03A9},
    /* 122 */ {"Ecirc", 0xCA},
    /* 123 */ {"Epsilon", 0x0395},
    /* 124 */ {"lsquo", 0x2018},
    /* 125 */ {"xi", 0x03BE},
    /* 126 */ {"Lambda", 0x039B},
    /* 127 */ {"Kappa", 0x039A},
    /* 128 */ {"divide", 0xF7},
    /* 129 */ {"igrave", 0xEC},
    /* 130 */ {"acute", 0xB4},
    /* 131 */ {"Euml", 0xCB},
    /* 132 */ {"ordf", 0xAA},
    /* 133 */ {"image", 0x2111},
    /* 134 */ {"Tau", 0x03A4},
    /* 135 */ {"Rho", 0x03A1},
    /* 136 */ {"ntilde", 0xF1},
    /* 137 */ {"aacute", 0xE1},
    /* 138 */ {"times", 0xD7},
    /* 139 */ {"omicron", 0x03BF},
    /* 140 */ {"oplus", 0x2295},
    /* 141 */ {"Zeta", 0x0396},
    /* 142 */ {"Eacute", 0xC9},
    /* 143 */ {"ordm", 0xBA},
    /* 144 */ {"Oslash", 0xD8},
    /* 145 */ {"Ccedil", 0xC7},
    /* 146 */ {"iquest", 0xBF},
    /* 147 */ {"omega", 0x03C9},
    /* 148 */ {"Psi", 0x03A8},
    /* 149 */ {"ecirc", 0xEA},
    /* 150 */ {"int", 0x222B},
    /* 151 */ {"trade", 0x2122},
    /* 152 */ {"kappa", 0x03BA},
    /* 153 */ {"Iota", 0x0399},
    /* 154 */ {"Delta", 0x0394},
    /* 155 */ {"Alpha", 0x0391},
    /* 156 */ {"Otilde", 0xD5},
    /* 157 */ {"sdot", 0x22C5},
    /* 158 */ {"cong", 0x2245},
    /* 159 */ {"rsaquo", 0x203A},
    /* 160 */ {"OElig", 0x0152},
    /* 161 */ {"diams", 0x2666},
    /* 162 */ {"phi", 0x03C6},
    /* 163 */ {"Beta", 0x0392},
    /* 164 */ {"szlig", 0xDF},
    /* 165 */ {"sup1", 0xB9},
    /* 166 */ {"reg", 0xAE},
    /* 167 */ {"harr", 0x2194},
    /* 168 */ {"hellip", 0x2026},
    /* 169 */ {"yuml", 0xFF},
    /* 170 */ {"sup2", 0xB2},
    /* 171 */ {"Gamma", 0x0393},
    /* 172 */ {"sup3", 0xB3},
    /* 173 */ {"forall", 0x2200},
    /* 174 */ {"bdquo", 0x201E},
    /* 175 */ {"spades", 0x2660},
    /* 176 */ {"Pi", 0x03A0},
    /* 177 */ {"Uacute", 0xDA},
    /* 178 */ {"Agrave", 0xC0},
    /* 179 */ {"permil", 0x2030},
    /* 180 */ {"mdash", 0x2014},
    /* 181 */ {"lArr", 0x21D0},
    /* 182 */ {"uarr", 0x2191},
    /* 183 */ {"Upsilon", 0x03A5},
    /* 184 */ {"pound", 0xA3},
    /* 185 */ {"lsaquo", 0x2039},
    /* 186 */ {"lrm", 0x200E},
    /* 187 */ {"lambda", 0x03BB},
    /* 188 */ {"delta", 0x03B4},
    /* 189 */ {"alpha", 0x03B1},
    /* 190 */ {"frasl", 0x2044},
    /* 191 */ {"thorn", 0xFE},
    /* 192 */ {"auml", 0xE4},
    /* 193 */ {"Mu", 0x039C},
    /* 194 */ {"nsub", 0x2284},
    /* 195 */ {"macr", 0xAF},
    /* 196 */ {"minus", 0x2212},
    /* 197 */ {"Nu", 0x039D},
    /* 198 */ {"Oacute", 0xD3},
    /* 199 */ {"prod", 0x220F},
    /* 200 */ {"Uuml", 0xDC},
    /* 201 */ {"iexcl", 0xA1},
    /* 202 */ {"lang", 0x2329},
    /* 203 */ {"tau", 0x03C4},
    /* 204 */ {"rho", 0x03C1},
    /* 205 */ {"gamma", 0x03B3},
    /* 206 */ {"loz", 0x25CA},
    /* 207 */ {"bull", 0x2022},
    /* 208 */ {"piv", 0x03D6},
    /* 209 */ {"eacute", 0xE9},
    /* 210 */ {"zwnj", 0x200C},
    /* 211 */ {"oslash", 0xF8},
    /* 212 */ {"ccedil", 0xE7},
    /* 213 */ {"THORN", 0xDE},
    /* 214 */ {"Iuml", 0xCF},
    /* 215 */ {"not", 0xAC},
    /* 216 */ {"sim", 0x223C},
    /* 217 */ {"thetasym", 0x03D1},
    /* 218 */ {"Acirc", 0xC2},
    /* 219 */ {"Dagger", 0x2021},
    /* 220 */ {"fnof", 0x0192},
    /* 221 */ {"rlm", 0x200F},
    /* 222 */ {"oline", 0x203E},
    /* 223 */ {"Chi", 0x03A7},
    /* 224 */ {"Xi", 0x039E},
    /* 225 */ {"otilde", 0xF5},
    /* 226 */ {"Iacute", 0xCD},
    /* 227 */ {"copy", 0xA9},
    /* 228 */ {"ndash", 0x2013},
    /* 229 */ {"ouml", 0xF6},
    /* 230 */ {"psi", 0x03C8},
    /* 231 */ {"sube", 0x2286},
    /* 232 */ {"emsp", 0x2003},
    /* 233 */ {"asymp", 0x2248},
    /* 234 */ {"prop", 0x221D},
    /* 235 */ {"infin", 0x221E},
    /* 236 */ {"empty", 0x2205},
    /* 237 */ {"uacute", 0xFA},
    /* 238 */ {"agrave", 0xE0},
    /* 239 */ {"shy", 0xAD},
    /* 240 */ {"ensp", 0x2002},
    /* 241 */ {"acirc", 0xE2},
    /* 242 */ {"sub", 0x2282},
    /* 243 */ {"epsilon", 0x03B5},
    /* 244 */ {"Yacute", 0xDD},
    /* 245 */ {"Egrave", 0xC8},
    /* 246 */ {"there4", 0x2234},
    /* 247 */ {"larr", 0x2190},
    /* 248 */ {"uml", 0xA8},
    /* 249 */ {"AMP", 0x26},
    /* 250 */ {"Sigma", 0x03A3},
    /* 251 */ {"Aring", 0xC5},
    /* 252 */ {"yen", 0xA5},
    /* 253 */ {"oacute", 0xF3},
    /* 254 */ {"raquo", 0xBB},
    /* 255 */ {"thinsp", 0x2009},
    /* 256 */ {"Ucirc", 0xDB},
};

static int GetEntity(std::string_view src)
{
    auto found = g_entityMap.find(src);
    if (found == g_entityMap.end())
    {
        // not found
        return -1;
    }
    return found->second;
}

static std::pair<const char *, int> getescapechar_sharp(const char *p)
{
    if (*p == 'x' || *p == 'X')
    {
        // hex
        p++;
        if (!IS_XDIGIT(*p))
        {
            return {p, -1};
        }
        uint32_t dummy = 0;
        for (; IS_XDIGIT(*p); p++)
        {
            dummy = dummy * 0x10 + GET_MYCDIGIT(*p);
        }
        if (*p == ';')
        {
            p++;
        }
        return {p, dummy};
    }
    else
    {
        // digit
        if (!IS_DIGIT(*p))
        {
            return {p, -1};
        }
        uint32_t dummy = 0;
        for (; IS_DIGIT(*p); p++)
        {
            dummy = dummy * 10 + GET_MYCDIGIT(*p);
        }
        if (*p == ';')
        {
            p++;
        }
        return {p, dummy};
    }

    assert(false);
    return std::make_pair(p, -1);
}

static std::string_view g_nonstrict_entities[] = {
    "lt",
    "gt",
    "amp",
    "quot",
    "nbsp",
};

static inline bool iequals(std::string_view l, std::string_view r)
{
    if (l.size() != r.size())
    {
        return false;
    }
    for (int i = 0; i < l.size(); ++i)
    {
        if (tolower(l[i]) != tolower(r[i]))
        {
            return false;
        }
    }
    return true;
}

static std::pair<const char *, int> getescapechar_entity(const char *p)
{
    auto q = p;
    for (p++; IS_ALNUM(*p); p++)
        ;
    auto word = std::string_view(q, p - q);

    auto strict_entity = true;
    if (*p != '=')
    {
        for (auto entity : g_nonstrict_entities)
        {
            /* a character entity MUST be terminated with ";". However,
            * there's MANY web pages which uses &lt , &gt or something
            * like them as &lt;, &gt;, etc. Therefore, we treat the most
            * popular character entities (including &#xxxx;) without
            * the last ";" as character entities. If the trailing character
            * is "=", it must be a part of query in an URL. So &lt=, &gt=, etc.
            * are not regarded as character entities.
            */
            if (iequals(word, entity))
            {
                strict_entity = false;
                break;
            }
        }
    }
    if (*p == ';')
        p++;
    else if (strict_entity)
    {
        return {p, -1};
    }

    return {p, GetEntity(word)};
}

int getescapechar(const char **str)
{
    const char *p = *str;
    if (*p == '&')
    {
        p++;
    }
    if (*p == '#')
    {
        auto [q, dummy] = getescapechar_sharp(p + 1);
        *str = q;
        return dummy;
    }
    if (!IS_ALPHA(*p))
    {
        *str = p;
        return -1;
    }

    auto [r, dummy] = getescapechar_entity(p);
    *str = r;
#ifndef NDEBUG
    if (dummy != -1)
    {
        auto a = 0;
    }
#endif
    return dummy;
}

std::pair<const char *, std::string_view> getescapecmd(const char *s, CharacterEncodingScheme ces)
{
    auto save = s;
    int ch = getescapechar(&s);
    if (ch >= 0)
    {
        // ENTITY
        return {s, std::string_view(conv_entity(ch, ces))};
    }
    else
    {
        // NOT ENTITY
        return {s, std::string_view(save, s - save)};
    }
}

///
/// &#12345; => \xxx\xxx\xxx
///
char *
html_unquote(const char *str, CharacterEncodingScheme ces)
{
#ifndef NDEBUG
    std::string org = str;
#endif

    Str tmp = Strnew();
    for (auto p = str; *p;)
    {
        if (*p == '&')
        {
            auto [pos, q] = getescapecmd(p, ces);
            p = pos;
            tmp->Push(q);
        }
        else
        {
            tmp->Push(*p);
            p++;
        }
    }

#ifndef NDEBUG
    assert(org == str);
#endif

    return tmp->ptr;
}
