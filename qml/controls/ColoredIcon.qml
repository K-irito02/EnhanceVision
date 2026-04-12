import QtQuick
import QtQuick.Effects
import "../styles"

Item {
    id: root
    
    property string source: ""
    property int iconSize: 18
    property color color: Theme.colors.icon
    readonly property string fallbackSource: Theme.icon(Theme.iconFallbackName)
    property string resolvedSource: source
    
    implicitWidth: iconSize
    implicitHeight: iconSize
    width: iconSize
    height: iconSize

    onSourceChanged: {
        resolvedSource = source
    }
    
    Image {
        id: iconImage
        anchors.centerIn: parent
        width: root.iconSize
        height: root.iconSize
        source: root.resolvedSource
        sourceSize: Qt.size(root.iconSize * 2, root.iconSize * 2)
        fillMode: Image.PreserveAspectFit
        smooth: true
        antialiasing: true
        visible: root.resolvedSource !== ""
        layer.enabled: true
        layer.effect: MultiEffect {
            colorizationColor: root.color
            colorization: 1.0
        }

        onStatusChanged: {
            if (status === Image.Error
                    && root.source !== ""
                    && root.resolvedSource !== root.fallbackSource
                    && root.fallbackSource !== "") {
                console.warn("[ColoredIcon] Failed to load icon:", root.source, "fallback:", root.fallbackSource)
                root.resolvedSource = root.fallbackSource
            }
        }
    }
}
