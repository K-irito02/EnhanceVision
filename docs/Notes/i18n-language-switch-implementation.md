# 国际化语言切换功能完善

**日期**: 2026-03-21
**作者**: AI Assistant

---

## 一、完成的功能

### 1. 创建的新组件

无新增组件，主要是对现有国际化系统的完善。

### 2. 修改的文件

| 文件 | 修改内容 |
|------|---------|
| `include/EnhanceVision/app/Application.h` | 添加 QTranslator 成员变量和方法声明 |
| `src/app/Application.cpp` | 实现翻译器初始化、动态切换和 QML 界面刷新 |
| `qml/styles/Theme.qml` | 与 SettingsController 同步语言设置 |
| `resources/i18n/app_en_US.ts` | 补充 337 条英文翻译 |
| `resources/i18n/app_zh_CN.ts` | 补充 337 条中文翻译 |

### 3. 实现的功能特性

- ✅ 应用启动时根据设置加载对应语言
- ✅ 点击语言切换按钮后界面立即切换语言
- ✅ 所有界面文本（按钮、标签、提示、对话框）正确显示翻译
- ✅ 语言设置持久化，重启后保持选择
- ✅ 中英文切换流畅

---

## 二、遇到的问题及解决方案

### 问题 1：翻译文件加载成功但界面没有变化

**现象**：日志显示翻译文件加载成功，但点击语言切换按钮后界面文本没有变化。

**原因**：
1. 翻译文件的 context 名称不正确（使用了 `QML` 而不是实际的文件名如 `TitleBar`、`SettingsPage`）
2. QML 中 `qsTr()` 的 context 是文件名，需要使用 lupdate 工具正确提取

**解决方案**：
使用 Qt 的 lupdate 工具正确提取翻译字符串：
```powershell
& "E:\Qt\6.10.2\msvc2022_64\bin\lupdate.exe" "qml" "src" -ts "resources/i18n/app_zh_CN.ts" "resources/i18n/app_en_US.ts" -no-obsolete
```

### 问题 2：翻译文件资源路径错误

**现象**：运行时提示 `Failed to load translation file: ":/qt/qml/EnhanceVision/resources/i18n/app_zh_CN.qm"`

**原因**：CMake 的 `qt_add_translations` 生成的资源路径与代码中使用的路径不一致。

**解决方案**：
通过检查构建输出找到正确的资源路径 `:/i18n/app_*.qm`，并修改代码中的路径。

### 问题 3：SettingsController.setLanguage 不可调用

**现象**：QML 中调用 `SettingsController.setLanguage(newLang)` 报错 `Property 'setLanguage' of object is not a function`

**原因**：Q_PROPERTY 的 WRITE 方法应该通过属性赋值来调用，而不是直接调用方法。

**解决方案**：
修改 Theme.qml 中的调用方式：
```qml
// 错误方式
SettingsController.setLanguage(newLang)

// 正确方式
SettingsController.language = newLang
```

---

## 三、技术实现细节

### QTranslator 动态切换流程

```
用户点击语言切换按钮
    ↓
Theme.toggleLanguage() / Theme.setLanguage()
    ↓
SettingsController.language = newLang (属性赋值)
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

### 关键代码

**Application.cpp 翻译器初始化**：
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
    if (m_translator) {
        QCoreApplication::removeTranslator(m_translator.data());
    }
    if (m_qtTranslator) {
        QCoreApplication::removeTranslator(m_qtTranslator.data());
    }

    m_translator.reset(new QTranslator);
    QString qmFile = QString(":/i18n/app_%1.qm").arg(language);
    if (m_translator->load(qmFile)) {
        QCoreApplication::installTranslator(m_translator.data());
    }

    m_qtTranslator.reset(new QTranslator);
    QString qtQmFile = QString("qtbase_%1").arg(language);
    QString qtTranslationsPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    if (m_qtTranslator->load(qtQmFile, qtTranslationsPath)) {
        QCoreApplication::installTranslator(m_qtTranslator.data());
    }

    return true;
}

void Application::onLanguageChanged()
{
    QString language = SettingsController::instance()->language();
    switchTranslator(language);

    if (m_mainWidget && m_mainWidget->engine()) {
        m_mainWidget->engine()->retranslate();
    }
}
```

**Theme.qml 语言同步**：
```qml
import EnhanceVision.Controllers

function toggleLanguage() {
    var newLang = (language === "zh_CN") ? "en_US" : "zh_CN"
    language = newLang
    SettingsController.language = newLang
}

Component.onCompleted: {
    language = SettingsController.language
}
```

---

## 四、使用方式

### 标题栏语言按钮
点击地球图标切换语言。

### 设置页面
通过下拉框选择语言（简体中文 / English）。

---

## 五、翻译文件维护

### 更新翻译字符串

当新增或修改 UI 文本时，需要更新翻译文件：

```powershell
# 提取新的翻译字符串
& "E:\Qt\6.10.2\msvc2022_64\bin\lupdate.exe" "qml" "src" -ts "resources/i18n/app_zh_CN.ts" "resources/i18n/app_en_US.ts" -no-obsolete

# 重新构建生成 .qm 文件
cmake --build build/msvc2022/Release --config Release
```

### 注意事项

1. **XML 特殊字符转义**：翻译文本中的 `&` 需要转义为 `&amp;`
2. **Context 名称**：QML 中 `qsTr()` 的 context 是文件名（如 `TitleBar`），不是 `QML`
3. **资源路径**：编译后的 `.qm` 文件路径为 `:/i18n/app_*.qm`
