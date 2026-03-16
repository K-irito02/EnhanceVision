/**
 * @file SubWindowHelper.cpp
 * @brief 子窗口辅助类实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/utils/SubWindowHelper.h"
#include <QEvent>
#include <QWindowStateChangeEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QCursor>

#ifdef Q_OS_WIN
#include <windowsx.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "user32.lib")

namespace EnhanceVision {
class SubWindowHelper;
}

static QHash<HWND, EnhanceVision::SubWindowHelper*> g_windowHelpers;
static WNDPROC g_originalQtWndProc = nullptr;

static LRESULT CALLBACK SubWindowWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto it = g_windowHelpers.find(hwnd);
    if (it == g_windowHelpers.end()) {
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    EnhanceVision::SubWindowHelper* helper = it.value();

    switch (msg) {
    case WM_NCHITTEST: {
        RECT windowRect;
        GetWindowRect(hwnd, &windowRect);

        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        int localX = x - windowRect.left;
        int localY = y - windowRect.top;
        int windowWidth = windowRect.right - windowRect.left;
        int windowHeight = windowRect.bottom - windowRect.top;

        int titleBarHeight = 44;

        if (localY >= 0 && localY < titleBarHeight) {
            bool inExcludeRegion = false;
            const auto& regions = helper->excludeRegions();
            for (const QRect& region : regions) {
                if (region.contains(localX, localY)) {
                    inExcludeRegion = true;
                    break;
                }
            }
            if (!inExcludeRegion) {
                return HTCAPTION;
            }
        }

        if (!helper->isWindowMaximized()) {
            int margin = helper->windowResizeMargin();

            bool onLeft = localX < margin;
            bool onRight = localX >= windowWidth - margin;
            bool onTop = localY < margin;
            bool onBottom = localY >= windowHeight - margin;

            if (onTop && onLeft) return HTTOPLEFT;
            if (onTop && onRight) return HTTOPRIGHT;
            if (onBottom && onLeft) return HTBOTTOMLEFT;
            if (onBottom && onRight) return HTBOTTOMRIGHT;
            if (onLeft) return HTLEFT;
            if (onRight) return HTRIGHT;
            if (onTop) return HTTOP;
            if (onBottom) return HTBOTTOM;
        }
        break;
    }

    case WM_GETMINMAXINFO: {
        MINMAXINFO* minMaxInfo = reinterpret_cast<MINMAXINFO*>(lParam);
        minMaxInfo->ptMinTrackSize.x = helper->windowMinWidth();
        minMaxInfo->ptMinTrackSize.y = helper->windowMinHeight();
        return 0;
    }
    }

    if (g_originalQtWndProc) {
        return CallWindowProc(g_originalQtWndProc, hwnd, msg, wParam, lParam);
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

#endif

namespace EnhanceVision {

SubWindowHelper::SubWindowHelper(QObject* parent)
    : QObject(parent)
{
}

SubWindowHelper::~SubWindowHelper()
{
    if (m_window) {
        m_window->removeEventFilter(this);
#ifdef Q_OS_WIN
        HWND hwnd = reinterpret_cast<HWND>(m_window->winId());
        g_windowHelpers.remove(hwnd);
#endif
    }
}

void SubWindowHelper::setWindow(QQuickWindow* window)
{
    if (m_window) {
        m_window->removeEventFilter(this);
#ifdef Q_OS_WIN
        HWND hwnd = reinterpret_cast<HWND>(m_window->winId());
        g_windowHelpers.remove(hwnd);
#endif
    }

    m_window = window;

    if (m_window) {
        m_window->installEventFilter(this);
        setupWindowFrame();

#ifdef Q_OS_WIN
        HWND hwnd = reinterpret_cast<HWND>(m_window->winId());
        g_windowHelpers.insert(hwnd, this);

        if (!g_originalQtWndProc) {
            g_originalQtWndProc = reinterpret_cast<WNDPROC>(
                SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(SubWindowWndProc)));
        } else {
            SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(SubWindowWndProc));
        }
#endif
    }
}

void SubWindowHelper::setMinWidth(int width)
{
    if (m_minWidth != width) {
        m_minWidth = width;
        emit minSizeChanged();
    }
}

void SubWindowHelper::setMinHeight(int height)
{
    if (m_minHeight != height) {
        m_minHeight = height;
        emit minSizeChanged();
    }
}

void SubWindowHelper::setResizeMargin(int margin)
{
    if (m_resizeMargin != margin) {
        m_resizeMargin = margin;
        emit resizeMarginChanged();
    }
}

void SubWindowHelper::clearExcludeRegions()
{
    m_excludeRegions.clear();
}

void SubWindowHelper::addExcludeRegion(int x, int y, int width, int height)
{
    m_excludeRegions.append(QRect(x, y, width, height));
}

bool SubWindowHelper::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_window) {
        if (event->type() == QEvent::WindowStateChange) {
            updateMaximizedState();
        }
    }
    return QObject::eventFilter(watched, event);
}

void SubWindowHelper::setupWindowFrame()
{
#ifdef Q_OS_WIN
    if (!m_window) return;

    HWND hwnd = reinterpret_cast<HWND>(m_window->winId());

    DWM_WINDOW_CORNER_PREFERENCE preference = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &preference, sizeof(preference));

    BOOL disableTransitions = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_TRANSITIONS_FORCEDISABLED, &disableTransitions, sizeof(disableTransitions));

    MARGINS margins = { 1, 1, 1, 1 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    DWORD style = ::GetWindowLongPtr(hwnd, GWL_STYLE);
    ::SetWindowLongPtr(hwnd, GWL_STYLE, style | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX);

    RECT rect;
    GetWindowRect(hwnd, &rect);
    SetWindowPos(hwnd, nullptr, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
                 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);
#endif
}

void SubWindowHelper::updateMaximizedState()
{
    if (!m_window) return;

    bool wasMaximized = m_isMaximized;
    m_isMaximized = m_window->windowStates() & Qt::WindowMaximized;

    if (wasMaximized != m_isMaximized) {
        emit maximizedChanged();
    }
}

void SubWindowHelper::minimize()
{
    if (m_window) {
#ifdef Q_OS_WIN
        HWND hwnd = reinterpret_cast<HWND>(m_window->winId());
        ShowWindow(hwnd, SW_MINIMIZE);
#else
        m_window->showMinimized();
#endif
    }
}

void SubWindowHelper::maximize()
{
    if (m_window && !m_isMaximized) {
        m_normalGeometry = m_window->geometry();
#ifdef Q_OS_WIN
        HWND hwnd = reinterpret_cast<HWND>(m_window->winId());
        ShowWindow(hwnd, SW_MAXIMIZE);
#else
        m_window->showMaximized();
#endif
        m_isMaximized = true;
        emit maximizedChanged();
    }
}

void SubWindowHelper::restore()
{
    if (m_window && m_isMaximized) {
#ifdef Q_OS_WIN
        HWND hwnd = reinterpret_cast<HWND>(m_window->winId());
        ShowWindow(hwnd, SW_RESTORE);
#else
        m_window->showNormal();
#endif
        m_isMaximized = false;
        emit maximizedChanged();
    }
}

void SubWindowHelper::toggleMaximize()
{
    if (m_isMaximized) {
        restore();
    } else {
        maximize();
    }
}

void SubWindowHelper::close()
{
    if (m_window) {
        m_window->close();
    }
}

bool SubWindowHelper::isMaximized() const
{
    return m_isMaximized;
}

void SubWindowHelper::startSystemMove()
{
    if (m_window) {
#ifdef Q_OS_WIN
        HWND hwnd = reinterpret_cast<HWND>(m_window->winId());
        ReleaseCapture();
        SendMessage(hwnd, WM_SYSCOMMAND, SC_MOVE + HTCAPTION, 0);
#else
        m_window->startSystemMove();
#endif
    }
}

} // namespace EnhanceVision
