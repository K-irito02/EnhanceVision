/**
 * @file MainWindow.h
 * @brief 主窗口类（QQuickWidget 容器）
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_MAINWINDOW_H
#define ENHANCEVISION_MAINWINDOW_H

#include <QMainWindow>

namespace EnhanceVision {

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_MAINWINDOW_H
