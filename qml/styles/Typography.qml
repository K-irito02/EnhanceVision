pragma Singleton
import QtQuick

/**
 * @brief 字体系统单例
 * 定义完整的字体集合、大小、粗细和行高
 * 参考功能设计文档 11.3 节
 */
QtObject {
    // ========== 字体家族 ==========
    readonly property string fontFamily: "Inter, -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif"
    readonly property string fontFamilyMono: "'SF Mono', Monaco, 'Cascadia Code', 'Roboto Mono', Consolas, 'Courier New', monospace"
    readonly property string fontFamilyCJK: "'PingFang SC', 'Microsoft YaHei', 'Hiragino Sans GB', 'STHeiti', sans-serif"

    // ========== 字体大小 (px) ==========
    readonly property var size: QtObject {
        readonly property int xs: 12      // 0.75rem - 辅助文字、标签
        readonly property int sm: 14      // 0.875rem - 小号文字、按钮
        readonly property int base: 16    // 1rem - 正文、输入框
        readonly property int lg: 18       // 1.125rem - 大号正文
        readonly property int xl: 20       // 1.25rem - 小标题
        readonly property int xxl: 24      // 1.5rem - 标题
        readonly property int xxxl: 30     // 1.875rem - 大标题
        readonly property int xxxxl: 36    // 2.25rem - 超大标题
    }

    // ========== 字体粗细 ==========
    readonly property var weight: QtObject {
        readonly property int normal: Font.Normal       // 400 - 正常
        readonly property int medium: Font.Medium       // 500 - 中等 - 按钮、标签
        readonly property int semibold: Font.DemiBold   // 600 - 半粗 - 标题
        readonly property int bold: Font.Bold           // 700 - 粗体 - 强调标题
    }

    // ========== 行高 ==========
    readonly property var lineHeight: QtObject {
        readonly property real tight: 1.25     // 紧凑 - 大标题
        readonly property real normal: 1.5      // 正常 - 正文
        readonly property real relaxed: 1.75    // 宽松 - 长文本
    }

    // ========== 预设字体样式 ==========
    // 页面大标题 - 24px, 600, 1.3
    readonly property var heading1: QtObject {
        readonly property int pixelSize: 24
        readonly property int weight: Font.DemiBold
        readonly property real lineHeight: 1.3
    }

    // 卡片标题 - 20px, 600, 1.4
    readonly property var heading2: QtObject {
        readonly property int pixelSize: 20
        readonly property int weight: Font.DemiBold
        readonly property real lineHeight: 1.4
    }

    // 小标题 - 18px, 500, 1.5
    readonly property var heading3: QtObject {
        readonly property int pixelSize: 18
        readonly property int weight: Font.Medium
        readonly property real lineHeight: 1.5
    }

    // 正文 - 16px, 400, 1.5
    readonly property var body: QtObject {
        readonly property int pixelSize: 16
        readonly property int weight: Font.Normal
        readonly property real lineHeight: 1.5
    }

    // 按钮文字 - 14px, 500, 1.5
    readonly property var button: QtObject {
        readonly property int pixelSize: 14
        readonly property int weight: Font.Medium
        readonly property real lineHeight: 1.5
    }

    // 标签文字 - 14px, 500, 1.5
    readonly property var label: QtObject {
        readonly property int pixelSize: 14
        readonly property int weight: Font.Medium
        readonly property real lineHeight: 1.5
    }

    // 辅助文字 - 12px, 400, 1.5
    readonly property var caption: QtObject {
        readonly property int pixelSize: 12
        readonly property int weight: Font.Normal
        readonly property real lineHeight: 1.5
    }

    // 输入框文字 - 16px, 400, 1.5
    readonly property var input: QtObject {
        readonly property int pixelSize: 16
        readonly property int weight: Font.Normal
        readonly property real lineHeight: 1.5
    }
}
