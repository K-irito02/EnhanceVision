# OBS C++ and Qt Templates

Ready-to-use templates for integrating C++ and Qt6 into OBS plugins.

## CMakeLists.txt (Qt Optional Build)

```cmake
cmake_minimum_required(VERSION 3.28)
project(my-plugin VERSION 1.0.0 LANGUAGES C CXX)

# C/C++ standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable Qt automoc (required for Q_OBJECT classes)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

# Cross-compilation option
option(CROSS_COMPILE_WINDOWS "Cross-compile for Windows from Linux" OFF)

# Find OBS
if(CROSS_COMPILE_WINDOWS OR DEFINED OBS_SOURCE_DIR)
    # Headers-only build
    add_library(obs-headers INTERFACE)
    target_include_directories(obs-headers INTERFACE
        "${OBS_SOURCE_DIR}/libobs"
        "${OBS_SOURCE_DIR}/frontend/api"
    )
    set(OBS_TARGET obs-headers)
    set(HAS_FRONTEND_API ON)
else()
    # Native build
    find_package(libobs REQUIRED)
    find_package(obs-frontend-api QUIET)
    set(OBS_TARGET OBS::libobs)
    if(obs-frontend-api_FOUND)
        set(HAS_FRONTEND_API ON)
    else()
        set(HAS_FRONTEND_API OFF)
    endif()
endif()

# Find Qt6 (optional)
find_package(Qt6 COMPONENTS Widgets QUIET)
if(Qt6_FOUND AND HAS_FRONTEND_API)
    message(STATUS "Qt6 + Frontend API found - building with settings dialog")
    set(BUILD_WITH_QT ON)
else()
    message(STATUS "Qt6 or Frontend API not found - building without settings dialog")
    set(BUILD_WITH_QT OFF)
endif()

# Create plugin module - C sources
add_library(${PROJECT_NAME} MODULE
    src/plugin-main.c
    src/my-source.c
)

# Add frontend based on Qt availability
if(BUILD_WITH_QT)
    target_sources(${PROJECT_NAME} PRIVATE
        src/frontend.cpp
        src/settings-dialog.cpp
    )
    target_compile_definitions(${PROJECT_NAME} PRIVATE BUILD_WITH_QT)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets)

    if(obs-frontend-api_FOUND)
        target_link_libraries(${PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
    endif()
else()
    target_sources(${PROJECT_NAME} PRIVATE
        src/frontend-simple.c
    )
endif()

# Platform-specific
if(WIN32 OR CROSS_COMPILE_WINDOWS)
    target_sources(${PROJECT_NAME} PRIVATE src/platform/socket-win32.c)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32 comctl32)

    if(CROSS_COMPILE_WINDOWS OR CMAKE_C_COMPILER_ID STREQUAL "GNU")
        set_target_properties(${PROJECT_NAME} PROPERTIES
            LINK_FLAGS "${CMAKE_CURRENT_SOURCE_DIR}/src/plugin.def -Wl,--unresolved-symbols=ignore-all"
        )
    else()
        set_target_properties(${PROJECT_NAME} PROPERTIES
            LINK_FLAGS "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/src/plugin.def"
        )
    endif()
else()
    target_sources(${PROJECT_NAME} PRIVATE src/platform/socket-posix.c)
endif()

# Link OBS
target_link_libraries(${PROJECT_NAME} PRIVATE ${OBS_TARGET})

# Output properties
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" OUTPUT_NAME "${PROJECT_NAME}")
```

## frontend.h (C-Compatible Header)

```c
/* frontend.h - Frontend API (C-compatible)
 *
 * This header is included from both C and C++ files.
 * Implementation is in frontend.cpp (Qt) or frontend-simple.c (no Qt).
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize frontend (call from obs_module_load) */
void frontend_init(void);

/* Cleanup frontend (call from obs_module_unload) */
void frontend_cleanup(void);

/* Show settings dialog (may be no-op if Qt not available) */
void frontend_show_settings(void);

#ifdef __cplusplus
}
#endif
```

## frontend.cpp (Qt Implementation)

```cpp
/* frontend.cpp - Qt-based frontend implementation
 *
 * Provides settings dialog and Tools menu integration.
 * Only compiled when BUILD_WITH_QT is defined.
 */

extern "C" {
#include <obs-module.h>
#include <obs-frontend-api.h>
}

#include "settings-dialog.h"
#include <QMainWindow>
#include <QMessageBox>

static SettingsDialog *g_settings_dialog = nullptr;

/* C-compatible API */
extern "C" {

static void on_tools_menu_clicked(void *data)
{
    (void)data;
    frontend_show_settings();
}

static void on_frontend_event(enum obs_frontend_event event, void *data)
{
    (void)data;

    switch (event) {
    case OBS_FRONTEND_EVENT_FINISHED_LOADING:
        /* OBS UI is fully loaded - safe to create Qt widgets now */
        break;

    case OBS_FRONTEND_EVENT_EXIT:
        /* Clean up Qt resources before OBS exits */
        if (g_settings_dialog) {
            delete g_settings_dialog;
            g_settings_dialog = nullptr;
        }
        break;

    default:
        break;
    }
}

void frontend_init(void)
{
    /* Add Tools menu item */
    obs_frontend_add_tools_menu_item(
        obs_module_text("MyPlugin.Settings"),
        on_tools_menu_clicked,
        nullptr
    );

    /* Subscribe to frontend events */
    obs_frontend_add_event_callback(on_frontend_event, nullptr);

    blog(LOG_INFO, "[my-plugin] Frontend initialized with Qt support");
}

void frontend_cleanup(void)
{
    obs_frontend_remove_event_callback(on_frontend_event, nullptr);

    if (g_settings_dialog) {
        delete g_settings_dialog;
        g_settings_dialog = nullptr;
    }
}

void frontend_show_settings(void)
{
    QMainWindow *main_window = static_cast<QMainWindow *>(
        obs_frontend_get_main_window()
    );

    if (!g_settings_dialog) {
        g_settings_dialog = new SettingsDialog(main_window);
    }

    /* Load current settings into dialog */
    // g_settings_dialog->setHost(...);
    // g_settings_dialog->setPort(...);

    if (g_settings_dialog->exec() == QDialog::Accepted) {
        /* User clicked OK - save settings */
        QString host = g_settings_dialog->host();
        int port = g_settings_dialog->port();

        blog(LOG_INFO, "[my-plugin] Settings saved: %s:%d",
             host.toUtf8().constData(), port);

        /* Apply settings to plugin... */
    }
}

} /* extern "C" */
```

## frontend-simple.c (No Qt Fallback)

```c
/* frontend-simple.c - Minimal frontend without Qt
 *
 * Used when Qt is not available (cross-compilation, etc.).
 * Provides stub implementations of the frontend API.
 */

#include <obs-module.h>
#include "frontend.h"

void frontend_init(void)
{
    blog(LOG_INFO, "[my-plugin] Frontend initialized without Qt UI");
}

void frontend_cleanup(void)
{
    /* Nothing to clean up */
}

void frontend_show_settings(void)
{
    blog(LOG_WARNING, "[my-plugin] Settings dialog not available - "
                      "plugin built without Qt support");
}
```

## settings-dialog.h (Qt Dialog Header)

```cpp
/* settings-dialog.h - Plugin settings dialog
 *
 * Modal dialog for configuring plugin settings.
 * Uses Qt6 Widgets.
 */

#pragma once

#include <QDialog>

class QLineEdit;
class QSpinBox;
class QCheckBox;
class QComboBox;

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog() override = default;

    /* Getters */
    QString host() const;
    int port() const;
    bool enabled() const;
    int quality() const;

    /* Setters */
    void setHost(const QString &host);
    void setPort(int port);
    void setEnabled(bool enabled);
    void setQuality(int quality);

private slots:
    void onTestConnection();

private:
    void setupUi();
    void createConnections();

    QLineEdit *m_hostEdit;
    QSpinBox *m_portSpin;
    QCheckBox *m_enabledCheck;
    QComboBox *m_qualityCombo;
};
```

## settings-dialog.cpp (Qt Dialog Implementation)

```cpp
/* settings-dialog.cpp - Plugin settings dialog implementation */

#include "settings-dialog.h"

#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi();
    createConnections();
}

void SettingsDialog::setupUi()
{
    setWindowTitle(tr("Plugin Settings"));
    setMinimumWidth(350);

    /* Connection settings group */
    auto *connectionGroup = new QGroupBox(tr("Connection"), this);
    auto *connectionLayout = new QFormLayout(connectionGroup);

    m_hostEdit = new QLineEdit(this);
    m_hostEdit->setPlaceholderText(tr("127.0.0.1"));
    connectionLayout->addRow(tr("Host:"), m_hostEdit);

    m_portSpin = new QSpinBox(this);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(5000);
    connectionLayout->addRow(tr("Port:"), m_portSpin);

    auto *testButton = new QPushButton(tr("Test Connection"), this);
    testButton->setObjectName("testButton");
    connectionLayout->addRow("", testButton);

    /* Options group */
    auto *optionsGroup = new QGroupBox(tr("Options"), this);
    auto *optionsLayout = new QFormLayout(optionsGroup);

    m_enabledCheck = new QCheckBox(tr("Enable plugin"), this);
    m_enabledCheck->setChecked(true);
    optionsLayout->addRow(m_enabledCheck);

    m_qualityCombo = new QComboBox(this);
    m_qualityCombo->addItem(tr("Low"), 0);
    m_qualityCombo->addItem(tr("Medium"), 1);
    m_qualityCombo->addItem(tr("High"), 2);
    m_qualityCombo->setCurrentIndex(1);
    optionsLayout->addRow(tr("Quality:"), m_qualityCombo);

    /* Buttons */
    auto *okButton = new QPushButton(tr("OK"), this);
    okButton->setDefault(true);
    auto *cancelButton = new QPushButton(tr("Cancel"), this);
    auto *applyButton = new QPushButton(tr("Apply"), this);

    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(applyButton);

    /* Main layout */
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(connectionGroup);
    mainLayout->addWidget(optionsGroup);
    mainLayout->addStretch();
    mainLayout->addLayout(buttonLayout);

    /* Connect buttons */
    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(applyButton, &QPushButton::clicked, this, [this]() {
        /* Emit signal or call save function */
        QMessageBox::information(this, tr("Apply"), tr("Settings applied."));
    });
    connect(testButton, &QPushButton::clicked, this, &SettingsDialog::onTestConnection);
}

void SettingsDialog::createConnections()
{
    /* Additional signal/slot connections */
}

void SettingsDialog::onTestConnection()
{
    /* Test connection to configured host:port */
    QMessageBox::information(this, tr("Test"),
        tr("Testing connection to %1:%2...")
            .arg(host())
            .arg(port()));
}

/* Getters */
QString SettingsDialog::host() const
{
    return m_hostEdit->text().isEmpty() ? "127.0.0.1" : m_hostEdit->text();
}

int SettingsDialog::port() const
{
    return m_portSpin->value();
}

bool SettingsDialog::enabled() const
{
    return m_enabledCheck->isChecked();
}

int SettingsDialog::quality() const
{
    return m_qualityCombo->currentData().toInt();
}

/* Setters */
void SettingsDialog::setHost(const QString &host)
{
    m_hostEdit->setText(host);
}

void SettingsDialog::setPort(int port)
{
    m_portSpin->setValue(port);
}

void SettingsDialog::setEnabled(bool enabled)
{
    m_enabledCheck->setChecked(enabled);
}

void SettingsDialog::setQuality(int quality)
{
    int index = m_qualityCombo->findData(quality);
    if (index >= 0) {
        m_qualityCombo->setCurrentIndex(index);
    }
}
```

## plugin-main.c (C Entry Point)

```c
/* plugin-main.c - OBS plugin module entry point
 *
 * This is pure C for maximum compatibility.
 * C++ features are in separate .cpp files.
 */

#include <obs-module.h>
#include "frontend.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("my-plugin", "en-US")

extern struct obs_source_info my_source;

bool obs_module_load(void)
{
    blog(LOG_INFO, "[my-plugin] Loading...");

    /* Register sources/outputs/etc. */
    obs_register_source(&my_source);

    /* Initialize frontend (Qt or simple) */
    frontend_init();

    blog(LOG_INFO, "[my-plugin] Loaded successfully");
    return true;
}

void obs_module_unload(void)
{
    blog(LOG_INFO, "[my-plugin] Unloading...");

    frontend_cleanup();

    blog(LOG_INFO, "[my-plugin] Unloaded");
}

const char *obs_module_description(void)
{
    return "My OBS Plugin - Does cool stuff";
}
```

## Dockable Widget Template

```cpp
/* dock-widget.h - OBS dock panel */

#pragma once

#include <QDockWidget>
#include <QWidget>

class DockWidget : public QWidget {
    Q_OBJECT

public:
    explicit DockWidget(QWidget *parent = nullptr);

private:
    void setupUi();
};

/* dock-widget.cpp */
#include "dock-widget.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>

DockWidget::DockWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void DockWidget::setupUi()
{
    auto *layout = new QVBoxLayout(this);

    auto *label = new QLabel(tr("Plugin Status"), this);
    layout->addWidget(label);

    auto *button = new QPushButton(tr("Action"), this);
    layout->addWidget(button);

    layout->addStretch();
}

/* In frontend.cpp: */
#include <obs-frontend-api.h>

static QDockWidget *g_dock = nullptr;

void frontend_init(void)
{
    QMainWindow *main = (QMainWindow *)obs_frontend_get_main_window();

    g_dock = new QDockWidget(tr("My Plugin"), main);
    g_dock->setWidget(new DockWidget(g_dock));

    obs_frontend_add_dock(g_dock);
}
```

## Worker Thread Template

```cpp
/* worker-thread.h - Background processing thread */

#pragma once

#include <QThread>
#include <QMutex>
#include <atomic>

class WorkerThread : public QThread {
    Q_OBJECT

public:
    explicit WorkerThread(QObject *parent = nullptr);
    ~WorkerThread() override;

    void stop();
    void setConfig(const QString &host, int port);

signals:
    void statusChanged(const QString &status);
    void errorOccurred(const QString &error);
    void dataReceived(const QByteArray &data);

protected:
    void run() override;

private:
    std::atomic<bool> m_running{false};
    QMutex m_mutex;
    QString m_host;
    int m_port{0};
};

/* worker-thread.cpp */
#include "worker-thread.h"

WorkerThread::WorkerThread(QObject *parent)
    : QThread(parent)
{
}

WorkerThread::~WorkerThread()
{
    stop();
}

void WorkerThread::stop()
{
    m_running = false;
    if (isRunning()) {
        wait(5000);  /* Wait up to 5 seconds */
    }
}

void WorkerThread::setConfig(const QString &host, int port)
{
    QMutexLocker lock(&m_mutex);
    m_host = host;
    m_port = port;
}

void WorkerThread::run()
{
    m_running = true;
    emit statusChanged(tr("Started"));

    while (m_running) {
        /* Do work here */
        QString host;
        int port;
        {
            QMutexLocker lock(&m_mutex);
            host = m_host;
            port = m_port;
        }

        /* Process... */
        QThread::msleep(100);
    }

    emit statusChanged(tr("Stopped"));
}
```

## Locale File Template (en-US.ini)

```ini
# en-US.ini - English (US) localization
# Place in data/locale/en-US.ini

# Tools menu
MyPlugin.Settings="My Plugin Settings"

# Settings dialog
Settings.Title="Plugin Settings"
Settings.Connection="Connection"
Settings.Host="Host"
Settings.Port="Port"
Settings.TestConnection="Test Connection"
Settings.Options="Options"
Settings.Enabled="Enable plugin"
Settings.Quality="Quality"
Settings.Quality.Low="Low"
Settings.Quality.Medium="Medium"
Settings.Quality.High="High"

# Buttons
Button.OK="OK"
Button.Cancel="Cancel"
Button.Apply="Apply"

# Status messages
Status.Connected="Connected"
Status.Disconnected="Disconnected"
Status.Error="Error: %1"
```
