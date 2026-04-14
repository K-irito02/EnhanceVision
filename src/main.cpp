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
#include <QIcon>
#include <QLocale>
#include <QSettings>
#include <QStandardPaths>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include "EnhanceVision/app/Application.h"
#include "EnhanceVision/controllers/SettingsController.h"
#include "EnhanceVision/core/LifecycleSupervisor.h"
#include "EnhanceVision/services/InstallMaintenanceService.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <processthreadsapi.h>
#include <shellapi.h>
#endif

namespace {
    QFile* logFile = nullptr;
    QString logFilePath;
#ifdef Q_OS_WIN
    wchar_t crashLogPath[MAX_PATH] = L"";
#endif

    QString ensureLogDirectory()
    {
        QSettings settings(EnhanceVision::SettingsController::settingsFilePath(), QSettings::IniFormat);
        const QString configuredDataPath = settings.value(QStringLiteral("behavior/customDataPath"), QString()).toString();

        bool fallbackActive = false;
        QString fallbackReason;
        QString basePath = EnhanceVision::SettingsController::resolveEffectiveDataPath(
            configuredDataPath,
            &fallbackActive,
            &fallbackReason,
            QCoreApplication::applicationDirPath());

        QDir logDir(QDir(basePath).filePath("logs"));
        if (!logDir.exists()) {
            logDir.mkpath(".");
        }

        if (fallbackActive) {
            qWarning() << "[main] Falling back to default data directory for logs:" << fallbackReason
                       << "configured path:" << configuredDataPath
                       << "effective path:" << basePath;
        }

        return logDir.absolutePath();
    }

    QString readStartupLanguage()
    {
        QSettings settings(EnhanceVision::SettingsController::settingsFilePath(), QSettings::IniFormat);
        const QString configuredLanguage = settings
            .value(QStringLiteral("appearance/language"), QString())
            .toString()
            .trimmed();

        const auto normalizeLanguage = [](const QString& value) -> QString {
            const QString lowered = value.toLower();
            if (lowered.startsWith(QStringLiteral("en"))) {
                return QStringLiteral("en_US");
            }
            if (lowered.startsWith(QStringLiteral("zh"))) {
                return QStringLiteral("zh_CN");
            }
            return {};
        };

        QString language = normalizeLanguage(configuredLanguage);
        if (!language.isEmpty()) {
            return language;
        }

#ifdef Q_OS_WIN
        // Fallback to installer-selected language for first run when settings are
        // absent or malformed.
        auto resolveInstallerLanguage = [](const QString& rootPath) -> QString {
            QSettings registry(rootPath, QSettings::NativeFormat);
            const int installerLangId = registry.value(QStringLiteral("Installer Language"), 0).toInt();
            if (installerLangId == 1033) {
                return QStringLiteral("en_US");
            }
            if (installerLangId == 2052) {
                return QStringLiteral("zh_CN");
            }
            return {};
        };

        language = resolveInstallerLanguage(QStringLiteral("HKEY_LOCAL_MACHINE\\Software\\EnhanceVision"));
        if (language.isEmpty()) {
            language = resolveInstallerLanguage(QStringLiteral("HKEY_CURRENT_USER\\Software\\EnhanceVision"));
        }
        if (!language.isEmpty()) {
            return language;
        }
#endif

        return QStringLiteral("zh_CN");
    }

    void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
    {
        Q_UNUSED(context)

        if (type == QtDebugMsg || type == QtInfoMsg) {
            return;
        }

        if (!logFile || !logFile->isOpen()) {
            return;
        }

        QTextStream out(logFile);
        out.setEncoding(QStringConverter::Utf8);

        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
        QString typeStr;

        switch (type) {
        case QtWarningMsg:
            typeStr = "WARN";
            break;
        case QtCriticalMsg:
            typeStr = "CRIT";
            break;
        case QtFatalMsg:
            typeStr = "FATAL";
            break;
        default:
            typeStr = "MSG";
            break;
        }

        out << QString("[%1] [%2] %3\n").arg(timestamp, typeStr, msg);
        out.flush();

        fprintf(stderr, "[%s] [%s] %s\n", 
                timestamp.toLocal8Bit().constData(),
                typeStr.toLocal8Bit().constData(),
                msg.toLocal8Bit().constData());
    }

#ifdef Q_OS_WIN
    void enableCrossIntegrityDropMessages()
    {
        // When launched from elevated installer, the process can run elevated.
        // Allow drag/drop messages from normal-integrity Explorer windows.
        HMODULE user32 = GetModuleHandleW(L"user32.dll");
        if (!user32) {
            return;
        }

        using ChangeWindowMessageFilterFn = BOOL (WINAPI*)(UINT, DWORD);
        auto* changeFilter = reinterpret_cast<ChangeWindowMessageFilterFn>(
            GetProcAddress(user32, "ChangeWindowMessageFilter"));
        if (!changeFilter) {
            return;
        }

        constexpr DWORD kMsgFilterAdd = 1; // MSGFLT_ADD
        constexpr UINT kWmCopyGlobalData = 0x0049;
        changeFilter(WM_DROPFILES, kMsgFilterAdd);
        changeFilter(WM_COPYDATA, kMsgFilterAdd);
        changeFilter(kWmCopyGlobalData, kMsgFilterAdd);
    }

    void writeCrashLog(const char* message)
    {
        // 输出到 stderr
        fprintf(stderr, "%s\n", message);
        fflush(stderr);
        
        // 写入崩溃日志文件
        HANDLE hFile = CreateFileW(
            crashLogPath[0] != L'\0' ? crashLogPath : L"crash.log",
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

        writeCrashLog(msg);
        
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
    enableCrossIntegrityDropMessages();
#endif

    QApplication app(argc, argv);
    QApplication::setApplicationName("EnhanceVision");
    QApplication::setApplicationVersion("0.1.0");
    QApplication::setOrganizationName("EnhanceVision");
    QLocale::setDefault(QLocale(readStartupLanguage()));

    QString installMaintenanceError;
    if (!EnhanceVision::InstallMaintenanceService::applyPendingIntent(&installMaintenanceError) &&
        !installMaintenanceError.isEmpty()) {
        qWarning() << "[main] Install maintenance failed:" << installMaintenanceError;
    }
    
    QApplication::setWindowIcon(QIcon(":/icons/app_icon.png"));
    
    QQuickStyle::setStyle("Basic");
    
    const QString logDirPath = ensureLogDirectory();
    logFilePath = QDir(logDirPath).filePath("runtime_output.log");
#ifdef Q_OS_WIN
    const QString nativeCrashLogPath = QDir::toNativeSeparators(QDir(logDirPath).filePath("crash.log"));
    const int crashPathLength = qMin(nativeCrashLogPath.size(), static_cast<int>(MAX_PATH) - 1);
    nativeCrashLogPath.left(crashPathLength).toWCharArray(crashLogPath);
    crashLogPath[crashPathLength] = L'\0';
#endif
    logFile = new QFile(logFilePath);
    
    if (logFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qInstallMessageHandler(messageHandler);
    } else {
        qWarning() << "Failed to create log file:" << logFilePath;
    }

    int result = 0;
    {
        EnhanceVision::Application application;
        application.initialize();
        application.show();
        result = app.exec();
    }

    if (auto* lifecycleSupervisor = EnhanceVision::LifecycleSupervisor::instanceIfExists()) {
        lifecycleSupervisor->markTerminationComplete();
    }
    
    // 清理日志
    if (logFile) {
        logFile->close();
        delete logFile;
        logFile = nullptr;
    }
    
    return result;
}
