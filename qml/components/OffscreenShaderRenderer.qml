import QtQuick
import "../styles"

/**
 * @brief 离屏 Shader 渲染器
 * 
 * 用于在后台渲染 Shader 效果并导出图像。
 * 确保导出效果与预览效果完全一致。
 * 支持队列处理多个导出请求。
 */
Item {
    id: root
    visible: false
    
    property string currentExportId: ""
    property string currentImagePath: ""
    property string currentOutputPath: ""
    property var currentShaderParams: ({})
    property bool isExporting: false
    property var exportQueue: []
    
    signal exportReady(string exportId, bool success, string outputPath, string error)
    
    Image {
        id: sourceImage
        visible: false
        asynchronous: true
        cache: false
        fillMode: Image.PreserveAspectFit
        
        onStatusChanged: {
            if (status === Image.Ready) {
                Qt.callLater(startExport)
            } else if (status === Image.Error) {
                finishExport(false, "Failed to load image")
            }
        }
    }
    
    FullShaderEffect {
        id: shaderEffect
        anchors.fill: parent
        source: sourceImage
        
        // 使用 !== undefined 检查，避免值为 0 时被错误替换为默认值
        brightness: root.currentShaderParams.brightness !== undefined ? root.currentShaderParams.brightness : 0.0
        contrast: root.currentShaderParams.contrast !== undefined ? root.currentShaderParams.contrast : 1.0
        saturation: root.currentShaderParams.saturation !== undefined ? root.currentShaderParams.saturation : 1.0
        hue: root.currentShaderParams.hue !== undefined ? root.currentShaderParams.hue : 0.0
        sharpness: root.currentShaderParams.sharpness !== undefined ? root.currentShaderParams.sharpness : 0.0
        blurAmount: root.currentShaderParams.blur !== undefined ? root.currentShaderParams.blur : 0.0
        denoise: root.currentShaderParams.denoise !== undefined ? root.currentShaderParams.denoise : 0.0
        exposure: root.currentShaderParams.exposure !== undefined ? root.currentShaderParams.exposure : 0.0
        gamma: root.currentShaderParams.gamma !== undefined ? root.currentShaderParams.gamma : 1.0
        temperature: root.currentShaderParams.temperature !== undefined ? root.currentShaderParams.temperature : 0.0
        tint: root.currentShaderParams.tint !== undefined ? root.currentShaderParams.tint : 0.0
        vignette: root.currentShaderParams.vignette !== undefined ? root.currentShaderParams.vignette : 0.0
        highlights: root.currentShaderParams.highlights !== undefined ? root.currentShaderParams.highlights : 0.0
        shadows: root.currentShaderParams.shadows !== undefined ? root.currentShaderParams.shadows : 0.0
    }
    
    function requestExport(exportId, imagePath, shaderParams, outputPath) {
        var task = {
            exportId: exportId,
            imagePath: imagePath,
            shaderParams: shaderParams || {},
            outputPath: outputPath
        }
        
        if (root.isExporting) {
            root.exportQueue.push(task)
            return
        }
        
        startExportTask(task)
    }
    
    function startExportTask(task) {
        
        root.isExporting = true
        root.currentExportId = task.exportId
        root.currentImagePath = task.imagePath
        root.currentOutputPath = task.outputPath
        root.currentShaderParams = task.shaderParams
        
        var src = task.imagePath
        if (!src.startsWith("file:///") && !src.startsWith("qrc:/") && !src.startsWith("demo://")) {
            src = "file:///" + src
        }
        
        sourceImage.source = ""
        sourceImage.source = src
    }
    
    function startExport() {
        if (!root.isExporting) {
            return
        }
        
        var imgWidth = sourceImage.sourceSize.width > 0 ? sourceImage.sourceSize.width : sourceImage.implicitWidth
        var imgHeight = sourceImage.sourceSize.height > 0 ? sourceImage.sourceSize.height : sourceImage.implicitHeight
        
        if (imgWidth <= 0 || imgHeight <= 0) {
            finishExport(false, "Invalid image size")
            return
        }
        
        root.width = imgWidth
        root.height = imgHeight
        
        Qt.callLater(doGrab)
    }
    
    function doGrab() {
        var grabResult = shaderEffect.grabToImage(function(result) {
            
            if (!result) {
                finishExport(false, "grabToImage returned null")
                return
            }
            
            var saveResult = false
            try {
                var outputPath = root.currentOutputPath
                var fileUrl = "file:///" + outputPath.replace(/\\/g, "/")
                saveResult = result.saveToFile(fileUrl)
            } catch (e) {
                finishExport(false, "Error saving image: " + e)
                return
            }
            
            if (saveResult) {
                finishExport(true, "")
            } else {
                finishExport(false, "Failed to save image")
            }
        })
        
        if (!grabResult) {
            finishExport(false, "grabToImage failed to start")
        }
    }
    
    function finishExport(success, errorMessage) {
        root.exportReady(root.currentExportId, success, root.currentOutputPath, errorMessage)
        
        root.isExporting = false
        
        if (root.exportQueue.length > 0) {
            var nextTask = root.exportQueue.shift()
            Qt.callLater(function() {
                startExportTask(nextTask)
            })
        }
    }
    
    Connections {
        target: imageExportService
        enabled: typeof imageExportService !== "undefined"
        
        function onExportRequested(exportId, imagePath, shaderParams, outputPath) {
            root.requestExport(exportId, imagePath, shaderParams, outputPath)
        }
    }
    
    onExportReady: function(exportId, success, outputPath, error) {
        if (typeof imageExportService !== "undefined") {
            imageExportService.reportExportResult(exportId, success, outputPath, error)
        }
    }
}
