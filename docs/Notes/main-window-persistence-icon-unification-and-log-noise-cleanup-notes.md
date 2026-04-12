# 主窗口持久化、图标统一与运行日志降噪笔记

## 概述

本次变更同时处理了三类长期维护问题：

1. 主应用程序窗口的几何状态无法在重启后恢复
2. 主题图标资源存在重复注册与路径分裂，部分图标在亮暗主题下无法稳定显示
3. 运行日志中 `INFO/DEBUG` 输出过多，影响排查真正的 `WARN/ERROR`

**创建日期**: 2026-04-12

---

## 变更内容

| 文件路径 | 修改内容 |
|----------|----------|
| `src/app/Application.cpp` | 接入 `UIStateController` 的窗口几何恢复与保存流程，支持主窗口重启恢复，保存正常态几何与最大化状态 |
| `include/EnhanceVision/app/Application.h` | 增加窗口布局恢复/保存相关成员声明 |
| `src/controllers/UIStateController.cpp` | 扩展 `windows.<key>` 持久化结构，统一管理窗口布局状态 |
| `include/EnhanceVision/controllers/UIStateController.h` | 新增 `WindowLayout` 数据结构与对应接口 |
| `CMakeLists.txt` | 统一收敛 Theme SVG 图标资源入口 |
| `resources/qml.qrc` | 移除 Theme SVG 图标重复注册，仅保留应用位图资源 |
| `qml/styles/Theme.qml` | 统一图标取用入口，增加空名称与兜底处理 |
| `qml/controls/ColoredIcon.qml` | 增加资源加载失败告警与统一兜底图标 |
| `src/main.cpp` | 过滤 `QtInfoMsg` / `QtDebugMsg`，删除启动过程中的冗余信息级输出 |
| `.agents/rules/02-architecture.md` | 补充分层持久化边界与窗口恢复约束 |
| `.agents/rules/04-qml-standards.md` | 补充 Theme 图标统一入口与位图资源边界 |
| `.agents/rules/10-skills-and-tools.md` | 补充日志治理与文档同步要求 |
| `README.md` / `README_CN.md` | 补充窗口恢复与图标统一说明 |
| `QML-README.md` | 补充图标与窗口状态约定 |

---

## 设计要点

### 1. 窗口状态分层

- `SettingsController` 继续只处理用户偏好与业务设置
- `UIStateController` 负责重启后应恢复的 UI 状态
- 窗口布局统一存放在 `windows.<key>` 下，避免每个窗口各自发明格式
- 恢复前必须做屏幕可见性校验，防止窗口几何落到屏幕外

### 2. 图标统一链路

- Theme SVG 图标统一从 `Theme.icon()` 进入
- 着色与主题切换统一交给 `ColoredIcon`
- 品牌 Logo、固定颜色位图继续使用 `Image`
- 新增图标必须先进入统一资源入口，再由页面消费，避免路径分裂

### 3. 日志治理

- 运行日志默认只保留 warning/error/fatal
- `INFO/DEBUG` 仅在临时排障时保留，不作为常驻输出
- 第三方库构建警告需区分来源，避免误判为项目自身问题

---

## 日志审查结论

- 应用运行日志中未发现新的业务错误
- 之前的 `INFO` 输出属于可移除噪声，已在消息处理器中统一过滤
- 构建日志中的残余警告来自第三方库源码，不属于当前变更范围

---

## 验证情况

| 验证项 | 结果 |
|-------|------|
| Release 构建 | ✅ 通过 |
| 单元测试 | ✅ 通过 |
| 运行日志审查 | ✅ 已完成 |
| 文档同步 | ✅ 已完成 |

