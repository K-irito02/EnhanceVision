/**
 * @file WindowHelper.h
 * @brief 窗口辅助类，提供窗口控制功能给 QML
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_WINDOWHELPER_H
#define ENHANCEVISION_WINDOWHELPER_H

#include <QObject>
#include <QQuickWidget>
#include <QPoint>
#include <QRect>
#include <QAbstractNativeEventFilter>
#include <QVector>

namespace EnhanceVision {

class WindowHelper : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
    Q_PROPERTY(bool maximized READ isMaximized NOTIFY maximizedChanged)
    Q_PROPERTY(bool resizing READ isResizing NOTIFY resizingChanged)
    Q_PROPERTY(int resizeMargin READ resizeMargin WRITE setResizeMargin NOTIFY resizeMarginChanged)

public:
    static WindowHelper* instance();

    void setWindow(QQuickWidget* window);

    Q_INVOKABLE void minimize();
    Q_INVOKABLE void maximize();
    Q_INVOKABLE void restore();
    Q_INVOKABLE void toggleMaximize();
    Q_INVOKABLE void close();
    Q_INVOKABLE bool isMaximized() const;
    Q_INVOKABLE void startSystemMove();
    Q_INVOKABLE void prepareRestoreAndMove(int mouseX, int mouseY, int areaWidth, int areaHeight);
    Q_INVOKABLE void clearExcludeRegions();
    Q_INVOKABLE void addExcludeRegion(int x, int y, int width, int height);
    
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
    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

private:
    explicit WindowHelper(QObject* parent = nullptr);
    Q_DISABLE_COPY(WindowHelper)

    QQuickWidget* m_window = nullptr;
    bool m_isMaximized = false;
    bool m_isResizing = false;
    int m_resizeMargin = 8;
    int m_minWidth = 800;
    int m_minHeight = 600;
    QVector<QRect> m_excludeRegions;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_WINDOWHELPER_H
