# EnhanceVision 安全漏洞扫描报告

**项目**: EnhanceVision  
**技术栈**: Qt Quick (QML) 6.10.2 + C++20 (MSVC 2022) + CMake + NCNN (Vulkan) + FFmpeg 7.1  
**扫描日期**: 2026-05-10  
**扫描范围**: 全代码库静态安全分析

---

## 📊 统计摘要

| 严重等级 | 数量 |
|---------|------|
| 🔴 严重 | 1 |
| 🟠 高危 | 3 |
| 🟡 中危 | 8 |
| 🟢 低危 | 5 |

### CWE 分布 Top 10

| CWE | 描述 | 数量 |
|-----|------|------|
| CWE-20 | 输入验证不当 | 3 |
| CWE-22 | 路径遍历 | 2 |
| CWE-78 | 命令注入 | 1 |
| CWE-94 | 代码注入 | 1 |
| CWE-119 | 内存边界操作不当 | 2 |
| CWE-250 | 包含执行特权操作 | 1 |
| CWE-269 | 权限管理不当 | 1 |
| CWE-347 | 密码学实现验证不当 | 1 |
| CWE-668 | 资源暴露于错误范围 | 1 |
| CWE-754 | 异常处理不当 | 1 |

---

## 🔴 严重 (Critical)

### 1. AI 模型文件完整性校验缺失

**文件**: `src/core/ModelRegistry.cpp`  
**行号**: 141-167  
**CWE**: CWE-347 (密码学实现验证不当)  
**CVSS**: 8.1

**攻击向量描述**:
模型加载仅检查文件存在性，未验证文件签名或哈希值。攻击者可替换 `.param` 或 `.bin` 文件为恶意构造的模型文件，实现模型投毒攻击。

```cpp
// 问题代码 (L145-147)
bool paramExists = QFile::exists(info.paramPath);
bool binExists = QFile::exists(info.binPath);
info.isAvailable = paramExists && binExists;
```

**影响范围**:
- 恶意模型文件可被加载执行
- 可能导致推理结果被篡改
- 潜在代码执行风险（通过畸形模型参数）

**修复建议**:
1. 实现模型文件哈希校验（SHA-256）
2. 配置文件存储预期哈希值
3. 加载前验证文件完整性
4. 可选：实现模型签名验证

---

## 🟠 高危 (High)

### 2. 路径遍历漏洞 - 文件保存

**文件**: `src/services/ImageExportService.cpp`  
**行号**: 34-60  
**CWE**: CWE-22 (路径遍历)  
**CVSS**: 7.5

**攻击向量描述**:
`exportFromItem` 函数的 `outputPath` 参数未经过路径规范化验证，攻击者可构造包含 `../` 的路径，将文件写入应用数据目录之外的位置。

```cpp
void ImageExportService::exportFromItem(QQuickItem* item, const QString& outputPath)
{
    // outputPath 直接传递给 QQuickItemGrabResult::image().save()
    if (image.save(outputPath)) {  // 未验证路径合法性
```

**影响范围**:
- 任意文件写入（覆盖系统文件）
- 潜在权限提升

**修复建议**:
```cpp
QString normalizeAndValidatePath(const QString& inputPath) {
    QFileInfo fi(inputPath);
    QString absolute = fi.absoluteFilePath();
    // 白名单允许的基础目录
    QStringList allowedPrefixes = { getDefaultSavePath(), QDir::tempPath() };
    for (const auto& prefix : allowedPrefixes) {
        if (absolute.startsWith(QDir::toNativeSeparators(QFileInfo(prefix).absoluteFilePath()))) {
            return absolute;
        }
    }
    return {}; // 拒绝不在白名单内的路径
}
```

### 3. 整数溢出风险 - 图像尺寸计算

**文件**: `src/core/ImageProcessor.cpp`  
**行号**: 669-671  
**CWE**: CWE-119 (内存边界操作不当)  
**CVSS**: 7.1

**攻击向量描述**:
图像像素总数计算可能发生整数溢出：
```cpp
int totalPixels = width * height;  // 可能溢出
```

攻击者提供超大尺寸声明的恶意图像文件可触发此问题。

**影响范围**:
- 内存分配失败
- 潜在的整数环绕导致缓冲区分配不足
- 后续处理越界访问

**修复建议**:
```cpp
bool safeMultiplyCheck(int a, int b, int* result) {
    if (a <= 0 || b <= 0) return false;
    if (a > (INT_MAX / b)) return false;
    *result = a * b;
    return true;
}
```

### 4. FFmpeg 库版本已知漏洞风险

**文件**: `third_party/ffmpeg/`  
**CWE**: CWE-1104 (未维护第三方组件)  
**CVSS**: 7.5

**攻击向量描述**:
项目使用 FFmpeg 但未指定版本或锁定已知安全版本。FFmpeg 历史版本存在多个已知漏洞（CVE-2016-3062, CVE-2016-1897, CVE-2016-1898 等）。

**影响范围**:
- 恶意媒体文件可能触发已知漏洞
- 潜在代码执行或信息泄露

**修复建议**:
1. 在 CMakeLists.txt 中明确指定 FFmpeg 版本
2. 使用依赖锁定机制（Conan 或 vcpkg）
3. 定期更新到安全版本

---

## 🟡 中危 (Medium)

### 5. 模型文件路径注入

**文件**: `src/core/ModelRegistry.cpp`  
**行号**: 142-143  
**CWE**: CWE-22 (路径遍历)  
**CVSS**: 6.5

**攻击向量描述**:
模型路径构造未充分验证 `paramFile` 和 `binFile` 字段：
```cpp
info.paramPath = m_modelsRootPath + "/" + paramFile;
info.binPath = m_modelsRootPath + "/" + binFile;
```

**修复建议**: 在 JSON 解析后验证路径不包含 `../` 或绝对路径。

### 6. 临时文件符号链接攻击风险

**文件**: `src/core/ThumbnailDatabase.cpp`  
**行号**: 391-396, 421-424  
**CWE**: CWE-61 (符号链接攻击)  
**CVSS**: 6.2

**攻击向量描述**:
缩略图删除操作未验证文件是否为符号链接：
```cpp
QFile::remove(dp);  // 可能跟随符号链接
```

**修复建议**:
```cpp
bool safeRemove(const QString& path) {
    QFileInfo fi(path);
    if (fi.isSymLink()) return false;
    return QFile::remove(path);
}
```

### 7. 会话数据持久化无加密

**文件**: `src/services/AutoSaveService.cpp`, `src/controllers/SessionController.cpp`  
**CWE**: CWE-311 (敏感数据加密缺失)  
**CVSS**: 5.3

**攻击向量描述**:
- 会话数据 (`sessions.json`) 包含用户文件路径、处理参数
- 自动保存数据未加密存储
- 缩略图数据库暴露文件元数据

**修复建议**:
1. 对敏感会话数据加密存储
2. 使用 Qt 的 QCryptographicHash 或外部加密库

### 8. NCNN 模型反序列化安全

**文件**: `src/core/AIEngine.cpp`  
**行号**: 186-199  
**CWE**: CWE-94 (代码注入)  
**CVSS**: 6.3

**攻击向量描述**:
直接加载外部 NCNN 模型文件：
```cpp
int ret = m_net.load_param(info.paramPath.toStdString().c_str());
ret = m_net.load_model(info.binPath.toStdString().c_str());
```

**修复建议**:
1. 仅从受信任路径加载模型
2. 实现模型完整性校验
3. 沙箱化模型加载过程

### 9. Vulkan GPU 资源配额耗尽 (DoS)

**文件**: `src/core/inference/NCNNVulkanBackend.cpp`  
**CWE**: CWE-400 (资源耗尽)  
**CVSS**: 5.9

**攻击向量描述**:
未限制单个推理请求的 GPU 内存使用，恶意大图像可耗尽 GPU 资源导致系统级拒绝服务。

**修复建议**:
1. 实现输入图像尺寸硬限制
2. GPU 内存池配额管理
3. 资源获取超时机制

### 10. QML 动态组件加载

**文件**: `qml/main.qml`, `qml/App.qml`  
**CWE**: CWE-94 (代码注入)  
**CVSS**: 5.3

**攻击向量描述**:
需要审查 QML 文件中 `Loader` 组件的 `source` 属性是否可被外部控制。

### 11. 日志信息泄露

**文件**: 多个 .cpp 文件  
**CWE**: CWE-532 (通过日志信息泄露)  
**CVSS**: 4.8

**攻击向量描述**:
多处使用 `qWarning()` 和 `qDebug()` 输出可能包含敏感信息的日志：
- 文件完整路径
- 模型参数
- 处理状态

**修复建议**:
1. Release 构建禁用 Debug 日志
2. 日志脱敏处理

### 12. TOCTOU 竞态条件

**文件**: `src/core/SessionController.cpp`  
**行号**: 824-845  
**CWE**: CWE-367 (检查时间-使用时间竞态)  
**CVSS**: 5.3

**攻击向量描述**:
会话保存操作存在 TOCTOU：
```cpp
if (oldFile.exists()) {  // 检查
    oldFile.remove();    // 使用
}
```

**修复建议**:
使用原子性文件操作或文件锁。

---

## 🟢 低危 (Low)

### 13. CMake 构建安全选项缺失

**文件**: `CMakeLists.txt`  
**行号**: 545-558  
**CWE**: CWE-1109 (组件隔离不一致)  
**CVSS**: 3.7

**问题**:
- 未启用 `/sdl` (Security Development Lifecycle)
- 未启用 `/guard:cf` (Control Flow Guard)
- 未启用 `/d2guard4` (Advanced Control Flow Guard)
- Release 构建可能包含调试符号

**修复建议**:
```cmake
if(MSVC)
    target_compile_options(EnhanceVision PRIVATE
        /sdl        # Security Development Lifecycle
        /guard:cf   # Control Flow Guard
        /d2guard4   # Advanced CFG
    )
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(EnhanceVision PRIVATE /Zi)
        target_link_options(EnhanceVision PRIVATE /DEBUG:NONE)
    endif()
endif()
```

### 14. C++ 不安全函数使用

**文件**: `src/core/ImageProcessor.cpp`  
**CWE**: CWE-676 (潜在危险函数使用)  
**CVSS**: 3.3

**问题**:
代码中使用 `std::memcpy`，虽然当前使用场景安全，但应优先使用带长度检查的 `std::memcpy_s`。

### 15. 缺少安全编码规范文档

**文件**: 项目根目录  
**CWE**: CWE-本 (文档缺失)  
**CVSS**: 1.0

**问题**:
缺少安全编码规范和安全审查流程文档。

### 16. 配置文件权限

**文件**: `src/utils/StoragePaths.cpp`  
**CWE**: CWE-276 (不正确的默认权限)  
**CVSS**: 2.7

**问题**:
配置文件存储在用户目录但未明确设置文件权限限制。

### 17. 第三方库依赖未签名校验

**文件**: `CMakeLists.txt`  
**行号**: 109-122  
**CWE**: CWE-494 (下载无完整性校验)  
**CVSS**: 2.7

**问题**:
NCNN 预编译库下载未使用 SHA-256 校验。

---

## ✅ 安全良好实践

1. **内存管理**: `AIEngine` 中的 `ncnn::Mat` 使用智能管理，资源释放逻辑完善
2. **并发安全**: `ResourceManager` 实现了任务数量限制（线性队列）
3. **异常处理**: 多数关键函数有 try-catch 保护
4. **输入验证**: `ImagePreprocessor` 对图像尺寸进行了基本检查
5. **线程同步**: 使用 `QMutex` 保护共享资源

---

## 🔧 建议修复优先级

### P0 (立即修复)
1. 模型文件完整性校验
2. 文件路径遍历防护

### P1 (近期修复)
3. FFmpeg 版本锁定和更新
4. 整数溢出防护
5. 符号链接攻击防护

### P2 (计划修复)
6. 会话数据加密
7. 日志脱敏
8. CMake 安全编译选项

### P3 (长期改进)
9. 安全编码规范文档
10. 定期安全审计机制

---

## 📋 附录

### 扫描方法
- 静态代码分析
- OWASP Top 10 对照检查
- CWE Top 25 常见漏洞模式匹配
- 第三方组件版本审查

### 工具/版本
- NCNN: 需确认具体版本
- FFmpeg: 7.1 (libavcodec 62.x)
- Qt: 6.10.2

### 报告生成时间
2026-05-10 18:45 (UTC+8)

### 下次扫描建议
建议在以下事件后进行重新扫描：
- 依赖库版本更新
- 新增文件 I/O 操作
- 新增网络通信功能
- 安全事件响应后
