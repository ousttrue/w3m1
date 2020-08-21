#pragma once
#include <vector>
#include <string>
#include <string_view>
#include "bufferpoint.h"
#include "stream/http.h"

struct Image;
struct FormItemList;
struct Anchor
{
    std::string url;
    std::string target;
    HttpReferrerPolicy referer;
    std::string title;
    unsigned char accesskey = 0;
    BufferPoint start;
    BufferPoint end;
    int hseq = 0;
    char slave = 0;
    short y = 0;
    short rows = 0;
    Image *image = nullptr;
    std::shared_ptr<FormItemList> item;

    static Anchor CreateHref(std::string_view url, std::string_view target, HttpReferrerPolicy referer,
                             std::string title, unsigned char key, int line, int pos)
    {
        return Anchor{
            url : std::move(std::string(url)),
            target : std::move(std::string(target)),
            referer : referer,
            title : std::move(std::string(title)),
            accesskey : key,
            start : {
                line : line,
                pos : pos,
            },
            end : {
                line : line,
                pos : pos,
            },
        };
    }

    static Anchor CreateName(std::string_view url, int line, int pos)
    {
        return Anchor{
            url : std::move(std::string(url)),
            accesskey : '\0',
            start : {
                line : line,
                pos : pos,
            },
            end : {
                line : line,
                pos : pos,
            },
        };
    }

    static Anchor CreateImage(std::string_view url, std::string_view title, int line, int pos)
    {
        return Anchor{
            url : std::move(std::string(url)),
            title : std::move(std::string(title)),
            accesskey : '\0',
            start : {
                line : line,
                pos : pos,
            },
            end : {
                line : line,
                pos : pos,
            },
        };
    }

    int CmpOnAnchor(const BufferPoint &bp) const;
};

class AnchorList
{
    mutable int m_acache = -1;

public:
    std::vector<Anchor> anchors;

    void clear()
    {
        m_acache = -1;
    }

    size_t size() const
    {
        return anchors.size();
    }

    operator bool() const
    {
        return !anchors.empty();
    }

    Anchor *Put(const Anchor &a);

    const Anchor *RetrieveAnchor(const BufferPoint &bp) const;
    const Anchor *SearchByUrl(const char *str) const;
    const Anchor *ClosestNext(const Anchor *an, int x, int y) const;
    const Anchor *ClosestPrev(const Anchor *an, int x, int y) const;
};
