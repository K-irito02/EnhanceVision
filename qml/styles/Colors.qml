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
        readonly property color background: "#F0F4FA"
        readonly property color foreground: "#0A1628"

        // 卡片和容器
        readonly property color card: "#FFFFFF"
        readonly property color cardForeground: "#0A1628"
        readonly property color cardBorder: "#D0DAE8"

        // 弹出层
        readonly property color popover: "#FFFFFF"
        readonly property color popoverForeground: "#0A1628"

        // 主要强调色 - 克莱因蓝系
        readonly property color primary: "#002FA7"
        readonly property color primaryForeground: "#FFFFFF"
        readonly property color primaryHover: "#0040C8"
        readonly property color primarySubtle: "#E8EEFB"
        readonly property color primaryLight: "#5B8DEF"

        // 次要色 - 蓝灰系
        readonly property color secondary: "#E8EDF5"
        readonly property color secondaryForeground: "#1A2744"
        readonly property color secondaryHover: "#D0DAE8"

        // 柔和色
        readonly property color muted: "#E8EDF5"
        readonly property color mutedForeground: "#5A6A85"

        // 强调色 - 宝石蓝系
        readonly property color accent: "#E0ECFF"
        readonly property color accentForeground: "#002FA7"

        // 危险色 - 红色系
        readonly property color destructive: "#DC2626"
        readonly property color destructiveForeground: "#FFFFFF"
        readonly property color destructiveSubtle: "#FEF2F2"

        // 边框和输入框
        readonly property color border: "#D0DAE8"
        readonly property color borderHover: "#A8B8D0"
        readonly property color input: "#FFFFFF"
        readonly property color inputBackground: "#F5F8FC"
        readonly property color inputBorder: "#C0CDE0"

        // 开关
        readonly property color switchBackground: "#C0CDE0"
        readonly property color switchChecked: "#002FA7"

        // 焦点环
        readonly property color ring: "#002FA7"
        readonly property color ringOffset: "#FFFFFF"

        // 图表颜色
        readonly property color chart1: "#002FA7"
        readonly property color chart2: "#1A56DB"
        readonly property color chart3: "#3B82F6"
        readonly property color chart4: "#60A5FA"
        readonly property color chart5: "#93C5FD"

        // 侧边栏
        readonly property color sidebar: "#F5F8FC"
        readonly property color sidebarForeground: "#2A3F5F"
        readonly property color sidebarBorder: "#D0DAE8"
        readonly property color sidebarPrimary: "#002FA7"
        readonly property color sidebarPrimaryForeground: "#FFFFFF"
        readonly property color sidebarAccent: "#E0ECFF"
        readonly property color sidebarAccentForeground: "#0A1628"

        // 克莱因蓝 (Klein Blue) - 品牌色
        readonly property color kleinBlue: "#002FA7"
        readonly property color kleinBlueLight: "#1A56DB"
        readonly property color kleinBlueDark: "#001D70"

        // 补充颜色
        readonly property color success: "#059669"
        readonly property color successSubtle: "#ECFDF5"
        readonly property color warning: "#D97706"
        readonly property color warningSubtle: "#FFFBEB"
        readonly property color surface: "#FFFFFF"
        readonly property color surfaceHover: "#F5F8FC"

        // 标题栏
        readonly property color titleBar: "#FFFFFF"
        readonly property color titleBarBorder: "#D0DAE8"

        // 图标颜色
        readonly property color icon: "#5A6A85"
        readonly property color iconHover: "#002FA7"
        readonly property color iconActive: "#002FA7"
    }

    // ========== 暗色主题 (Dark Theme) ==========
    readonly property var dark: QtObject {
        // 基础颜色
        readonly property color background: "#080C14"
        readonly property color foreground: "#E8EDF5"

        // 卡片和容器
        readonly property color card: "#0F1520"
        readonly property color cardForeground: "#E8EDF5"
        readonly property color cardBorder: "#1A2744"

        // 弹出层
        readonly property color popover: "#0F1520"
        readonly property color popoverForeground: "#E8EDF5"

        // 主要强调色 - 亮克莱因蓝系
        readonly property color primary: "#3B82F6"
        readonly property color primaryForeground: "#FFFFFF"
        readonly property color primaryHover: "#60A5FA"
        readonly property color primarySubtle: "#0F1D3A"
        readonly property color primaryLight: "#93C5FD"

        // 次要色 - 深蓝灰系
        readonly property color secondary: "#1A2744"
        readonly property color secondaryForeground: "#C0CDE0"
        readonly property color secondaryHover: "#243456"

        // 柔和色
        readonly property color muted: "#1A2744"
        readonly property color mutedForeground: "#7A8BA8"

        // 强调色 - 深宝石蓝
        readonly property color accent: "#1A2744"
        readonly property color accentForeground: "#60A5FA"

        // 危险色 - 亮红色系
        readonly property color destructive: "#EF4444"
        readonly property color destructiveForeground: "#FFFFFF"
        readonly property color destructiveSubtle: "#1A0A0A"

        // 边框和输入框
        readonly property color border: "#1A2744"
        readonly property color borderHover: "#243456"
        readonly property color input: "#0F1520"
        readonly property color inputBackground: "#0A1020"
        readonly property color inputBorder: "#243456"

        // 开关
        readonly property color switchBackground: "#243456"
        readonly property color switchChecked: "#3B82F6"

        // 焦点环
        readonly property color ring: "#3B82F6"
        readonly property color ringOffset: "#080C14"

        // 图表颜色
        readonly property color chart1: "#60A5FA"
        readonly property color chart2: "#3B82F6"
        readonly property color chart3: "#93C5FD"
        readonly property color chart4: "#1A56DB"
        readonly property color chart5: "#BFDBFE"

        // 侧边栏
        readonly property color sidebar: "#060A12"
        readonly property color sidebarForeground: "#C0CDE0"
        readonly property color sidebarBorder: "#1A2744"
        readonly property color sidebarPrimary: "#3B82F6"
        readonly property color sidebarPrimaryForeground: "#FFFFFF"
        readonly property color sidebarAccent: "#1A2744"
        readonly property color sidebarAccentForeground: "#E8EDF5"

        // 克莱因蓝 (Klein Blue) - 品牌色
        readonly property color kleinBlue: "#002FA7"
        readonly property color kleinBlueLight: "#3B82F6"
        readonly property color kleinBlueDark: "#001D70"

        // 补充颜色
        readonly property color success: "#34D399"
        readonly property color successSubtle: "#052E16"
        readonly property color warning: "#FBBF24"
        readonly property color warningSubtle: "#1C1A05"
        readonly property color surface: "#0F1520"
        readonly property color surfaceHover: "#1A2744"

        // 标题栏
        readonly property color titleBar: "#0A1020"
        readonly property color titleBarBorder: "#1A2744"

        // 图标颜色
        readonly property color icon: "#7A8BA8"
        readonly property color iconHover: "#60A5FA"
        readonly property color iconActive: "#3B82F6"
    }
}
