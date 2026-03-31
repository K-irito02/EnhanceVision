/**
 * @file SettingsController.h
 * @brief 设置控制器 - 单例，负责主题、语言、配置等管理
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_SETTINGSCONTROLLER_H
#define ENHANCEVISION_SETTINGSCONTROLLER_H

#include <QObject>
#include <QSettings>
#include <QString>

namespace EnhanceVision {

/**
 * @brief 设置控制器 - 单例模式
 */
class SettingsController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(bool sidebarExpanded READ sidebarExpanded WRITE setSidebarExpanded NOTIFY sidebarExpandedChanged)
    Q_PROPERTY(QString defaultSavePath READ defaultSavePath WRITE setDefaultSavePath NOTIFY defaultSavePathChanged)
    Q_PROPERTY(bool autoSaveResult READ autoSaveResult WRITE setAutoSaveResult NOTIFY autoSaveResultChanged)
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool autoReprocessShaderEnabled READ autoReprocessShaderEnabled WRITE setAutoReprocessShaderEnabled NOTIFY autoReprocessShaderEnabledChanged)
    Q_PROPERTY(bool autoReprocessAIEnabled READ autoReprocessAIEnabled WRITE setAutoReprocessAIEnabled NOTIFY autoReprocessAIEnabledChanged)
    Q_PROPERTY(bool autoReprocessAllEnabled READ autoReprocessAllEnabled WRITE setAutoReprocessAllEnabled NOTIFY autoReprocessAllEnabledChanged)
    Q_PROPERTY(bool lastExitClean READ lastExitClean NOTIFY lastExitCleanChanged)
    Q_PROPERTY(bool crashDetectedOnStartup READ crashDetectedOnStartup NOTIFY crashDetectedOnStartupChanged)
    Q_PROPERTY(QString lastExitReason READ lastExitReason NOTIFY lastExitReasonChanged)
    Q_PROPERTY(bool videoAutoPlay READ videoAutoPlay WRITE setVideoAutoPlay NOTIFY videoAutoPlayChanged)
    Q_PROPERTY(bool videoAutoPlayOnSwitch READ videoAutoPlayOnSwitch WRITE setVideoAutoPlayOnSwitch NOTIFY videoAutoPlayOnSwitchChanged)
    Q_PROPERTY(bool videoRestorePosition READ videoRestorePosition WRITE setVideoRestorePosition NOTIFY videoRestorePositionChanged)

    Q_PROPERTY(QString customDataPath READ customDataPath WRITE setCustomDataPath NOTIFY customDataPathChanged)
    Q_PROPERTY(qint64 aiProcessedSize READ aiProcessedSize NOTIFY dataSizeChanged)
    Q_PROPERTY(qint64 shaderImageSize READ shaderImageSize NOTIFY dataSizeChanged)
    Q_PROPERTY(qint64 shaderVideoSize READ shaderVideoSize NOTIFY dataSizeChanged)
    Q_PROPERTY(qint64 logSize READ logSize NOTIFY dataSizeChanged)
    Q_PROPERTY(qint64 totalCacheSize READ totalCacheSize NOTIFY dataSizeChanged)
    Q_PROPERTY(int thumbnailCacheCount READ thumbnailCacheCount NOTIFY dataSizeChanged)

public:
    static SettingsController* instance();
    static void destroyInstance();

    QString theme() const;
    void setTheme(const QString& theme);

    QString language() const;
    void setLanguage(const QString& language);

    bool sidebarExpanded() const;
    void setSidebarExpanded(bool expanded);

    QString defaultSavePath() const;
    void setDefaultSavePath(const QString& path);

    bool autoSaveResult() const;
    void setAutoSaveResult(bool autoSave);

    int volume() const;
    void setVolume(int volume);

    bool autoReprocessShaderEnabled() const;
    void setAutoReprocessShaderEnabled(bool enabled);

    bool autoReprocessAIEnabled() const;
    void setAutoReprocessAIEnabled(bool enabled);

    bool autoReprocessAllEnabled() const;
    void setAutoReprocessAllEnabled(bool enabled);

    bool videoAutoPlay() const;
    void setVideoAutoPlay(bool autoPlay);

    bool videoAutoPlayOnSwitch() const;
    void setVideoAutoPlayOnSwitch(bool autoPlay);

    bool videoRestorePosition() const;
    void setVideoRestorePosition(bool restore);

    bool lastExitClean() const;
    bool crashDetectedOnStartup() const;
    QString lastExitReason() const;

    void markAppRunning();
    void markAppExiting();
    void markNormalExit(const QString& reason = QStringLiteral("normal"));
    bool checkAndHandleCrashRecovery();

    QString customDataPath() const;
    void setCustomDataPath(const QString& path);

    qint64 aiProcessedSize() const;
    qint64 shaderImageSize() const;
    qint64 shaderVideoSize() const;
    qint64 logSize() const;
    qint64 totalCacheSize() const;
    int thumbnailCacheCount() const;

    Q_INVOKABLE QString effectiveDataPath() const;

    Q_INVOKABLE void saveSettings();
    Q_INVOKABLE void loadSettings();
    Q_INVOKABLE void resetToDefaults();
    Q_INVOKABLE QString getSetting(const QString& key, const QString& defaultValue = QString());
    Q_INVOKABLE void setSetting(const QString& key, const QString& value);

    Q_INVOKABLE void refreshDataSize();
    Q_INVOKABLE bool clearAIProcessedData();
    Q_INVOKABLE bool clearShaderImageData();
    Q_INVOKABLE bool clearShaderVideoData();
    Q_INVOKABLE bool clearLogs();
    Q_INVOKABLE bool clearAllCache();
    Q_INVOKABLE QString formatSize(qint64 bytes) const;

signals:
    void themeChanged();
    void languageChanged();
    void sidebarExpandedChanged();
    void defaultSavePathChanged();
    void autoSaveResultChanged();
    void volumeChanged();
    void settingsChanged();
    void autoReprocessShaderEnabledChanged();
    void autoReprocessAIEnabledChanged();
    void autoReprocessAllEnabledChanged();
    void lastExitCleanChanged();
    void crashDetected();
    void crashDetectedOnStartupChanged();
    void lastExitReasonChanged();
    void videoAutoPlayChanged();
    void videoAutoPlayOnSwitchChanged();
    void videoRestorePositionChanged();
    void customDataPathChanged();
    void dataSizeChanged();

private:
    explicit SettingsController(QObject* parent = nullptr);
    ~SettingsController() override;

    SettingsController(const SettingsController&) = delete;
    SettingsController& operator=(const SettingsController&) = delete;

    qint64 calculateDirectorySize(const QString& path) const;
    bool clearDirectory(const QString& path);
    QString getAIProcessedPath() const;
    QString getShaderImagePath() const;
    QString getShaderVideoPath() const;
    QString getLogPath() const;

    static SettingsController* s_instance;

    QSettings* m_settings;
    QString m_theme;
    QString m_language;
    bool m_sidebarExpanded;
    QString m_defaultSavePath;
    bool m_autoSaveResult;
    int m_volume;
    bool m_autoReprocessShaderEnabled;
    bool m_autoReprocessAIEnabled;
    bool m_lastExitClean;
    bool m_crashDetectedOnStartup;
    QString m_lastExitReason;
    bool m_videoAutoPlay;
    bool m_videoAutoPlayOnSwitch;
    bool m_videoRestorePosition;
    QString m_customDataPath;

    qint64 m_aiProcessedSize;
    qint64 m_shaderImageSize;
    qint64 m_shaderVideoSize;
    qint64 m_logSize;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_SETTINGSCONTROLLER_H
