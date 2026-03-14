pragma Singleton
import QtQuick

QtObject {
    id: root

    property var toastComponent: null
    property var dialogComponent: null
    
    property var _confirmCallback: null
    property var _cancelCallback: null

    function showInfo(message, duration) {
        if (toastComponent) {
            toastComponent.show(message, 0, duration)
        }
    }

    function showSuccess(message, duration) {
        if (toastComponent) {
            toastComponent.show(message, 1, duration)
        }
    }

    function showWarning(message, duration) {
        if (toastComponent) {
            toastComponent.show(message, 2, duration)
        }
    }

    function showError(message, duration) {
        if (toastComponent) {
            toastComponent.show(message, 3, duration)
        }
    }

    function showInfoDialog(title, message) {
        if (dialogComponent) {
            dialogComponent.showSecondaryButton = false
            dialogComponent.primaryButtonText = qsTr("确定")
            dialogComponent.showDialog(title, message, 0)
        }
    }

    function showSuccessDialog(title, message) {
        if (dialogComponent) {
            dialogComponent.showSecondaryButton = false
            dialogComponent.primaryButtonText = qsTr("确定")
            dialogComponent.showDialog(title, message, 1)
        }
    }

    function showWarningDialog(title, message) {
        if (dialogComponent) {
            dialogComponent.showSecondaryButton = false
            dialogComponent.primaryButtonText = qsTr("确定")
            dialogComponent.showDialog(title, message, 2)
        }
    }

    function showErrorDialog(title, message) {
        if (dialogComponent) {
            dialogComponent.showSecondaryButton = false
            dialogComponent.primaryButtonText = qsTr("确定")
            dialogComponent.showDialog(title, message, 3)
        }
    }

    function showConfirmDialog(title, message, confirmCallback, cancelCallback, confirmBtnText, cancelBtnText) {
        if (dialogComponent) {
            _confirmCallback = confirmCallback
            _cancelCallback = cancelCallback
            dialogComponent.showSecondaryButton = true
            dialogComponent.primaryButtonText = confirmBtnText ? confirmBtnText : qsTr("确定")
            dialogComponent.secondaryButtonText = cancelBtnText ? cancelBtnText : qsTr("取消")
            dialogComponent.showDialog(title, message, 4)
        }
    }
    
    function handleConfirm() {
        if (_confirmCallback) {
            _confirmCallback()
            _confirmCallback = null
        }
        _cancelCallback = null
    }
    
    function handleCancel() {
        if (_cancelCallback) {
            _cancelCallback()
            _cancelCallback = null
        }
        _confirmCallback = null
    }
    
    function showFileFormatNotSupported(fileName) {
        showError(qsTr("文件格式不支持: %1").arg(fileName))
    }

    function showFileTooLarge(fileName, maxSize) {
        showError(qsTr("文件过大: %1 (最大限制: %2)").arg(fileName).arg(maxSize))
    }

    function showProcessingFailed(errorMessage) {
        showErrorDialog(qsTr("处理失败"), errorMessage)
    }

    function showDiskSpaceLow() {
        showWarningDialog(qsTr("磁盘空间不足"), qsTr("磁盘空间不足，请清理后重试"))
    }

    function showModelFileCorrupted() {
        showErrorDialog(qsTr("模型文件损坏"), qsTr("模型文件损坏，请重新下载"))
    }

    function showNetworkError() {
        showError(qsTr("网络连接失败"))
    }

    function showQueueFull() {
        showWarning(qsTr("队列已满，请等待"))
    }

    function showFileNotFound() {
        showWarning(qsTr("文件不存在，已移除"))
    }

    function showNoPermission() {
        showError(qsTr("无权限读取该文件"))
    }

    function showGpuMemoryLow() {
        showWarning(qsTr("GPU 内存不足，已切换到 CPU"))
    }

    function showProcessingTimeout() {
        showWarningDialog(qsTr("处理超时"), qsTr("处理超时，是否取消？"))
    }
}
