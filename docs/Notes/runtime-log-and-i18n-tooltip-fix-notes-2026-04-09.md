# 运行日志治理与国际化/Tooltip修复记录

## 概述

本次工作聚焦三个目标：

1. 清理 `runtime_output.log` 中可避免的持续性告警
2. 修复消息卡片提示关闭状态在重启后回弹的问题
3. 优化 CPU/GPU 子模式 Tooltip 的完整显示与中英文布局

**创建日期**: 2026-04-09

---

## 变更文件

| 文件路径 | 修改内容 |
|----------|----------|
| `src/app/Application.cpp` | Qt 翻译加载增加候选名与路径回退；英文环境跳过 Qt 基础翻译加载；删除冗余翻译成功日志与帧探针附加日志 |
| `src/controllers/SessionController.cpp` | 提示关闭状态变更后改为 `saveSessionsImmediately()`，避免重启前未落盘导致提示回弹 |
| `qml/controls/RichTooltip.qml` | 行布局改为标签列+值列自适应，移除导致截断的占位方式，默认支持多行完整展示 |
| `qml/components/AIParamsPanel.qml` | CPU/GPU Tooltip 中英文分宽度与间距自适应，关闭行数截断，保证信息完整显示 |
| `.agents/rules/06-i18n.md` | 补充 Qt 基础翻译加载策略与国际化日志约束 |
| `docs/Learning/rules/06-i18n-rules.md` | 同步学习规则：翻译回退策略与日志约束 |
| `README.md` | 补充日志与国际化排障说明，修正规则目录路径 |

---

## 关键设计决策

1. Qt 翻译加载采用“候选名 + 双路径回退”：
   - 候选名：`qtbase_xx_YY` -> `qt_xx_YY` -> `qtbase_xx` -> `qt_xx`
   - 路径：`applicationDirPath()/translations` -> `QLibraryInfo::TranslationsPath`
2. 英文语言默认跳过 Qt 基础翻译加载，避免无意义 `WARN`。
3. 消息卡片提示关闭属于用户显式操作，采用立即持久化而非防抖保存。
4. Tooltip 展示策略从“固定比例+截断”调整为“可换行完整展示优先”。

---

## 日志审查结论

- 本轮主要持续性告警为：Qt 基础翻译加载失败（`qtbase_en_US`）。
- 已通过加载策略修复并降低日志噪声。
- 其余少量告警（如系统层面剪贴板重试）不属于本项目业务逻辑缺陷。

---

## 验证情况

| 验证项 | 结果 |
|-------|------|
| Release 构建（首次） | ✅ 通过 |
| Release 构建（后续） | ⚠️ 受已运行 `EnhanceVision.exe` 文件锁影响，链接阶段失败 |
| 代码静态审查 | ✅ 通过（无新增明显逻辑冲突） |

> 说明：后续一次构建失败由可执行文件占用导致，不是编译错误。

---

## 第二轮：Tooltip 布局精细化优化（2026-04-09）

### 概述

针对用户反馈的两个 Tooltip 显示问题进行布局优化：

1. **AI 模型性能提示卡**：预测时间数值右对齐，提升数据可读性
2. **CPU/GPU 子模式提示卡**：布局紧凑化，控制卡片宽度，中英文自适应

---

### 变更文件

| 文件路径 | 修改内容 |
|----------|----------|
| `qml/controls/RichTooltip.qml` | 新增 `valueHorizontalAlignment` 属性（默认 `Text.AlignLeft`），支持调用方自定义值文本对齐方式 |
| `qml/components/AIModelPanel.qml` | `perfTooltip` 设置 `valueHorizontalAlignment: Text.AlignRight`，预测时间右对齐显示 |
| `qml/components/AIParamsPanel.qml` | CPU/GPU Tooltip 布局参数紧凑化（maxWidth、rowLeftMargin、rowSpacing、labelMinWidth 等全面缩小） |

---

### 具体变更详情

#### RichTooltip.qml — 新增属性

```qml
property int valueHorizontalAlignment: Text.AlignLeft  // 第32行新增
```

value Text 的 `horizontalAlignment` 从硬编码 `Text.AlignLeft` 改为绑定到该属性。

#### AIModelPanel.qml — 性能提示右对齐

```qml
RichTooltip {
    id: perfTooltip
    valueHorizontalAlignment: Text.AlignRight  // 第223行新增
}
```

效果：GPU 图片 `1.4s`、GPU 视频 `2.2min` 等时间值在卡片内右对齐。

#### AIParamsPanel.qml — CPU/GPU Tooltip 紧凑化

| 参数 | 修改前 (EN / ZH) | 修改后 (EN / ZH) | 说明 |
|------|-------------------|-------------------|------|
| `maxWidth` | 560 / 440 | **380 / 340** | 卡片宽度缩小约 32%/23% |
| `rowLeftMargin` | 14 / 20 | **8 / 10** | 左侧标签左移 |
| `rowRightMargin` | 12 / 14 | **8 / 10** | 右侧收窄 |
| `rowSpacing` | 12 / 10 | **6 / 6** | 标签与描述间距减半 |
| `labelMinWidth` | 78 / 52 | **56 / 44** | 标签区域收窄 |
| `labelMaxRatio` | 0.28 / 0.20 | **0.22 / 0.18** | 标签占比减小 |
| `valueMaxRatio` | 0.70 / 0.74 | **0.76 / 0.78** | 描述区占比增大 |

保持 `wrapValueText: true` + `valueMaximumLineCount: 0` 不变，长描述文本自动多行换行。

---

### 设计决策

1. **属性扩展优于硬编码**：通过新增 `valueHorizontalAlignment` 属性实现灵活对齐，而非创建新组件或条件分支
2. **紧凑化策略**：同时缩小宽度和间距，避免单一调整导致的不平衡
3. **中英文差异化参数**：保留语言判断分支，英文标签更长需要更多空间
4. **向后兼容**：新属性默认值 `Text.AlignLeft` 保持原有行为不变

---

### 验证情况

| 验证项 | 结果 |
|-------|------|
| CMake 配置 | ✅ 通过 |
| Release 构建 | ✅ 通过（exit code 0） |
| 应用程序启动 | ✅ 正常运行 |
| 构建日志警告 | ⚠️ 仅 1 条标准 MSBuild 环境提示（VCINSTALLDIR），不影响运行 |
| 代码静态审查 | ✅ 无冗余调试信息 |

