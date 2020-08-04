#pragma once
#include "html.h"
#include "Str.h"

/* Parsed Tag structure */

struct parsed_tag
{
    unsigned char tagid;
    unsigned char *attrid;
    char **value;
    unsigned char *map;
    char need_reconstruct;
};

#define parsedtag_accepts(tag, id) ((tag)->map && (tag)->map[id] != MAX_TAGATTR)
#define parsedtag_exists(tag, id) (parsedtag_accepts(tag, id) && ((tag)->attrid[(tag)->map[id]] != ATTR_UNKNOWN))
#define parsedtag_delete(tag, id) (parsedtag_accepts(tag, id) && ((tag)->attrid[(tag)->map[id]] = ATTR_UNKNOWN))
#define parsedtag_need_reconstruct(tag) ((tag)->need_reconstruct)
#define parsedtag_attname(tag, i) (AttrMAP[(tag)->attrid[i]].name)

extern struct parsed_tag *parse_tag(char **s, int internal);
extern int parsedtag_get_value(struct parsed_tag *tag, int id, void *value);
inline std::string_view parsedtag_get_value(const struct parsed_tag &tag, int id)
{
    char *value;
    parsedtag_get_value(const_cast<struct parsed_tag *>(&tag), id, &value);
    if (!value)
    {
        return "";
    }
    return value;
}

extern int parsedtag_set_value(struct parsed_tag *tag, int id, const char *value);
extern Str parsedtag2str(struct parsed_tag *tag);
