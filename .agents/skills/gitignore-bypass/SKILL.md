---
name: "gitignore-bypass"
description: "GitIgnore 临时绕过技能。当需要访问被 .gitignore 排除的文件/目录时调用。"
---

# GitIgnore 临时绕过

临时禁用 .gitignore 规则以访问被忽略的文件/目录，完成后自动恢复。

## 使用场景

| 场景 | 路径 |
|------|------|
| 查看 AI 模型文件 | `resources/models/` |
| 访问第三方库 | `third_party/` |
| 检查测试资源 | `tests/testAssetsDirectory/` |
| 查看构建产物 | `build/` |

## 执行流程

### 1. 检查路径是否被忽略

```powershell
git check-ignore <path>
```

### 2. 注释相关规则

```powershell
# 读取 .gitignore 内容
$content = Get-Content .gitignore -Raw

# 注释匹配的规则（在行首添加 # ）
$content = $content -replace "^($rule)", "# `$1"

# 写回文件
Set-Content .gitignore $content -NoNewline
```

### 3. 恢复 .gitignore

```powershell
# 取消注释，恢复原始规则
$content = Get-Content .gitignore -Raw
$content = $content -replace "^# ", ""
Set-Content .gitignore $content -NoNewline
```

## 注意事项

- **临时性**：仅在当前会话中有效，不提交任何更改
- **原子性**：注释和恢复必须成对出现
- **最小影响**：只注释必要的规则
- **无备份**：直接修改 .gitignore，依赖注释机制恢复

## 错误处理

| 错误 | 处理方式 |
|------|----------|
| 路径不存在 | 直接返回错误 |
| 规则匹配失败 | 告警并继续 |
| 恢复失败 | 告警并手动提示 |
