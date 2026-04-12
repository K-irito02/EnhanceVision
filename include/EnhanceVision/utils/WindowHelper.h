/**
 * @file WindowHelper.h
 * @brief 窗口辅助类，提供窗口控制功能给 QML
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_WINDOWHELPER_H
#define ENHANCEVISION_WINDOWHELPER_H

#include <QObject>
#include <QPoint>
#include <QRect>
#include <QVector>

class QQuickWindow;

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace EnhanceVision {

class WindowHelper : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool maximized READ isMaximized NOTIFY maximizedChanged)
    Q_PROPERTY(bool resizing READ isResizing NOTIFY resizingChanged)
    Q_PROPERTY(int resizeMargin READ resizeMargin WRITE setResizeMargin NOTIFY resizeMarginChanged)

public:
    static WindowHelper* instance();

    void setWindow(QQuickWindow* window);

    Q_INVOKABLE void minimize();
    Q_INVOKABLE void maximize();
    Q_INVOKABLE void restore();
    Q_INVOKABLE void toggleMaximize();
    Q_INVOKABLE void close();
    Q_INVOKABLE bool isMaximized() const;
    QRect normalGeometry() const { return m_normalGeometry; }
    Q_INVOKABLE void startSystemMove();
    Q_INVOKABLE void prepareRestoreAndMove(int mouseX, int mouseY, int areaWidth, int areaHeight);
    Q_INVOKABLE void clearExcludeRegions();
    Q_INVOKABLE void addExcludeRegion(int x, int y, int width, int height);
    Q_INVOKABLE void setOverrideCursor(int cursorShape);
    Q_INVOKABLE void restoreOverrideCursor();
    
    bool isResizing() const { return m_isResizing; }
    
    int resizeMargin() const { return m_resizeMargin; }
    void setResizeMargin(int margin);

signals:
    void maximizedChanged();
    void resizingChanged();
    void resizeMarginChanged();
    void resizeStarted();
    void resizeFinished();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    explicit WindowHelper(QObject* parent = nullptr);
    Q_DISABLE_COPY(WindowHelper)

    void setupWindowFrame();
    void updateFrameForState();
    void updateMaximizedState();
    void updateNormalGeometry();
    void setResizing(bool resizing);

    QQuickWindow* m_window = nullptr;
    bool m_isMaximized = false;
    bool m_isResizing = false;
    int m_resizeMargin = 8;
    int m_minWidth = 800;
    int m_minHeight = 600;
    int m_titleBarHeight = 48;
    QRect m_normalGeometry;
    QVector<QRect> m_excludeRegions;

#ifdef Q_OS_WIN
    friend LRESULT CALLBACK WindowHelperWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_WINDOWHELPER_H
