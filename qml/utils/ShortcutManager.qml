import QtQuick
import QtQuick.Controls

/**
 * @brief 快捷键管理器
 *
 * 设计原则：
 * 1. 集中管理所有应用快捷键，便于维护和扩展
 * 2. 通过信号解耦，快捷键触发与业务逻辑分离
 * 3. 支持跨平台快捷键映射（Mac 使用 Cmd，其他使用 Ctrl）
 * 4. 可通过 enabled 属性控制快捷键可用性
 *
 * 使用方式：
 * 在需要响应快捷键的组件中：
 * 1. 引入 ShortcutManager
 * 2. 连接相应信号到处理函数
 */
Item {
    id: root

    // ========== 属性 ==========

    /**
     * @brief 是否启用快捷键
     * 可用于在特定场景下禁用快捷键（如对话框打开时）
     */
    property bool enabled: true

    /**
     * @brief 当前页面索引
     * 用于控制快捷键仅在主页面生效
     * 0: 主页面, 1: 设置页面
     */
    property int currentPage: 0

    /**
     * @brief 是否在主页面
     * 快捷键仅在主页面生效
     */
    readonly property bool isOnMainPage: currentPage === 0

    /**
     * @brief 是否使用 Mac 风格快捷键（Cmd 代替 Ctrl）
     * 根据操作系统自动检测
     */
    readonly property bool useMacShortcuts: Qt.platform.os === "osx"

    // ========== 信号 ==========

    /**
     * @brief 添加文件请求信号
     * 当用户按下 Ctrl+O (或 Mac 上的 Cmd+O) 时触发
     */
    signal addFilesRequested()

    /**
     * @brief 新会话请求信号
     * 当用户按下 Ctrl+N (或 Mac 上的 Cmd+N) 时触发
     */
    signal newSessionRequested()

    // ========== 快捷键定义 ==========

    /**
     * @brief 添加文件快捷键
     * Ctrl+O (Windows/Linux) / Cmd+O (Mac)
     */
    Shortcut {
        id: addFilesShortcut
        sequence: useMacShortcuts ? "Cmd+O" : "Ctrl+O"
        enabled: root.enabled && root.isOnMainPage
        onActivated: {
            console.log("[ShortcutManager] Add files shortcut activated")
            root.addFilesRequested()
        }
    }

    /**
     * @brief 新会话快捷键
     * Ctrl+N (Windows/Linux) / Cmd+N (Mac)
     */
    Shortcut {
        id: newSessionShortcut
        sequence: useMacShortcuts ? "Cmd+N" : "Ctrl+N"
        enabled: root.enabled && root.isOnMainPage
        onActivated: {
            console.log("[ShortcutManager] New session shortcut activated")
            root.newSessionRequested()
        }
    }

    // ========== 公共方法 ==========

    /**
     * @brief 临时禁用所有快捷键
     * 适用于打开对话框、模态窗口等场景
     */
    function disableShortcuts() {
        root.enabled = false
    }

    /**
     * @brief 恢复启用所有快捷键
     */
    function enableShortcuts() {
        root.enabled = true
    }

    /**
     * @brief 切换快捷键启用状态
     */
    function toggleShortcuts() {
        root.enabled = !root.enabled
    }
}
