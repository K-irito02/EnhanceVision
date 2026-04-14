# 安装升级后缓存统计显示修复笔记

## 概述

修复再次安装并选择不同应用数据目录后，设置页“缓存管理”下 AI/Shader 处理结果错误显示为 `0 B` 的问题。

创建日期：2026-04-14

---

## 变更概述

### 修改文件

| 文件路径 | 修改内容 |
|----------|----------|
| `packaging/installer/setup.nsi` | 修正旧数据目录探测顺序，避免升级意图写入错误旧目录 |
| `src/services/InstallMaintenanceService.cpp` | 为 `keep/migrate` 增加旧数据目录候选探测与兜底 |
| `tests/InstallMaintenanceServiceTest.cpp` | 补充错误旧路径回退场景测试 |
| `src/controllers/ProcessingController.cpp` | 清理高频 `qInfo` 并降低正常关闭日志级别 |
| `src/core/LifecycleSupervisor.cpp` | 将正常关闭请求从 `WARN` 降为信息级 |

---

## 设计结论

1. 安装器不能只依赖 `settings.ini` 中的 `customDataPath` 判断旧目录。
2. 首启维护服务必须具备独立纠错能力，不能完全信任安装器写入的历史意图。
3. 缓存管理、会话内容、恢复快照都必须跟随同一条“真实生效数据目录”链路。

---

## 验证

| 场景 | 结果 |
|------|------|
| `EnhanceVisionInstallMaintenanceServiceTest` 直跑 | 通过 |
| Release 主程序重新构建 | 通过 |
| NSIS 安装器重新编译 | 通过 |

---

## 额外整理

- 清理多处无诊断价值的 `qInfo/qDebug` 输出。
- 将正常关闭流程中的日志噪声从告警级别降级，避免日志误报。
