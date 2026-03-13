import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "./styles"

/**
 * @brief 应用程序入口点
 * 
 * 主 Item，负责初始化并加载 App 根组件
 * 参考功能设计文档 0.2 和架构设计文档 02-architecture.md
 */
Item {
    id: root
    
    // ========== 组件加载 ==========
    App {
        id: app
        anchors.fill: parent
    }
}
