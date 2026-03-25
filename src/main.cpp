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
#include <cstdlib>
#include "EnhanceVision/app/Application.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <processthreadsapi.h>
#endif

namespace {
    QFile* logFile = nullptr;
    QString logFilePath;

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
            return;
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

#ifdef Q_OS_WIN
    void writeCrashLog(const char* message)
    {
        // 输出到 stderr
        fprintf(stderr, "%s\n", message);
        fflush(stderr);
        
        // 写入崩溃日志文件
        HANDLE hFile = CreateFileW(
            L"logs/crash.log",
            FILE_APPEND_DATA,
            FILE_SHARE_READ,
            nullptr,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );
        
        if (hFile != INVALID_HANDLE_VALUE) {
            SYSTEMTIME st;
            GetLocalTime(&st);
            char buffer[512];
            int len = snprintf(buffer, sizeof(buffer),
                "[%04d-%02d-%02d %02d:%02d:%02d.%03d] [CRASH] %s\n",
                st.wYear, st.wMonth, st.wDay,
                st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                message);
            
            DWORD written;
            WriteFile(hFile, buffer, len, &written, nullptr);
            FlushFileBuffers(hFile);
            CloseHandle(hFile);
        }
    }

    LONG WINAPI SehExceptionHandler(PEXCEPTION_POINTERS pExceptionInfo)
    {
        // 直接输出到 stderr，不使用任何可能依赖堆的函数
        DWORD exceptionCode = pExceptionInfo->ExceptionRecord->ExceptionCode;
        void* exceptionAddress = pExceptionInfo->ExceptionRecord->ExceptionAddress;
        
        // 使用最原始的方式输出
        char msg[128];
        snprintf(msg, sizeof(msg), "\n[CRASH] Exception: 0x%08X at %p\n", exceptionCode, exceptionAddress);
        
        // 直接写入 stderr
        HANDLE hStdErr = GetStdHandle(STD_ERROR_HANDLE);
        if (hStdErr != INVALID_HANDLE_VALUE) {
            DWORD written;
            WriteFile(hStdErr, msg, strlen(msg), &written, nullptr);
            FlushFileBuffers(hStdErr);
        }
        
        // 立即终止进程，不调用任何清理函数
        TerminateProcess(GetCurrentProcess(), 1);
        
        // 如果 TerminateProcess 失败，使用 NtTerminateProcess
        typedef NTSTATUS(NTAPI* NtTerminateProcessPtr)(HANDLE, NTSTATUS);
        HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
        if (hNtdll) {
            NtTerminateProcessPtr pNtTerminateProcess = 
                (NtTerminateProcessPtr)GetProcAddress(hNtdll, "NtTerminateProcess");
            if (pNtTerminateProcess) {
                pNtTerminateProcess(GetCurrentProcess(), 1);
            }
        }
        
        // 最后使用 exit
        _exit(1);
        
        return EXCEPTION_CONTINUE_SEARCH;
    }
#endif
}

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    // 设置 SEH 异常处理器，捕获崩溃并强制终止进程
    SetUnhandledExceptionFilter(SehExceptionHandler);
#endif

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
    logFilePath = "logs/runtime_output.log";
    logFile = new QFile(logFilePath);
    
    if (logFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qInstallMessageHandler(messageHandler);
        qInfo() << "Log file created:" << logFilePath;
    } else {
        qWarning() << "Failed to create log file:" << logFilePath;
    }

    qInfo() << "========================================";
    qInfo() << "EnhanceVision starting...";
    qInfo() << "Qt version:" << qVersion();
    qInfo() << "========================================";

    EnhanceVision::Application application;
    application.initialize();
    application.show();

    int result = app.exec();
    
    // 清理日志
    qInfo() << "EnhanceVision exiting with code:" << result;
    if (logFile) {
        logFile->close();
        delete logFile;
        logFile = nullptr;
    }
    
    return result;
}
