# 自定义标题栏功能修复计划

## 问题分析

### 问题1：MediaViewerWindow 放大查看窗口标题栏按钮不起作用

**根本原因**：`SubWindowHelper.cpp` 中的 `WM_NCHITTEST` 消息处理存在以下问题：

1. **排除区域机制未生效**：虽然 `SubWindowHelper` 类定义了 `m_excludeRegions` 成员变量和相关方法（`clearExcludeRegions`、`addExcludeRegion`），但在 `SubWindowWndProc` 函数中**完全没有使用**这些排除区域，而是硬编码了 `localX >= windowWidth - 50` 的判断逻辑。

2. **标题栏高度不一致**：
   - `SubWindowWndProc` 硬编码标题栏高度为 **40px**
   - `MediaViewerWindow.qml` 标题栏实际高度为 **44px**
   - 这导致标题栏顶部 4px 区域无法正确响应拖拽

**代码位置**：[SubWindowHelper.cpp:37-87](file:///e:/QtAudio-VideoLearning/EnhanceVision/src/utils/SubWindowHelper.cpp#L37-L87)

```cpp
// 问题代码
int titleBarHeight = 40;  // 硬编码，与实际不符
bool inRightArea = localX >= windowWidth - 50;  // 硬编码，未使用排除区域
```

### 问题2：Shader 模式对话框标题栏设计检查

**当前设计分析**：

| 对话框 | 标题栏高度 | 排除区域实现 | 问题 |
|--------|-----------|-------------|------|
| 新建类别 | 40px | 尝试使用排除区域 | 排除区域未生效 |
| 保存风格 | 40px | 尝试使用排除区域 | 排除区域未生效 |
| 重命名类别 | 40px | 尝试使用排除区域 | 排除区域未生效 |
| 删除类别 | 40px | 尝试使用排除区域 | 排除区域未生效 |

**设计合理性**：
- 标题栏高度 40px 与 `SubWindowWndProc` 硬编码值一致，不存在高度问题
- 但排除区域机制同样未生效，依赖硬编码的 50px 右侧区域

---

## 解决方案

### 方案概述

修改 `SubWindowHelper.cpp` 中的 `SubWindowWndProc` 函数，使其：
1. 正确使用 `m_excludeRegions` 排除区域
2. 支持动态标题栏高度配置

### 详细修改步骤

#### 步骤1：修改 SubWindowHelper.h

添加 `titleBarHeight` 属性：

```cpp
// SubWindowHelper.h
Q_PROPERTY(int titleBarHeight READ titleBarHeight WRITE setTitleBarHeight NOTIFY titleBarHeightChanged)

public:
    int titleBarHeight() const { return m_titleBarHeight; }
    void setTitleBarHeight(int height);

signals:
    void titleBarHeightChanged();

private:
    int m_titleBarHeight = 40;  // 默认值
```

#### 步骤2：修改 SubWindowHelper.cpp

修改 `SubWindowWndProc` 函数，使其正确使用排除区域：

```cpp
static LRESULT CALLBACK SubWindowWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto it = g_windowHelpers.find(hwnd);
    if (it == g_windowHelpers.end()) {
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    EnhanceVision::SubWindowHelper* helper = it.value();

    switch (msg) {
    case WM_NCHITTEST: {
        RECT windowRect;
        GetWindowRect(hwnd, &windowRect);

        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        int localX = x - windowRect.left;
        int localY = y - windowRect.top;
        int windowWidth = windowRect.right - windowRect.left;
        int windowHeight = windowRect.bottom - windowRect.top;

        int titleBarHeight = helper->titleBarHeight();  // 使用动态高度
        int margin = helper->windowResizeMargin();

        // 标题栏区域处理
        if (localY >= 0 && localY < titleBarHeight) {
            // 检查是否在排除区域内
            bool inExcludeRegion = false;
            for (const QRect& region : helper->excludeRegions()) {
                if (region.contains(localX, localY)) {
                    inExcludeRegion = true;
                    break;
                }
            }
            
            if (!inExcludeRegion) {
                // 非排除区域，处理边角调整大小
                if (!helper->isWindowMaximized()) {
                    bool onLeft = localX < margin;
                    bool onRight = localX >= windowWidth - margin;
                    bool onTop = localY < margin;
                    
                    if (onTop && onLeft) return HTTOPLEFT;
                    if (onTop && onRight) return HTTOPRIGHT;
                    if (onTop) return HTTOP;
                    if (onLeft) return HTLEFT;
                    if (onRight) return HTRIGHT;
                }
                return HTCAPTION;  // 允许拖拽
            }
        }
        
        // 窗口边框调整大小
        if (!helper->isWindowMaximized()) {
            bool onLeft = localX < margin;
            bool onRight = localX >= windowWidth - margin;
            bool onTop = localY < margin;
            bool onBottom = localY >= windowHeight - margin;

            if (onTop && onLeft) return HTTOPLEFT;
            if (onTop && onRight) return HTTOPRIGHT;
            if (onBottom && onLeft) return HTBOTTOMLEFT;
            if (onBottom && onRight) return HTBOTTOMRIGHT;
            if (onLeft) return HTLEFT;
            if (onRight) return HTRIGHT;
            if (onTop) return HTTOP;
            if (onBottom) return HTBOTTOM;
        }
        break;
    }
    // ... 其他消息处理保持不变
    }
}
```

#### 步骤3：添加 excludeRegions() 访问方法

在 `SubWindowHelper` 类中添加公共访问方法：

```cpp
// SubWindowHelper.h
public:
    const QList<QRect>& excludeRegions() const { return m_excludeRegions; }
```

#### 步骤4：更新 QML 组件

**MediaViewerWindow.qml**：设置正确的标题栏高度

```qml
SubWindowHelper {
    id: windowHelper
    Component.onCompleted: {
        windowHelper.setWindow(root)
        windowHelper.setMinWidth(480)
        windowHelper.setMinHeight(360)
        windowHelper.setTitleBarHeight(44)  // 添加此行
    }
}
```

**ShaderParamsPanel.qml**：各对话框保持默认 40px 即可（与 DialogTitleBar 高度一致）

---

## 修改文件清单

| 文件 | 修改内容 |
|------|---------|
| `include/EnhanceVision/utils/SubWindowHelper.h` | 添加 `titleBarHeight` 属性和 `excludeRegions()` 访问方法 |
| `src/utils/SubWindowHelper.cpp` | 修改 `SubWindowWndProc` 使用排除区域，添加 `setTitleBarHeight` 实现 |

---

## 验证测试

修改完成后需验证：

1. **MediaViewerWindow**：
   - [ ] 标题栏按钮（全屏、关闭）可正常点击
   - [ ] 标题栏拖拽功能正常
   - [ ] 窗口边框调整大小功能正常
   - [ ] "查看原图"按钮可正常点击

2. **Shader 模式对话框**：
   - [ ] 新建类别对话框关闭按钮可正常点击
   - [ ] 保存风格对话框关闭按钮可正常点击
   - [ ] 重命名类别对话框关闭按钮可正常点击
   - [ ] 删除类别对话框关闭按钮可正常点击

---

## 风险评估

- **风险等级**：低
- **影响范围**：仅修改 `SubWindowHelper` 类，不影响其他组件
- **兼容性**：向后兼容，默认值保持 40px
