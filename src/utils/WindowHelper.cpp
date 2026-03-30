/**
 * @file WindowHelper.cpp
 * @brief 窗口辅助类实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/utils/WindowHelper.h"
#include <QEvent>
#include <QWindowStateChangeEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QMouseEvent>
#include <QWindow>
#include <QTimer>
#include <QCursor>
#include <QCoreApplication>
#include <QApplication>
#include <QWidget>

#ifdef Q_OS_WIN
#include <windowsx.h>
#include <dwmapi.h>
#include <windows.h>
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "user32.lib")
#endif

namespace EnhanceVision {

WindowHelper* WindowHelper::instance()
{
    static WindowHelper helper;
    return &helper;
}

WindowHelper::WindowHelper(QObject* parent)
    : QObject(parent)
{
}

void WindowHelper::setWindow(QQuickWidget* window)
{
    if (m_window) {
        m_window->removeEventFilter(this);
        QGuiApplication::instance()->removeNativeEventFilter(this);
    }
    m_window = window;
    if (m_window) {
        m_window->installEventFilter(this);
        QGuiApplication::instance()->installNativeEventFilter(this);
        
#ifdef Q_OS_WIN
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
        if (event->type() == QEvent::WindowStateChange) {
            bool wasMaximized = m_isMaximized;
            m_isMaximized = m_window->isMaximized();
            
            if (wasMaximized != m_isMaximized) {
                emit maximizedChanged();
            }
        }
        
        if (event->type() == QEvent::Resize) {
            if (!m_isResizing) {
                m_isResizing = true;
                emit resizingChanged();
                emit resizeStarted();
            }
        }
        
        if (event->type() == QEvent::Move) {
            if (!m_isResizing) {
                m_isResizing = true;
                emit resizingChanged();
                emit resizeStarted();
            }
        }
        
        if (event->type() == QEvent::Expose) {
            if (m_isResizing) {
                m_isResizing = false;
                emit resizingChanged();
                emit resizeFinished();
            }
        }
    }
    return QObject::eventFilter(watched, event);
}

bool WindowHelper::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result)
{
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG") {
        MSG* msg = static_cast<MSG*>(message);
        
        if (msg->message == WM_NCHITTEST && m_window) {
            HWND hwnd = reinterpret_cast<HWND>(m_window->winId());
            if (msg->hwnd != hwnd) {
                return false;
            }
            
            RECT windowRect;
            GetWindowRect(hwnd, &windowRect);
            
            int x = GET_X_LPARAM(msg->lParam);
            int y = GET_Y_LPARAM(msg->lParam);
            
            int localX = x - windowRect.left;
            int localY = y - windowRect.top;
            int windowWidth = windowRect.right - windowRect.left;
            int windowHeight = windowRect.bottom - windowRect.top;
            
            int margin = m_resizeMargin;
            
            bool onLeft = localX < margin;
            bool onRight = localX >= windowWidth - margin;
            bool onTop = localY < margin;
            bool onBottom = localY >= windowHeight - margin;
            
            if (!m_isMaximized) {
                if (onTop && onLeft) {
                    *result = HTTOPLEFT;
                    return true;
                }
                if (onTop && onRight) {
                    *result = HTTOPRIGHT;
                    return true;
                }
                if (onBottom && onLeft) {
                    *result = HTBOTTOMLEFT;
                    return true;
                }
                if (onBottom && onRight) {
                    *result = HTBOTTOMRIGHT;
                    return true;
                }
                if (onLeft) {
                    *result = HTLEFT;
                    return true;
                }
                if (onRight) {
                    *result = HTRIGHT;
                    return true;
                }
                if (onTop) {
                    *result = HTTOP;
                    return true;
                }
                if (onBottom) {
                    *result = HTBOTTOM;
                    return true;
                }
            }
            
            if (localY >= 0 && localY < 48) {
                for (const QRect& region : m_excludeRegions) {
                    if (region.contains(localX, localY)) {
                        return false;
                    }
                }
                *result = HTCAPTION;
                return true;
            }
        }
        
        if (msg->message == WM_GETMINMAXINFO && m_window) {
            HWND hwnd = reinterpret_cast<HWND>(m_window->winId());
            if (msg->hwnd == hwnd) {
                MINMAXINFO* minMaxInfo = reinterpret_cast<MINMAXINFO*>(msg->lParam);
                minMaxInfo->ptMinTrackSize.x = m_minWidth;
                minMaxInfo->ptMinTrackSize.y = m_minHeight;
                *result = 0;
                return true;
            }
        }
        
        if (msg->message == WM_ENTERSIZEMOVE && m_window) {
            if (!m_isResizing) {
                m_isResizing = true;
                emit resizingChanged();
                emit resizeStarted();
            }
        }
        
        if (msg->message == WM_EXITSIZEMOVE && m_window) {
            if (m_isResizing) {
                m_isResizing = false;
                emit resizingChanged();
                emit resizeFinished();
            }
        }
    }
#else
    Q_UNUSED(eventType)
    Q_UNUSED(message)
    Q_UNUSED(result)
#endif
    return false;
}

void WindowHelper::minimize()
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

void WindowHelper::maximize()
{
    if (m_window && !m_isMaximized) {
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

void WindowHelper::restore()
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
    if (m_window && m_isMaximized) {
#ifdef Q_OS_WIN
        HWND hwnd = reinterpret_cast<HWND>(m_window->winId());
        
        QRect maxGeom = m_window->geometry();
        QRect normalGeom = m_window->normalGeometry();
        
        if (normalGeom.width() <= 0 || normalGeom.height() <= 0) {
            normalGeom.setWidth(m_minWidth);
            normalGeom.setHeight(m_minHeight);
        }
        
        qreal xRatio = static_cast<qreal>(mouseX) / areaWidth;
        
        int newWindowX = static_cast<int>(normalGeom.width() * xRatio);
        int newWindowY = mouseY;
        
        normalGeom.moveTopLeft(QPoint(maxGeom.left() + mouseX - newWindowX, maxGeom.top() + mouseY - newWindowY));
        
        m_isMaximized = false;
        emit maximizedChanged();
        
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

void WindowHelper::startSystemMove()
{
    if (m_window) {
#ifdef Q_OS_WIN
        HWND hwnd = reinterpret_cast<HWND>(m_window->winId());
        ReleaseCapture();
        SendMessage(hwnd, WM_SYSCOMMAND, SC_MOVE + HTCAPTION, 0);
#else
        if (m_window->windowHandle()) {
            m_window->windowHandle()->startSystemMove();
        }
#endif
    }
}

void WindowHelper::clearExcludeRegions()
{
    m_excludeRegions.clear();
}

void WindowHelper::addExcludeRegion(int x, int y, int width, int height)
{
    m_excludeRegions.append(QRect(x, y, width, height));
}

} // namespace EnhanceVision
