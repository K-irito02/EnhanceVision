import QtQuick

Item {
    id: container
    
    property var source: null
    property real brightness: 0.0
    property real contrast: 1.0
    property real saturation: 1.0
    property real hue: 0.0
    property real sharpness: 0.0
    property real blurAmount: 0.0
    property real denoise: 0.0
    property real exposure: 0.0
    property real gamma: 1.0
    property real temperature: 0.0
    property real tint: 0.0
    property real vignette: 0.0
    property real highlights: 0.0
    property real shadows: 0.0
    
    readonly property bool hasModifications: 
        Math.abs(brightness) > 0.001 ||
        Math.abs(contrast - 1.0) > 0.001 ||
        Math.abs(saturation - 1.0) > 0.001 ||
        Math.abs(hue) > 0.001 ||
        Math.abs(sharpness) > 0.001 ||
        Math.abs(blurAmount) > 0.001 ||
        Math.abs(denoise) > 0.001 ||
        Math.abs(exposure) > 0.001 ||
        Math.abs(gamma - 1.0) > 0.001 ||
        Math.abs(temperature) > 0.001 ||
        Math.abs(tint) > 0.001 ||
        Math.abs(vignette) > 0.001 ||
        Math.abs(highlights) > 0.001 ||
        Math.abs(shadows) > 0.001
    
    property size imgSize: {
        if (!source) return Qt.size(1, 1)
        var w = 1
        var h = 1
        if (source.sourceSize && source.sourceSize.width > 0 && source.sourceSize.height > 0) {
            w = source.sourceSize.width
            h = source.sourceSize.height
        } else if (source.implicitWidth > 0 && source.implicitHeight > 0) {
            w = source.implicitWidth
            h = source.implicitHeight
        } else if (source.width > 0 && source.height > 0) {
            w = source.width
            h = source.height
        }
        return Qt.size(w, h)
    }
    
    property real imageAspect: imgSize.width > 0 && imgSize.height > 0 ? imgSize.width / imgSize.height : 1.0
    property real containerAspect: width > 0 && height > 0 ? width / height : 1.0
    
    property real fittedWidth: {
        if (width <= 0 || height <= 0) return imgSize.width > 0 ? imgSize.width : 100
        return imageAspect > containerAspect ? width : height * imageAspect
    }
    property real fittedHeight: {
        if (width <= 0 || height <= 0) return imgSize.height > 0 ? imgSize.height : 100
        return imageAspect > containerAspect ? width / imageAspect : height
    }
    
    ShaderEffect {
        id: shaderEffect
        anchors.centerIn: parent
        width: Math.max(1, container.fittedWidth)
        height: Math.max(1, container.fittedHeight)
        
        property var source: container.source
        property real brightness: container.brightness
        property real contrast: container.contrast
        property real saturation: container.saturation
        property real hue: container.hue
        property real sharpness: container.sharpness
        property real blurAmount: container.blurAmount
        property real denoise: container.denoise
        property real exposure: container.exposure
        property real gamma: container.gamma
        property real temperature: container.temperature
        property real tint: container.tint
        property real vignette: container.vignette
        property real highlights: container.highlights
        property real shadows: container.shadows
        property size imgSize: container.imgSize
        
        vertexShader: "qrc:///resources/shaders/fullshader.vert.qsb"
        fragmentShader: "qrc:///resources/shaders/fullshader.frag.qsb"
        
        supportsAtlasTextures: true
    }
}
