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

    // ========== 暗色主题 (Dark Theme) - 克莱因蓝深色系 ==========
    readonly property var dark: QtObject {
        // 基础颜色 - 深邃蓝黑
        readonly property color background: "#050A18"
        readonly property color foreground: "#E8EDF5"

        // 卡片和容器 - 深蓝灰
        readonly property color card: "#0A1428"
        readonly property color cardForeground: "#E8EDF5"
        readonly property color cardBorder: "#152040"

        // 弹出层
        readonly property color popover: "#0A1428"
        readonly property color popoverForeground: "#E8EDF5"

        // 主要强调色 - 克莱因蓝系 (Klein Blue #002FA7)
        readonly property color primary: "#1E56D0"
        readonly property color primaryForeground: "#FFFFFF"
        readonly property color primaryHover: "#2E6AE6"
        readonly property color primarySubtle: "#0D1A38"
        readonly property color primaryLight: "#5B8DEF"

        // 次要色 - 深蓝灰系
        readonly property color secondary: "#152040"
        readonly property color secondaryForeground: "#C0CDE0"
        readonly property color secondaryHover: "#1A2850"

        // 柔和色
        readonly property color muted: "#152040"
        readonly property color mutedForeground: "#7A8BA8"

        // 强调色 - 深宝石蓝
        readonly property color accent: "#152040"
        readonly property color accentForeground: "#5B8DEF"

        // 危险色 - 亮红色系
        readonly property color destructive: "#EF4444"
        readonly property color destructiveForeground: "#FFFFFF"
        readonly property color destructiveSubtle: "#1A0A0A"

        // 边框和输入框
        readonly property color border: "#152040"
        readonly property color borderHover: "#1E3060"
        readonly property color input: "#0A1428"
        readonly property color inputBackground: "#080E20"
        readonly property color inputBorder: "#1E3060"

        // 开关
        readonly property color switchBackground: "#1E3060"
        readonly property color switchChecked: "#1E56D0"

        // 焦点环
        readonly property color ring: "#1E56D0"
        readonly property color ringOffset: "#050A18"

        // 图表颜色 - 克莱因蓝渐变
        readonly property color chart1: "#5B8DEF"
        readonly property color chart2: "#1E56D0"
        readonly property color chart3: "#7AA2F7"
        readonly property color chart4: "#002FA7"
        readonly property color chart5: "#A0C4FF"

        // 侧边栏 - 深蓝色系
        readonly property color sidebar: "#040812"
        readonly property color sidebarForeground: "#C0CDE0"
        readonly property color sidebarBorder: "#152040"
        readonly property color sidebarPrimary: "#1E56D0"
        readonly property color sidebarPrimaryForeground: "#FFFFFF"
        readonly property color sidebarAccent: "#152040"
        readonly property color sidebarAccentForeground: "#E8EDF5"

        // 克莱因蓝 (Klein Blue) - 品牌色
        readonly property color kleinBlue: "#002FA7"
        readonly property color kleinBlueLight: "#1E56D0"
        readonly property color kleinBlueDark: "#001870"

        // 补充颜色
        readonly property color success: "#34D399"
        readonly property color successSubtle: "#052E16"
        readonly property color warning: "#FBBF24"
        readonly property color warningSubtle: "#1C1A05"
        readonly property color surface: "#0A1428"
        readonly property color surfaceHover: "#152040"

        // 标题栏 - 深邃蓝色
        readonly property color titleBar: "#060C1A"
        readonly property color titleBarBorder: "#152040"

        // 图标颜色 - 淡蓝色系
        readonly property color icon: "#7AA2F7"
        readonly property color iconHover: "#A0C4FF"
        readonly property color iconActive: "#5B8DEF"
    }
}
