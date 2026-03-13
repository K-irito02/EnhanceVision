pragma Singleton
import QtQuick

/**
 * @brief 颜色系统单例
 * 定义亮色主题和暗色主题的完整颜色集合
 * 参考功能设计文档 11.2 节
 */
QtObject {
    // ========== 亮色主题 (Light Theme) ==========
    readonly property var light: QtObject {
        // 基础颜色
        readonly property color background: "#fafafa"
        readonly property color foreground: "#18181b"

        // 卡片和容器
        readonly property color card: "#ffffff"
        readonly property color cardForeground: "#18181b"
        readonly property color cardBorder: "#e4e4e7"

        // 弹出层
        readonly property color popover: "#ffffff"
        readonly property color popoverForeground: "#18181b"

        // 主要强调色 - 蓝色系
        readonly property color primary: "#2563eb"
        readonly property color primaryForeground: "#ffffff"
        readonly property color primaryHover: "#1d4ed8"
        readonly property color primarySubtle: "#eff6ff"

        // 次要色 - 灰色系
        readonly property color secondary: "#f4f4f5"
        readonly property color secondaryForeground: "#27272a"
        readonly property color secondaryHover: "#e4e4e7"

        // 柔和色
        readonly property color muted: "#f4f4f5"
        readonly property color mutedForeground: "#71717a"

        // 强调色 - 紫色系
        readonly property color accent: "#f3e8ff"
        readonly property color accentForeground: "#6b21a8"

        // 危险色 - 红色系
        readonly property color destructive: "#dc2626"
        readonly property color destructiveForeground: "#ffffff"
        readonly property color destructiveSubtle: "#fef2f2"

        // 边框和输入框
        readonly property color border: "#e4e4e7"
        readonly property color borderHover: "#d4d4d8"
        readonly property color input: "#ffffff"
        readonly property color inputBackground: "#fafafa"
        readonly property color inputBorder: "#d4d4d8"

        // 开关
        readonly property color switchBackground: "#d4d4d8"
        readonly property color switchChecked: "#2563eb"

        // 焦点环
        readonly property color ring: "#2563eb"
        readonly property color ringOffset: "#ffffff"

        // 图表颜色
        readonly property color chart1: "#2563eb"
        readonly property color chart2: "#7c3aed"
        readonly property color chart3: "#db2777"
        readonly property color chart4: "#ea580c"
        readonly property color chart5: "#16a34a"

        // 侧边栏
        readonly property color sidebar: "#f8fafc"
        readonly property color sidebarForeground: "#334155"
        readonly property color sidebarBorder: "#e2e8f0"
        readonly property color sidebarPrimary: "#2563eb"
        readonly property color sidebarPrimaryForeground: "#ffffff"
        readonly property color sidebarAccent: "#f1f5f9"
        readonly property color sidebarAccentForeground: "#1e293b"

        // 克莱因蓝 (Klein Blue) - 特殊强调
        readonly property color kleinBlue: "#002FA7"
        
        // 补充颜色
        readonly property color success: "#16a34a"
        readonly property color surface: "#ffffff"
    }

    // ========== 暗色主题 (Dark Theme) ==========
    readonly property var dark: QtObject {
        // 基础颜色
        readonly property color background: "#0f0f11"
        readonly property color foreground: "#fafafa"

        // 卡片和容器
        readonly property color card: "#18181b"
        readonly property color cardForeground: "#fafafa"
        readonly property color cardBorder: "#27272a"

        // 弹出层
        readonly property color popover: "#18181b"
        readonly property color popoverForeground: "#fafafa"

        // 主要强调色 - 亮蓝色系
        readonly property color primary: "#3b82f6"
        readonly property color primaryForeground: "#ffffff"
        readonly property color primaryHover: "#60a5fa"
        readonly property color primarySubtle: "#1e3a5f"

        // 次要色 - 深灰色系
        readonly property color secondary: "#27272a"
        readonly property color secondaryForeground: "#e4e4e7"
        readonly property color secondaryHover: "#3f3f46"

        // 柔和色
        readonly property color muted: "#27272a"
        readonly property color mutedForeground: "#a1a1aa"

        // 强调色 - 紫色光晕
        readonly property color accent: "#3f3f46"
        readonly property color accentForeground: "#c084fc"

        // 危险色 - 亮红色系
        readonly property color destructive: "#ef4444"
        readonly property color destructiveForeground: "#ffffff"
        readonly property color destructiveSubtle: "#450a0a"

        // 边框和输入框
        readonly property color border: "#27272a"
        readonly property color borderHover: "#3f3f46"
        readonly property color input: "#27272a"
        readonly property color inputBackground: "#18181b"
        readonly property color inputBorder: "#3f3f46"

        // 开关
        readonly property color switchBackground: "#3f3f46"
        readonly property color switchChecked: "#3b82f6"

        // 焦点环
        readonly property color ring: "#3b82f6"
        readonly property color ringOffset: "#0f0f11"

        // 图表颜色
        readonly property color chart1: "#60a5fa"
        readonly property color chart2: "#a78bfa"
        readonly property color chart3: "#f472b6"
        readonly property color chart4: "#fb923c"
        readonly property color chart5: "#4ade80"

        // 侧边栏
        readonly property color sidebar: "#0f0f11"
        readonly property color sidebarForeground: "#e2e8f0"
        readonly property color sidebarBorder: "#27272a"
        readonly property color sidebarPrimary: "#3b82f6"
        readonly property color sidebarPrimaryForeground: "#ffffff"
        readonly property color sidebarAccent: "#27272a"
        readonly property color sidebarAccentForeground: "#f8fafc"

        // 克莱因蓝 (Klein Blue) - 特殊强调
        readonly property color kleinBlue: "#002FA7"
        
        // 补充颜色
        readonly property color success: "#4ade80"
        readonly property color surface: "#18181b"
    }
}
