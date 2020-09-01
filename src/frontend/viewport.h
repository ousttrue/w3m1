#pragma once
#include <tuple>
#include <math.h>

//
// viewport for buffer
//
struct Viewport
{
    // viewport top left corner
    short rootX = 0;
    short rootY = 0;
    // viewport size
    short cols = 0;
    short lines = 0;
    // buffer local cursor position
    short cursorX = 0;
    short cursorY = 0;

    short right() const { return rootX + cols; }
    short bottom() const { return rootY + lines; }
    void resetCursor()
    {
        cursorX = 0;
        cursorY = 0;
    }

    std::tuple<int, int> globalXY() const
    {
        return std::make_pair(
            rootX + cursorX,
            rootY + cursorY);
    }

    // void updateRootX(int lastRealLineNumber)
    // {
    //     if (rootX == 0)
    //     {
    //         if (lastRealLineNumber)
    //             rootX = (int)(log(lastRealLineNumber + 0.1) / log(10)) + 2;
    //         if (rootX < 5)
    //             rootX = 5;
    //         if (rootX > cols)
    //             rootX = cols;
    //         cols = cols - rootX;
    //     }
    // }
};
