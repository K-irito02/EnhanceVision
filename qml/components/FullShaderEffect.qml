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
    
    property int debounceInterval: 30
    
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
    
    property real _cachedBrightness: brightness
    property real _cachedContrast: contrast
    property real _cachedSaturation: saturation
    property real _cachedHue: hue
    property real _cachedSharpness: sharpness
    property real _cachedBlurAmount: blurAmount
    property real _cachedDenoise: denoise
    property real _cachedExposure: exposure
    property real _cachedGamma: gamma
    property real _cachedTemperature: temperature
    property real _cachedTint: tint
    property real _cachedVignette: vignette
    property real _cachedHighlights: highlights
    property real _cachedShadows: shadows
    
    property bool _paramsDirty: false
    property bool _isUserInteracting: false
    
    Timer {
        id: debounceTimer
        interval: container.debounceInterval
        repeat: false
        onTriggered: {
            _applyCachedParams()
        }
    }
    
    Timer {
        id: interactionEndTimer
        interval: 100
        repeat: false
        onTriggered: {
            container._isUserInteracting = false
            if (container._paramsDirty) {
                _applyCachedParams()
            }
        }
    }
    
    function _applyCachedParams() {
        shaderEffect.brightness = _cachedBrightness
        shaderEffect.contrast = _cachedContrast
        shaderEffect.saturation = _cachedSaturation
        shaderEffect.hue = _cachedHue
        shaderEffect.sharpness = _cachedSharpness
        shaderEffect.blurAmount = _cachedBlurAmount
        shaderEffect.denoise = _cachedDenoise
        shaderEffect.exposure = _cachedExposure
        shaderEffect.gamma = _cachedGamma
        shaderEffect.temperature = _cachedTemperature
        shaderEffect.tint = _cachedTint
        shaderEffect.vignette = _cachedVignette
        shaderEffect.highlights = _cachedHighlights
        shaderEffect.shadows = _cachedShadows
        container._paramsDirty = false
    }
    
    function _updateParam(paramName, value) {
        var cachedName = "_cached" + paramName.charAt(0).toUpperCase() + paramName.slice(1)
        if (container[cachedName] !== value) {
            container[cachedName] = value
            container._paramsDirty = true
            container._isUserInteracting = true
            interactionEndTimer.restart()
            debounceTimer.restart()
        }
    }
    
    onBrightnessChanged: _updateParam("brightness", brightness)
    onContrastChanged: _updateParam("contrast", contrast)
    onSaturationChanged: _updateParam("saturation", saturation)
    onHueChanged: _updateParam("hue", hue)
    onSharpnessChanged: _updateParam("sharpness", sharpness)
    onBlurAmountChanged: _updateParam("blurAmount", blurAmount)
    onDenoiseChanged: _updateParam("denoise", denoise)
    onExposureChanged: _updateParam("exposure", exposure)
    onGammaChanged: _updateParam("gamma", gamma)
    onTemperatureChanged: _updateParam("temperature", temperature)
    onTintChanged: _updateParam("tint", tint)
    onVignetteChanged: _updateParam("vignette", vignette)
    onHighlightsChanged: _updateParam("highlights", highlights)
    onShadowsChanged: _updateParam("shadows", shadows)
    
    ShaderEffect {
        id: shaderEffect
        anchors.centerIn: parent
        width: Math.max(1, container.fittedWidth)
        height: Math.max(1, container.fittedHeight)
        
        property var source: container.source
        property real brightness: container._cachedBrightness
        property real contrast: container._cachedContrast
        property real saturation: container._cachedSaturation
        property real hue: container._cachedHue
        property real sharpness: container._cachedSharpness
        property real blurAmount: container._cachedBlurAmount
        property real denoise: container._cachedDenoise
        property real exposure: container._cachedExposure
        property real gamma: container._cachedGamma
        property real temperature: container._cachedTemperature
        property real tint: container._cachedTint
        property real vignette: container._cachedVignette
        property real highlights: container._cachedHighlights
        property real shadows: container._cachedShadows
        property size imgSize: container.imgSize
        
        vertexShader: "qrc:///resources/shaders/fullshader.vert.qsb"
        fragmentShader: "qrc:///resources/shaders/fullshader.frag.qsb"
        
        supportsAtlasTextures: true
        
        property bool _sourceReady: container.source && container.source.status === Image.Ready
        
        visible: container.hasModifications && _sourceReady
        
        opacity: _sourceReady ? 1.0 : 0.0
        
        Behavior on opacity {
            NumberAnimation { duration: 100 }
        }
    }
    
    Connections {
        target: container.source
        enabled: container.source !== null
        function onStatusChanged() {
            if (container.source.status === Image.Ready) {
                shaderEffect._sourceReady = true
            }
        }
    }
    
    Component.onCompleted: {
        _cachedBrightness = brightness
        _cachedContrast = contrast
        _cachedSaturation = saturation
        _cachedHue = hue
        _cachedSharpness = sharpness
        _cachedBlurAmount = blurAmount
        _cachedDenoise = denoise
        _cachedExposure = exposure
        _cachedGamma = gamma
        _cachedTemperature = temperature
        _cachedTint = tint
        _cachedVignette = vignette
        _cachedHighlights = highlights
        _cachedShadows = shadows
    }
}
