import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../styles"
import "../components"

/**
 * @brief 主页面组件
 * 
 * 主应用页面，包含文件列表、预览面板和控制面板
 * 参考功能设计文档 0.2 和 UI 设计文档 08-ui-design.md
 */
Item {
    id: root
    
    // ========== 主布局 ==========
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacing._3
        spacing: Theme.spacing._3
        
        // ========== 文件列表 ==========
        FileList {
            id: fileList
            Layout.fillWidth: true
            Layout.preferredHeight: 140
        }
        
        // ========== 分隔线 ==========
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.colors.border
        }
        
        // ========== 中间区域（预览和控制面板） ==========
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacing._3
            
            // ========== 预览面板 ==========
            PreviewPane {
                id: previewPane
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 400
            }
            
            // ========== 控制面板 ==========
            ControlPanel {
                id: controlPanel
                Layout.preferredWidth: 320
                Layout.minimumWidth: 280
                Layout.fillHeight: true
            }
        }
    }
}
