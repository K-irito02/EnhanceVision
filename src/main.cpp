/**
 * @file main.cpp
 * @brief EnhanceVision 应用程序入口
 * @author Qt客户端开发工程师
 */

#include <QApplication>
#include <QQuickStyle>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <QDebug>
#include "EnhanceVision/app/Application.h"

namespace {
    QFile* logFile = nullptr;

    void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
    {
        Q_UNUSED(context)
        
        if (!logFile || !logFile->isOpen()) {
            return;
        }
        
        QTextStream out(logFile);
        out.setEncoding(QStringConverter::Utf8);
        
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
        QString typeStr;
        
        switch (type) {
        case QtDebugMsg:
            typeStr = "DEBUG";
            break;
        case QtInfoMsg:
            typeStr = "INFO";
            break;
        case QtWarningMsg:
            typeStr = "WARN";
            break;
        case QtCriticalMsg:
            typeStr = "CRIT";
            break;
        case QtFatalMsg:
            typeStr = "FATAL";
            break;
        }
        
        out << QString("[%1] [%2] %3\n").arg(timestamp, typeStr, msg);
        out.flush();
        
        // 同时输出到控制台
        fprintf(stderr, "[%s] [%s] %s\n", 
                timestamp.toLocal8Bit().constData(),
                typeStr.toLocal8Bit().constData(),
                msg.toLocal8Bit().constData());
    }
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("EnhanceVision");
    QApplication::setApplicationVersion("0.1.0");
    QApplication::setOrganizationName("EnhanceVision");
    
    QQuickStyle::setStyle("Basic");
    
    // 创建日志目录
    QDir logDir("logs");
    if (!logDir.exists()) {
        logDir.mkpath(".");
    }
    
    // 使用固定的日志文件名，每次启动覆盖
    QString logFileName = "logs/runtime_output.log";
    logFile = new QFile(logFileName);
    
    if (logFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qInstallMessageHandler(messageHandler);
        qDebug() << "Log file created:" << logFileName;
    } else {
        qWarning() << "Failed to create log file:" << logFileName;
    }

    qDebug() << "========================================";
    qDebug() << "EnhanceVision starting...";
    qDebug() << "Qt version:" << qVersion();
    qDebug() << "========================================";

    EnhanceVision::Application application;
    application.initialize();
    application.show();

    int result = app.exec();
    
    // 清理日志
    qDebug() << "EnhanceVision exiting with code:" << result;
    if (logFile) {
        logFile->close();
        delete logFile;
        logFile = nullptr;
    }
    
    return result;
}
