pragma Singleton
import QtQuick
import EnhanceVision.Controllers

/**
 * @brief AI 模型配置状态管理单例
 *
 * 职责：
 * 1. 缓存所有模型配置信息，避免频繁调用 C++ 方法
 * 2. 管理每个模型的参数状态，确保参数独立存储
 * 3. 提供稳定的属性绑定接口，解决频繁切换模型时绑定断开的问题
 * 4. 实现参数持久化，用户修改的参数在模型切换时保留
 */
QtObject {
    id: store

    property var registry: processingController ? processingController.modelRegistry : null

    signal modelSelected(string modelId)
    signal paramsChanged(string modelId)

    property string _currentModelId: ""

    property var _modelInfoCache: ({})

    property var _paramsCache: ({})

    property var _defaultParamsCache: ({})

    readonly property string currentModelId: _currentModelId

    readonly property var currentModelInfo: {
        if (_currentModelId === "" || !registry) return null
        return _getModelInfoCached(_currentModelId)
    }

    property var _currentParamsInternal: ({})

    readonly property var currentModelParams: _currentParamsInternal

    readonly property bool hasCurrentModel: _currentModelId !== "" && currentModelInfo !== null

    function _refreshCurrentParams() {
        if (_currentModelId === "") {
            _currentParamsInternal = {}
            return
        }
        var cached = _paramsCache[_currentModelId]
        if (cached !== undefined) {
            _currentParamsInternal = cached
        } else {
            _currentParamsInternal = _getDefaultParams(_currentModelId)
        }
    }

    function _getModelInfoCached(modelId) {
        if (!modelId || !registry) return null
        if (!_modelInfoCache[modelId]) {
            var info = registry.getModelInfoMap(modelId)
            if (info && Object.keys(info).length > 0) {
                _modelInfoCache[modelId] = info
            }
        }
        return _modelInfoCache[modelId] || null
    }

    function _getDefaultParams(modelId) {
        if (!modelId) return {}
        if (_defaultParamsCache[modelId]) {
            return JSON.parse(JSON.stringify(_defaultParamsCache[modelId]))
        }
        var info = _getModelInfoCached(modelId)
        if (!info || !info.supportedParams) return {}
        var defaults = {}
        var params = info.supportedParams
        var keys = Object.keys(params)
        for (var i = 0; i < keys.length; i++) {
            var key = keys[i]
            var param = params[key]
            if (param && param["default"] !== undefined) {
                if (param.type === "bool") {
                    defaults[key] = param["default"] === true || param["default"] === "true"
                } else if (param.type === "int") {
                    defaults[key] = parseInt(param["default"]) || 0
                } else if (param.type === "float") {
                    defaults[key] = parseFloat(param["default"]) || 0.0
                } else {
                    defaults[key] = param["default"]
                }
            }
        }
        _defaultParamsCache[modelId] = defaults
        return JSON.parse(JSON.stringify(defaults))
    }

    function selectModel(modelId) {
        if (_currentModelId === modelId) return
        _currentModelId = modelId
        _refreshCurrentParams()
        modelSelected(modelId)
    }

    function updateParam(paramKey, value) {
        if (_currentModelId === "") return
        var currentParams = _currentParamsInternal
        var newParams = {}
        var keys = Object.keys(currentParams)
        for (var i = 0; i < keys.length; i++) {
            newParams[keys[i]] = currentParams[keys[i]]
        }
        newParams[paramKey] = value
        _paramsCache[_currentModelId] = newParams
        _currentParamsInternal = newParams
        paramsChanged(_currentModelId)
    }

    function updateParams(newParams) {
        if (_currentModelId === "") return
        _paramsCache[_currentModelId] = JSON.parse(JSON.stringify(newParams))
        _currentParamsInternal = _paramsCache[_currentModelId]
        paramsChanged(_currentModelId)
    }

    function resetCurrentParams() {
        if (_currentModelId === "") return
        var defaults = _getDefaultParams(_currentModelId)
        _paramsCache[_currentModelId] = defaults
        _currentParamsInternal = defaults
        paramsChanged(_currentModelId)
    }

    function resetAllParams() {
        _paramsCache = {}
        if (_currentModelId !== "") {
            _refreshCurrentParams()
            paramsChanged(_currentModelId)
        }
    }

    function getParams() {
        var params = {}
        params.modelId = _currentModelId
        var currentParams = currentModelParams
        var keys = Object.keys(currentParams)
        for (var i = 0; i < keys.length; i++) {
            params[keys[i]] = currentParams[keys[i]]
        }
        return params
    }

    function getParam(paramKey) {
        return currentModelParams[paramKey]
    }

    function hasParamModifications() {
        var currentParams = currentModelParams
        var defaults = _getDefaultParams(_currentModelId)
        var keys = Object.keys(defaults)
        for (var i = 0; i < keys.length; i++) {
            var key = keys[i]
            if (currentParams[key] !== defaults[key]) {
                return true
            }
        }
        return false
    }

    function refreshCache() {
        _modelInfoCache = {}
        _defaultParamsCache = {}
    }

    Component.onCompleted: {
        if (registry) {
            refreshCache()
        }
        if (registry && registry.modelsChanged) {
            registry.modelsChanged.connect(refreshCache)
        }
    }
}
