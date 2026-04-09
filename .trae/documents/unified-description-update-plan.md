# 计划：统一修改产品描述文字

## 任务概述
将"高性能图像与视频增强工具"更改为"图像处理与AI推理画质增强工具"，并同步更新所有相关文件，确保全项目描述统一一致。

## 需要修改的文件清单

### 1. QML 源码（About 对话框显示文本）
- **文件**: [SettingsPage.qml](qml/pages/SettingsPage.qml) 第 1852 行
- **修改**: `qsTr("高性能图像与视频增强工具")` → `qsTr("图像处理与AI推理画质增强工具")`

### 2. 国际化翻译文件（中文）
- **文件**: [app_zh_CN.ts](resources/i18n/app_zh_CN.ts) 第 3376-3377 行
- **修改**:
  - `<source>高性能图像与视频增强工具</source>` → `<source>图像处理与AI推理画质增强工具</source>`
  - `<translation>高性能图像与视频增强工具</translation>` → `<translation>图像处理与AI推理画质增强工具</translation>`

### 3. 国际化翻译文件（英文）
- **文件**: [app_en_US.ts](resources/i18n/app_en_US.ts) 第 3376-3377 行
- **修改**:
  - `<source>图像处理与AI推理画质增强工具</source>` （source 跟随中文）
  - `<translation>High-performance Image and Video Enhancement Tool</translation>` → **需确定英文翻译**

### 4. README 英文版
- **文件**: [README.md](README.md) 第 5 行
- **当前**: `A desktop image and video enhancement tool built with **Qt 6.10.2 + QML**...`
- **修改**: 更新为与新描述一致的英文表述

### 5. README 中文版
- **文件**: [README_CN.md](README_CN.md) 第 5 行
- **当前**: `基于 **Qt 6.10.2 + QML** 的桌面端图像与视频画质增强工具`
- **修改**: 更新为"基于 **Qt 6.10.2 + QML** 的桌面端图像处理与AI推理画质增强工具"

### 6. CHANGELOG.md
- **文件**: [CHANGELOG.md](CHANGELOG.md) 第 159 行
- **当前**: `基于Qt 6.10.2 + QML的桌面端图像视频增强工具`
- **修改**: 更新为新描述

### 7. MainWindow.cpp 窗口标题（建议同步）
- **文件**: [MainWindow.cpp](src/app/MainWindow.cpp) 第 14 行
- **当前**: `setWindowTitle(tr("EnhanceVision - 图像处理与增强工具"));`
- **建议**: 同步更新为 `setWindowTitle(tr("EnhanceVision - 图像处理与AI推理画质增强工具"));`

## 建议补充完善的内容

### A. 英文翻译对齐
新描述"图像处理与AI推理画质增强工具"的英文翻译需要确定，建议：
- **方案一**: `Image Processing and AI Inference Quality Enhancement Tool`（直译，准确但较长）
- **方案二**: `Image Processor with AI-Powered Quality Enhancement`（更简洁自然）

### B. MainWindow.cpp 标题一致性
当前窗口标题是 `"EnhanceVision - 图像处理与增强工具"`，与 About 对话框中的描述不一致，建议一并同步。

### C. CHANGELOG 历史记录
CHANGELOG 中第 159 行的 v0.1.0 初始描述属于历史记录，通常不建议修改历史 changelog。但如果追求完全一致，可以更新。

## 执行步骤

1. 修改 `qml/pages/SettingsPage.qml` — About 对话框中的描述文字
2. 修改 `resources/i18n/app_zh_CN.ts` — 中文翻译 source 和 translation
3. 修改 `resources/i18n/app_en_US.ts` — 英文翻译 source 和 translation
4. 修改 `README.md` — 英文 README 开篇描述
5. 修改 `README_CN.md` — 中文 README 开篇描述
6. 修改 `src/app/MainWindow.cpp` — 窗口标题（可选，建议同步）
7. 修改 `CHANGELOG.md` — v0.1.0 初始条目描述（可选）
8. 执行构建验证（qt-build-and-fix）
