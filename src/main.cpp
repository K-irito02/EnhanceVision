/**
 * @file main.cpp
 * @brief EnhanceVision 应用程序入口
 * @author Qt客户端开发工程师
 */

#include <QApplication>
#include <QQuickStyle>
#include "EnhanceVision/app/Application.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("EnhanceVision");
    QApplication::setApplicationVersion("0.1.0");
    QApplication::setOrganizationName("EnhanceVision");
    
    QQuickStyle::setStyle("Basic");

    EnhanceVision::Application application;
    application.initialize();
    application.show();

    return app.exec();
}
