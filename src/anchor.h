#pragma once
#include <vector>

struct Image;
struct Anchor
{
    char *url;
    char *target;
    char *referer;
    char *title;
    unsigned char accesskey;
    BufferPoint start;
    BufferPoint end;
    int hseq;
    char slave;
    short y;
    short rows;
    Image *image;

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
    Put(char *url, char *target, char *referer, char *title, unsigned char key, int line, int pos);

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
};

const Anchor *searchAnchor(const AnchorList &al, char *str);
const Anchor *closest_next_anchor(AnchorList &a, const Anchor *an, int x, int y);
const Anchor *closest_prev_anchor(AnchorList &a, const Anchor *an, int x, int y);
