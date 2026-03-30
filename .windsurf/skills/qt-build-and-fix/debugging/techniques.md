# 本地调试技巧

## 核心理念

**所有调试操作在本地编辑器完成，不依赖第三方调试工具。**

- 通过添加日志信息进行诊断
- 分析系统日志和运行时输出
- 使用命令行工具检查进程和依赖
- 避免使用外部IDE调试器

## 日志驱动调试

### 增强日志输出

```cpp
// 在关键位置添加详细日志
qInfo() << "[ModuleName][FunctionName] step 1"
        << "variable1:" << value1
        << "variable2:" << value2
        << "thread:" << QThread::currentThreadId();
fflush(stdout);  // 立即刷新，确保崩溃前日志已写入

// 危险操作前添加检查
qInfo() << "[ModuleName][FunctionName] before dangerous operation"
        << "input_size:" << input.size()
        << "input_format:" << input.format();
fflush(stdout);

// 危险操作
result = dangerousOperation();

qInfo() << "[ModuleName][FunctionName] after dangerous operation"
        << "result:" << result
        << "result_valid:" << !result.isNull();
fflush(stdout);
```

### 日志格式标准化

```cpp
// 设置统一日志格式
qSetMessagePattern("[%{time yyyy-MM-dd hh:mm:ss.zzz}] [%{type}] [%{threadid}] %{message}");

// 日志级别使用规范
qDebug() << "[DEBUG] 详细调试信息";   // 开发阶段使用
qInfo() << "[INFO] 一般信息";        // 正常流程信息
qWarning() << "[WARN] 警告信息";     // 可恢复的问题
qCritical() << "[CRIT] 错误信息";    // 严重错误
```

## Windows系统诊断

### 进程状态检查

```powershell
# 查看所有 EnhanceVision 进程
Get-Process | Where-Object { $_.Name -like "*EnhanceVision*" } | Select-Object Id, ProcessName, CPU, StartTime

# 检查进程是否响应
$process = Get-Process -Name "EnhanceVision" -ErrorAction SilentlyContinue
if ($process) {
    Write-Host "进程状态: 响应=$($process.Responding) CPU=$($process.CPU)% 内存=$($process.WorkingSet64/1MB)MB"
} else {
    Write-Host "进程未运行"
}
```

### 系统事件日志分析

```powershell
# 查看最近的程序崩溃事件
Get-WinEvent -FilterHashtable @{
    LogName='Application'; 
    ProviderName='Application Error'
} -MaxEvents 5 2>$null | Select-Object TimeCreated, Message | Format-List

# 查看程序相关的所有事件
Get-WinEvent -FilterHashtable @{
    LogName='Application';
    Data='EnhanceVision.exe'
} -MaxEvents 10 2>$null | Select-Object TimeCreated, LevelDisplayName, Message
```

### DLL依赖检查

```powershell
# 检查程序DLL依赖
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

## QML调试技巧

### QML日志输出

```qml
// 使用 console.log 进行调试
import QtQuick

Item {
    function testFunction(value) {
        console.log("[QML][testFunction] input:", value, "type:", typeof value)
        
        // 处理逻辑
        let result = value * 2
        
        console.log("[QML][testFunction] output:", result)
        return result
    }
}
```

### 属性绑定调试

```qml
// 监听属性变化
Rectangle {
    width: 100
    height: 100
    
    onWidthChanged: {
        console.log("[QML][Rectangle] width changed:", width)
    }
    
    onHeightChanged: {
        console.log("[QML][Rectangle] height changed:", height)
    }
}
```

## 多线程调试

### 线程安全日志

```cpp
// 线程安全的日志输出
void logThreadInfo(const QString& functionName) {
    qInfo() << "[Thread]" << QThread::currentThreadId() 
            << "[Function]" << functionName 
            << "[IsMainThread]" << (QThread::currentThread() == qApp->thread());
}

// 在函数开始和结束时调用
void processImage(const QImage& image) {
    logThreadInfo("processImage");
    
    qInfo() << "[processImage] image size:" << image.size() 
            << "format:" << image.format();
    
    // 处理逻辑
    
    logThreadInfo("processImage");
}
```

### 信号槽连接调试

```cpp
// 检查信号槽连接结果
bool connectResult = connect(sender, &Sender::signal, 
                           receiver, &Receiver::slot);
if (!connectResult) {
    qWarning() << "[SignalSlot] 连接失败:" 
               << "sender:" << sender->metaObject()->className()
               << "receiver:" << receiver->metaObject()->className();
} else {
    qInfo() << "[SignalSlot] 连接成功:" 
            << "sender:" << sender->metaObject()->className()
            << "receiver:" << receiver->metaObject()->className();
}
```

## 内存问题诊断

### 内存泄漏检测

```cpp
// 对象生命周期跟踪
class ObjectTracker {
public:
    static void trackObject(QObject* obj, const QString& type) {
        qInfo() << "[Memory] Object created:" 
                << "type:" << type 
                << "address:" << obj 
                << "thread:" << QThread::currentThreadId();
    }
    
    static void untrackObject(QObject* obj, const QString& type) {
        qInfo() << "[Memory] Object destroyed:" 
                << "type:" << type 
                << "address:" << obj 
                << "thread:" << QThread::currentThreadId();
    }
};

// 在构造函数和析构函数中使用
MyClass::MyClass() {
    ObjectTracker::trackObject(this, "MyClass");
}

MyClass::~MyClass() {
    ObjectTracker::untrackObject(this, "MyClass");
}
```

### 崩溃点定位

```cpp
// 在可能崩溃的位置添加检查点
void riskyOperation(QImage* image) {
    qInfo() << "[CrashDebug] Entering riskyOperation" 
            << "image ptr:" << image 
            << "image null:" << (image == nullptr);
    fflush(stdout);
    
    if (!image) {
        qCritical() << "[CrashDebug] Null image pointer!";
        return;
    }
    
    qInfo() << "[CrashDebug] Image info:" 
            << "size:" << image->size() 
            << "format:" << image->format() 
            << "bytesPerLine:" << image->bytesPerLine();
    fflush(stdout);
    
    // 危险操作
    QImage result = image->convertToFormat(QImage::Format_RGB888);
    
    qInfo() << "[CrashDebug] Operation completed" 
            << "result null:" << result.isNull() 
            << "result size:" << result.size();
    fflush(stdout);
}
```

## 性能分析

### 简单性能测量

```cpp
// 使用 QElapsedTimer 测量执行时间
void measurePerformance() {
    QElapsedTimer timer;
    timer.start();
    
    // 要测量的代码
    processLargeDataset();
    
    qint64 elapsed = timer.elapsed();
    qInfo() << "[Performance] processLargeDataset took:" << elapsed << "ms";
    
    if (elapsed > 1000) {
        qWarning() << "[Performance] Slow operation detected:" << elapsed << "ms";
    }
}
```

## 注意事项

- **日志刷新**：在可能崩溃的位置使用 `fflush(stdout)` 确保日志写入
- **线程信息**：在多线程环境中始终记录线程ID
- **异常捕获**：使用 try-catch 包装可能抛出异常的代码
- **资源检查**：在使用资源前检查指针和对象有效性
- **避免阻塞**：不要在UI线程中添加过多的日志输出
