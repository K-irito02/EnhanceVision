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
    Q_PROPERTY(int maxConcurrentTasks READ maxConcurrentTasks WRITE setMaxConcurrentTasks NOTIFY maxConcurrentTasksChanged)
    Q_PROPERTY(QString defaultSavePath READ defaultSavePath WRITE setDefaultSavePath NOTIFY defaultSavePathChanged)
    Q_PROPERTY(bool autoSaveResult READ autoSaveResult WRITE setAutoSaveResult NOTIFY autoSaveResultChanged)

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

    int maxConcurrentTasks() const;
    void setMaxConcurrentTasks(int count);

    QString defaultSavePath() const;
    void setDefaultSavePath(const QString& path);

    bool autoSaveResult() const;
    void setAutoSaveResult(bool autoSave);

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
    void maxConcurrentTasksChanged();
    void defaultSavePathChanged();
    void autoSaveResultChanged();
    void settingsChanged();

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
    int m_maxConcurrentTasks;
    QString m_defaultSavePath;
    bool m_autoSaveResult;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_SETTINGSCONTROLLER_H
