# 国际化语言切换功能完善计划

## 问题分析

### 当前状态
| 组件 | 状态 | 说明 |
|------|------|------|
| 翻译文件 | ⚠️ 不完整 | `app_zh_CN.ts` 和 `app_en_US.ts` 存在，但缺少大量新增文本 |
| CMake 配置 | ✅ 已配置 | `qt_add_translations` 已配置 |
| QML qsTr() | ✅ 已使用 | 100+ 处使用 `qsTr()` |
| C++ tr() | ✅ 已使用 | 11 个文件使用 `tr()` |
| 语言切换 UI | ✅ 已实现 | TitleBar 和 SettingsPage 有切换按钮 |
| Theme.qml | ✅ 已实现 | 有 `language` 属性和切换函数 |
| SettingsController | ✅ 已实现 | 有 `language` 属性，支持持久化 |
| **QTranslator** | ❌ 未实现 | **核心问题：没有翻译器加载和切换逻辑** |

### 核心问题
1. **QTranslator 未初始化**: 应用启动时未加载翻译器
2. **动态切换未实现**: 语言切换后不会重新加载翻译
3. **翻译文件不完整**: 缺少大量新增文本的翻译

---

## 实现方案

### 步骤 1: 修改 Application.h - 添加翻译器支持

**文件**: `include/EnhanceVision/app/Application.h`

添加内容：
```cpp
#include <QTranslator>
#include <QSharedPointer>

// 新增成员
QSharedPointer<QTranslator> m_translator;
QSharedPointer<QTranslator> m_qtTranslator;

// 新增方法
void setupTranslator();
bool switchTranslator(const QString& language);
```

### 步骤 2: 修改 Application.cpp - 实现翻译器逻辑

**文件**: `src/app/Application.cpp`

实现内容：
1. `setupTranslator()` - 初始化翻译器，根据 SettingsController 加载默认语言
2. `switchTranslator(const QString& language)` - 动态切换翻译器
3. 连接 SettingsController::languageChanged 信号

关键代码逻辑：
```cpp
void Application::setupTranslator()
{
    QString language = SettingsController::instance()->language();
    switchTranslator(language);
    
    connect(SettingsController::instance(), &SettingsController::languageChanged,
            this, &Application::onLanguageChanged);
}

bool Application::switchTranslator(const QString& language)
{
    // 移除旧翻译器
    if (m_translator) {
        QCoreApplication::removeTranslator(m_translator.data());
    }
    if (m_qtTranslator) {
        QCoreApplication::removeTranslator(m_qtTranslator.data());
    }
    
    // 加载应用翻译
    m_translator.reset(new QTranslator);
    QString qmFile = QString(":/qt/qml/EnhanceVision/resources/i18n/app_%1.qm").arg(language);
    if (m_translator->load(qmFile)) {
        QCoreApplication::installTranslator(m_translator.data());
    }
    
    // 加载 Qt 基础翻译（对话框按钮等）
    m_qtTranslator.reset(new QTranslator);
    QString qtQmFile = QString("qtbase_%1").arg(language);
    if (m_qtTranslator->load(qtQmFile, QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        QCoreApplication::installTranslator(m_qtTranslator.data());
    }
    
    return true;
}

void Application::onLanguageChanged()
{
    QString language = SettingsController::instance()->language();
    switchTranslator(language);
    
    // 刷新 QML 界面
    emit m_mainWidget->engine()->retranslate();
}
```

### 步骤 3: 修改 Theme.qml - 同步 SettingsController

**文件**: `qml/styles/Theme.qml`

修改内容：
1. 初始化时从 SettingsController 读取语言设置
2. 语言切换时同步到 SettingsController

```qml
import EnhanceVision.Controllers

Component.onCompleted: {
    language = SettingsController.language
}

function toggleLanguage() {
    var newLang = (language === "zh_CN") ? "en_US" : "zh_CN"
    language = newLang
    SettingsController.setLanguage(newLang)
}

function setLanguage(lang) {
    language = lang
    SettingsController.setLanguage(lang)
}
```

### 步骤 4: 更新翻译文件

**操作步骤**：
1. 运行 CMake 构建生成 `.qm` 文件
2. 使用 Qt Linguist 或手动编辑 `.ts` 文件补充缺失翻译

**需要翻译的主要文本**（基于 qsTr 搜索结果）：

| 类别 | 示例文本 |
|------|---------|
| 标题栏 | 新建会话、展开/收缩会话栏、浏览模式、快捷键说明、切换到亮色/暗色主题、最小化、最大化、还原、关闭 |
| 侧边栏 | 批量操作、取消批量、全选、取消全选、新建会话、清空选中会话、删除选中会话、清空会话、删除会话 |
| 文件列表 | 文件列表、添加文件、选择媒体文件、所有支持的文件、图片文件、视频文件、所有文件、拖拽文件到此处或点击添加 |
| 控制面板 | Shader、AI、展开控制面板、收缩控制面板、AI 模型、模型说明、通用场景超分辨率、动漫插画专用优化、通用图像降噪 |
| 设置页面 | 设置、返回、外观、主题、暗色、亮色、语言、性能、最大并发任务、缓存管理、清除缩略图缓存、关于、高性能图像与视频增强工具 |
| 媒体查看器 | 媒体查看器、独立窗口、嵌入、全屏、退出全屏、播放/暂停、静音、源件、成果、无媒体文件 |
| 消息列表 | 暂无处理任务、添加文件并点击发送开始处理 |
| 主页面 | 释放以添加文件、开始新的处理任务、添加文件并选择处理模式来开始、AI推理 |
| 预设风格 | 柔和、鲜艳、明亮、暗调、高对比、低对比、清透、冷调、暖调、青橙、粉嫩、冰蓝、金秋、森林、复古... |
| 分类标签 | 全部、基础、色调、复古、电影、黑白、场景、艺术、社交 |
| 对话框 | 确定、取消 |
| 通知 | 文件格式不支持、文件过大、处理失败、磁盘空间不足、模型文件损坏、网络连接失败、队列已满、文件不存在、无权限读取、GPU内存不足、处理超时 |

### 步骤 5: 修改 CMakeLists.txt - 确保翻译资源正确打包

**文件**: `CMakeLists.txt`

检查 `qt_add_translations` 配置：
```cmake
qt_add_translations(EnhanceVision
    TS_FILES ${TS_FILES}
    LUPDATE_OPTIONS -no-obsolete
    LRELEASE_OPTIONS -compress
    RESOURCE_PREFIX "/qt/qml/EnhanceVision"
)
```

### 步骤 6: 构建和测试

1. 运行 CMake 配置和构建
2. 检查 `.qm` 文件是否生成
3. 测试语言切换功能

---

## 文件修改清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `include/EnhanceVision/app/Application.h` | 修改 | 添加翻译器成员和方法声明 |
| `src/app/Application.cpp` | 修改 | 实现翻译器初始化和切换逻辑 |
| `qml/styles/Theme.qml` | 修改 | 同步 SettingsController 语言设置 |
| `resources/i18n/app_en_US.ts` | 修改 | 补充缺失的英文翻译 |
| `resources/i18n/app_zh_CN.ts` | 修改 | 补充缺失的中文翻译 |

---

## 技术要点

### QTranslator 动态切换流程
```
用户点击语言切换按钮
    ↓
Theme.toggleLanguage() / Theme.setLanguage()
    ↓
SettingsController.setLanguage()
    ↓
SettingsController::languageChanged 信号
    ↓
Application::onLanguageChanged()
    ↓
Application::switchTranslator()
    ↓
QCoreApplication::removeTranslator() + installTranslator()
    ↓
QQmlEngine::retranslate() 刷新 QML 界面
```

### 翻译文件路径
- 编译前: `resources/i18n/app_*.ts`
- 编译后: 嵌入资源 `:/qt/qml/EnhanceVision/resources/i18n/app_*.qm`

### 注意事项
1. **QML 刷新**: 使用 `QQmlEngine::retranslate()` 刷新所有 qsTr() 绑定
2. **信号连接**: 确保 SettingsController 和 Application 的信号正确连接
3. **资源路径**: 确认 `.qm` 文件的资源路径正确
4. **Qt 基础翻译**: 加载 `qtbase_*.qm` 以翻译 Qt 内置文本

---

## 预期结果

1. ✅ 应用启动时根据设置加载对应语言
2. ✅ 点击语言切换按钮后界面立即切换语言
3. ✅ 所有界面文本（按钮、标签、提示、对话框）正确显示翻译
4. ✅ 语言设置持久化，重启后保持选择
5. ✅ 中英文切换流畅，无遗漏文本
