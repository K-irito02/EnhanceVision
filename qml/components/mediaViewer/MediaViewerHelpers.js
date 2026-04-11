.pragma library

function hasDistinctResult(file) {
    return !!(file && file.resultPath && file.originalPath && file.resultPath !== file.originalPath)
}

function resolveCurrentSource(file, messageMode, showOriginal) {
    if (!file) {
        return ""
    }

    if (messageMode) {
        if (showOriginal) {
            return file.originalPath || file.filePath || ""
        }
        return file.resultPath || file.filePath || file.originalPath || ""
    }

    return file.filePath || file.originalPath || ""
}

function resultSource(file) {
    if (!file) {
        return ""
    }
    return file.resultPath || file.filePath || ""
}

function originalSource(file) {
    if (!file) {
        return ""
    }
    return file.originalPath || file.filePath || ""
}

function shouldShowCompare(shaderEnabled, messageMode, file) {
    return (!messageMode && shaderEnabled) || (messageMode && hasDistinctResult(file))
}

function resolveSource(source) {
    if (!source) {
        return ""
    }

    var normalized = source.toString()
    if (normalized.startsWith("file:///") || normalized.startsWith("qrc:/") || normalized.startsWith("demo://")
            || normalized.startsWith("http://") || normalized.startsWith("https://")) {
        return normalized
    }

    normalized = normalized.replace(/\\/g, "/")

    if (/^[A-Za-z]:\//.test(normalized)) {
        return "file:///" + normalized
    }
    if (normalized.startsWith("/")) {
        return "file://" + normalized
    }

    return Qt.resolvedUrl(normalized)
}

function formatTime(ms) {
    if (ms === undefined || ms === null || isNaN(ms) || ms < 0) {
        return "00:00"
    }

    var totalSeconds = Math.floor(ms / 1000)
    var hours = Math.floor(totalSeconds / 3600)
    var minutes = Math.floor((totalSeconds % 3600) / 60)
    var seconds = totalSeconds % 60

    if (hours > 0) {
        return hours.toString() + ":" + minutes.toString().padStart(2, "0") + ":" + seconds.toString().padStart(2, "0")
    }

    return minutes.toString().padStart(2, "0") + ":" + seconds.toString().padStart(2, "0")
}

function normalizeThumbnailKey(source) {
    if (!source || source === "") {
        return ""
    }
    if (source.startsWith("file:///")) {
        return source.substring(8)
    }
    return source
}

function baseThumbnailKey(file) {
    if (!file) {
        return ""
    }
    return normalizeThumbnailKey(file.originalPath || file.filePath || "")
}

function preferredThumbnailKey(file) {
    if (!file) {
        return ""
    }
    if (file.processedThumbnailId && file.processedThumbnailId !== "") {
        return file.processedThumbnailId
    }
    return normalizeThumbnailKey(file.resultPath || "")
}

function computeIdealWindowSize(mediaW, mediaH, scaleFactor, isVideo, showThumbnailBar) {
    var effectiveScaleFactor = Math.max(1, scaleFactor || 1)
    var contentWidth = (mediaW > 0 ? mediaW : 1280) * effectiveScaleFactor
    var contentHeight = (mediaH > 0 ? mediaH : 720) * effectiveScaleFactor

    var decorationHeight = 44
    if (isVideo) {
        decorationHeight += 80
    }
    if (showThumbnailBar) {
        decorationHeight += 60
    }

    var targetWidth = contentWidth + 16
    var targetHeight = contentHeight + decorationHeight

    var maxWidth = Math.max(480, Math.floor(Screen.desktopAvailableWidth * 0.80))
    var maxHeight = Math.max(360, Math.floor(Screen.desktopAvailableHeight * 0.80))
    var scaleToFit = Math.min(1.0, Math.min(maxWidth / targetWidth, maxHeight / targetHeight))

    targetWidth = Math.max(480, Math.floor(targetWidth * scaleToFit))
    targetHeight = Math.max(360, Math.floor(targetHeight * scaleToFit))

    return Qt.size(targetWidth, targetHeight)
}

function fileIndexById(fileId, files) {
    if (!fileId || !files) {
        return -1
    }

    for (var i = 0; i < files.length; ++i) {
        if (files[i] && files[i].id === fileId) {
            return i
        }
    }

    return -1
}

function fileIndexByPath(filePath, files) {
    if (!filePath || !files) {
        return -1
    }

    for (var i = 0; i < files.length; ++i) {
        if (files[i] && files[i].filePath === filePath) {
            return i
        }
    }

    return -1
}

function isAppendOnlyUpdate(currentFiles, nextFiles) {
    if (!currentFiles || !nextFiles || currentFiles.length === 0 || nextFiles.length <= currentFiles.length) {
        return false
    }

    for (var i = 0; i < currentFiles.length; ++i) {
        var currentFile = currentFiles[i]
        var nextFile = nextFiles[i]
        if (!currentFile || !nextFile || currentFile.id !== nextFile.id) {
            return false
        }
    }

    return true
}

function resolveNextIndex(currentFiles, nextFiles, currentIndex, preferredFileId, preferredFilePath, selectedFileId, selectedFilePath) {
    var resolvedIndex = fileIndexById(preferredFileId || "", nextFiles)
    if (resolvedIndex < 0) {
        resolvedIndex = fileIndexByPath(preferredFilePath || "", nextFiles)
    }
    if (resolvedIndex < 0) {
        resolvedIndex = fileIndexById(selectedFileId || "", nextFiles)
    }
    if (resolvedIndex < 0) {
        resolvedIndex = fileIndexByPath(selectedFilePath || "", nextFiles)
    }
    if (resolvedIndex < 0 && currentIndex >= 0 && currentIndex < nextFiles.length) {
        resolvedIndex = currentIndex
    }
    if (resolvedIndex < 0) {
        resolvedIndex = 0
    }
    return resolvedIndex
}
