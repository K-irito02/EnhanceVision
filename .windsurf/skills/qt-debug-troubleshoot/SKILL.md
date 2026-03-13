---
name: "qt-debug-troubleshoot"
description: "用于在 Qt、多线程、网络编程与音视频处理场景下进行代码审查、定位报错并修复问题的技能。"
---

# Qt Debug & Troubleshooting Skill

面向 Qt GUI、RHI、多线程、网络通信以及音视频处理模块的系统化排查与修复流程，确保在提交代码前完成严格的审查与验证。

## 适用场景
- 新增或修改 Qt 相关功能后需要做静态/动态代码审查。
- 出现多线程竞态、死锁、内存泄漏或 UI 卡顿等问题。
- 网络收发、协议解析、异步 IO 行为异常。
- 音视频编解码、渲染、同步流程中的崩溃、花屏、音画不同步等问题。
- 需要对复杂回归问题进行分层定位并提供修复策略。

## 执行流程
1. **准备与信息收集**
   - 阅读需求/变更说明以及相关日志、崩溃堆栈。
   - 确认编译器、Qt 版本、第三方库版本保持一致。
   - 若有复现脚本或录屏，先行整理。

2. **静态代码审查**
   - 按模块梳理关键类（UI、核心线程、网络、音视频管线），检查接口契约、锁粒度、信号槽线程亲和性。
   - 对照 `.trae/rules` 与项目编码规范，标记潜在风险点（未初始化成员、悬空指针、越界访问等）。

3. **最小可复现与断点规划**
   - 构建精简复现路径：优先在 Debug 配置下运行，必要时注入测试桩。
   - 预先规划断点/日志注入位置：线程创建、重要回调、缓冲区写入、网络报文入口等。

4. **动态调试策略**
   - 使用 Qt Creator/MSVC 调试器观察线程栈，配合 `QLoggingCategory`、`qCDebug` 增加分类日志。
   - 多线程问题：检查 `QMetaObject::invokeMethod` 的连接类型、`QMutex/QReadWriteLock` 使用是否匹配、线程间资源交接是否复制数据。
   - 网络问题：抓包（Wireshark）或启用协议级日志，验证超时/重传策略。
   - 音视频问题：核对时间戳、缓冲队列深度、GPU 纹理生命周期，必要时使用 RenderDoc/PIX。

5. **修复与代码更新**
   - 根据定位结论实施最小化修复：加锁顺序调整、队列去抖、重试机制、资源释放等。
   - 补充单元/集成测试或最少添加断言与日志，防止回归。
   - 确保跨线程信号槽使用 `Qt::QueuedConnection` 或 `QThread::currentThread()` 检查。

6. **验证与回归**
   - 重跑复现用例 + 全量关键路径（渲染、编码、网络同步）。
   - 监控 CPU/GPU/内存占用，确认无新警告或泄漏。
   - 更新问题记录，输出根因、修复摘要及后续建议。

## 常用工具与命令
- `cmake --build . --config Debug`：获取含调试符号的构建。
- Qt 调试：`QT_DEBUG_PLUGINS=1`, `QT_FATAL_WARNINGS=1` 环境变量。
- 日志：`QLoggingCategory::setFilterRules("*.debug=true")`。
- 多线程分析：`ConcrtResourceViewer`、`VS Thread Analyzer`、`QFutureWatcher`。
- 网络诊断：`tcpdump`/`Wireshark`、`QtNetworkAuth` 调试输出。
- 音视频：`ffprobe`、`RenderDoc`、`PIX`、自定义帧导出工具。

## 注意事项
- 保持线程安全原则：UI 线程只做 UI 操作，耗时任务放入 `QThreadPool`/`QtConcurrent`。
- 优先使用 RAII（`QScopedPointer`, `std::unique_ptr`）避免资源泄漏。
- 任何修复必须附带原因说明与验证结论，必要时在 MR/PR 中同步。
- 若问题可能影响发布版本，立刻通知维护者并创建 hotfix 任务。
