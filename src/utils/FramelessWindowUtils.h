#pragma once

#include <QRect>
#include <QVector>
#include <optional>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "user32.lib")
#endif

namespace EnhanceVision::FramelessWindowUtils {

#ifdef Q_OS_WIN

inline bool isWindowMaximizedOrFullscreen(HWND hwnd)
{
    if (IsZoomed(hwnd)) {
        return true;
    }

    RECT windowRect;
    GetWindowRect(hwnd, &windowRect);

    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo = {};
    monitorInfo.cbSize = sizeof(monitorInfo);
    GetMonitorInfo(monitor, &monitorInfo);

    return windowRect.left <= monitorInfo.rcMonitor.left
        && windowRect.top <= monitorInfo.rcMonitor.top
        && windowRect.right >= monitorInfo.rcMonitor.right
        && windowRect.bottom >= monitorInfo.rcMonitor.bottom;
}

inline bool isWindowSnappedToEdge(HWND hwnd)
{
    if (IsZoomed(hwnd)) {
        return true;
    }

    RECT windowRect;
    GetWindowRect(hwnd, &windowRect);

    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo = {};
    monitorInfo.cbSize = sizeof(monitorInfo);
    GetMonitorInfo(monitor, &monitorInfo);

    if (windowRect.left <= monitorInfo.rcMonitor.left
        && windowRect.top <= monitorInfo.rcMonitor.top
        && windowRect.right >= monitorInfo.rcMonitor.right
        && windowRect.bottom >= monitorInfo.rcMonitor.bottom) {
        return true;
    }

    const RECT workArea = monitorInfo.rcWork;
    const LONG workWidth = workArea.right - workArea.left;
    const LONG workHeight = workArea.bottom - workArea.top;
    const LONG windowWidth = windowRect.right - windowRect.left;
    const LONG windowHeight = windowRect.bottom - windowRect.top;

    const bool fillsWidth = (windowRect.left == workArea.left && windowRect.right == workArea.right)
        || (windowWidth >= workWidth);
    const bool fillsHeight = (windowRect.top == workArea.top && windowRect.bottom == workArea.bottom)
        || (windowHeight >= workHeight);

    return fillsWidth || fillsHeight;
}

inline void updateWindowFrame(HWND hwnd, bool removeMargins, bool squareCorners)
{
    const MARGINS margins = removeMargins ? MARGINS{0, 0, 0, 0} : MARGINS{1, 1, 1, 1};
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    const DWM_WINDOW_CORNER_PREFERENCE preference = squareCorners ? DWMWCP_DONOTROUND : DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &preference, sizeof(preference));
}

inline void setupFramelessWindow(HWND hwnd)
{
    const DWM_WINDOW_CORNER_PREFERENCE preference = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &preference, sizeof(preference));

    const BOOL disableTransitions = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_TRANSITIONS_FORCEDISABLED, &disableTransitions, sizeof(disableTransitions));

    const MARGINS margins = {1, 1, 1, 1};
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    const DWORD style = ::GetWindowLongPtr(hwnd, GWL_STYLE);
    ::SetWindowLongPtr(hwnd, GWL_STYLE, style | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX);

    RECT rect;
    GetWindowRect(hwnd, &rect);
    SetWindowPos(hwnd, nullptr, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
                 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);
}

struct HitTestRequest {
    int localX = 0;
    int localY = 0;
    int windowWidth = 0;
    int windowHeight = 0;
    int titleBarHeight = 0;
    int resizeMargin = 0;
    bool isMaximized = false;
    bool isFullScreen = false;
    const QVector<QRect>* excludeRegions = nullptr;
};

inline bool isInExcludeRegion(const QVector<QRect>* excludeRegions, int x, int y)
{
    if (!excludeRegions) {
        return false;
    }

    for (const QRect& region : *excludeRegions) {
        if (region.contains(x, y)) {
            return true;
        }
    }
    return false;
}

inline std::optional<LRESULT> hitTestFramelessWindow(const HitTestRequest& request)
{
    const bool allowResize = !request.isMaximized && !request.isFullScreen;
    const bool onLeft = request.localX < request.resizeMargin;
    const bool onRight = request.localX >= request.windowWidth - request.resizeMargin;
    const bool onTop = request.localY < request.resizeMargin;
    const bool onBottom = request.localY >= request.windowHeight - request.resizeMargin;

    if (request.localY >= 0 && request.localY < request.titleBarHeight
        && !isInExcludeRegion(request.excludeRegions, request.localX, request.localY)) {
        if (allowResize) {
            if (onTop && onLeft) {
                return HTTOPLEFT;
            }
            if (onTop && onRight) {
                return HTTOPRIGHT;
            }
            if (onTop) {
                return HTTOP;
            }
            if (onLeft) {
                return HTLEFT;
            }
            if (onRight) {
                return HTRIGHT;
            }
        }
        return HTCAPTION;
    }

    if (!allowResize) {
        return std::nullopt;
    }

    if (onTop && onLeft) {
        return HTTOPLEFT;
    }
    if (onTop && onRight) {
        return HTTOPRIGHT;
    }
    if (onBottom && onLeft) {
        return HTBOTTOMLEFT;
    }
    if (onBottom && onRight) {
        return HTBOTTOMRIGHT;
    }
    if (onLeft) {
        return HTLEFT;
    }
    if (onRight) {
        return HTRIGHT;
    }
    if (onTop) {
        return HTTOP;
    }
    if (onBottom) {
        return HTBOTTOM;
    }

    return std::nullopt;
}

inline void updateWindowFrameForCurrentState(HWND hwnd)
{
    const bool maxOrFull = isWindowMaximizedOrFullscreen(hwnd);
    const bool snapped = isWindowSnappedToEdge(hwnd);
    updateWindowFrame(hwnd, maxOrFull || snapped, maxOrFull);
}

#endif

} // namespace EnhanceVision::FramelessWindowUtils
