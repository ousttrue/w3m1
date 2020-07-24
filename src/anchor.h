
#pragma once
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
    int m_anchormax = 0;
    int m_size = 0;

public:
    Anchor *anchors = nullptr;

    void clear()
    {
        anchors = nullptr;
        m_size = 0;
        m_anchormax = 0;
        m_acache = -1;
    }

    int size() const
    {
        return m_size;
    }

    operator bool() const
    {
        return m_size > 0;
    }

    Anchor *
    Put(char *url, char *target, char *referer, char *title, unsigned char key, int line, int pos);

    Anchor *
    RetrieveAnchor(const BufferPoint &bp) const;

    Anchor *
    RetrieveAnchor(int line, int pos) const
    {
        BufferPoint bp;
        bp.line = line;
        bp.pos = pos;
        return RetrieveAnchor(bp);
    }
};

Anchor *searchAnchor(const AnchorList &al, char *str);
Anchor *closest_next_anchor(AnchorList &a, Anchor *an, int x, int y);
Anchor *closest_prev_anchor(AnchorList &a, Anchor *an, int x, int y);
