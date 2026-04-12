/**
 * @file WindowHelper.cpp
 * @brief 窗口辅助类实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/utils/WindowHelper.h"
#include "FramelessWindowUtils.h"
#include <QCoreApplication>
#include <QCursor>
#include <QEvent>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QTimer>

namespace EnhanceVision {

#ifdef Q_OS_WIN
#include <windowsx.h>

namespace {
WNDPROC g_originalWindowProc = nullptr;
WindowHelper* g_windowHelper = nullptr;
}

LRESULT CALLBACK WindowHelperWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    using namespace FramelessWindowUtils;

    if (!g_windowHelper) {
        return g_originalWindowProc
            ? CallWindowProc(g_originalWindowProc, hwnd, msg, wParam, lParam)
            : DefWindowProc(hwnd, msg, wParam, lParam);
    }

    switch (msg) {
    case WM_ENTERSIZEMOVE:
        g_windowHelper->setResizing(true);
        break;
    case WM_EXITSIZEMOVE:
        g_windowHelper->setResizing(false);
        g_windowHelper->updateNormalGeometry();
        break;
    case WM_NCHITTEST: {
        RECT windowRect;
        GetWindowRect(hwnd, &windowRect);

        const int localX = GET_X_LPARAM(lParam) - windowRect.left;
        const int localY = GET_Y_LPARAM(lParam) - windowRect.top;
        const int windowWidth = windowRect.right - windowRect.left;
        const int windowHeight = windowRect.bottom - windowRect.top;

        const auto result = hitTestFramelessWindow({
            .localX = localX,
            .localY = localY,
            .windowWidth = windowWidth,
            .windowHeight = windowHeight,
            .titleBarHeight = g_windowHelper->m_titleBarHeight,
            .resizeMargin = g_windowHelper->m_resizeMargin,
            .isMaximized = g_windowHelper->m_isMaximized,
            .isFullScreen = false,
            .excludeRegions = &g_windowHelper->m_excludeRegions
        });
        if (result.has_value()) {
            return *result;
        }
        break;
    }
    case WM_GETMINMAXINFO: {
        auto* minMaxInfo = reinterpret_cast<MINMAXINFO*>(lParam);
        minMaxInfo->ptMinTrackSize.x = g_windowHelper->m_minWidth;
        minMaxInfo->ptMinTrackSize.y = g_windowHelper->m_minHeight;
        return 0;
    }
    case WM_NCCALCSIZE:
        if (wParam == TRUE) {
            if (IsZoomed(hwnd)) {
                auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
                HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
                MONITORINFO monitorInfo = {};
                monitorInfo.cbSize = sizeof(monitorInfo);
                GetMonitorInfo(monitor, &monitorInfo);
                params->rgrc[0] = monitorInfo.rcWork;
            }
            // Force full repaint during interactive resize. This avoids preserved
            // client-bit reuse artifacts (black gap / detached content) when
            // resizing from left/top edges where move+resize happens together.
            return WVR_REDRAW;
        }
        break;
    case WM_ERASEBKGND:
        // Qt Quick owns painting. Prevent GDI background erase flicker.
        return 1;
    case WM_SIZE:
        if (IsWindowVisible(hwnd)) {
            g_windowHelper->updateFrameForState();
        }
        break;
    default:
        break;
    }

    return g_originalWindowProc
        ? CallWindowProc(g_originalWindowProc, hwnd, msg, wParam, lParam)
        : DefWindowProc(hwnd, msg, wParam, lParam);
}
#endif

WindowHelper* WindowHelper::instance()
{
    static WindowHelper helper;
    return &helper;
}

WindowHelper::WindowHelper(QObject* parent)
    : QObject(parent)
{
}

void WindowHelper::setWindow(QQuickWindow* window)
{
    if (m_window == window) {
        return;
    }

    if (m_window) {
        m_window->removeEventFilter(this);
#ifdef Q_OS_WIN
        HWND oldHwnd = reinterpret_cast<HWND>(m_window->winId());
        if (g_windowHelper == this) {
            g_windowHelper = nullptr;
        }
        if (g_originalWindowProc) {
            SetWindowLongPtr(oldHwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_originalWindowProc));
            g_originalWindowProc = nullptr;
        }
#endif
    }

    m_window = window;

    if (!m_window) {
        return;
    }

    m_window->installEventFilter(this);
    updateMaximizedState();
    updateNormalGeometry();
    setupWindowFrame();
}

void WindowHelper::setResizeMargin(int margin)
{
    if (m_resizeMargin != margin) {
        m_resizeMargin = margin;
        emit resizeMarginChanged();
    }
}

bool WindowHelper::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_window) {
        switch (event->type()) {
        case QEvent::WindowStateChange:
            updateMaximizedState();
            QTimer::singleShot(0, this, &WindowHelper::updateFrameForState);
            break;
        case QEvent::Show:
            setupWindowFrame();
            updateNormalGeometry();
            QTimer::singleShot(0, this, &WindowHelper::updateFrameForState);
            break;
        case QEvent::Move:
        case QEvent::Resize:
            updateNormalGeometry();
            break;
        default:
            break;
        }
    }

    return QObject::eventFilter(watched, event);
}

void WindowHelper::minimize()
{
    if (!m_window) {
        return;
    }

#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(m_window->winId());
    ShowWindow(hwnd, SW_MINIMIZE);
#else
    m_window->showMinimized();
#endif
}

void WindowHelper::maximize()
{
    if (!m_window || m_isMaximized) {
        return;
    }

    updateNormalGeometry();

#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(m_window->winId());
    ShowWindow(hwnd, SW_MAXIMIZE);
#else
    m_window->showMaximized();
#endif

    m_isMaximized = true;
    emit maximizedChanged();
}

void WindowHelper::restore()
{
    if (!m_window || !m_isMaximized) {
        return;
    }

#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(m_window->winId());
    ShowWindow(hwnd, SW_RESTORE);
#else
    m_window->showNormal();
#endif

    m_isMaximized = false;
    emit maximizedChanged();
}

void WindowHelper::toggleMaximize()
{
    if (m_isMaximized) {
        restore();
    } else {
        maximize();
    }
}

void WindowHelper::close()
{
    if (m_window) {
        m_window->close();
    }
}

bool WindowHelper::isMaximized() const
{
    return m_isMaximized;
}

void WindowHelper::prepareRestoreAndMove(int mouseX, int mouseY, int areaWidth, int areaHeight)
{
    Q_UNUSED(areaHeight)

    if (!m_window || !m_isMaximized) {
        return;
    }

#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(m_window->winId());
    QRect maximizedGeometry = m_window->geometry();
    QRect normalGeometry = m_normalGeometry;

    if (normalGeometry.width() <= 0 || normalGeometry.height() <= 0) {
        normalGeometry.setSize(QSize(m_minWidth, m_minHeight));
    }

    const qreal xRatio = static_cast<qreal>(mouseX) / areaWidth;
    const int newWindowX = static_cast<int>(normalGeometry.width() * xRatio);
    const int newWindowY = mouseY;

    normalGeometry.moveTopLeft(QPoint(maximizedGeometry.left() + mouseX - newWindowX,
                                      maximizedGeometry.top() + mouseY - newWindowY));

    m_isMaximized = false;
    emit maximizedChanged();

    ShowWindow(hwnd, SW_RESTORE);
    m_window->setGeometry(normalGeometry);
    QCoreApplication::processEvents();

    const QPoint cursorPos = QCursor::pos();
    const int lParam = MAKELPARAM(cursorPos.x(), cursorPos.y());
    ReleaseCapture();
    SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, lParam);
#else
    Q_UNUSED(mouseX)
    Q_UNUSED(mouseY)
    Q_UNUSED(areaWidth)
#endif
}

void WindowHelper::startSystemMove()
{
    if (!m_window) {
        return;
    }

#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(m_window->winId());
    ReleaseCapture();
    SendMessage(hwnd, WM_SYSCOMMAND, SC_MOVE + HTCAPTION, 0);
#else
    m_window->startSystemMove();
#endif
}

void WindowHelper::clearExcludeRegions()
{
    m_excludeRegions.clear();
}

void WindowHelper::addExcludeRegion(int x, int y, int width, int height)
{
    m_excludeRegions.append(QRect(x, y, width, height));
}

void WindowHelper::setOverrideCursor(int cursorShape)
{
    QGuiApplication::setOverrideCursor(QCursor(static_cast<Qt::CursorShape>(cursorShape)));
}

void WindowHelper::restoreOverrideCursor()
{
    QGuiApplication::restoreOverrideCursor();
}

void WindowHelper::setupWindowFrame()
{
#ifdef Q_OS_WIN
    if (!m_window) {
        return;
    }

    HWND hwnd = reinterpret_cast<HWND>(m_window->winId());
    FramelessWindowUtils::setupFramelessWindow(hwnd);

    if (g_windowHelper == this && g_originalWindowProc) {
        return;
    }

    if (g_originalWindowProc && g_windowHelper) {
        HWND previousHwnd = reinterpret_cast<HWND>(g_windowHelper->m_window ? g_windowHelper->m_window->winId() : 0);
        if (previousHwnd) {
            SetWindowLongPtr(previousHwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_originalWindowProc));
        }
    }

    g_windowHelper = this;
    g_originalWindowProc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WindowHelperWndProc)));
#endif
}

void WindowHelper::updateFrameForState()
{
#ifdef Q_OS_WIN
    if (!m_window) {
        return;
    }

    HWND hwnd = reinterpret_cast<HWND>(m_window->winId());
    FramelessWindowUtils::updateWindowFrameForCurrentState(hwnd);
#endif
}

void WindowHelper::updateMaximizedState()
{
    if (!m_window) {
        return;
    }

    const bool wasMaximized = m_isMaximized;
    m_isMaximized = m_window->windowStates().testFlag(Qt::WindowMaximized);
    if (wasMaximized != m_isMaximized) {
        emit maximizedChanged();
    }
}

void WindowHelper::updateNormalGeometry()
{
    if (!m_window || m_isMaximized || m_isResizing) {
        return;
    }

    const QRect geometry = m_window->geometry();
    if (geometry.isValid() && geometry.width() > 0 && geometry.height() > 0) {
        m_normalGeometry = geometry;
    }
}

void WindowHelper::setResizing(bool resizing)
{
    if (m_isResizing == resizing) {
        return;
    }

    m_isResizing = resizing;
    emit resizingChanged();
    if (m_isResizing) {
        emit resizeStarted();
    } else {
        emit resizeFinished();
    }
}

} // namespace EnhanceVision
