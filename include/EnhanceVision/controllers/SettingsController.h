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

public:
    /**
     * @brief 获取单例实例
     * @return SettingsController 实例指针
     */
    static SettingsController* instance();

    /**
     * @brief 销毁单例实例
     */
    static void destroyInstance();

    // 属性访问器
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

    bool lastExitClean() const;
    bool crashDetectedOnStartup() const;
    QString lastExitReason() const;

    void markAppRunning();
    void markAppExiting();
    void markNormalExit(const QString& reason = QStringLiteral("normal"));
    bool checkAndHandleCrashRecovery();

    // Q_INVOKABLE 方法
    Q_INVOKABLE void saveSettings();
    Q_INVOKABLE void loadSettings();
    Q_INVOKABLE void resetToDefaults();
    Q_INVOKABLE QString getSetting(const QString& key, const QString& defaultValue = QString());
    Q_INVOKABLE void setSetting(const QString& key, const QString& value);

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

private:
    explicit SettingsController(QObject* parent = nullptr);
    ~SettingsController() override;

    // 禁用拷贝构造和赋值
    SettingsController(const SettingsController&) = delete;
    SettingsController& operator=(const SettingsController&) = delete;

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
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_SETTINGSCONTROLLER_H
