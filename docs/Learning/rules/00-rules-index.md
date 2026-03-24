# 规则总览（EnhanceVision）

> 目标：保证后续开发中代码统一、结构清晰、可持续维护。

## 规则文件导航

- `01-project-baseline-rules.md`：项目基线与边界
- `02-architecture-layering-rules.md`：分层架构与模块职责
- `03-cpp-backend-rules.md`：C++ 后端规范
- `04-qml-frontend-rules.md`：QML 前端规范
- `05-ui-ux-theme-rules.md`：UI/UX 与主题规范
- `06-i18n-rules.md`：国际化规范
- `07-performance-threading-rules.md`：性能与多线程规范
- `08-shader-image-rules.md`：Shader 与图像处理规范
- `09-build-release-rules.md`：构建、发布与部署规范
- `10-testing-logging-rules.md`：测试与日志规范
- `11-dependency-cross-platform-rules.md`：依赖治理与跨平台规范
- `12-maintenance-documentation-rules.md`：维护与文档同步规范

## 执行优先级

1. 架构与边界不破坏
2. 线程与性能不退化
3. 代码风格与目录结构统一
4. 文档与规则同步更新

## 通用红线

- 不在 UI 线程执行耗时任务
- 不在 QML 中堆叠复杂业务逻辑
- 不引入未经评估的第三方依赖
- 不提交无用代码、无用资源、无说明变更
