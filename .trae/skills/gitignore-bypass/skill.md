---
name: "gitignore-bypass"
description: GitIgnore 临时绕过技能 - 自动注释和恢复 .gitignore 规则
---

# GitIgnore 临时绕过技能

## 职责边界

- **单一职责**：临时禁用 .gitignore 规则以访问被忽略的文件/目录，完成后自动恢复
- **自动化流程**：注释 → 恢复，无需手动干预
- **安全性**：确保 .gitignore 始终恢复到原始状态

## 使用场景

- 需要查看 `resources/models/` 中的 AI 模型文件
- 需要访问 `third_party/` 中的第三方库
- 需要检查 `tests/testAssetsDirectory/` 中的测试资源
- 任何被 .gitignore 排除但需要临时访问的文件/目录

## 执行流程

### 1. 检查路径是否被忽略
```bash
# 检查目标路径是否在 .gitignore 中
git check-ignore <path>
```

### 2. 注释相关规则
```bash
# 注释匹配目标路径的规则
sed -i 's/^# 替换为实际的规则匹配逻辑/' .gitignore
```

### 3. 恢复 .gitignore
```bash
# 取消注释，恢复原始规则
sed -i 's/^# #//' .gitignore
```

## 实现细节

### 路径匹配逻辑
- 精确匹配：`resources/models/`
- 模式匹配：`*.log`、`build/`
- 递归匹配：`third_party/`

### 注释/取消注释逻辑
- **注释**：在规则行首添加 `# `
- **恢复**：移除 `# ` 前缀，恢复原始规则

## 使用示例

```bash
# 查看被忽略的 AI 模型目录
git check-ignore resources/models/
# 输出：resources/models/ (确认被忽略)

# 执行绕过流程
# 1. 注释相关规则
# 2. 访问 resources/models/
# 3. 恢复 .gitignore
```

## 注意事项

- **临时性**：仅在当前会话中有效，不提交任何更改
- **原子性**：注释和恢复必须成对出现
- **最小影响**：只注释必要的规则，不影响其他忽略项
- **无备份**：直接修改 .gitignore，依赖注释/取消注释机制

## 错误处理

- 路径不存在 → 直接返回错误
- 规则匹配失败 → 告警并继续
- 恢复失败 → 告警并手动提示
