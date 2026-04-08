# 崩溃诊断

## Windows 事件日志查看

当程序崩溃时，Windows 会将崩溃信息记录到事件日志中：

```powershell
# 查看最近的程序崩溃事件
Get-WinEvent -FilterHashtable @{LogName='Application'; ProviderName='Application Error'} -MaxEvents 5 2>$null | Select-Object TimeCreated, Message | Format-List
```

**输出示例**：
```
TimeCreated : 2026/3/26 0:33:19
Message     : 出错应用程序名称： EnhanceVision.exe，版本： 0.0.0.0
              出错模块名称： ntdll.dll
              异常代码： 0xc0000374
              错误偏移： 0x00000000001176e5
```

## 常见异常代码

| 异常代码 | 含义 | 常见原因 |
|----------|------|----------|
| `0xc0000005` | 访问违规 | 空指针、越界访问、使用已释放的内存 |
| `0xc0000374` | 堆损坏 | 内存越界写入、双重释放、使用已释放的内存 |
| `0xc0000409` | 栈缓冲区溢出 | 局部数组越界、递归过深 |
| `0xc000001d` | 非法指令 | 函数指针错误、虚函数表损坏 |
| `0xc00000fd` | 栈溢出 | 无限递归、过大的局部变量 |

## 进程管理命令

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

## 崩溃诊断流程

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

## 内存问题诊断

```powershell
# 检查是否有崩溃转储文件
Get-ChildItem -Path $env:LOCALAPPDATA\CrashDumps -Filter "*.dmp" -ErrorAction SilentlyContinue | Select-Object Name, LastWriteTime, Length

# 检查程序目录下的转储文件
Get-ChildItem -Path "build\msvc2022\Release\Release" -Filter "*.dmp" -Recurse -ErrorAction SilentlyContinue | Select-Object Name, LastWriteTime
```

## Visual Studio 调试崩溃

1. 打开 Visual Studio
2. 调试 → 附加到进程 → 选择 EnhanceVision.exe
3. 调试 → 窗口 → 异常设置 → 勾选 "C++ Exceptions"
4. 当程序崩溃时，VS 会自动断点在崩溃位置

## WinDbg 分析

```powershell
# 基本命令
!analyze -v           # 分析崩溃原因
k                     # 查看调用栈
~*k                   # 查看所有线程调用栈
!heap -p -a <地址>    # 查看堆信息
```

## 常见崩溃场景

### QImage 格式转换崩溃

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

### 内存访问越界

```cpp
// 添加边界检查
qInfo() << "Array size:" << array.size() << "accessing index:" << index;
if (index < 0 || index >= array.size()) {
    qWarning() << "Index out of bounds!";
    return;
}
```

### 堆损坏

```cpp
// 使用安全的拷贝方式
const int srcBytesPerLine = src.bytesPerLine();
const int dstBytesPerLine = dst.bytesPerLine();
const int copyBytes = std::min(srcBytesPerLine, dstBytesPerLine);
std::memcpy(dst.bits(), src.constBits(), copyBytes);
```

### 多线程问题

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
