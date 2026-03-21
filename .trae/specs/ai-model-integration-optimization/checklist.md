# Checklist

## 模型配置检查

- [x] models.json 中所有模型都有完整的 supportedParams 配置
- [x] RIFE 视频插帧模型已添加到 models.json
- [x] 所有参数都有正确的类型定义（int/float/bool/enum/string）
- [x] 所有参数都有默认值定义
- [x] 所有参数都有 min/max 范围定义（数值类型）

## UI 组件检查

- [x] AIParamsPanel 正确显示 bool 类型参数（Switch 控件）
- [x] AIParamsPanel 正确显示 enum 类型参数（ComboBox 控件）
- [x] AIParamsPanel 正确显示 int/float 类型参数（Slider 控件）
- [x] 参数标签支持国际化显示
- [x] 参数值变化正确触发 paramsChanged 信号

## 参数处理检查

- [x] AIEngine 正确合并用户参数和默认参数
- [x] 参数值在有效范围内
- [x] 未设置的参数使用默认值
- [x] 参数正确传递到推理流程

## 功能测试检查

- [x] 选择不同模型时参数面板正确更新
- [x] 修改参数后推理结果正确
- [x] GPU/CPU 切换正常工作
- [x] 分块大小设置正常工作

## 构建和运行检查

- [x] 项目成功构建
- [x] 程序正常启动
- [x] 无运行时错误或警告
