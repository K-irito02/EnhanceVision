import QtQuick

Item {
    id: root
    
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
    
    readonly property bool hasColorModifications:
        Math.abs(brightness) > 0.001 ||
        Math.abs(contrast - 1.0) > 0.001 ||
        Math.abs(saturation - 1.0) > 0.001 ||
        Math.abs(exposure) > 0.001 ||
        Math.abs(gamma - 1.0) > 0.001 ||
        Math.abs(highlights) > 0.001 ||
        Math.abs(shadows) > 0.001
    
    readonly property bool hasHueModifications:
        Math.abs(hue) > 0.001 ||
        Math.abs(temperature) > 0.001 ||
        Math.abs(tint) > 0.001
    
    readonly property bool hasSpatialModifications:
        Math.abs(sharpness) > 0.001 ||
        Math.abs(blurAmount) > 0.001 ||
        Math.abs(denoise) > 0.001
    
    readonly property bool hasEffectModifications:
        Math.abs(vignette) > 0.001
    
    readonly property bool hasModifications:
        hasColorModifications || hasHueModifications ||
        hasSpatialModifications || hasEffectModifications
    
    property bool enableMultiPass: true
    
    ShaderEffectSource {
        id: pass1Output
        sourceItem: source
        live: false
        hideSource: false
        visible: false
        textureSize: imgSize
        recursive: false
        mipmap: false
        smooth: true
    }
    
    ShaderEffect {
        id: pass1Color
        anchors.fill: parent
        visible: root.enableMultiPass && root.hasColorModifications && root.hasModifications
        
        property var source: root.source
        property real brightness: root.brightness
        property real contrast: root.contrast
        property real saturation: root.saturation
        property real exposure: root.exposure
        property real gamma: root.gamma
        property real temperature: 0.0
        property real tint: 0.0
        property real hue: 0.0
        property real sharpness: 0.0
        property real blurAmount: 0.0
        property real denoise: 0.0
        property real vignette: 0.0
        property real highlights: root.highlights
        property real shadows: root.shadows
        property size imgSize: root.imgSize
        
        vertexShader: "qrc:///resources/shaders/fullshader.vert.qsb"
        fragmentShader: "qrc:///resources/shaders/fullshader.frag.qsb"
        
        supportsAtlasTextures: true
    }
    
    ShaderEffectSource {
        id: pass2Output
        sourceItem: pass1Color
        live: root.enableMultiPass && root.hasColorModifications
        hideSource: false
        visible: false
        textureSize: imgSize
        recursive: false
        mipmap: false
        smooth: true
    }
    
    ShaderEffect {
        id: pass2Hue
        anchors.fill: parent
        visible: root.enableMultiPass && root.hasHueModifications && root.hasModifications
        
        property var source: root.hasColorModifications ? pass2Output : root.source
        property real brightness: 0.0
        property real contrast: 1.0
        property real saturation: 1.0
        property real exposure: 0.0
        property real gamma: 1.0
        property real temperature: root.temperature
        property real tint: root.tint
        property real hue: root.hue
        property real sharpness: 0.0
        property real blurAmount: 0.0
        property real denoise: 0.0
        property real vignette: 0.0
        property real highlights: 0.0
        property real shadows: 0.0
        property size imgSize: root.imgSize
        
        vertexShader: "qrc:///resources/shaders/fullshader.vert.qsb"
        fragmentShader: "qrc:///resources/shaders/fullshader.frag.qsb"
        
        supportsAtlasTextures: true
    }
    
    ShaderEffectSource {
        id: pass3Output
        sourceItem: pass2Hue
        live: root.enableMultiPass && root.hasHueModifications
        hideSource: false
        visible: false
        textureSize: imgSize
        recursive: false
        mipmap: false
        smooth: true
    }
    
    ShaderEffect {
        id: pass3Spatial
        anchors.fill: parent
        visible: root.enableMultiPass && root.hasSpatialModifications && root.hasModifications
        
        property var source: {
            if (root.hasHueModifications) return pass3Output
            if (root.hasColorModifications) return pass2Output
            return root.source
        }
        property real brightness: 0.0
        property real contrast: 1.0
        property real saturation: 1.0
        property real exposure: 0.0
        property real gamma: 1.0
        property real temperature: 0.0
        property real tint: 0.0
        property real hue: 0.0
        property real sharpness: root.sharpness
        property real blurAmount: root.blurAmount
        property real denoise: root.denoise
        property real vignette: 0.0
        property real highlights: 0.0
        property real shadows: 0.0
        property size imgSize: root.imgSize
        
        vertexShader: "qrc:///resources/shaders/fullshader.vert.qsb"
        fragmentShader: "qrc:///resources/shaders/fullshader.frag.qsb"
        
        supportsAtlasTextures: true
    }
    
    ShaderEffectSource {
        id: pass4Output
        sourceItem: pass3Spatial
        live: root.enableMultiPass && root.hasSpatialModifications
        hideSource: false
        visible: false
        textureSize: imgSize
        recursive: false
        mipmap: false
        smooth: true
    }
    
    ShaderEffect {
        id: pass4Effect
        anchors.fill: parent
        visible: root.enableMultiPass && root.hasEffectModifications && root.hasModifications
        
        property var source: {
            if (root.hasSpatialModifications) return pass4Output
            if (root.hasHueModifications) return pass3Output
            if (root.hasColorModifications) return pass2Output
            return root.source
        }
        property real brightness: 0.0
        property real contrast: 1.0
        property real saturation: 1.0
        property real exposure: 0.0
        property real gamma: 1.0
        property real temperature: 0.0
        property real tint: 0.0
        property real hue: 0.0
        property real sharpness: 0.0
        property real blurAmount: 0.0
        property real denoise: 0.0
        property real vignette: root.vignette
        property real highlights: 0.0
        property real shadows: 0.0
        property size imgSize: root.imgSize
        
        vertexShader: "qrc:///resources/shaders/fullshader.vert.qsb"
        fragmentShader: "qrc:///resources/shaders/fullshader.frag.qsb"
        
        supportsAtlasTextures: true
    }
    
    ShaderEffect {
        id: singlePassFallback
        anchors.fill: parent
        visible: !root.enableMultiPass && root.hasModifications
        
        property var source: root.source
        property real brightness: root.brightness
        property real contrast: root.contrast
        property real saturation: root.saturation
        property real hue: root.hue
        property real sharpness: root.sharpness
        property real blurAmount: root.blurAmount
        property real denoise: root.denoise
        property real exposure: root.exposure
        property real gamma: root.gamma
        property real temperature: root.temperature
        property real tint: root.tint
        property real vignette: root.vignette
        property real highlights: root.highlights
        property real shadows: root.shadows
        property size imgSize: root.imgSize
        
        vertexShader: "qrc:///resources/shaders/fullshader.vert.qsb"
        fragmentShader: "qrc:///resources/shaders/fullshader.frag.qsb"
        
        supportsAtlasTextures: true
    }
}
