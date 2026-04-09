# 贡献指南

[English](CONTRIBUTING.md) | 简体中文

感谢您有兴趣为 EnhanceVision 做出贡献！本文档将帮助您了解如何参与项目开发。

## 目录

- [行为准则](#行为准则)
- [如何贡献](#如何贡献)
- [开发环境设置](#开发环境设置)
- [代码规范](#代码规范)
- [提交规范](#提交规范)
- [Pull Request 流程](#pull-request-流程)

## 行为准则

本项目采用贡献者公约作为行为准则。参与本项目即表示您同意遵守其条款。请阅读 [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) 了解详情。

## 如何贡献

### 报告 Bug

如果您发现了 Bug，请通过 [GitHub Issues](https://github.com/K-irito02/EnhanceVision/issues) 提交报告。提交前请：

1. 搜索现有 Issues，确认该问题尚未被报告
2. 使用 Issue 模板填写详细信息：
   - 操作系统和版本
   - EnhanceVision 版本
   - 复现步骤
   - 预期行为与实际行为
   - 截图或日志（如适用）

### 建议新功能

欢迎提出新功能建议！请通过 GitHub Issues 提交，并详细描述：

1. 功能描述
2. 使用场景
3. 可能的实现方案（可选）

### 提交代码

1. Fork 本仓库
2. 创建功能分支 (`git checkout -b feature/amazing-feature`)
3. 提交更改 (`git commit -m 'feat: add amazing feature'`)
4. 推送到分支 (`git push origin feature/amazing-feature`)
5. 创建 Pull Request

## 开发环境设置

### 系统要求

- Windows 10/11 64-bit
- Visual Studio 2022 (MSVC)
- CMake 3.20+
- Qt 6.10.2
- Vulkan SDK

### 构建步骤

```powershell
# 克隆仓库
git clone https://github.com/K-irito02/EnhanceVision.git
cd EnhanceVision

# 初始化子模块
git submodule update --init --recursive

# Debug 构建
cmake --preset windows-msvc-2022-debug
cmake --build build/msvc2022/Debug --config Debug -j 8

# Release 构建
cmake --preset windows-msvc-2022-release
cmake --build build/msvc2022/Release --config Release -j 8
```

### 运行测试

```powershell
ctest --test-dir build/msvc2022/Debug -C Debug --output-on-failure
```

## 代码规范

### C++ 规范

详见 [.trae/rules/03-cpp-standards.md](.trae/rules/03-cpp-standards.md)

关键要点：
- 使用 C++20 特性
- 遵循 RAII 原则
- 使用智能指针管理内存
- 添加适当的注释

### QML 规范

详见 [.trae/rules/04-qml-standards.md](.trae/rules/04-qml-standards.md)

关键要点：
- 使用声明式语法
- 组件职责单一
- 属性命名使用 camelCase

### 代码格式化

提交前请确保代码格式正确：

```powershell
# 使用 clang-format 格式化 C++ 代码
clang-format -i src/**/*.cpp include/**/*.h
```

## 提交规范

本项目采用 [Conventional Commits](https://www.conventionalcommits.org/) 规范：

```
<type>(<scope>): <description>

[optional body]

[optional footer(s)]
```

### 类型 (type)

| 类型 | 说明 |
|------|------|
| `feat` | 新功能 |
| `fix` | Bug 修复 |
| `docs` | 文档更新 |
| `style` | 代码格式（不影响功能） |
| `refactor` | 代码重构 |
| `perf` | 性能优化 |
| `test` | 测试相关 |
| `chore` | 构建/工具相关 |

### 示例

```
feat(ai): add Real-ESRGAN model support

- Add model loading for Real-ESRGAN x4plus
- Implement image preprocessing pipeline
- Support GPU acceleration via Vulkan

Closes #123
```

## Pull Request 流程

1. **确保通过所有检查**
   - 代码编译无错误
   - 所有测试通过
   - 代码格式符合规范

2. **PR 标题格式**
   - 使用 Conventional Commits 格式
   - 例如：`feat: add batch export feature`

3. **PR 描述模板**
   ```markdown
   ## 变更类型
   - [ ] Bug 修复
   - [ ] 新功能
   - [ ] 重构
   - [ ] 文档更新

   ## 变更说明
   [描述您的变更]

   ## 测试
   [描述如何测试这些变更]

   ## 截图（如适用）
   [添加截图]

   ## 相关 Issue
   Closes #
   ```

4. **代码审查**
   - 至少需要一位维护者审核
   - 解决所有审查意见
   - 保持提交历史整洁（必要时使用 squash）

## 许可证

通过贡献代码，您同意您的贡献将按照 MIT 许可证授权。
