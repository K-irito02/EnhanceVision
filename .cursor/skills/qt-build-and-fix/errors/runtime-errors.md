# 运行时错误

## 程序启动崩溃

### 诊断步骤

```powershell
# 查看运行时日志
Get-Content -Path "logs\runtime_output.log"

# 检查 DLL 依赖
if (Test-Path "build\msvc2022\Release\Release\EnhanceVision.exe") {
    # 使用 dumpbin 检查依赖（需要Visual Studio）
    $vsPath = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC"
    $dumpbin = Get-ChildItem -Path $vsPath -Recurse -Name "dumpbin.exe" | Select-Object -First 1
    
    if ($dumpbin) {
        & "$vsPath\$dumpbin" /dependents "build\msvc2022\Release\Release\EnhanceVision.exe"
    } else {
        Write-Host "dumpbin 未找到，请安装 Visual Studio"
    }
}
```

### Qt DLL 缺失

```powershell
# 运行 windeployqt
& "E:\Qt\6.10.2\msvc2022_64\bin\windeployqt.exe" `
    "build\msvc2022\Release\Release\EnhanceVision.exe" `
    --release --qmldir "qml"
```

### QML 文件未找到

```cpp
// 检查 QML 资源路径
QQmlApplicationEngine engine;
engine.addImportPath("qrc:/qml");
engine.loadFromModule("EnhanceVision", "Main");
```

### 插件未加载

```cpp
// main.cpp
#include <QtPlugin>
Q_IMPORT_PLUGIN(QWindowsVistaStylePlugin);
```

## 内存泄漏

### 诊断工具

1. **Visual Studio 诊断工具**：
   - 调试 → 性能探查器 → 内存使用

2. **VLD (Visual Leak Detector)**：

```cmake
find_package(VLD REQUIRED)
target_link_libraries(EnhanceVision PRIVATE VLD::VLD)
```

```cpp
// main.cpp
#include <vld.h>
```

3. **Qt 对象树检查**：

```cpp
QObject* obj = new QObject(parent);  // 父对象会自动删除
QObject* obj = new QObject();        // 需要手动删除
```

## 性能问题

### 诊断方法

```cpp
// 使用 QElapsedTimer 测量
QElapsedTimer timer;
timer.start();
// ... 代码 ...
qDebug() << "耗时:" << timer.elapsed() << "ms";

// 使用 Qt 性能分析器
// Qt Creator → Analyze → QML Profiler
```

### UI 线程阻塞

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

### 频繁重绘

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

## 故障排除流程

### 运行时错误流程

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

### 性能问题流程

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
