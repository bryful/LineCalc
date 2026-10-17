#pragma once
#include "framework.h"
#include <algorithm>

inline RECT popupBounds(const RECT& work, const RECT* parent, int width, int height, bool alignRight = false)
{
    const LONG availableWidth = (std::max)(1L, work.right - work.left);
    const LONG availableHeight = (std::max)(1L, work.bottom - work.top);
    const LONG actualWidth = (std::clamp)(static_cast<LONG>(width), 1L, availableWidth);
    const LONG actualHeight = (std::clamp)(static_cast<LONG>(height), 1L, availableHeight);
    LONG x = work.left + (availableWidth - actualWidth) / 2;
    LONG y = work.top + (availableHeight - actualHeight) / 2;
    if (parent)
    {
        x = alignRight ? parent->right - actualWidth : parent->left;
        y = parent->bottom;
        // 下側に収まらない場合は、まず親ウィンドウの上側を選ぶ。
        if (y > work.top + availableHeight - actualHeight) y = parent->top - actualHeight;
    }
    x = (std::clamp)(x, work.left, work.left + availableWidth - actualWidth);
    y = (std::clamp)(y, work.top, work.top + availableHeight - actualHeight);
    return { x, y, x + actualWidth, y + actualHeight };
}

inline RECT popupWorkArea(HWND owner)
{
    POINT cursor{};
    GetCursorPos(&cursor);
    const auto monitor = owner ? MonitorFromWindow(owner, MONITOR_DEFAULTTONEAREST) :
        MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{sizeof(info)};
    if (GetMonitorInfoW(monitor, &info)) return info.rcWork;
    RECT work{0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    return work;
}
