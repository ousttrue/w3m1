#pragma once
#include <vector>
#include <string>
#include <string_view>
#include <memory>
#include "bufferpoint.h"
#include "stream/http.h"

struct Image;
struct FormItem;
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
    std::shared_ptr<FormItem> item;

    static std::shared_ptr<Anchor> CreateHref(std::string_view url, std::string_view target, HttpReferrerPolicy referer,
                                std::string title, unsigned char key, int line, int pos)
    {
        auto p = std::make_shared<Anchor>();
        *p = Anchor{
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
        return p;
    }

    static std::shared_ptr<Anchor> CreateName(std::string_view url, int line, int pos)
    {
        auto p = std::make_shared<Anchor>();
        *p = Anchor{
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
        return p;
    }

    static std::shared_ptr<Anchor> CreateImage(std::string_view url, std::string_view title, int line, int pos)
    {
        auto p = std::make_shared<Anchor>();
        *p = Anchor{
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
        return p;
    }

    int CmpOnAnchor(const BufferPoint &bp) const;
};
using AnchorPtr = std::shared_ptr<Anchor>;

class AnchorList
{
    mutable int m_acache = -1;

public:
    std::vector<AnchorPtr> anchors;

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

    void Put(const AnchorPtr &a);

    AnchorPtr RetrieveAnchor(const BufferPoint &bp) const;
    AnchorPtr SearchByUrl(const char *str) const;
    AnchorPtr ClosestNext(const AnchorPtr &an, int x, int y) const;
    AnchorPtr ClosestPrev(const AnchorPtr &an, int x, int y) const;
};
