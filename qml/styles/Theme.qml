pragma Singleton
import QtQuick

/**
 * @brief 主题系统单例
 * 管理亮色/深色主题切换，整合颜色、字体、间距、圆角、阴影等系统
 * 参考功能设计文档 11.2-11.5 节
 */
QtObject {
    id: root

    // ========== 主题设置 ==========
    /**
     * @brief 当前是否为暗色主题
     * 默认值为 true（暗色主题）
     */
    property bool isDark: true

    /**
     * @brief 当前语言代码
     * "zh_CN" 或 "en_US"
     */
    property string language: "zh_CN"

    /**
     * @brief 切换主题
     */
    function toggle() {
        isDark = !isDark
    }

    /**
     * @brief 设置主题
     * @param dark 是否为暗色主题
     */
    function setDark(dark) {
        isDark = dark
    }

    /**
     * @brief 切换语言
     */
    function toggleLanguage() {
        language = (language === "zh_CN") ? "en_US" : "zh_CN"
    }

    /**
     * @brief 设置语言
     * @param lang 语言代码
     */
    function setLanguage(lang) {
        language = lang
    }

    // ========== 颜色系统 ==========
    /**
     * @brief 当前主题的颜色集合
     * 根据 isDark 属性自动切换亮色/暗色主题
     */
    readonly property var colors: isDark ? Colors.dark : Colors.light

    // ========== 字体系统 ==========
    readonly property var typography: Typography

    // ========== 图标路径辅助 ==========
    /**
     * @brief 获取图标资源路径
     * 暗色主题返回蓝色图标，亮色主题返回黑色图标
     * @param name 图标名称（不含后缀）
     * @return 图标 URL
     */
    function icon(name) {
        var iconPath = isDark ? "icons-dark" : "icons"
        return "qrc:/qt/qml/EnhanceVision/resources/" + iconPath + "/" + name + ".svg"
    }

    // ========== 间距系统 (px) ==========
    readonly property var spacing: QtObject {
        readonly property int _0: 0       // 0px
        readonly property int _1: 4       // 4px
        readonly property int _2: 8       // 8px
        readonly property int _3: 12      // 12px
        readonly property int _4: 16      // 16px
        readonly property int _5: 20      // 20px
        readonly property int _6: 24      // 24px
        readonly property int _8: 32      // 32px
        readonly property int _10: 40     // 40px
        readonly property int _12: 48     // 48px
        readonly property int _16: 64     // 64px
        readonly property int _20: 80     // 80px
        readonly property int _24: 96     // 96px
    }

    // ========== 圆角系统 (px) ==========
    readonly property var radius: QtObject {
        readonly property int xs: 4       // 4px - 微圆角
        readonly property int sm: 6       // 6px - 小圆角
        readonly property int md: 8       // 8px - 中圆角
        readonly property int lg: 10      // 10px - 大圆角 (默认)
        readonly property int xl: 12      // 12px - 超大圆角
        readonly property int xxl: 16     // 16px - 特大圆角
        readonly property int full: 9999  // 全圆角
    }

    // ========== 动画系统 ==========
    readonly property var animation: QtObject {
        readonly property int fast: 100    // 100ms - 微交互
        readonly property int normal: 200  // 200ms - 标准过渡
        readonly property int slow: 300    // 300ms - 大动作
        readonly property int slower: 500  // 500ms - 页面过渡
    }

    // ========== 阴影系统 ==========
    readonly property var shadow: QtObject {
        // 亮色主题阴影
        readonly property var light: QtObject {
            readonly property string sm: "0 1px 2px 0 rgba(0, 47, 167, 0.05)"
            readonly property string defaultShadow: "0 1px 3px 0 rgba(0, 47, 167, 0.08), 0 1px 2px -1px rgba(0, 47, 167, 0.06)"
            readonly property string md: "0 4px 6px -1px rgba(0, 47, 167, 0.08), 0 2px 4px -2px rgba(0, 47, 167, 0.06)"
            readonly property string lg: "0 10px 15px -3px rgba(0, 47, 167, 0.08), 0 4px 6px -4px rgba(0, 47, 167, 0.06)"
            readonly property string xl: "0 20px 25px -5px rgba(0, 47, 167, 0.1), 0 8px 10px -6px rgba(0, 47, 167, 0.06)"
        }

        // 暗色主题阴影
        readonly property var dark: QtObject {
            readonly property string sm: "0 1px 2px 0 rgba(0, 0, 0, 0.4)"
            readonly property string defaultShadow: "0 1px 3px 0 rgba(0, 0, 0, 0.5), 0 1px 2px -1px rgba(0, 0, 0, 0.5)"
            readonly property string md: "0 4px 6px -1px rgba(0, 0, 0, 0.6), 0 2px 4px -2px rgba(0, 0, 0, 0.5)"
            readonly property string lg: "0 10px 15px -3px rgba(0, 0, 0, 0.6), 0 4px 6px -4px rgba(0, 0, 0, 0.5)"
            readonly property string xl: "0 20px 25px -5px rgba(0, 0, 0, 0.6), 0 8px 10px -6px rgba(0, 0, 0, 0.5)"
        }

        // 当前主题的阴影
        readonly property var current: isDark ? dark : light
    }

    // ========== 渐变系统 ==========
    readonly property var gradient: QtObject {
        // 亮色主题渐变
        readonly property var light: QtObject {
            readonly property string welcome: "linear-gradient(135deg, #E8EEFB 0%, #D0DFFB 50%, #E8EEFB 100%)"
            readonly property string card: "linear-gradient(135deg, #FFFFFF 0%, #F5F8FC 100%)"
            readonly property string primary: "linear-gradient(135deg, #002FA7 0%, #1A56DB 100%)"
        }

        // 暗色主题渐变
        readonly property var dark: QtObject {
            readonly property string welcome: "linear-gradient(135deg, #080C14 0%, #0F1D3A 50%, #080C14 100%)"
            readonly property string card: "linear-gradient(135deg, #0F1520 0%, #1A2744 100%)"
            readonly property string primary: "linear-gradient(135deg, #1A56DB 0%, #3B82F6 100%)"
        }

        // 当前主题的渐变
        readonly property var current: isDark ? dark : light
    }
}
