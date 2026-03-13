/**
 * @file MainWindow.cpp
 * @brief 主窗口类实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/app/MainWindow.h"

namespace EnhanceVision {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("EnhanceVision - 图像处理与增强工具"));
    resize(1280, 720);
}

MainWindow::~MainWindow()
{
}

} // namespace EnhanceVision
