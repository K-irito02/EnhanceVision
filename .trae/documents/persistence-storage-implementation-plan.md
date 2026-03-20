# EnhanceVision 持久化存储机制分析与实现计划

## 一、当前实现状态分析

### 1. 已实现的持久化功能

#### 1.1 全局配置持久化（SettingsController + QSettings）

| 配置项 | 存储键 | 默认值 | 状态 | 设置页面 |
|--------|--------|--------|------|----------|
| 主题设置 | `appearance/theme` | "dark" | ✅ 已实现 | ✅ 已有UI |
| 语言设置 | `appearance/language` | "zh_CN" | ✅ 已实现 | ✅ 已有UI |
| 侧边栏展开 | `sidebar/expanded` | true | ✅ 已实现 | ❌ 无UI |
| 最大并发任务 | `performance/maxConcurrent` | 2 | ✅ 已实现 | ⚠️ UI未连接 |
| 默认保存路径 | `behavior/defaultSavePath` | Pictures/EnhanceVision | ✅ 已实现 | ❌ 无UI |
| 自动保存结果 | `behavior/autoSave` | false | ✅ 已实现 | ❌ 无UI |

**存储技术**：QSettings + INI 格式  
**存储路径**：`C:\Users\<用户名>\AppData\Local\EnhanceVision\settings.ini`

### 2. 未实现的持久化功能

#### 2.1 会话数据持久化 ❌

**当前状态**：
- `SessionController::saveSessions()` 和 `loadSessions()` 为空实现
- 会话数据仅存在于内存中，程序关闭后丢失

#### 2.2 音量设置持久化 ❌

**当前状态**：
- SettingsController 中无音量属性
- 媒体播放器音量仅在运行时存在

---

## 二、实现计划

### 任务 1：实现会话数据持久化

#### 1.1 存储设计

**存储格式**：JSON 文件  
**存储路径**：`AppDataLocation/EnhanceVision/sessions.json`

#### 1.2 实现步骤

**步骤 1.1**：修改 `SessionController.h` - 添加 JSON 序列化方法声明

**步骤 1.2**：实现 `SessionController.cpp` - 实现会话序列化/反序列化

**步骤 1.3**：修改 `Application.cpp` - 添加启动加载和退出保存

---

### 任务 2：实现音量设置持久化

#### 2.1 存储设计

**存储位置**：QSettings (`audio/volume`)  
**数据范围**：0-100（整数）  
**默认值**：80

#### 2.2 实现步骤

**步骤 2.1**：修改 `SettingsController.h` - 添加 volume 属性

**步骤 2.2**：修改 `SettingsController.cpp` - 实现音量持久化

---

### 任务 3：完善设置页面 UI

#### 3.1 当前设置页面分析

当前 `SettingsPage.qml` 已有：
- ✅ 主题切换（暗色/亮色）
- ✅ 语言切换（中文/英文）
- ⚠️ 最大并发任务（UI 存在但未连接到 SettingsController）
- ⚠️ 清除缓存按钮（UI 存在但未实现功能）

缺失的配置项 UI：
- ❌ 默认保存路径配置
- ❌ 自动保存结果开关
- ❌ 音量设置

#### 3.2 设置页面改进计划

**新增配置分组**：

1. **外观设置**（已有，完善）
   - 主题切换 ✅
   - 语言切换 ✅

2. **行为设置**（新增）
   - 默认保存路径（带文件夹选择器）
   - 自动保存结果开关

3. **音频设置**（新增）
   - 音量滑块（0-100）

4. **性能设置**（已有，完善）
   - 最大并发任务（连接到 SettingsController）
   - 清除缓存功能实现

---

## 三、文件修改清单

### C++ 文件

| 文件 | 修改内容 |
|------|----------|
| `include/EnhanceVision/controllers/SessionController.h` | 添加 JSON 序列化方法声明 |
| `src/controllers/SessionController.cpp` | 实现 `saveSessions()` 和 `loadSessions()` |
| `include/EnhanceVision/controllers/SettingsController.h` | 添加 `volume` 属性 |
| `src/controllers/SettingsController.cpp` | 实现音量持久化 |
| `src/app/Application.cpp` | 添加会话加载/保存调用 |

### QML 文件

| 文件 | 修改内容 |
|------|----------|
| `qml/pages/SettingsPage.qml` | 完善设置页面 UI，添加行为设置、音频设置等 |
| `qml/components/EmbeddedMediaViewer.qml` | 应用音量设置 |
| `qml/components/MediaViewerWindow.qml` | 应用音量设置 |

---

## 四、详细实现步骤

### 步骤 1：会话数据持久化

#### 1.1 SessionController.h 添加内容

```cpp
private:
    QString sessionsFilePath() const;
    QJsonObject sessionToJson(const Session& session) const;
    Session jsonToSession(const QJsonObject& json) const;
    QJsonObject messageToJson(const Message& message) const;
    Message jsonToMessage(const QJsonObject& json) const;
    QJsonObject mediaFileToJson(const MediaFile& file) const;
    MediaFile jsonToMediaFile(const QJsonObject& json) const;
    QJsonObject shaderParamsToJson(const ShaderParams& params) const;
    ShaderParams jsonToShaderParams(const QJsonObject& json) const;
```

#### 1.2 SessionController.cpp 实现内容

- `saveSessions()`: 序列化所有会话到 JSON 文件
- `loadSessions()`: 从 JSON 文件加载会话
- 各辅助方法：实现数据结构与 JSON 的互转

#### 1.3 Application.cpp 修改

```cpp
void Application::initialize() {
    // ... 现有代码 ...
    m_sessionController->loadSessions();  // 添加：启动时加载会话
}

Application::~Application() {
    m_sessionController->saveSessions();  // 添加：退出时保存会话
    // ... 现有代码 ...
}
```

---

### 步骤 2：音量设置持久化

#### 2.1 SettingsController.h 添加内容

```cpp
Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)

public:
    int volume() const;
    void setVolume(int volume);

signals:
    void volumeChanged();

private:
    int m_volume;
```

#### 2.2 SettingsController.cpp 添加内容

- 构造函数：初始化 `m_volume = 80`
- `saveSettings()`: 添加 `m_settings->setValue("audio/volume", m_volume);`
- `loadSettings()`: 添加 `m_volume = m_settings->value("audio/volume", 80).toInt();`
- `resetToDefaults()`: 添加 `m_volume = 80;`

---

### 步骤 3：完善设置页面

#### 3.1 SettingsPage.qml 新增内容

**行为设置分组**：
```qml
// ========== 行为设置 ==========
Rectangle {
    ColumnLayout {
        // 默认保存路径
        RowLayout {
            Text { text: qsTr("默认保存路径") }
            TextField { 
                text: SettingsController.defaultSavePath
                readOnly: true
            }
            Button { 
                text: qsTr("浏览...")
                onClicked: folderDialog.open()
            }
        }
        
        // 自动保存结果
        RowLayout {
            Text { text: qsTr("自动保存结果") }
            Switch { 
                checked: SettingsController.autoSaveResult
                onCheckedChanged: SettingsController.autoSaveResult = checked
            }
        }
    }
}
```

**音频设置分组**：
```qml
// ========== 音频设置 ==========
Rectangle {
    ColumnLayout {
        RowLayout {
            Text { text: qsTr("默认音量") }
            Slider {
                from: 0
                to: 100
                value: SettingsController.volume
                onValueChanged: SettingsController.volume = value
            }
            Text { text: SettingsController.volume + "%" }
        }
    }
}
```

**完善性能设置**：
```qml
// 最大并发任务 - 连接到 SettingsController
ComboBox { 
    model: ["1", "2", "3", "4"]
    currentIndex: SettingsController.maxConcurrentTasks - 1
    onCurrentIndexChanged: SettingsController.maxConcurrentTasks = currentIndex + 1
}

// 清除缓存 - 实现功能
Button { 
    text: qsTr("清除缩略图缓存")
    onClicked: {
        // 调用 C++ 清除缓存方法
    }
}
```

---

## 五、存储路径汇总

| 数据类型 | 存储路径 | 格式 |
|----------|----------|------|
| 全局配置 | `%LocalAppData%/EnhanceVision/settings.ini` | INI |
| 会话数据 | `%LocalAppData%/EnhanceVision/sessions.json` | JSON |

---

## 六、可靠性和安全性评估

### 6.1 可靠性措施

1. **原子写入**：JSON 文件使用临时文件 + 重命名方式写入
2. **备份机制**：保存前备份旧文件
3. **错误处理**：加载失败时使用默认值
4. **版本控制**：JSON 文件包含版本号

### 6.2 安全性措施

1. **路径安全**：使用 `QStandardPaths` 获取标准存储路径
2. **权限控制**：存储在用户目录
3. **数据验证**：加载 JSON 时验证数据完整性

---

## 七、实现优先级

1. **高优先级**：会话数据持久化（用户体验核心功能）
2. **高优先级**：完善设置页面 UI（用户可配置全局设置）
3. **中优先级**：音量设置持久化（用户体验增强功能）

---

## 八、测试验证

1. 创建多个会话，添加消息，关闭程序后重新打开验证数据恢复
2. 在设置页面修改各项配置，关闭程序后重新打开验证设置保持
3. 修改音量设置，关闭程序后重新打开验证设置保持
4. 删除存储文件，验证程序能正常启动并使用默认值
