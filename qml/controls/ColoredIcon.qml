import QtQuick
import QtQuick.Effects
import "../styles"

Item {
    id: root
    
    property string source: ""
    property int iconSize: 18
    property color color: Theme.colors.icon
    
    implicitWidth: iconSize
    implicitHeight: iconSize
    width: iconSize
    height: iconSize
    
    Image {
        id: iconImage
        anchors.centerIn: parent
        width: root.iconSize
        height: root.iconSize
        source: root.source
        sourceSize: Qt.size(root.iconSize * 2, root.iconSize * 2)
        fillMode: Image.PreserveAspectFit
        smooth: true
        antialiasing: true
        visible: root.source !== ""
        layer.enabled: true
        layer.effect: MultiEffect {
            colorizationColor: root.color
            colorization: 1.0
        }
        
            }
}
