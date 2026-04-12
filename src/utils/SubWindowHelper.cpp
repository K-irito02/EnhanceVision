/**
 * @file SubWindowHelper.cpp
 * @brief 子窗口辅助类实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/utils/SubWindowHelper.h"
#include "FramelessWindowUtils.h"
#include <QEvent>
#include <QCursor>
#include <QCoreApplication>

#ifdef Q_OS_WIN
#include <windowsx.h>

namespace EnhanceVision {
class SubWindowHelper;
}

static QHash<HWND, EnhanceVision::SubWindowHelper*> g_windowHelpers;
static QHash<HWND, WNDPROC> g_originalWndProcs;

static LRESULT CALLBACK SubWindowWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    using namespace EnhanceVision::FramelessWindowUtils;

    auto it = g_windowHelpers.find(hwnd);
    if (it == g_windowHelpers.end()) {
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    EnhanceVision::SubWindowHelper* helper = it.value();

    switch (msg) {
    case WM_ENTERSIZEMOVE: {
        helper->setDragging(true);
        break;
    }
    case WM_EXITSIZEMOVE: {
        helper->setDragging(false);
        break;
    }
    case WM_NCLBUTTONDOWN: {
        if (wParam == HTCAPTION && helper->isWindowFullScreen()) {
            RECT windowRect;
            GetWindowRect(hwnd, &windowRect);
            
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            
            int localX = x - windowRect.left;
            int localY = y - windowRect.top;
            int windowWidth = windowRect.right - windowRect.left;
            
            helper->handleFullScreenDrag(localX, localY, windowWidth);
            return 0;
        }
        break;
    }
    case WM_NCHITTEST: {
        if (helper->isDragging()) {
            return HTCAPTION;
        }
        
        RECT windowRect;
        GetWindowRect(hwnd, &windowRect);

        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        int localX = x - windowRect.left;
        int localY = y - windowRect.top;
        int windowWidth = windowRect.right - windowRect.left;
        int windowHeight = windowRect.bottom - windowRect.top;

        int titleBarHeight = helper->windowTitleBarHeight();
        int margin = helper->windowResizeMargin();

        const auto result = hitTestFramelessWindow({
            .localX = localX,
            .localY = localY,
            .windowWidth = windowWidth,
            .windowHeight = windowHeight,
            .titleBarHeight = titleBarHeight,
            .resizeMargin = margin,
            .isMaximized = helper->isWindowMaximized(),
            .isFullScreen = helper->isWindowFullScreen(),
            .excludeRegions = &helper->excludeRegions()
        });
        if (result.has_value()) {
            return *result;
        }
        break;
    }

    case WM_GETMINMAXINFO: {
        MINMAXINFO* minMaxInfo = reinterpret_cast<MINMAXINFO*>(lParam);
        minMaxInfo->ptMinTrackSize.x = helper->windowMinWidth();
        minMaxInfo->ptMinTrackSize.y = helper->windowMinHeight();
        return 0;
    }
    case WM_NCCALCSIZE: {
        if (wParam == TRUE) {
            if (IsZoomed(hwnd)) {
                NCCALCSIZE_PARAMS* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
                HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
                MONITORINFO mi = {};
                mi.cbSize = sizeof(mi);
                GetMonitorInfo(monitor, &mi);
                params->rgrc[0] = mi.rcWork;
            }
            return WVR_REDRAW;
        }
        break;
    }
    case WM_ERASEBKGND: {
        return 1;
    }
    case WM_SIZE: {
        if (IsWindowVisible(hwnd)) {
            Q_UNUSED(wParam)
            updateWindowFrameForCurrentState(hwnd);
        }
        break;
    }
    }

    auto procIt = g_originalWndProcs.find(hwnd);
    if (procIt != g_originalWndProcs.end() && procIt.value()) {
        return CallWindowProc(procIt.value(), hwnd, msg, wParam, lParam);
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
        
        auto procIt = g_originalWndProcs.find(hwnd);
        if (procIt != g_originalWndProcs.end() && procIt.value()) {
            SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(procIt.value()));
            g_originalWndProcs.remove(hwnd);
        }
#endif
    }
}

void SubWindowHelper::setWindow(QQuickWindow* window)
{
    if (m_window) {
        m_window->removeEventFilter(this);
#ifdef Q_OS_WIN
        HWND oldHwnd = reinterpret_cast<HWND>(m_window->winId());
        g_windowHelpers.remove(oldHwnd);
        
        auto procIt = g_originalWndProcs.find(oldHwnd);
        if (procIt != g_originalWndProcs.end() && procIt.value()) {
            SetWindowLongPtr(oldHwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(procIt.value()));
            g_originalWndProcs.remove(oldHwnd);
        }
#endif
    }

    m_window = window;

    if (m_window) {
        m_window->installEventFilter(this);
        setupWindowFrame();

#ifdef Q_OS_WIN
        HWND hwnd = reinterpret_cast<HWND>(m_window->winId());
        g_windowHelpers.insert(hwnd, this);

        WNDPROC originalProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(SubWindowWndProc)));
        g_originalWndProcs.insert(hwnd, originalProc);
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

void SubWindowHelper::setTitleBarHeight(int height)
{
    if (m_titleBarHeight != height) {
        m_titleBarHeight = height;
        emit titleBarHeightChanged();
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
            QMetaObject::invokeMethod(this, &SubWindowHelper::updateFrameForState, Qt::QueuedConnection);
        } else if (event->type() == QEvent::Show) {
#ifdef Q_OS_WIN
            HWND hwnd = reinterpret_cast<HWND>(m_window->winId());
            
            if (!g_windowHelpers.contains(hwnd)) {
                QMutableHashIterator<HWND, SubWindowHelper*> it(g_windowHelpers);
                while (it.hasNext()) {
                    it.next();
                    if (it.value() == this) {
                        HWND oldHwnd = it.key();
                        
                        auto procIt = g_originalWndProcs.find(oldHwnd);
                        if (procIt != g_originalWndProcs.end() && procIt.value()) {
                            SetWindowLongPtr(oldHwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(procIt.value()));
                            g_originalWndProcs.remove(oldHwnd);
                        }
                        it.remove();
                        break;
                    }
                }
                
                g_windowHelpers.insert(hwnd, this);
                WNDPROC originalProc = reinterpret_cast<WNDPROC>(
                    SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(SubWindowWndProc)));
                g_originalWndProcs.insert(hwnd, originalProc);
                
                setupWindowFrame();
            }
#endif
        }
    }
    return QObject::eventFilter(watched, event);
}

void SubWindowHelper::setupWindowFrame()
{
#ifdef Q_OS_WIN
    if (!m_window) return;

    HWND hwnd = reinterpret_cast<HWND>(m_window->winId());
    FramelessWindowUtils::setupFramelessWindow(hwnd);
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

bool SubWindowHelper::isWindowFullScreen() const
{
    if (m_window) {
        return m_window->windowStates() & Qt::WindowFullScreen;
    }
    return false;
}

void SubWindowHelper::handleFullScreenDrag(int mouseX, int mouseY, int areaWidth)
{
    if (m_window && isWindowFullScreen()) {
#ifdef Q_OS_WIN
        HWND hwnd = reinterpret_cast<HWND>(m_window->winId());
        
        QRect fullScreenGeom = m_window->geometry();
        QRect normalGeom = m_normalGeometry;
        
        if (normalGeom.width() <= 0 || normalGeom.height() <= 0) {
            normalGeom.setWidth(m_minWidth);
            normalGeom.setHeight(m_minHeight);
        }
        
        qreal xRatio = static_cast<qreal>(mouseX) / areaWidth;
        
        int newWindowX = static_cast<int>(normalGeom.width() * xRatio);
        
        normalGeom.moveTopLeft(QPoint(fullScreenGeom.left() + mouseX - newWindowX, fullScreenGeom.top() + mouseY));
        
        m_isDragging = true;
        emit draggingChanged();
        
        m_window->showNormal();
        
        m_window->setGeometry(normalGeom);
        
        QCoreApplication::processEvents();
        
        QPoint cursorPos = QCursor::pos();
        int lParam = MAKELPARAM(cursorPos.x(), cursorPos.y());
        
        ReleaseCapture();
        SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, lParam);
#else
        Q_UNUSED(mouseX)
        Q_UNUSED(mouseY)
        Q_UNUSED(areaWidth)
#endif
    }
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

void SubWindowHelper::saveNormalGeometry()
{
    if (m_window) {
        m_normalGeometry = m_window->geometry();
    }
}

void SubWindowHelper::prepareRestoreAndMove(int mouseX, int mouseY, int areaWidth, int areaHeight)
{
    if (m_window) {
        bool isFullScreen = m_window->windowStates() & Qt::WindowFullScreen;
        bool needRestore = m_isMaximized || isFullScreen;
        
        if (needRestore) {
#ifdef Q_OS_WIN
            HWND hwnd = reinterpret_cast<HWND>(m_window->winId());
            
            QRect maxGeom = m_window->geometry();
            QRect normalGeom = m_normalGeometry;
            
            if (normalGeom.width() <= 0 || normalGeom.height() <= 0) {
                normalGeom.setWidth(m_minWidth);
                normalGeom.setHeight(m_minHeight);
            }
            
            qreal xRatio = static_cast<qreal>(mouseX) / areaWidth;
            
            int newWindowX = static_cast<int>(normalGeom.width() * xRatio);
            int newWindowY = mouseY;
            
            normalGeom.moveTopLeft(QPoint(maxGeom.left() + mouseX - newWindowX, maxGeom.top() + mouseY - newWindowY));
            
            m_isDragging = true;
            emit draggingChanged();
            
            ShowWindow(hwnd, SW_RESTORE);
            m_window->setGeometry(normalGeom);
            
            QCoreApplication::processEvents();
            
            QPoint cursorPos = QCursor::pos();
            int lParam = MAKELPARAM(cursorPos.x(), cursorPos.y());
            
            ReleaseCapture();
            SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, lParam);
#else
            Q_UNUSED(mouseX)
            Q_UNUSED(mouseY)
            Q_UNUSED(areaWidth)
            Q_UNUSED(areaHeight)
#endif
        }
    }
}

void SubWindowHelper::setDragging(bool dragging)
{
    if (m_isDragging != dragging) {
        m_isDragging = dragging;
        emit draggingChanged();
    }
}

void SubWindowHelper::updateFrameForState()
{
#ifdef Q_OS_WIN
    if (!m_window) return;
    HWND hwnd = reinterpret_cast<HWND>(m_window->winId());
    FramelessWindowUtils::updateWindowFrameForCurrentState(hwnd);
#endif
}

} // namespace EnhanceVision
