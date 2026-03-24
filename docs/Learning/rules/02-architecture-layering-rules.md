# 02 分层架构与模块职责规则

## 分层约束

- UI 层：`qml/`（页面、组件、控件、样式）
- 业务层：`controllers/`、`models/`、`providers/`、`services/`
- 核心层：`core/`（处理引擎、缓存、任务协调）

## 职责清晰

- `Controller`：对外能力编排与状态暴露
- `Model`：列表数据与角色字段
- `Provider`：图像/缩略图桥接
- `Service`：跨模块复用能力
- `Core`：算法、处理、调度

## 通信规则

- 优先 `Q_PROPERTY + NOTIFY` 做状态同步
- 命令型操作使用 `Q_INVOKABLE`
- 异步结果统一通过信号回传

## 禁止项

- 控制器直接操作 QML 视图对象
- QML 直接承载复杂业务流程
- 跨层循环依赖
