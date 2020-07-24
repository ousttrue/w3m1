#pragma once
#include <vector>

struct Image;
struct FormItemList;
struct Anchor
{
    char *url = nullptr;
    char *target = nullptr;
    char *referer = nullptr;
    char *title = nullptr;
    unsigned char accesskey = 0;
    BufferPoint start;
    BufferPoint end;
    int hseq = 0;
    char slave = 0;
    short y = 0;
    short rows = 0;
    Image *image = nullptr;
    FormItemList *item = nullptr;

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

    Anchor *
    Put(char *url, char *target, char *referer, char *title, unsigned char key, int line, int pos, FormItemList *fi = nullptr);

    const Anchor *
    RetrieveAnchor(const BufferPoint &bp) const;

    const Anchor *
    RetrieveAnchor(int line, int pos) const
    {
        BufferPoint bp;
        bp.line = line;
        bp.pos = pos;
        return RetrieveAnchor(bp);
    }

    const Anchor *SearchByUrl(const char *str) const;
    const Anchor *ClosestNext(const Anchor *an, int x, int y) const;
    const Anchor *ClosestPrev(const Anchor *an, int x, int y) const;
};
