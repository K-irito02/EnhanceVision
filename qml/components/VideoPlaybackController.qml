import QtQuick
import QtMultimedia
import EnhanceVision.Controllers

/**
 * @brief 视频播放控制器 - 管理三个独立的播放控制功能
 *
 * 功能说明：
 * 1. 开/切自动播放 (videoAutoPlay): 从其他媒体切换到视频或初次打开视频时，是否自动播放
 * 2. 源/结自动播放 (videoAutoPlayOnSwitch): 点击"源件/结果"按钮切换时，是否自动播放
 * 3. 源/结恢复进度 (videoRestorePosition): 点击"源件/结果"按钮切换时，是否恢复播放进度
 *
 * 设计原则：
 * - 三个功能完全独立，互不影响
 * - 用户手动播放/暂停操作不影响任何设置
 * - 使用 BufferedMedia 状态确保媒体完全就绪后再执行操作
 */
QtObject {
    id: controller

    // ========== 外部绑定属性 ==========
    property var mediaPlayer: null
    property bool isVideo: false
    property string currentSource: ""
    property var currentFile: null

    // ========== 内部状态 ==========
    // 切换模式：0=无，1=普通文件打开，2=源件/结果切换
    property int _switchMode: 0
    // 切换前保存的播放进度（仅用于源件/结果切换）
    property int _savedPosition: 0
    // 切换前保存的播放速率
    property real _savedPlaybackRate: 1.0
    // 当前正在处理的源
    property string _pendingSource: ""
    // 是否已处理完成当前源
    property bool _handled: false
    
    // ========== 公开状态（供 UI 绑定）==========
    // 是否正在源/结切换中（用于显示加载图标）
    readonly property bool isLoading: _switchMode === 2
    // 切换期间冻结的进度值（用于进度条平滑过渡）
    property int frozenPosition: 0

    // ========== 信号 ==========
    signal progressRestored(int position)

    // ========== 延迟执行定时器 ==========
    property Timer _delayTimer: Timer {
        interval: 80
        repeat: false
        onTriggered: controller._executeDelayedAction()
    }
    
    // 播放延迟定时器（seek完成后再播放）
    property Timer _playTimer: Timer {
        interval: 100
        repeat: false
        onTriggered: {
            if (controller.mediaPlayer && SettingsController.videoAutoPlayOnSwitch) {
                controller.mediaPlayer.play()
            }
        }
    }
    
    property int _delayedAction: 0  // 0=无，1=普通播放，2=源结切换播放

    // ========== 公共方法 ==========

    /**
     * @brief 处理普通文件打开（从其他媒体切换到视频，或初次打开视频）
     * 触发条件：currentIndex 变化、从图片切到视频等
     * 控制按钮：开/切自动播放
     */
    function handleFileOpen() {
        if (_switchMode === 2) {
            return
        }
        
        _switchMode = 1
        _handled = false
        _pendingSource = ""
    }

    /**
     * @brief 准备源件/结果切换（在 showOriginal 变化时调用）
     * 保存当前状态，标记为源件/结果切换模式
     */
    function prepareSourceResultSwitch() {
        if (!mediaPlayer) return
        
        // 保存当前状态
        var currentPos = mediaPlayer.position
        var duration = mediaPlayer.duration
        
        // 检查视频是否已播放完毕（位置接近或等于时长）
        var isAtEnd = duration > 0 && currentPos >= duration - 100
        
        // 如果视频已播放完毕，从头开始；否则保存当前位置
        _savedPosition = isAtEnd ? 0 : currentPos
        _savedPlaybackRate = mediaPlayer.playbackRate
        _switchMode = 2
        _handled = false
        _pendingSource = ""
        
        // 冻结进度条显示（避免从00:00跳跃）
        frozenPosition = _savedPosition
    }

    /**
     * @brief 处理媒体状态变化
     * @param status MediaPlayer.MediaStatus 值
     */
    function handleMediaStatusChanged(status) {
        if (!mediaPlayer) return

        var currentSrc = mediaPlayer.source.toString()

        if (status === MediaPlayer.BufferedMedia || status === MediaPlayer.LoadedMedia) {
            if (_handled && currentSrc === _pendingSource) {
                return
            }

            _pendingSource = currentSrc
            _handled = true

            if (_switchMode === 2) {
                _delayedAction = 2
                _delayTimer.restart()
            } else if (_switchMode === 1) {
                _delayedAction = 1
                _delayTimer.restart()
            }
        }
    }

    /**
     * @brief 执行延迟操作（由 Timer 调用）
     */
    function _executeDelayedAction() {
        if (!mediaPlayer) return

        if (_delayedAction === 2) {
            mediaPlayer.playbackRate = _savedPlaybackRate

            var needSeek = SettingsController.videoRestorePosition && _savedPosition > 0
            var targetPos = 0
            
            if (needSeek) {
                targetPos = Math.min(_savedPosition, mediaPlayer.duration > 0 ? mediaPlayer.duration : _savedPosition)
            }

            if (SettingsController.videoAutoPlayOnSwitch) {
                mediaPlayer.play()
                
                if (needSeek) {
                    Qt.callLater(function() {
                        if (mediaPlayer) {
                            mediaPlayer.position = targetPos
                            progressRestored(targetPos)
                        }
                    })
                }
            } else {
                if (needSeek) {
                    mediaPlayer.position = targetPos
                    progressRestored(targetPos)
                }
                mediaPlayer.pause()
            }

            _switchMode = 0
        } else if (_delayedAction === 1) {
            if (SettingsController.videoAutoPlay) {
                Qt.callLater(function() {
                    if (mediaPlayer) {
                        mediaPlayer.play()
                    }
                })
            }

            _switchMode = 0
        }

        _delayedAction = 0
    }

    /**
     * @brief 处理媒体加载完成（兼容旧接口）
     */
    function handleMediaLoaded() {
        handleMediaStatusChanged(MediaPlayer.LoadedMedia)
    }

    /**
     * @brief 处理媒体加载错误
     */
    function handleMediaError(error, errorString) {
        _resetState()
    }

    /**
     * @brief 重置所有内部状态
     */
    function _resetState() {
        _switchMode = 0
        _savedPosition = 0
        _savedPlaybackRate = 1.0
        _pendingSource = ""
        _handled = false
        _delayedAction = 0
        frozenPosition = 0
        _delayTimer.stop()
        _playTimer.stop()
    }
}
