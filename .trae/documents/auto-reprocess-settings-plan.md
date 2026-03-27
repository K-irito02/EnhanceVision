# 自动重新处理机制改进优化计划（修订版）

## 一、需求分析

### 1.1 用户核心需求
1. 将"自动重新处理"机制配置到设置中
2. 支持一键开启/关闭全部模式的自动重新处理
3. 支持分别控制 Shader 模式和 AI 模式的自动重新处理
4. 控制应用中途关闭后重新打开时的自动重新处理行为
5. **新增：崩溃/闪退时自动关闭自动重新处理机制**
6. **新增：所有设置操作都需要持久化配置**
7. 删除对处理失败文件的自动重试机制
8. 失败文件支持手动重新处理（消息卡片按钮和右键菜单）

### 1.2 当前实现分析
- **位置**: `SessionController::loadSessionMessages()` (line 733-778)
- **逻辑**: 加载会话消息时，检查状态为 Pending 或 Processing 的文件，自动调用 `autoRetryInterruptedFiles()`
- **问题**: 
  - 无法按模式（Shader/AI）分别控制
  - 无用户可配置选项
  - 对失败文件也有自动重试逻辑
  - 无崩溃安全机制

## 二、设计方案

### 2.1 设置项设计

| 设置项 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| `autoReprocessShaderEnabled` | bool | true | Shader 模式自动重新处理开关 |
| `autoReprocessAIEnabled` | bool | true | AI 推理模式自动重新处理开关 |
| `lastExitClean` | bool | true | 上次退出是否正常（崩溃检测标志） |

### 2.2 崩溃检测机制

```
应用启动流程：
┌─────────────────────────────────────────────────────────────┐
│                     应用启动                                 │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│  检查 lastExitClean 标志                                    │
│  (从持久化配置读取)                                          │
└──────────────────────────┬──────────────────────────────────┘
                           │
              ┌────────────┴────────────┐
              │                         │
              ▼                         ▼
┌─────────────────────┐    ┌─────────────────────────────────┐
│ lastExitClean=true  │    │ lastExitClean=false (崩溃)      │
│ (正常退出)          │    │                                 │
└──────────┬──────────┘    │ 1. 关闭所有自动重新处理开关      │
           │               │    autoReprocessShaderEnabled=false│
           │               │    autoReprocessAIEnabled=false   │
           │               │ 2. 保存配置到持久化存储           │
           │               │ 3. 显示崩溃恢复提示               │
           │               └──────────────────┬──────────────┘
           │                                  │
           └────────────────┬─────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│  设置 lastExitClean = false (标记为"运行中")                │
│  保存到持久化配置                                            │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                     正常运行                                 │
└─────────────────────────────────────────────────────────────┘

应用退出流程：
┌─────────────────────────────────────────────────────────────┐
│                     应用退出                                 │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│  设置 lastExitClean = true (标记为"正常退出")               │
│  保存到持久化配置                                            │
└─────────────────────────────────────────────────────────────┘
```

### 2.3 UI 设计

在设置页面新增"处理恢复"设置组：

```
┌─────────────────────────────────────────────────────────────┐
│ ⚙️ 处理恢复                                                  │
├─────────────────────────────────────────────────────────────┤
│ 应用启动时自动恢复未完成的处理任务                            │
│                                                             │
│ 全部开启                                    [主开关 Switch]  │
│                                                             │
│ Shader 模式                                 [子开关 Switch]  │
│ AI 推理模式                                  [子开关 Switch]  │
│                                                             │
│ 💡 关闭后，应用中途关闭时的处理任务不会自动恢复，             │
│    您可以手动点击"重新处理"按钮进行处理                      │
└─────────────────────────────────────────────────────────────┘
```

### 2.4 崩溃恢复提示

当检测到崩溃后，显示提示：

```
┌─────────────────────────────────────────────────────────────┐
│ ⚠️ 检测到上次应用异常退出                                    │
│                                                             │
│ 为确保稳定性，已自动关闭"自动重新处理"功能。                 │
│ 您可以在设置中重新开启此功能。                               │
│                                                             │
│                                              [知道了]        │
└─────────────────────────────────────────────────────────────┘
```

### 2.5 交互逻辑
- **主开关**: 一键开启/关闭全部模式的自动重新处理
- **子开关**: 分别控制 Shader 和 AI 模式
- **联动关系**: 
  - 当两个子开关都开启时，主开关自动开启
  - 当两个子开关都关闭时，主开关自动关闭
  - 当只有一个子开关开启时，主开关显示中间状态
  - 点击主开关时，同时设置两个子开关为相同状态
- **崩溃联动**: 检测到崩溃时，自动关闭所有开关并持久化

## 三、实施步骤

### 3.1 后端 C++ 修改

#### 3.1.1 SettingsController.h 修改
- 添加 `autoReprocessShaderEnabled` 属性声明
- 添加 `autoReprocessAIEnabled` 属性声明
- 添加 `lastExitClean` 属性声明（崩溃检测标志）
- 添加 `autoReprocessAllEnabled` 便捷属性（用于主开关）
- 添加相应的信号和成员变量
- 添加 `markAppRunning()` 和 `markAppExiting()` 方法

#### 3.1.2 SettingsController.cpp 修改
- 构造函数初始化新成员变量
- 实现属性访问器和设置器
- 在 `saveSettings()` 中保存新设置
- 在 `loadSettings()` 中加载新设置
- 在 `resetToDefaults()` 中重置新设置
- 实现崩溃检测逻辑

#### 3.1.3 SessionController.cpp 修改
- 修改 `loadSessionMessages()` 方法
- 根据消息的处理模式和对应设置决定是否自动重新处理
- 删除对失败文件的自动重试逻辑

#### 3.1.4 Application.cpp 修改
- 在应用启动时调用崩溃检测
- 在应用退出时标记正常退出

#### 3.1.5 ProcessingController.cpp 修改
- 确认 `autoRetryInterruptedFiles()` 方法仅处理中断文件
- 确认失败文件不在此方法处理范围内

### 3.2 前端 QML 修改

#### 3.2.1 SettingsPage.qml 修改
- 在"行为"设置组后添加"处理恢复"设置组
- 添加主开关和两个子开关
- 实现开关联动逻辑
- 添加说明文本

#### 3.2.2 MainPage.qml 或 NotificationManager.qml
- 添加崩溃恢复提示弹窗

### 3.3 国际化修改

#### 3.3.1 app_zh_CN.ts 更新
- 添加新的翻译字符串

#### 3.3.2 app_en_US.ts 更新
- 添加英文翻译

## 四、详细代码修改清单

### 4.1 SettingsController.h
```cpp
// 新增属性
Q_PROPERTY(bool autoReprocessShaderEnabled READ autoReprocessShaderEnabled WRITE setAutoReprocessShaderEnabled NOTIFY autoReprocessShaderEnabledChanged)
Q_PROPERTY(bool autoReprocessAIEnabled READ autoReprocessAIEnabled WRITE setAutoReprocessAIEnabled NOTIFY autoReprocessAIEnabledChanged)
Q_PROPERTY(bool autoReprocessAllEnabled READ autoReprocessAllEnabled WRITE setAutoReprocessAllEnabled NOTIFY autoReprocessAllEnabledChanged)
Q_PROPERTY(bool lastExitClean READ lastExitClean NOTIFY lastExitCleanChanged)

// 新增方法声明
bool autoReprocessShaderEnabled() const;
void setAutoReprocessShaderEnabled(bool enabled);
bool autoReprocessAIEnabled() const;
void setAutoReprocessAIEnabled(bool enabled);
bool autoReprocessAllEnabled() const;
void setAutoReprocessAllEnabled(bool enabled);
bool lastExitClean() const;

// 崩溃检测方法
void markAppRunning();
void markAppExiting();
bool checkAndHandleCrashRecovery();

// 新增信号
void autoReprocessShaderEnabledChanged();
void autoReprocessAIEnabledChanged();
void autoReprocessAllEnabledChanged();
void lastExitCleanChanged();
void crashDetected();

// 新增成员变量
bool m_autoReprocessShaderEnabled;
bool m_autoReprocessAIEnabled;
bool m_lastExitClean;
```

### 4.2 SettingsController.cpp
- 构造函数初始化新成员变量
- 实现 getter/setter 方法
- saveSettings() 添加：
  ```cpp
  m_settings->setValue("reprocess/shaderEnabled", m_autoReprocessShaderEnabled);
  m_settings->setValue("reprocess/aiEnabled", m_autoReprocessAIEnabled);
  m_settings->setValue("system/lastExitClean", m_lastExitClean);
  ```
- loadSettings() 添加：
  ```cpp
  m_autoReprocessShaderEnabled = m_settings->value("reprocess/shaderEnabled", true).toBool();
  m_autoReprocessAIEnabled = m_settings->value("reprocess/aiEnabled", true).toBool();
  m_lastExitClean = m_settings->value("system/lastExitClean", true).toBool();
  ```
- 实现 `markAppRunning()`: 设置 lastExitClean = false 并保存
- 实现 `markAppExiting()`: 设置 lastExitClean = true 并保存
- 实现 `checkAndHandleCrashRecovery()`: 检测崩溃并处理

### 4.3 SessionController.cpp - loadSessionMessages()
```cpp
void SessionController::loadSessionMessages(const QString& sessionId)
{
    // ... 现有代码 ...
    
    if (m_processingController) {
        QStringList interruptedMessageIds;
        interruptedMessageIds.reserve(session->messages.size());

        for (const Message& msg : session->messages) {
            if (m_autoRetriedMessageIds.contains(msg.id)) {
                continue;
            }

            bool hasInterruptedFiles = false;
            for (const MediaFile& file : msg.mediaFiles) {
                // 仅处理中断的文件（Pending/Processing），不处理失败文件
                if (file.status == ProcessingStatus::Pending || 
                    file.status == ProcessingStatus::Processing) {
                    hasInterruptedFiles = true;
                    break;
                }
            }

            if (hasInterruptedFiles) {
                // 根据消息模式和设置决定是否自动重新处理
                auto* settings = SettingsController::instance();
                bool shouldAutoReprocess = false;
                
                if (msg.mode == ProcessingMode::Shader) {
                    shouldAutoReprocess = settings->autoReprocessShaderEnabled();
                } else if (msg.mode == ProcessingMode::AIInference) {
                    shouldAutoReprocess = settings->autoReprocessAIEnabled();
                }
                
                if (shouldAutoReprocess) {
                    m_autoRetriedMessageIds.insert(msg.id);
                    interruptedMessageIds.append(msg.id);
                }
            }
        }

        if (!interruptedMessageIds.isEmpty()) {
            QTimer::singleShot(0, this, [this, sessionId, interruptedMessageIds]() {
                if (!m_processingController) {
                    return;
                }

                for (const QString& messageId : interruptedMessageIds) {
                    m_processingController->autoRetryInterruptedFiles(messageId, sessionId);
                }
            });
        }
    }
}
```

### 4.4 Application.cpp
```cpp
// 在应用初始化时
void Application::initialize() {
    auto* settings = SettingsController::instance();
    
    // 检测崩溃并处理
    if (settings->checkAndHandleCrashRecovery()) {
        // 检测到崩溃，显示提示
        emit crashRecoveryNeeded();
    }
    
    // 标记应用正在运行
    settings->markAppRunning();
    
    // ... 其他初始化代码 ...
}

// 在应用退出时
void Application::cleanup() {
    auto* settings = SettingsController::instance();
    
    // 标记正常退出
    settings->markAppExiting();
    
    // ... 其他清理代码 ...
}
```

### 4.5 SettingsPage.qml - 新增设置组
```qml
Rectangle {
    Layout.fillWidth: true
    implicitHeight: reprocessCol.implicitHeight + 32
    color: Theme.colors.card
    border.width: 1
    border.color: Theme.colors.cardBorder
    radius: Theme.radius.lg

    ColumnLayout {
        id: reprocessCol
        anchors.fill: parent
        anchors.margins: 16
        spacing: 14

        RowLayout {
            spacing: 8
            ColoredIcon { iconSize: 18; source: Theme.icon("refresh-cw"); color: Theme.colors.icon }
            Text { text: qsTr("处理恢复"); color: Theme.colors.foreground; font.pixelSize: 15; font.weight: Font.DemiBold }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.colors.border }

        Text { 
            text: qsTr("应用启动时自动恢复未完成的处理任务")
            color: Theme.colors.mutedForeground
            font.pixelSize: 12
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Text { text: qsTr("全部开启"); color: Theme.colors.foreground; font.pixelSize: 13; Layout.minimumWidth: 80 }
            Item { Layout.fillWidth: true }
            Switch {
                id: allSwitch
                property bool updating: false
                checked: SettingsController.autoReprocessAllEnabled
                onCheckedChanged: {
                    if (!updating) {
                        SettingsController.autoReprocessAllEnabled = checked
                    }
                }
                
                Connections {
                    target: SettingsController
                    function onAutoReprocessAllEnabledChanged() {
                        allSwitch.updating = true
                        allSwitch.checked = SettingsController.autoReprocessAllEnabled
                        allSwitch.updating = false
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Text { text: qsTr("Shader 模式"); color: Theme.colors.foreground; font.pixelSize: 13; Layout.minimumWidth: 80 }
            Item { Layout.fillWidth: true }
            Switch {
                checked: SettingsController.autoReprocessShaderEnabled
                onCheckedChanged: SettingsController.autoReprocessShaderEnabled = checked
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Text { text: qsTr("AI 推理模式"); color: Theme.colors.foreground; font.pixelSize: 13; Layout.minimumWidth: 80 }
            Item { Layout.fillWidth: true }
            Switch {
                checked: SettingsController.autoReprocessAIEnabled
                onCheckedChanged: SettingsController.autoReprocessAIEnabled = checked
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: tipText.implicitHeight + 16
            radius: Theme.radius.sm
            color: Qt.rgba(Theme.colors.info.r, Theme.colors.info.g, Theme.colors.info.b, 0.1)
            
            Text {
                id: tipText
                anchors.fill: parent
                anchors.margins: 8
                text: qsTr("💡 关闭后，应用中途关闭时的处理任务不会自动恢复，您可以手动点击\"重新处理\"按钮进行处理")
                color: Theme.colors.info
                font.pixelSize: 11
                wrapMode: Text.Wrap
            }
        }
    }
}
```

## 五、国际化字符串

### 中文 (app_zh_CN.ts)
```xml
<message>
    <source>处理恢复</source>
    <translation>处理恢复</translation>
</message>
<message>
    <source>应用启动时自动恢复未完成的处理任务</source>
    <translation>应用启动时自动恢复未完成的处理任务</translation>
</message>
<message>
    <source>全部开启</source>
    <translation>全部开启</translation>
</message>
<message>
    <source>Shader 模式</source>
    <translation>Shader 模式</translation>
</message>
<message>
    <source>AI 推理模式</source>
    <translation>AI 推理模式</translation>
</message>
<message>
    <source>💡 关闭后，应用中途关闭时的处理任务不会自动恢复，您可以手动点击"重新处理"按钮进行处理</source>
    <translation>💡 关闭后，应用中途关闭时的处理任务不会自动恢复，您可以手动点击"重新处理"按钮进行处理</translation>
</message>
<message>
    <source>检测到上次应用异常退出</source>
    <translation>检测到上次应用异常退出</translation>
</message>
<message>
    <source>为确保稳定性，已自动关闭"自动重新处理"功能。您可以在设置中重新开启此功能。</source>
    <translation>为确保稳定性，已自动关闭"自动重新处理"功能。您可以在设置中重新开启此功能。</translation>
</message>
<message>
    <source>知道了</source>
    <translation>知道了</translation>
</message>
```

### 英文 (app_en_US.ts)
```xml
<message>
    <source>处理恢复</source>
    <translation>Processing Recovery</translation>
</message>
<message>
    <source>应用启动时自动恢复未完成的处理任务</source>
    <translation>Automatically resume unfinished processing tasks on startup</translation>
</message>
<message>
    <source>全部开启</source>
    <translation>Enable All</translation>
</message>
<message>
    <source>Shader 模式</source>
    <translation>Shader Mode</translation>
</message>
<message>
    <source>AI 推理模式</source>
    <translation>AI Inference Mode</translation>
</message>
<message>
    <source>💡 关闭后，应用中途关闭时的处理任务不会自动恢复，您可以手动点击"重新处理"按钮进行处理</source>
    <translation>💡 When disabled, interrupted tasks won't auto-resume. You can manually click "Reprocess" to continue.</translation>
</message>
<message>
    <source>检测到上次应用异常退出</source>
    <translation>Abnormal exit detected</translation>
</message>
<message>
    <source>为确保稳定性，已自动关闭"自动重新处理"功能。您可以在设置中重新开启此功能。</source>
    <translation>Auto-reprocess has been disabled for stability. You can re-enable it in Settings.</translation>
</message>
<message>
    <source>知道了</source>
    <translation>OK</translation>
</message>
```

## 六、测试要点

1. **设置持久化测试**
   - 修改设置后关闭应用
   - 重新打开应用验证设置项保存

2. **崩溃检测测试**
   - 模拟崩溃（强制结束进程）
   - 重新打开应用验证自动重新处理已关闭
   - 验证崩溃提示显示

3. **自动重新处理测试**
   - Shader 模式：添加文件 → 开始处理 → 中途关闭 → 重新打开验证
   - AI 模式：同上
   - 分别测试开启/关闭设置

4. **主开关联动测试**
   - 点击主开关，验证子开关同步
   - 分别操作子开关，验证主开关状态

5. **失败文件测试**
   - 确认失败文件不自动重试
   - 确认手动重新处理按钮正常工作

6. **主题切换测试**
   - 验证设置页面在深色/浅色主题下显示正常

7. **国际化测试**
   - 验证中英文切换后文本显示正确

## 七、风险评估

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| 设置迁移 | 已有用户升级后默认开启 | 默认值设为 true，保持现有行为 |
| 崩溃误判 | 正常退出被误判为崩溃 | 使用 QSettings 确保原子写入 |
| 性能影响 | 启动时额外检查 | 检查逻辑简单，影响可忽略 |

## 八、文件修改清单

1. `include/EnhanceVision/controllers/SettingsController.h`
2. `src/controllers/SettingsController.cpp`
3. `src/controllers/SessionController.cpp`
4. `src/app/Application.h`
5. `src/app/Application.cpp`
6. `qml/pages/SettingsPage.qml`
7. `qml/pages/MainPage.qml` (崩溃提示弹窗)
8. `resources/i18n/app_zh_CN.ts`
9. `resources/i18n/app_en_US.ts`
