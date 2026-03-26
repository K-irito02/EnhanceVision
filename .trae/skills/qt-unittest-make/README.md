# Qt Unittest Make Skill

为 Qt 项目自动生成单元测试代码的技能。

## 功能特性

- **100% 函数覆盖率**: 自动为每个 public/protected 函数生成测试用例
- **智能 LSP 分析**: 使用 `lsp_document_symbols`, `lsp_goto_definition`, `lsp_find_references` 精确分析类结构
- **Stub 智能生成**: 内化完整的 Stub 模式库，自动生成 UI、信号、虚函数、重载函数的 Stub
- **智能 CMake 合并**: 根据项目具体情况优化合并，确保通用性
- **支持增量更新**: 对比现有测试，补全未覆盖的函数
- **强制验证构建**: 生成后必须编译成功才能报告完成
- **严谨错误处理**: 每个编译错误最多重试 3 次，最大循环 10 次
- **明确子 Agent 调用**: SKILL.md 明确说明必须调用子 Agent，不能跳过

## 与 qt-unittest-build 的关系

```
qt-unittest-build (构建框架)
    ↓ 生成测试框架（CMake、目录结构、stub工具）

qt-unittest-make (生成测试代码)
    ↓ 生成测试用例（100% 函数覆盖率）
    ↓ 验证构建（必须成功才能报告完成）
```

**协作流程**:
1. 首次使用：运行 `qt-unittest-build` 生成测试框架
2. 生成测试：运行 `qt-unittest-make` 生成测试代码
3. 增量更新：代码变更后，再次运行 `qt-unittest-make` 补全测试

## 使用方法

### 场景 1: 为模块批量生成测试

**触发方式**:
```
为 src/lib/ui 模块创建单元测试
```

**执行流程**:
1. 扫描模块所有类（glob 查找 `.h/.hpp` 文件）
2. 使用 LSP 分析每个类结构
3. 为每个类生成测试文件（`test_myclass.cpp`）
4. 生成测试用例（100% 函数覆盖率）
5. 生成 Stub 插桩（UI、信号、虚函数等）
6. 智能合并 CMake 配置
7. 验证构建（必须成功才能报告完成）

**输出**:
```
autotests/
├── CMakeLists.txt (已更新)
└── ui/
    ├── CMakeLists.txt (新建)
    ├── test_myclass.cpp
    ├── test_anotherclass.cpp
    └── ...
```

### 场景 2: 为单个类创建/补全测试

**触发方式**:
```
为 src/test/myclass.cpp 创建单元测试
```

或

```
为 MyClass 补全测试
```

**执行流程**:
1. 检查现有测试文件
2. 如果存在：对比已测试 vs 未测试函数
3. 如果不存在：完整生成测试
4. 补全或生成测试用例
5. 更新 CMake 配置（如需要）
6. 验证构建

**增量更新示例**:
```
现有测试：test_myclass.cpp
已测试函数：10/15
未测试函数：5

结果：
- 追加 5 个测试用例
- 覆盖率：100%
```

## 技术规范

### 测试框架

**仅支持 Google Test**:
```cpp
#include <gtest/gtest.h>

class MyClassTest : public ::testing::Test {
    // 测试类
};

TEST_F(MyClassTest, Method_Scenario_Result) {
    // 测试用例
}
```

### 命名规范

**文件命名**: `test_myclass.cpp`（小写）

**测试类命名**: `MyClassTest`（PascalCase）

**测试用例命名**: `{Feature}_{Scenario}_{ExpectedResult}`

**示例**:
- `Calculate_TwoNumbers_ReturnsSum` - 基本功能
- `Process_NullInput_HandlesGracefully` - 错误处理
- `SetValue_MaximumValue_ClampsCorrectly` - 边界条件

### LSP 工具使用

```bash
# 提取类结构
lsp_document_symbols "src/lib/ui/myclass.h"

# 读取函数实现
lsp_goto_definition "src/lib/ui/myclass.cpp" "MyClass::calculate"

# 查找依赖
lsp_find_references "src/lib/ui/myclass.h" "MyClass"
```

### Stub 插桩模式

**UI 显示/隐藏**:
```cpp
stub.set_lamda(&QWidget::show, [](QWidget *) {
    __DBG_STUB_INVOKE__
});

stub.set_lamda(&QWidget::hide, [](QWidget *) {
    __DBG_STUB_INVOKE__
});
```

**对话框执行**:
```cpp
stub.set_lamda(VADDR(QDialog, exec), [] {
    __DBG_STUB_INVOKE__
    return QDialog::Accepted;
});
```

**信号监听**:
```cpp
QSignalSpy spy(obj, &MyClass::signalChanged);
EXPECT_EQ(spy.count(), 1);
EXPECT_EQ(spy.at(0).at(0).toInt(), expected);
```

**虚函数**:
```cpp
stub.set_lamda(VADDR(MyClass, virtualMethod), []() {
    __DBG_STUB_INVOKE__
});
```

**重载函数**:
```cpp
stub.set_lamda(
    static_cast<int (MyClass::*)(int, int)>(&MyClass::overloadedMethod),
    [](MyClass *self, int a, int b) -> int {
        __DBG_STUB_INVOKE__
        return a + b;
    }
);
```

### CMake 智能合并

**策略**: 根据 CMake 具体情况优化合并，确保通用性

**分析项目**:
```bash
# 根目录 CMakeLists.txt
- 项目名称: project(...)
- Qt 版本: find_package(Qt5/Qt6 ...)
- 依赖库: find_package(...)
```

**生成通用 CMakeLists.txt**:
```cmake
cmake_minimum_required(VERSION 3.16)

# 使用变量而非硬编码
set(QT_VERSION "6")  # 从项目推断
set(PROJECT_LIBRARIES "")

file(GLOB TEST_SOURCES "test_*.cpp")

add_executable(test_{module_name} ${TEST_SOURCES})

target_link_libraries(test_{module_name}
    PRIVATE
    GTest::gtest
    GTest::gtest_main
    Qt${QT_VERSION}::Test
)

target_include_directories(test_{module_name}
    PRIVATE
    ${{CMAKE_SOURCE_DIR}}/autotests/3rdparty/stub
    ${{CMAKE_SOURCE_DIR}}/{source_module_path}
)

gtest_discover_tests(test_{module_name})
```

## 验证构建（强制性）

### 编译验证流程

1. **创建构建目录**: `mkdir -p build-autotests && cd build-autotests`
2. **配置 CMake**: `cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON`
3. **编译测试**: `cmake --build . -j$(nproc)`
4. **错误处理**:
   - 提取所有编译错误
   - 分类每个错误（链接、头文件、Stub、类型、CMake）
   - **每个错误最多重试 3 次**
   - 所有错误都修正后重新编译
   - **最大循环 10 次**（防止无限循环）
5. **编译成功**: 报告完成 ✅
6. **编译失败**: 报告详细的错误分析和修正建议 ✗

### 编译错误重试逻辑

**重要**: 重试逻辑是"**每个错误重试 3 次**"，不是全局 3 次。

**示例**:
```
编译错误：
- 错误1: undefined reference to QWidget::show
- 错误2: stub.set_lamda 编译失败

重试逻辑：
- 错误1: 尝试1 -> 失败, 尝试2 -> 失败, 尝试3 -> 成功
- 错误2: 尝试1 -> 成功

重新编译 -> 成功
```

**循环控制**:
```
while (还有编译错误 && 循环次数 < 10) {
    对于每个错误 {
        if (该错误重试次数 < 3) {
            应用修正
            重试次数++
        } else {
            标记为"无法自动修正"
        }
    }

    如果有"无法自动修正"的错误 {
        跳出循环，报告失败
    }

    重新编译
    循环次数++
}
```

### 常见编译错误及修正

| 错误类型 | 匹配模式 | 原因 | 修正方案 |
|---------|---------|------|---------|
| 链接错误 | `undefined reference to` | 缺少库链接 | 添加 `target_link_libraries(Qt${QT_VERSION}::Widgets)` |
| 头文件错误 | `No such file or directory` | 缺少头文件路径 | 添加 `target_include_directories(${{CMAKE_SOURCE_DIR}}/autotests/3rdparty/stub)` |
| Stub 签名错误 | `stub.set_lamda` 编译失败 | 函数签名不匹配 | 使用 LSP 重新获取函数签名 |
| 类型错误 | `expected primary-expression` | 返回类型或参数类型错误 | 检查 LSP 分析结果 |
| CMake 语法错误 | `CMake Error` | CMakeLists.txt 语法错误 | 检查 CMakeLists.txt |

## 测试用例设计

### AAA 模式

每个测试用例遵循 Arrange-Act-Assert 模式：

```cpp
TEST_F(MyClassTest, Method_Scenario_Result) {
    // Arrange: 准备测试数据
    obj->setInput(42);

    // Act: 执行被测试的操作
    int result = obj->calculate();

    // Assert: 验证结果
    EXPECT_EQ(result, 84);
}
```

## 常见问题

### Q: 编译错误重试逻辑是什么？

A: **每个编译错误最多重试 3 次**，不是全局 3 次。所有错误都修正后重新编译，最大循环 10 次防止无限循环。

### Q: 如何处理复杂的依赖关系？

A: 使用 `lsp_find_references` 分析依赖关系，自动生成对应的 Stub 插桩。

### Q: CMake 合并策略是什么？

A: 智能 CMake 合并根据项目具体情况优化：
1. 分析现有 CMakeLists.txt
2. 提取 Qt 版本、依赖库、包含目录模式
3. 生成通用 CMakeLists.txt（使用变量）
4. 保持项目现有风格

### Q: 如何保证 100% 函数覆盖率？

A:
1. 使用 `lsp_document_symbols` 提取所有 public/protected 函数
2. 为每个函数至少生成一个测试用例
3. 考虑边界条件、错误处理、特殊场景

### Q: 增量更新如何工作？

A:
1. 对比现有测试文件中的已测试函数
2. 提取所有函数（LSP）
3. 计算未覆盖函数
4. 为未覆盖函数追加测试用例

### Q: 编译失败会怎么办？

A:
1. 技能会自动尝试修正（每个错误最多重试 3 次）
2. 如果 10 次循环后仍失败，会报告详细的错误分析和修正建议
3. 绝不会告诉用户"测试已生成"（如果编译失败）

## 与其他技能的对比

| 特性 | qt-unittest-build | qt-unittest-make | qt-cpp-unittest-generation |
|------|------------------|------------------|----------------------------|
| **功能** | 生成测试框架 | 生成测试代码 | 生成测试代码 |
| **测试框架** | GTest/Qt Test | 仅 GTest | GTest/Qt Test |
| **LSP 工具** | 无 | 必需 | 必需 |
| **CMake** | 自动生成 | 智能合并 | 无 |
| **覆盖率要求** | 无 | 100% | 无 |
| **增量更新** | 不支持 | 支持 | 不支持 |
| **构建验证** | 可选 | **必须成功才能报告** | 无 |
| **错误重试** | 无 | **每个错误重试 3 次** | 无 |

## 子 Agent 架构

**设计目的**:
1. **独立上下文**: 为单个类或小模块生成测试时，独立上下文避免污染
2. **并行执行**: 为多个类批量生成测试时，可以 fork 多个子 Agent 并行执行，提高效率
3. **任务隔离**: 子 Agent 失败不影响主 Agent，便于错误处理

**调用方式**:
```python
task(
    description="生成单元测试代码",
    prompt="完整任务描述，包括目标、要求、验证流程",
    subagent_type="general"
)
```

**重要**: 必须调用子 Agent，不能跳过直接手动工作。

## 依赖要求

- Google Test (libgtest-dev, libgmock-dev)
- LSP 工具 (OpenCode 内置)
- Qt 项目结构（CMake）
- 测试框架（由 qt-unittest-build 生成）

## 许可证

MIT License

## 贡献

欢迎提交 Issue 和 Pull Request！

## 相关资源

- **Google Test 文档**: https://google.github.io/googletest/
- **Qt Test 文档**: https://doc.qt.io/qt-6/qtest-overview.html
- **Stub-Ext 源码**: https://github.com/manfredlohw/cpp-stub
