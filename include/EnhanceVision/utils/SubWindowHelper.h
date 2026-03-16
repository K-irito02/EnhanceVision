/**
 * @file SubWindowHelper.h
 * @brief 子窗口辅助类，为 QML Window 提供窗口控制功能
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_SUBWINDOWHELPER_H
#define ENHANCEVISION_SUBWINDOWHELPER_H

#include <QObject>
#include <QQuickWindow>
#include <QPoint>
#include <QRect>
#include <QVector>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace EnhanceVision {

class SubWindowHelper : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool maximized READ isMaximized NOTIFY maximizedChanged)
    Q_PROPERTY(int minWidth READ minWidth WRITE setMinWidth NOTIFY minSizeChanged)
    Q_PROPERTY(int minHeight READ minHeight WRITE setMinHeight NOTIFY minSizeChanged)
    Q_PROPERTY(int resizeMargin READ resizeMargin WRITE setResizeMargin NOTIFY resizeMarginChanged)

public:
    explicit SubWindowHelper(QObject* parent = nullptr);
    ~SubWindowHelper() override;

    Q_INVOKABLE void setWindow(QQuickWindow* window);
    Q_INVOKABLE void minimize();
    Q_INVOKABLE void maximize();
    Q_INVOKABLE void restore();
    Q_INVOKABLE void toggleMaximize();
    Q_INVOKABLE void close();
    Q_INVOKABLE bool isMaximized() const;
    Q_INVOKABLE void startSystemMove();

    int minWidth() const { return m_minWidth; }
    int minHeight() const { return m_minHeight; }
    Q_INVOKABLE void setMinWidth(int width);
    Q_INVOKABLE void setMinHeight(int height);

    int resizeMargin() const { return m_resizeMargin; }
    void setResizeMargin(int margin);

    Q_INVOKABLE void clearExcludeRegions();
    Q_INVOKABLE void addExcludeRegion(int x, int y, int width, int height);

    const QVector<QRect>& excludeRegions() const { return m_excludeRegions; }
    bool isWindowMaximized() const { return m_isMaximized; }
    int windowResizeMargin() const { return m_resizeMargin; }
    int windowMinWidth() const { return m_minWidth; }
    int windowMinHeight() const { return m_minHeight; }

signals:
    void maximizedChanged();
    void minSizeChanged();
    void resizeMarginChanged();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void setupWindowFrame();
    void updateMaximizedState();

    QQuickWindow* m_window = nullptr;
    bool m_isMaximized = false;
    int m_minWidth = 400;
    int m_minHeight = 300;
    int m_resizeMargin = 8;
    QRect m_normalGeometry;
    QVector<QRect> m_excludeRegions;

#ifdef Q_OS_WIN
    friend LRESULT CALLBACK SubWindowWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_SUBWINDOWHELPER_H
