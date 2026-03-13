pragma Singleton
import QtQuick

/**
 * @brief 通知管理器单例
 * 统一管理 Toast 和 Dialog 的显示
 * 参考功能设计文档 6.1 和 6.2 节
 */
QtObject {
    id: root

    // ========== 属性定义 ==========
    /**
     * @brief Toast 组件引用
     */
    property var toastComponent: null

    /**
     * @brief Dialog 组件引用
     */
    property var dialogComponent: null

    // ========== 快捷方法 - Toast ==========
    /**
     * @brief 显示信息提示
     * @param message 消息内容
     * @param duration 显示时长（毫秒）
     */
    function showInfo(message, duration) {
        if (toastComponent) {
            toastComponent.show(message, 0, duration)
        }
    }

    /**
     * @brief 显示成功提示
     * @param message 消息内容
     * @param duration 显示时长（毫秒）
     */
    function showSuccess(message, duration) {
        if (toastComponent) {
            toastComponent.show(message, 1, duration)
        }
    }

    /**
     * @brief 显示警告提示
     * @param message 消息内容
     * @param duration 显示时长（毫秒）
     */
    function showWarning(message, duration) {
        if (toastComponent) {
            toastComponent.show(message, 2, duration)
        }
    }

    /**
     * @brief 显示错误提示
     * @param message 消息内容
     * @param duration 显示时长（毫秒）
     */
    function showError(message, duration) {
        if (toastComponent) {
            toastComponent.show(message, 3, duration)
        }
    }

    // ========== 快捷方法 - Dialog ==========
    /**
     * @brief 显示信息对话框
     * @param title 标题
     * @param message 消息内容
     */
    function showInfoDialog(title, message) {
        if (dialogComponent) {
            dialogComponent.showSecondaryButton = false
            dialogComponent.primaryButtonText = qsTr("确定")
            dialogComponent.show(title, message, 0)
        }
    }

    /**
     * @brief 显示成功对话框
     * @param title 标题
     * @param message 消息内容
     */
    function showSuccessDialog(title, message) {
        if (dialogComponent) {
            dialogComponent.showSecondaryButton = false
            dialogComponent.primaryButtonText = qsTr("确定")
            dialogComponent.show(title, message, 1)
        }
    }

    /**
     * @brief 显示警告对话框
     * @param title 标题
     * @param message 消息内容
     */
    function showWarningDialog(title, message) {
        if (dialogComponent) {
            dialogComponent.showSecondaryButton = false
            dialogComponent.primaryButtonText = qsTr("确定")
            dialogComponent.show(title, message, 2)
        }
    }

    /**
     * @brief 显示错误对话框
     * @param title 标题
     * @param message 消息内容
     */
    function showErrorDialog(title, message) {
        if (dialogComponent) {
            dialogComponent.showSecondaryButton = false
            dialogComponent.primaryButtonText = qsTr("确定")
            dialogComponent.show(title, message, 3)
        }
    }

    /**
     * @brief 显示确认对话框
     * @param title 标题
     * @param message 消息内容
     * @param confirmCallback 确认回调函数
     * @param cancelCallback 取消回调函数
     */
    function showConfirmDialog(title, message, confirmCallback, cancelCallback) {
        if (dialogComponent) {
            dialogComponent.showSecondaryButton = true
            dialogComponent.primaryButtonText = qsTr("确定")
            dialogComponent.secondaryButtonText = qsTr("取消")
            
            // 连接信号
            dialogComponent.primaryButtonClicked.connect(function() {
                if (confirmCallback) confirmCallback()
            })
            dialogComponent.secondaryButtonClicked.connect(function() {
                if (cancelCallback) cancelCallback()
            })
            
            dialogComponent.show(title, message, 4)
        }
    }

    // ========== 错误场景快捷方法 ==========
    /**
     * @brief 文件格式不支持提示
     * @param fileName 文件名
     */
    function showFileFormatNotSupported(fileName) {
        showError(qsTr("文件格式不支持: %1").arg(fileName))
    }

    /**
     * @brief 文件过大提示
     * @param fileName 文件名
     * @param maxSize 最大限制大小
     */
    function showFileTooLarge(fileName, maxSize) {
        showError(qsTr("文件过大: %1 (最大限制: %2)").arg(fileName).arg(maxSize))
    }

    /**
     * @brief 处理失败提示
     * @param errorMessage 错误信息
     */
    function showProcessingFailed(errorMessage) {
        showErrorDialog(qsTr("处理失败"), errorMessage)
    }

    /**
     * @brief 磁盘空间不足提示
     */
    function showDiskSpaceLow() {
        showWarningDialog(qsTr("磁盘空间不足"), qsTr("磁盘空间不足，请清理后重试"))
    }

    /**
     * @brief 模型文件损坏提示
     */
    function showModelFileCorrupted() {
        showErrorDialog(qsTr("模型文件损坏"), qsTr("模型文件损坏，请重新下载"))
    }

    /**
     * @brief 网络连接失败提示
     */
    function showNetworkError() {
        showError(qsTr("网络连接失败"))
    }

    /**
     * @brief 队列已满提示
     */
    function showQueueFull() {
        showWarning(qsTr("队列已满，请等待"))
    }

    /**
     * @brief 文件不存在提示
     */
    function showFileNotFound() {
        showWarning(qsTr("文件不存在，已移除"))
    }

    /**
     * @brief 无读取权限提示
     */
    function showNoPermission() {
        showError(qsTr("无权限读取该文件"))
    }

    /**
     * @brief GPU内存不足提示
     */
    function showGpuMemoryLow() {
        showWarning(qsTr("GPU 内存不足，已切换到 CPU"))
    }

    /**
     * @brief 处理超时提示
     */
    function showProcessingTimeout() {
        showWarningDialog(qsTr("处理超时"), qsTr("处理超时，是否取消？"))
    }
}
