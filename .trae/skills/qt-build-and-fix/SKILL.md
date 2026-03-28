---
name: "qt-build-and-fix"
description: "Qt 项目构建和调试专家 - 自动化 EnhanceVision 项目（Qt Quick + C++ + NCNN）的构建、运行、调试和修复。当用户要求构建项目、运行程序、解决编译错误、调试运行时问题、检查代码质量、配置环境或任何与构建相关的问题时，必须使用此技能。即使只是简单的构建请求，也应使用此技能确保遵循项目构建规范。"
---

# Qt Build and Fix Skill

自动化 EnhanceVision 项目（Qt Quick + C++ + NCNN）的构建、运行和修复生命周期管理。

## 核心原则

1. **增量构建优先** - 不删除 build 目录，除非必要
2. **日志驱动诊断** - 所有操作记录日志便于分析
3. **构建后验证** - 每次成功构建后运行程序验证
4. **错误循环修复** - 持续修复直到零错误零警告
5. **测试循环验证**：启动程序后等待用户手动测试，通过用户终端输入 y/n 确认是否进行日志检查，循环直到问题解决

## 项目架构

| 层级 | 技术 | 版本 |
|------|------|------|
| UI 框架 | Qt Quick (QML) + Qt Widgets | Qt 6.10.2 |
| 图形渲染 | Qt Scene Graph + RHI | Vulkan/D3D11/Metal |
| C++ 后端 | Core 引擎 + Controllers + Models | C++20 |
| AI 推理 | NCNN (Vulkan 加速) | latest |
| 多媒体 | FFmpeg 7.1 + Qt Multimedia | 预编译库 |
| 构建系统 | CMake | 3.20+ |
| 编译器 | MSVC 2022 | VS 17 |

## 环境配置

### 必需环境

| 工具 | 路径 | 验证命令 |
|------|------|----------|
| CMake | `C:\Program Files\CMake\bin\cmake.exe` | `cmake --version` |
| Qt 6.10.2 | `E:\Qt\6.10.2\msvc2022_64` | `E:\Qt\6.10.2\msvc2022_64\bin\qmake.exe --version` |
| MSVC 2022 | Visual Studio 17 2022 | `cl` (在 Developer Command Prompt) |

### 环境验证脚本

```powershell
# 验证 CMake
if (-not (Test-Path "C:\Program Files\CMake\bin\cmake.exe")) {
    Write-Error "CMake 未找到，请安装 CMake 3.20+"
    exit 1
}

# 验证 Qt
if (-not (Test-Path "E:\Qt\6.10.2\msvc2022_64")) {
    Write-Error "Qt 6.10.2 未找到，请检查安装路径"
    exit 1
}

# 验证项目文件
if (-not (Test-Path "CMakeLists.txt")) {
    Write-Error "CMakeLists.txt 未找到，请在项目根目录执行"
    exit 1
}

Write-Host "环境验证通过"
```

## 执行流程

### 1. 清理日志

每次执行前清空 `logs/` 目录所有旧日志文件。

```powershell
if (Test-Path "logs") { 
    Remove-Item -Path "logs\*" -Recurse -Force -ErrorAction SilentlyContinue
} else { 
    New-Item -ItemType Directory -Path "logs" -Force | Out-Null
}
```

### 2. CMake 配置

```powershell
# 首次构建或配置变更时执行
& "C:\Program Files\CMake\bin\cmake.exe" --preset windows-msvc-2022-release 2>&1 | Tee-Object -FilePath "logs/cmake_configure.log"
```

### 3. 编译项目

```powershell
# 增量构建（推荐）
& "C:\Program Files\CMake\bin\cmake.exe" --build build/msvc2022/Release --config Release -j 8 2>&1 | Tee-Object -FilePath "logs/build_output.log"
```

### 4. 分析编译结果

```powershell
# 检查错误
$errors = Select-String -Path "logs\build_output.log" -Pattern "error C\d+" | Select-Object -Unique
$warnings = Select-String -Path "logs\build_output.log" -Pattern "warning C\d+" | Select-Object -Unique

if ($errors) {
    Write-Host "发现 $($errors.Count) 个错误"
    $errors | ForEach-Object { Write-Host $_.Line }
}

if ($warnings) {
    Write-Host "发现 $($warnings.Count) 个警告"
}
```

### 5. 运行验证

**重要**：每次构建编译成功后，必须运行程序进行验证。

```powershell
# 关闭已运行的程序
Get-Process | Where-Object { $_.Name -like "*EnhanceVision*" } | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 2

# 启动程序
if ($LASTEXITCODE -eq 0) {
    Start-Process -FilePath "build\msvc2022\Release\Release\EnhanceVision.exe"
}
```

### 6. 测试循环验证

**重要**：启动程序后，必须进行测试循环验证，直到问题解决。

```powershell
# 测试循环流程
while ($true) {
    # 1. 等待用户确认测试完成
    Write-Host "程序已启动，请手动测试程序功能..."
    $response = Read-Host "测试完成后是否检查日志？(y/n)"
    
    if ($response -ne 'y') {
        Write-Host "跳过日志检查，测试验证完成"
        break
    }
    
    # 2. 检查日志文件
    Write-Host "检查日志文件..."
    $logContent = Get-Content -Path "logs\runtime_output.log" -ErrorAction SilentlyContinue
    
    # 3. 分析日志中的错误和警告
    $errors = $logContent | Select-String -Pattern "\[CRIT\]|\[ERROR\]|错误|异常|崩溃|failed|error" -CaseSensitive:$false
    $warnings = $logContent | Select-String -Pattern "\[WARN\]|警告|warning" -CaseSensitive:$false
    
    # 4. 输出诊断结果
    if ($errors) {
        Write-Host "发现错误信息："
        $errors | ForEach-Object { Write-Host "  $_" }
    }
    
    if ($warnings) {
        Write-Host "发现警告信息："
        $warnings | ForEach-Object { Write-Host "  $_" }
    }
    
    # 5. 检查是否还有问题
    if (-not $errors -and -not $warnings) {
        Write-Host "日志检查通过，无错误和警告"
        
        # 询问用户是否还有问题
        $response = Read-Host "是否还有问题需要修复？(y/n)"
        if ($response -ne 'y') {
            Write-Host "测试验证完成"
            break
        }
    } else {
        Write-Host "发现日志问题，需要修复..."
        # 返回构建流程进行修复
        break
    }
}
```

**测试循环说明**：

1. **等待测试**：程序启动后提示用户手动测试程序功能
2. **用户确认**：用户测试完成后，通过用户终端输入 y/n 决定是否检查日志
3. **日志检查**：如果用户选择 y，自动读取并分析日志文件中的错误和警告
4. **问题诊断**：输出发现的问题信息
5. **循环验证**：如果发现问题，返回修复流程；如果无问题，询问用户确认
6. **持续循环**：直到所有问题解决且用户确认无误

### 7. 查看运行时日志

```powershell
# 查看最近 100 行日志
Get-Content -Path "logs\runtime_output.log" -Tail 100

# 实时监控日志
Get-Content -Path "logs\runtime_output.log" -Wait
```

## 常见错误解决方案

### C++ 编译错误

#### C1083: 无法打开包括文件

**原因**：头文件路径错误或模块未添加

**解决方案**：

1. 检查 CMakeLists.txt 中是否添加了对应的 Qt 模块：

```cmake
find_package(Qt6 REQUIRED COMPONENTS 
    Core 
    Gui 
    Quick 
    Widgets
    Multimedia  # 确保包含所需模块
)
```

2. 检查头文件路径：

```cmake
target_include_directories(EnhanceVision PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EnhanceVision
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
```

3. 检查 Qt 模块链接：

```cmake
target_link_libraries(EnhanceVision PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Widgets
    Qt6::Multimedia
)
```

#### C2011: 重定义

**原因**：类型重复定义，通常是由于头文件重复包含

**解决方案**：

1. 添加头文件保护：

```cpp
#ifndef CLASS_NAME_H
#define CLASS_NAME_H

// 类定义

#endif // CLASS_NAME_H
```

2. 或使用 `#pragma once`：

```cpp
#pragma once

// 类定义
```

3. 检查是否有循环依赖，使用前置声明：

```cpp
// 前置声明
class OtherClass;

class MyClass {
    OtherClass* m_other;  // 使用指针，无需完整定义
};
```

#### C2027: 使用了未定义类型

**原因**：缺少头文件包含或前置声明不完整

**解决方案**：

1. 在 `.cpp` 文件中包含完整头文件：

```cpp
// MyClass.h
class OtherClass;  // 前置声明

class MyClass {
    void doSomething(OtherClass* other);
};

// MyClass.cpp
#include "OtherClass.h"  // 包含完整定义

void MyClass::doSomething(OtherClass* other) {
    other->method();  // 现在可以使用
}
```

#### C2039: 不是类的成员

**原因**：方法未声明或实现

**解决方案**：

1. 检查头文件中是否声明：

```cpp
class MyClass {
public:
    void myMethod();  // 确保声明
};
```

2. 检查实现文件中是否定义：

```cpp
void MyClass::myMethod() {
    // 实现
}
```

3. 检查 `const` 修饰符是否一致：

```cpp
// 头文件
void myMethod() const;

// 实现文件
void MyClass::myMethod() const {  // 必须一致
    // 实现
}
```

#### C2440: 类型转换错误

**原因**：类型不兼容

**解决方案**：

```cpp
// 错误：const char* 到 QString
QString str = "hello";  // C++11 前

// 正确
QString str = QStringLiteral("hello");
QString str = QLatin1String("hello");
QString str = QString::fromUtf8("hello");
```

#### C2664/C2672: 函数参数不匹配

**原因**：参数类型或数量不匹配

**解决方案**：

1. 检查函数签名：

```cpp
// 声明
void process(const QString& name, int value);

// 调用
process("test", 42);  // 正确，const char* 可隐式转换
process(42, "test");  // 错误，参数顺序错误
```

2. 使用 `qOverload` 解决重载歧义：

```cpp
connect(obj, qOverload<int>(&QComboBox::currentIndexChanged), 
        this, &MyClass::onIndexChanged);
```

### Qt MOC 错误

#### Q_ENUM 宏错误

**原因**：宏不在 Q_OBJECT 类内

**解决方案**：

```cpp
// 错误
class MyClass {
public:
    enum class Status { OK, Error };
};
Q_ENUM(MyClass::Status)  // 错误位置

// 正确
class MyClass : public QObject {
    Q_OBJECT
public:
    enum class Status { OK, Error };
    Q_ENUM(Status)  // 必须在类内
};
```

#### 未定义的 vtable

**原因**：Q_OBJECT 宏但未运行 MOC

**解决方案**：

1. 确保 CMakeLists.txt 中包含头文件：

```cmake
set(HEADERS
    include/EnhanceVision/core/MyClass.h
)

target_sources(EnhanceVision PRIVATE
    ${HEADERS}
    ${SOURCES}
)
```

2. 或使用 `AUTOMOC`：

```cmake
set(CMAKE_AUTOMOC ON)
```

#### 信号槽连接失败

**原因**：签名不匹配或对象问题

**解决方案**：

```cpp
// 检查连接返回值
bool connected = connect(sender, &Sender::signal, 
                         receiver, &Receiver::slot);
if (!connected) {
    qWarning() << "连接失败";
}

// 使用新式连接（编译时检查）
connect(sender, &Sender::signal, 
        receiver, &Receiver::slot);

// 处理重载信号
connect(comboBox, qOverload<int>(&QComboBox::currentIndexChanged),
        this, &MyClass::onIndexChanged);

// Lambda 连接
connect(button, &QPushButton::clicked, this, [this]() {
    // 处理
});
```

### QML 错误

#### QML 模块未找到

**原因**：QML 文件未注册

**解决方案**：

```cmake
qt_add_qml_module(EnhanceVision
    URI EnhanceVision
    VERSION 1.0
    QML_FILES
        qml/main.qml
        qml/App.qml
        qml/pages/MainPage.qml
        qml/components/Sidebar.qml
    RESOURCES
        resources/icons/...
)
```

#### QML 类型未定义

**原因**：缺少 import 或注册

**解决方案**：

```qml
// 检查 import
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EnhanceVision  // 自定义模块

// C++ 注册类型
qmlRegisterType<MyClass>("EnhanceVision", 1, 0, "MyClass");

// 或使用 QML_ELEMENT
class MyClass : public QObject {
    Q_OBJECT
    QML_ELEMENT
    // ...
};
```

#### 属性绑定失败

**原因**：属性未正确暴露

**解决方案**：

```cpp
// C++ 暴露属性
class MyClass : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    
public:
    QString name() const { return m_name; }
    void setName(const QString& name) {
        if (m_name != name) {
            m_name = name;
            emit nameChanged();
        }
    }
    
signals:
    void nameChanged();
    
private:
    QString m_name;
};
```

```qml
// QML 使用
MyClass {
    name: "test"  // 绑定
}
```

### 链接错误

#### LNK2019: 无法解析的外部符号

**原因**：库未链接或实现缺失

**解决方案**：

1. 检查库链接：

```cmake
target_link_libraries(EnhanceVision PRIVATE
    Qt6::Core
    Qt6::Gui
    # 添加缺失的库
)
```

2. 检查实现文件是否包含在构建中：

```cmake
set(SOURCES
    src/core/MyClass.cpp  # 确保包含
)
```

3. 检查第三方库路径：

```cmake
target_link_directories(EnhanceVision PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/lib
)
```

#### LNK2001: 无法解析的外部符号

**原因**：静态成员未定义

**解决方案**：

```cpp
// 头文件
class MyClass {
    static int s_count;  // 声明
};

// 实现文件
int MyClass::s_count = 0;  // 定义
```

### 运行时错误

#### 程序启动崩溃

**诊断步骤**：

```powershell
# 查看运行时日志
Get-Content -Path "logs\runtime_output.log"

# 检查 DLL 依赖
dumpbin /dependents build\msvc2022\Release\Release\EnhanceVision.exe
```

**常见原因和解决方案**：

1. **Qt DLL 缺失**：

```powershell
# 运行 windeployqt
& "E:\Qt\6.10.2\msvc2022_64\bin\windeployqt.exe" `
    "build\msvc2022\Release\Release\EnhanceVision.exe" `
    --release --qmldir "qml"
```

2. **QML 文件未找到**：

```cpp
// 检查 QML 资源路径
QQmlApplicationEngine engine;
engine.addImportPath("qrc:/qml");
engine.loadFromModule("EnhanceVision", "Main");
```

3. **插件未加载**：

```cpp
// main.cpp
#include <QtPlugin>
Q_IMPORT_PLUGIN(QWindowsVistaStylePlugin);
```

#### 内存泄漏

**诊断工具**：

1. **Visual Studio 诊断工具**：
   - 调试 → 性能探查器 → 内存使用

2. **VLD (Visual Leak Detector)**：

```cmake
# CMakeLists.txt
find_package(VLD REQUIRED)
target_link_libraries(EnhanceVision PRIVATE VLD::VLD)
```

```cpp
// main.cpp
#include <vld.h>
```

3. **Qt 对象树检查**：

```cpp
// 检查对象父子关系
QObject* obj = new QObject(parent);  // 父对象会自动删除
QObject* obj = new QObject();  // 需要手动删除
```

#### 性能问题

**诊断方法**：

```cpp
// 使用 QElapsedTimer 测量
QElapsedTimer timer;
timer.start();
// ... 代码 ...
qDebug() << "耗时:" << timer.elapsed() << "ms";

// 使用 Qt 性能分析器
// Qt Creator → Analyze → QML Profiler
```

**常见优化**：

1. **UI 线程阻塞**：

```cpp
// 错误：在主线程执行耗时操作
void onClicked() {
    processLargeData();  // 阻塞 UI
}

// 正确：使用工作线程
void onClicked() {
    QtConcurrent::run([this]() {
        processLargeData();
    });
}
```

2. **频繁重绘**：

```qml
// 错误：频繁更新
Timer {
    interval: 16
    running: true
    onTriggered: model.update()  // 每帧更新
}

// 正确：按需更新
Timer {
    interval: 100  // 降低更新频率
    running: true
    onTriggered: model.update()
}
```

## 构建优化

### 预编译头 (PCH)

```cmake
if(USE_PRECOMPILED_HEADERS AND MSVC)
    target_precompile_headers(EnhanceVision PRIVATE
        <memory>
        <string>
        <vector>
        <QObject>
        <QAbstractListModel>
        <QImage>
        <QDebug>
    )
endif()
```

### 多进程编译

```cmake
target_compile_options(EnhanceVision PRIVATE /MP)
```

### 编译缓存

```powershell
# 安装 sccache
cargo install sccache

# 配置环境变量
$env:CC = "sccache cl"
$env:CXX = "sccache cl"
```

### 增量构建最佳实践

| 场景 | 操作 |
|------|------|
| 修改少量源文件 | 直接增量构建 |
| 修改头文件 | 可能需要重新编译依赖文件 |
| 修改 CMakeLists.txt | 重新配置后增量构建 |
| 遇到奇怪错误 | 清理后重新构建 |

```powershell
# 清理构建（仅在必要时）
Remove-Item -Path "build\msvc2022" -Recurse -Force -ErrorAction SilentlyContinue

# 重新配置
& "C:\Program Files\CMake\bin\cmake.exe" --preset windows-msvc-2022-release

# 重新构建
& "C:\Program Files\CMake\bin\cmake.exe" --build build/msvc2022/Release --config Release -j 8
```

## 日志系统

### 日志文件

| 文件 | 内容 | 写入方式 |
|------|------|----------|
| `cmake_configure.log` | CMake 配置输出 | 覆盖 |
| `build_output.log` | 编译输出 | 覆盖 |
| `runtime_output.log` | 程序运行时日志 | 覆盖 |

### 日志格式

```
[2026-03-15 10:30:45.123] [DEBUG] 消息内容
[2026-03-15 10:30:45.124] [WARN] 警告内容
[2026-03-15 10:30:45.125] [CRIT] 错误内容
```

### 日志配置

```cpp
// 日志级别
enum class LogLevel {
    Debug,
    Info,
    Warning,
    Critical
};

// 设置日志级别
qSetMessagePattern("[%{time yyyy-MM-dd hh:mm:ss.zzz}] [%{type}] %{message}");
```

## 调试技巧

### 断点调试

1. **Visual Studio**：
   - F9 设置断点
   - F5 启动调试
   - F10 单步跳过
   - F11 单步进入

2. **Qt Creator**：
   - F9 设置断点
   - Ctrl+R 启动调试
   - F10 单步跳过
   - F11 单步进入

### QML 调试

```qml
// 使用 console.log
console.log("变量值:", value)

// 使用 Debugger
// Qt Creator → Debug → QML
```

### 远程调试

```powershell
# 启用远程调试
$env:QT_QPA_PLATFORM = "windows:fontengine=freetype"
./EnhanceVision.exe -qmljsdebugger=port:1234
```

## 故障排除流程

### 构建失败

```
1. 检查日志文件
   ↓
2. 识别错误类型
   ↓
3. 应用对应解决方案
   ↓
4. 增量构建验证
   ↓
5. 仍有错误？返回步骤 2
```

### 运行时错误

```
1. 查看运行时日志
   ↓
2. 检查 DLL 依赖
   ↓
3. 检查 QML 加载
   ↓
4. 使用调试器定位
   ↓
5. 修复并验证
   ↓
6. 启动程序并等待用户手动测试
   ↓
7. 用户确认后检查日志调试信息
   ↓
8. 仍有问题？返回步骤 1
```

### 性能问题

```
1. 使用性能分析器
   ↓
2. 识别瓶颈
   ↓
3. 优化代码
   ↓
4. 测量改进
   ↓
5. 持续优化
```

## 构建时间参考

| 场景 | 预计时间 | 说明 |
|------|----------|------|
| 首次完整构建 | 3-5 分钟 | 包括 NCNN 编译 |
| 增量构建（少量文件） | 10-30 秒 | 仅编译修改文件 |
| 仅修改 QML | 5-10 秒 | 资源更新 |
| 清理后重建 | 3-5 分钟 | 完整重新编译 |

## 注意事项

- **增量构建**：不要每次都删除 build 目录
- **日志覆盖**：每次执行前删除所有旧日志
- **警告等级**：MSVC `/W4`
- **Qt 路径**：`E:\Qt\6.10.2\msvc2022_64`
- **CMake 路径**：`C:\Program Files\CMake\bin\cmake.exe`
- **构建目录**：`build/msvc2022/Release/`
- **构建后运行**：每次构建成功后必须运行程序验证
- **关闭已运行程序**：启动新程序前必须先关闭已运行的相同程序
- **测试循环验证**：启动程序后等待用户手动测试，通过用户终端输入 y/n 确认是否进行日志检查，循环直到问题解决
- **日志驱动修复**：根据日志中的错误和警告信息进行针对性修复

## 程序崩溃诊断

### Windows 事件日志查看

当程序崩溃时，Windows 会将崩溃信息记录到事件日志中。使用以下命令查看崩溃详情：

```powershell
# 查看最近的程序崩溃事件
Get-WinEvent -FilterHashtable @{LogName='Application'; ProviderName='Application Error'} -MaxEvents 5 2>$null | Select-Object TimeCreated, Message | Format-List
```

**输出示例**：
```
TimeCreated : 2026/3/26 0:33:19
Message     : 出错应用程序名称： EnhanceVision.exe，版本： 0.0.0.0，时间戳： 0x69c40e22
              出错模块名称： ntdll.dll， 版本： 10.0.26100.7920，时间戳： 0x5ffc11eb
              异常代码： 0xc0000374
              错误偏移： 0x00000000001176e5
              出错进程 ID： 0x5BF4
              出错应用程序路径： E:\QtAudio-VideoLearning\EnhanceVision\build\msvc2022\Release\Release\EnhanceVision.exe
```

### 常见异常代码含义

| 异常代码 | 含义 | 常见原因 |
|----------|------|----------|
| `0xc0000005` | 访问违规 | 空指针、越界访问、使用已释放的内存 |
| `0xc0000374` | 堆损坏 | 内存越界写入、双重释放、使用已释放的内存 |
| `0xc0000409` | 栈缓冲区溢出 | 局部数组越界、递归过深 |
| `0xc000001d` | 非法指令 | 函数指针错误、虚函数表损坏 |
| `0xc00000fd` | 栈溢出 | 无限递归、过大的局部变量 |

### 进程管理命令

```powershell
# 查看所有 EnhanceVision 进程
Get-Process | Where-Object { $_.Name -like "*EnhanceVision*" } | Select-Object Id, ProcessName, CPU, StartTime

# 强制关闭所有 EnhanceVision 进程（普通权限）
Get-Process | Where-Object { $_.Name -like "*EnhanceVision*" } | Stop-Process -Force -ErrorAction SilentlyContinue

# 强制关闭所有 EnhanceVision 进程（管理员权限）
taskkill /F /IM EnhanceVision.exe

# 如果普通方式无法关闭，使用管理员权限
Start-Process powershell -Verb RunAs -ArgumentList "-Command taskkill /F /IM EnhanceVision.exe"
```

### 崩溃诊断流程

```
1. 发现程序崩溃
   ↓
2. 查看 Windows 事件日志
   Get-WinEvent -FilterHashtable @{LogName='Application'; ProviderName='Application Error'} -MaxEvents 5
   ↓
3. 分析异常代码
   - 0xc0000005 → 检查指针和内存访问
   - 0xc0000374 → 检查堆操作和内存写入
   ↓
4. 查看运行时日志最后输出
   Get-Content -Path "logs\runtime_output.log" -Tail 50
   ↓
5. 定位崩溃位置
   - 根据最后日志确定崩溃函数
   - 添加更多日志输出
   ↓
6. 修复并验证
   ↓
7. 仍有问题？返回步骤 2
```

### 添加诊断日志

在可疑位置添加详细日志，帮助定位崩溃点：

```cpp
// 在关键位置添加日志和刷新
qInfo() << "[ModuleName][FunctionName] step 1"
        << "variable1:" << value1
        << "variable2:" << value2;
fflush(stdout);  // 立即刷新，确保崩溃前日志已写入

// 危险操作前添加检查
qInfo() << "[ModuleName][FunctionName] before dangerous operation";
fflush(stdout);

// 危险操作
result = dangerousOperation();

qInfo() << "[ModuleName][FunctionName] after dangerous operation"
        << "result:" << result;
fflush(stdout);
```

### 内存问题诊断

```powershell
# 检查是否有崩溃转储文件
Get-ChildItem -Path $env:LOCALAPPDATA\CrashDumps -Filter "*.dmp" -ErrorAction SilentlyContinue | Select-Object Name, LastWriteTime, Length

# 检查程序目录下的转储文件
Get-ChildItem -Path "build\msvc2022\Release\Release" -Filter "*.dmp" -Recurse -ErrorAction SilentlyContinue | Select-Object Name, LastWriteTime
```

### 使用 Visual Studio 调试崩溃

1. **打开 Visual Studio**
2. **调试 → 附加到进程** → 选择 EnhanceVision.exe
3. **调试 → 窗口 → 异常设置** → 勾选 "Common Language Runtime Exceptions" 和 "C++ Exceptions"
4. 当程序崩溃时，VS 会自动断点在崩溃位置

### 使用 WinDbg 分析崩溃

```powershell
# 安装 Windows SDK (包含 WinDbg)
# 使用 WinDbg 打开转储文件

# 基本命令
!analyze -v           # 分析崩溃原因
k                     # 查看调用栈
~*k                   # 查看所有线程调用栈
!heap -p -a <地址>    # 查看堆信息
```

### 常见崩溃场景和解决方案

#### 1. QImage 格式转换崩溃

**症状**：在 `convertToFormat` 调用时崩溃

**原因**：源图像数据无效或格式不支持

**解决方案**：
```cpp
// 先创建深拷贝，确保数据独立
QImage inputCopy = input.copy();
QImage normalizedInput = inputCopy.convertToFormat(QImage::Format_RGB888);

// 检查转换结果
if (normalizedInput.isNull() || normalizedInput.format() != QImage::Format_RGB888) {
    qWarning() << "Format conversion failed";
    return;
}
```

#### 2. 内存访问越界

**症状**：异常代码 0xc0000005

**诊断**：
```cpp
// 添加边界检查
qInfo() << "Array size:" << array.size() << "accessing index:" << index;
if (index < 0 || index >= array.size()) {
    qWarning() << "Index out of bounds!";
    return;
}
```

#### 3. 堆损坏

**症状**：异常代码 0xc0000374

**常见原因**：
- `memcpy` 目标缓冲区太小
- 使用已释放的内存
- 双重释放

**解决方案**：
```cpp
// 使用安全的拷贝方式
const int srcBytesPerLine = src.bytesPerLine();
const int dstBytesPerLine = dst.bytesPerLine();
const int copyBytes = std::min(srcBytesPerLine, dstBytesPerLine);
std::memcpy(dst.bits(), src.constBits(), copyBytes);
```

#### 4. 多线程问题

**症状**：间歇性崩溃，难以复现

**诊断**：
```cpp
// 添加线程信息到日志
qInfo() << "[Thread]" << QThread::currentThread() << "entering function";
// ... 代码 ...
qInfo() << "[Thread]" << QThread::currentThread() << "leaving function";
```

**解决方案**：
- 使用 `QMutex` 保护共享资源
- 跨线程信号槽使用 `Qt::QueuedConnection`
- 避免在工作线程中访问 UI 对象
