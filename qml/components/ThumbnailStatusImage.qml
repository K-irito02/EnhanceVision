import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import "../styles"
import "../controls"

Item {
    id: root

    property string baseThumbnailId: ""
    property string preferredThumbnailId: ""
    property bool preferPreferredThumbnail: false
    property int mediaType: 0
    property size requestedSourceSize: Qt.size(Math.max(64, Math.round(width * 2)), Math.max(64, Math.round(height * 2)))
    property int fillMode: Image.PreserveAspectCrop
    property real cornerRadius: Theme.radius.md
    property color backgroundColor: Theme.colors.surface
    property color fallbackColor: Theme.colors.mutedForeground
    property bool showLoadingIndicator: true
    property bool showFailureBorder: false
    property color failureBorderColor: Theme.colors.destructive

    readonly property int stateMissing: 0
    readonly property int statePending: 1
    readonly property int stateReady: 2
    readonly property int stateFailed: 3

    property int _baseVersion: 0
    property int _preferredVersion: 0
    property int _baseState: stateMissing
    property int _preferredState: stateMissing

    readonly property bool _baseReady: _baseState === stateReady
    readonly property bool _preferredReady: _preferredState === stateReady
    readonly property bool _preferPreferred: preferPreferredThumbnail && preferredThumbnailId !== ""
    readonly property bool _hasAnyThumbnailKey: baseThumbnailId !== "" || (_preferPreferred && preferredThumbnailId !== "")
    readonly property bool _showPreferredLoading: _preferPreferred && !_preferredReady && _preferredState !== stateFailed
    readonly property string _activeThumbnailId: {
        if (_preferPreferred && _preferredReady) {
            return preferredThumbnailId
        }
        if (_baseReady) {
            return baseThumbnailId
        }
        return ""
    }
    readonly property int _activeVersion: {
        if (_activeThumbnailId === preferredThumbnailId) {
            return _preferredVersion
        }
        if (_activeThumbnailId === baseThumbnailId) {
            return _baseVersion
        }
        return 0
    }
    readonly property string _imageSource: _activeThumbnailId !== "" ? ("image://thumbnail/" + _activeThumbnailId + "?v=" + _activeVersion) : ""
    readonly property bool _hasReadyThumbnail: _imageSource !== ""
    readonly property bool _showFailureFallback: !_hasReadyThumbnail && (
        (_preferPreferred && _preferredState === stateFailed && !_baseReady)
        || (!_preferPreferred && _baseState === stateFailed)
        || (_preferPreferred && preferredThumbnailId === "" && _baseState === stateFailed)
    )
    readonly property bool _showPendingFallback: !_hasReadyThumbnail && !_showFailureFallback && _hasAnyThumbnailKey
    readonly property bool _showLoadingOverlay: showLoadingIndicator && (_showPendingFallback || (_hasReadyThumbnail && _showPreferredLoading))

    function _providerState(idValue) {
        if (!idValue || idValue === "" || typeof thumbnailProvider === "undefined") {
            return stateMissing
        }
        return thumbnailProvider.thumbnailState(idValue)
    }

    function _ensure(idValue) {
        if (!idValue || idValue === "" || typeof thumbnailProvider === "undefined") {
            return
        }
        thumbnailProvider.ensureThumbnail(idValue, requestedSourceSize.width, requestedSourceSize.height)
    }

    function _refreshStates() {
        _baseState = _providerState(baseThumbnailId)
        _preferredState = _providerState(preferredThumbnailId)

        if (_baseState !== stateReady) {
            _ensure(baseThumbnailId)
        }
        if (_preferPreferred && _preferredState !== stateReady) {
            _ensure(preferredThumbnailId)
        }
    }

    onBaseThumbnailIdChanged: {
        _baseVersion = 0
        _refreshStates()
    }
    onPreferredThumbnailIdChanged: {
        _preferredVersion = 0
        _refreshStates()
    }
    onPreferPreferredThumbnailChanged: _refreshStates()
    onRequestedSourceSizeChanged: _refreshStates()

    Component.onCompleted: _refreshStates()

    Connections {
        target: typeof thumbnailProvider !== "undefined" ? thumbnailProvider : null
        enabled: typeof thumbnailProvider !== "undefined"

        function onThumbnailStateChanged(idValue, state) {
            if (idValue === root.baseThumbnailId) {
                root._baseState = state
                if (state === root.stateReady) {
                    root._baseVersion++
                } else if (state === root.stateMissing) {
                    root._ensure(root.baseThumbnailId)
                }
            }

            if (idValue === root.preferredThumbnailId) {
                root._preferredState = state
                if (state === root.stateReady) {
                    root._preferredVersion++
                } else if (state === root.stateMissing && root._preferPreferred) {
                    root._ensure(root.preferredThumbnailId)
                }
            }
        }
    }

    Rectangle {
        id: frame
        anchors.fill: parent
        radius: root.cornerRadius
        color: root.backgroundColor
        border.width: root.showFailureBorder && root._showFailureFallback ? 2 : 0
        border.color: root.failureBorderColor

        Item {
            id: contentLayer
            anchors.fill: parent
            layer.enabled: root.cornerRadius > 0
            layer.samples: 4
            layer.effect: MultiEffect {
                maskEnabled: true
                maskThresholdMin: 0.5
                maskSpreadAtMin: 1.0
                maskSource: cornerMask
            }

            Image {
                anchors.fill: parent
                source: root._imageSource
                fillMode: root.fillMode
                asynchronous: true
                smooth: true
                mipmap: true
                cache: true
                sourceSize: root.requestedSourceSize
                visible: root._hasReadyThumbnail
            }

            Rectangle {
                anchors.fill: parent
                color: root.backgroundColor
                opacity: root._hasReadyThumbnail ? 0.2 : 0.92
                visible: root._showLoadingOverlay

                BusyIndicator {
                    anchors.centerIn: parent
                    running: parent.visible
                    implicitWidth: Math.min(root.width, root.height, 36)
                    implicitHeight: implicitWidth
                }
            }

            ColoredIcon {
                anchors.centerIn: parent
                source: root.mediaType === 1 ? Theme.icon("video") : Theme.icon("image")
                iconSize: Math.max(16, Math.min(root.width, root.height) * 0.34)
                color: root.fallbackColor
                visible: !root._hasReadyThumbnail && !root._showLoadingOverlay
            }
        }

        Item {
            id: cornerMask
            visible: false
            layer.enabled: true
            width: frame.width
            height: frame.height

            Rectangle {
                anchors.fill: parent
                radius: root.cornerRadius
                color: "white"
                antialiasing: true
            }
        }
    }
}
