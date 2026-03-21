# AI 推理控制面板问题修复计划

## 问题分析

### 问题 1：类别和模型列表不支持中英切换

**根本原因**：`ModelRegistry::loadModelsJson()` 在加载类别元数据时，没有保存 `name_en` 字段，导致 `getCategories()` 中的语言切换逻辑无法生效。

**代码位置**：
- [ModelRegistry.cpp:86-94](file:///e:/QtAudio-VideoLearning/EnhanceVision/src/core/ModelRegistry.cpp#L86-L94) - 加载类别元数据时缺少 `name_en`

### 问题 2：模型列表显示异常

**分析**：从日志看，模型数据加载正常（15个模型），但用户反馈显示有问题。需要进一步调查具体表现。

### 问题 3：滚动区域冲突

**根本原因**：`ControlPanel.qml` 中的 AI 模式使用 `ScrollView` 包裹 `AIModelPanel`，而 `AIModelPanel` 内部又有自己的 `ListView` 滚动区域，导致嵌套滚动冲突。

**代码位置**：
- [ControlPanel.qml:282-323](file:///e:/QtAudio-VideoLearning/EnhanceVision/qml/components/ControlPanel.qml#L282-L323) - 外层 ScrollView
- [AIModelPanel.qml:98-203](file:///e:/QtAudio-VideoLearning/EnhanceVision/qml/components/AIModelPanel.qml#L98-L203) - 内部 ListView

---

## 修复方案

### 步骤 1：修复类别国际化字段加载

**文件**：`src/core/ModelRegistry.cpp`

**修改内容**：
在 `loadModelsJson()` 函数中，加载类别元数据时添加 `name_en` 字段：

```cpp
// 加载类别元数据
QJsonObject categories = root["categories"].toObject();
for (auto it = categories.begin(); it != categories.end(); ++it) {
    QJsonObject catObj = it.value().toObject();
    QVariantMap catMeta;
    catMeta["id"] = it.key();
    catMeta["name"] = catObj["name"].toString();
    catMeta["name_en"] = catObj["name_en"].toString();  // 添加英文名称
    catMeta["icon"] = catObj["icon"].toString();
    catMeta["order"] = catObj["order"].toInt();
    m_categoryMeta[it.key()] = catMeta;
}
```

### 步骤 2：修复滚动区域冲突

**文件**：`qml/components/ControlPanel.qml`

**修改内容**：
移除 AI 模式的外层 `ScrollView`，让 `AIModelPanel` 自己管理滚动：

```qml
// ===== AI 推理模式参数 =====
ColumnLayout {
    id: aiContentLayout
    Layout.fillWidth: true
    Layout.fillHeight: true
    spacing: 10

    Components.AIModelPanel {
        id: aiModelPanel
        Layout.fillWidth: true
        Layout.fillHeight: true

        selectedModelId: root.aiSelectedModelId
        selectedCategory: root.aiSelectedCategory

        onModelSelected: function(modelId, category) {
            root.aiSelectedModelId = modelId
            root.aiSelectedCategory = category
            aiParamsPanel.modelId = modelId
        }
    }

    Rectangle {
        Layout.fillWidth: true
        height: 1
        color: Theme.colors.border
        visible: root.aiSelectedModelId !== ""
    }

    Components.AIParamsPanel {
        id: aiParamsPanel
        Layout.fillWidth: true
        visible: root.aiSelectedModelId !== ""
        modelId: root.aiSelectedModelId
    }
}
```

### 步骤 3：调整 AIModelPanel 布局结构

**文件**：`qml/components/AIModelPanel.qml`

**修改内容**：
重新组织布局，确保：
1. 类别区域固定在顶部（不参与滚动）
2. 模型列表区域独立滚动
3. 底部模型设置区域固定（不参与滚动）

```qml
ColumnLayout {
    id: root
    ...
    
    // ========== 类别标签（固定，不滚动）==========
    RowLayout { ... }
    
    // ========== 类别网格（固定，不滚动）==========
    Flow { ... }
    
    // ========== 分隔线 ==========
    Rectangle { ... }
    
    // ========== 模型列表（独立滚动区域）==========
    ListView {
        id: modelListView
        Layout.fillWidth: true
        Layout.fillHeight: true  // 占据剩余空间
        ...
    }
}
```

当前 `AIModelPanel` 的结构已经是正确的，主要问题在于外层的 `ScrollView`。

### 步骤 4：更新翻译文件

**文件**：`resources/i18n/app_zh_CN.ts` 和 `resources/i18n/app_en_US.ts`

确保翻译文件中包含所有需要翻译的字符串。

---

## 实施顺序

1. **修改 ModelRegistry.cpp** - 添加 `name_en` 字段加载
2. **修改 ControlPanel.qml** - 移除 AI 模式的外层 ScrollView
3. **测试验证** - 构建并测试语言切换和滚动行为

---

## 预期结果

1. 语言切换时，类别名称和模型描述正确显示中/英文
2. 滚动只影响模型列表区域，类别区域和底部设置区域保持固定
3. 模型列表正常显示所有可用模型
