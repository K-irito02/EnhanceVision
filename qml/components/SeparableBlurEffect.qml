import QtQuick

Item {
    id: root
    
    property var source: null
    property real blurRadius: 0.0
    property size imgSize: Qt.size(512, 512)
    
    readonly property bool hasBlur: blurRadius > 0.001
    
    ShaderEffectSource {
        id: horizontalOutput
        sourceItem: root.source
        live: root.hasBlur
        hideSource: false
        visible: false
        textureSize: imgSize
        recursive: false
        mipmap: false
        smooth: true
    }
    
    ShaderEffect {
        id: horizontalBlur
        anchors.fill: parent
        visible: root.hasBlur
        
        property var source: root.source
        property real blurRadius: root.blurRadius
        property size imgSize: root.imgSize
        
        vertexShader: "qrc:///resources/shaders/fullshader.vert.qsb"
        fragmentShader: "qrc:///resources/shaders/blur_horizontal.frag.qsb"
        
        supportsAtlasTextures: true
    }
    
    ShaderEffectSource {
        id: verticalPassSource
        sourceItem: horizontalBlur
        live: root.hasBlur
        hideSource: false
        visible: false
        textureSize: imgSize
        recursive: false
        mipmap: false
        smooth: true
    }
    
    ShaderEffect {
        id: verticalBlur
        anchors.fill: parent
        visible: root.hasBlur
        
        property var source: verticalPassSource
        property real blurRadius: root.blurRadius
        property size imgSize: root.imgSize
        
        vertexShader: "qrc:///resources/shaders/fullshader.vert.qsb"
        fragmentShader: "qrc:///resources/shaders/blur_vertical.frag.qsb"
        
        supportsAtlasTextures: true
    }
}
