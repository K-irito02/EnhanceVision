# 主题系统优化笔记

## 概述

本次优化解决了主题切换功能中的架构问题、性能问题和代码可维护性问题，主要包括：消除状态同步冗余、移除双图标集依赖、修复硬编码颜色。

**创建日期**: 2026-03-28
**作者**: AI Assistant

---

## 一、变更概述

### 1. 修改文件

| 文件路径 | 修改内容 |
|----------|----------|
| `qml/pages/SettingsPage.qml` | 移除重复的 `SettingsController.theme` 调用 |
| `qml/components/ShaderParamsPanel.qml` | 替换硬编码颜色为语义化颜色变量 |
| `qml/styles/Colors.qml` | 重命名 `onPrimary`/`onDestructive` 为 `textOnPrimary`/`textOnDestructive` |
| `CMakeLists.txt` | 启用响应文件支持，移除 icons-dark 引用 |
| `resources/qml.qrc` | 移除 icons-dark 资源引用 |
| `resources/icons/*.svg` | 修改 stroke 颜色为白色以支持动态着色 |

### 2. 删除文件

| 文件路径 | 删除原因 |
|----------|----------|
| `resources/icons-dark/` | 移除双图标集，统一使用单套图标 + 动态着色 |

---

## 二、实现的功能特性

- ✅ 消除状态同步冗余：`Theme.setDark()` 内部已处理持久化
- ✅ 移除双图标集：减少 50% 图标文件（136 → 68）
- ✅ 图标动态着色：暗色主题下图标显示淡蓝色
- ✅ 修复硬编码颜色：使用 `Theme.colors.*` 语义化变量
- ✅ 解决 Windows 命令行长度限制：启用响应文件支持

---

## 三、技术实现细节

### 3.1 状态同步优化

**问题**：`Theme.setDark()` 内部已调用 `SettingsController.theme`，组件又重复调用导致冗余。

**解决方案**：
```qml
// 修改前
onClicked: { Theme.setDark(true); SettingsController.theme = "dark" }

// 修改后
onClicked: { Theme.setDark(true) }
```

### 3.2 图标着色机制

**问题**：SVG 文件使用固定黑色 `stroke="#000000"`，导致暗色主题下图标不可见。

**解决方案**：
1. 将 SVG stroke 颜色改为白色 `stroke="#FFFFFF"`
2. 使用 `ColoredIcon` 组件的 `MultiEffect` 进行动态着色
3. 暗色主题下图标自动显示为淡蓝色 `Theme.colors.icon: "#7AA2F7"`

### 3.3 Windows 命令行长度限制

**问题**：MSBuild 生成的命令行超过 Windows 限制（约 8191 字符）。

**解决方案**：
```cmake
# CMakeLists.txt
set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_OBJECTS ON)
set(CMAKE_C_USE_RESPONSE_FILE_FOR_OBJECTS ON)
set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_INCLUDES ON)
set(CMAKE_C_USE_RESPONSE_FILE_FOR_INCLUDES ON)
```

### 3.4 QML 属性命名冲突

**问题**：`onPrimary` 和 `onDestructive` 以 `on` 开头，QML 将其解释为信号处理器。

**解决方案**：
```qml
// 修改前
readonly property color onPrimary: "#FFFFFF"
readonly property color onDestructive: "#FFFFFF"

// 修改后
readonly property color textOnPrimary: "#FFFFFF"
readonly property color textOnDestructive: "#FFFFFF"
```

---

## 四、遇到的问题及解决方案

### 问题 1：构建错误 MSB8066

**现象**：`Cannot open options file specified with @`

**原因**：Windows 命令行长度限制

**解决方案**：启用 CMake 响应文件支持

### 问题 2：QML 运行时错误

**现象**：`Cannot assign a value to a signal (expecting a script to be run)`

**原因**：属性名以 `on` 开头被解释为信号处理器

**解决方案**：重命名属性，添加 `text` 前缀

### 问题 3：暗色主题图标不可见

**现象**：所有 SVG 图标在暗色主题下显示为黑色

**原因**：SVG 文件使用固定黑色 stroke

**解决方案**：修改 SVG stroke 为白色，使用 MultiEffect 动态着色

---

## 五、测试验证

### 测试场景

| 场景 | 预期结果 | 实际结果 |
|------|----------|----------|
| 主题切换 | 即时生效，无闪烁 | ✅ 通过 |
| 图标颜色 | 暗色主题下显示淡蓝色 | ✅ 通过 |
| 设置持久化 | 重启后保持主题设置 | ✅ 通过 |
| 构建编译 | 无错误无警告 | ✅ 通过 |

---

## 六、性能影响

| 指标 | 变更前 | 变更后 | 影响 |
|------|--------|--------|------|
| 图标文件数 | 136 个 | 68 个 | -50% |
| 图标切换闪烁 | 有 | 无 | 改善 |
| 包体积 | 较大 | 减少 | 改善 |

---

## 七、后续工作

- [ ] 继续消除其他文件中的硬编码颜色
- [ ] 添加主题切换动画效果
- [ ] 支持自定义主题色
