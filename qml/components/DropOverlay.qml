import QtQuick
import QtQuick.Effects
import "../styles"

/**
 * @brief 拖拽文件覆盖层组件
 *
 * 当用户拖拽文件到应用窗口时显示的艺术字提示覆盖层。
 * 特性：
 * - 华文行楷艺术字体
 * - 光束碰撞爆炸动画背景（Background Beams With Collision 风格）
 * - 响应式字号（根据窗口尺寸自适应）
 * - 最高层级显示（z: 9999）
 * - 支持中英文国际化
 */
Item {
    id: root

    // ========== 属性 ==========
    property bool active: false
    property string text: qsTr("释放以添加文件")

    // 内部状态
    property bool _animating: false

    // 层级：确保在所有元素之上
    z: 9999
    visible: active || _animating

    // ========== 字体加载 ==========
    FontLoader {
        id: chineseFont
        source: "qrc:/qt/qml/EnhanceVision/resources/font/白舟鯨海酔侯書体.ttf"
    }

    FontLoader {
        id: englishFont
        source: "qrc:/qt/qml/EnhanceVision/resources/font/华文行楷.ttf"
    }

    // 根据文本内容判断使用哪个字体
    property bool _isChineseText: /[\u4e00-\u9fa5]/.test(root.text)
    property string _fontFamily: _isChineseText ? 
        (chineseFont.status === FontLoader.Ready ? chineseFont.name : "SimSun") :
        (englishFont.status === FontLoader.Ready ? englishFont.name : "Arial")

    // ========== 背景遮罩 ==========
    Rectangle {
        id: backdrop
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.isDark ? "#0d1117" : "#f8fafc" }
            GradientStop { position: 1.0; color: Theme.isDark ? "#161b22" : "#e2e8f0" }
        }
        opacity: 0

        Behavior on opacity {
            NumberAnimation {
                duration: 250
                easing.type: Easing.OutCubic
            }
        }
    }

    // ========== 雨滴容器 ==========
    Item {
        id: beamsContainer
        anchors.fill: parent
        opacity: 0
        clip: true

        // ========== 底部地面线（碰撞区域） ==========
        Rectangle {
            id: groundLine
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 60
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width * 0.9
            height: 1
            color: Qt.rgba(99, 68, 245, 0.3)
            
            layer.enabled: true
            layer.effect: MultiEffect {
                blurEnabled: true
                blur: 0.5
                blurMax: 8
            }
        }

        // ========== 雨滴生成器 ==========
        Repeater {
            id: beamRepeater
            model: 15

            Item {
                id: raindropItem
                width: 3
                height: 60 + Math.random() * 80
                x: root.width * Math.random()
                y: -height - Math.random() * 300
                opacity: 0

                property real dropDelay: Math.random() * 2000
                property real dropDuration: 1800 + Math.random() * 1500
                property real groundY: root.height - 60 - height
                property alias animation: dropAnimation

                // 雨滴主体（前粗后细的渐变线条）
                Canvas {
                    id: raindropCanvas
                    anchors.fill: parent
                    
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
                        
                        // 创建渐变（从上到下：细→粗→细）
                        var gradient = ctx.createLinearGradient(width/2, 0, width/2, height)
                        gradient.addColorStop(0, "transparent")
                        gradient.addColorStop(0.1, "rgba(24, 204, 252, 0.3)")
                        gradient.addColorStop(0.3, "rgba(99, 68, 245, 0.7)")
                        gradient.addColorStop(0.6, "rgba(174, 72, 255, 0.9)")
                        gradient.addColorStop(0.85, "rgba(255, 255, 255, 1)")
                        gradient.addColorStop(1, "rgba(255, 255, 255, 0.8)")
                        
                        // 绘制雨滴形状（前端稍粗）
                        ctx.beginPath()
                        ctx.moveTo(width/2, 0)
                        // 左边线（从细到粗）
                        ctx.bezierCurveTo(width/2 - 0.3, height * 0.3, 
                                          width/2 - 0.8, height * 0.7, 
                                          width/2 - 1.2, height)
                        // 底部圆弧
                        ctx.arc(width/2, height, 1.2, Math.PI, 0, true)
                        // 右边线（从粗到细）
                        ctx.bezierCurveTo(width/2 + 0.8, height * 0.7,
                                          width/2 + 0.3, height * 0.3,
                                          width/2, 0)
                        ctx.closePath()
                        ctx.fillStyle = gradient
                        ctx.fill()
                    }
                    
                    Component.onCompleted: requestPaint()
                }

                // 雨滴下落动画
                SequentialAnimation {
                    id: dropAnimation
                    running: false
                    loops: Animation.Infinite

                    PauseAnimation { duration: raindropItem.dropDelay }

                    // 淡入
                    NumberAnimation {
                        target: raindropItem
                        property: "opacity"
                        from: 0
                        to: 0.85
                        duration: 100
                    }

                    // 加速下落
                    NumberAnimation {
                        target: raindropItem
                        property: "y"
                        from: -raindropItem.height - Math.random() * 100
                        to: raindropItem.groundY
                        duration: raindropItem.dropDuration
                        easing.type: Easing.InQuad
                    }

                    // 碰撞溅射
                    ScriptAction {
                        script: {
                            // 在碰撞位置生成溅射粒子
                            var collisionX = raindropItem.x + raindropItem.width / 2
                            var collisionY = root.height - 60
                            root._spawnSplash(collisionX, collisionY)
                        }
                    }

                    // 雨滴消失
                    NumberAnimation {
                        target: raindropItem
                        property: "opacity"
                        to: 0
                        duration: 80
                        easing.type: Easing.OutCubic
                    }

                    // 重置
                    ScriptAction {
                        script: {
                            raindropItem.x = root.width * Math.random()
                            raindropItem.y = -raindropItem.height - Math.random() * 300
                            raindropItem.dropDelay = Math.random() * 1200
                            raindropItem.dropDuration = 1800 + Math.random() * 1500
                        }
                    }

                    PauseAnimation { duration: Math.random() * 500 }
                }
            }
        }

        // ========== 溅射粒子容器（使用绝对坐标） ==========
        Item {
            id: splashContainer
            anchors.fill: parent

            Repeater {
                id: splashRepeater
                model: 30

                Item {
                    id: splashItem
                    width: 3 + Math.random() * 6
                    height: width
                    x: 0
                    y: 0
                    visible: false

                    property real spreadX: 0
                    property real spreadY: 0
                    property real fallY: 0
                    property real startX: 0
                    property real startY: 0
                    property alias animation: splashAnim

                    Rectangle {
                        id: splashDrop
                        anchors.fill: parent
                        radius: width / 2
                        opacity: 0
                        color: ["#18CCFC", "#6344F5", "#AE48FF", "#FFFFFF", "#87CEEB", "#E0E7FF"][index % 6]
                        
                        layer.enabled: true
                        layer.effect: MultiEffect {
                            blurEnabled: true
                            blur: 0.2
                            blurMax: 4
                        }
                    }

                    // 溅射动画（慢动作效果，持续约1.3秒）
                    SequentialAnimation {
                        id: splashAnim
                        running: false

                        ScriptAction {
                            script: splashItem.visible = true
                        }

                        // 第一阶段：向上弹起（慢动作）
                        ParallelAnimation {
                            NumberAnimation {
                                target: splashDrop
                                property: "opacity"
                                from: 0
                                to: 1
                                duration: 100
                            }
                            NumberAnimation {
                                target: splashItem
                                property: "x"
                                from: splashItem.startX
                                to: splashItem.startX + splashItem.spreadX
                                duration: 500
                                easing.type: Easing.OutCubic
                            }
                            NumberAnimation {
                                target: splashItem
                                property: "y"
                                from: splashItem.startY
                                to: splashItem.startY + splashItem.spreadY
                                duration: 500
                                easing.type: Easing.OutCubic
                            }
                            NumberAnimation {
                                target: splashItem
                                property: "scale"
                                from: 0.3
                                to: 1.0
                                duration: 300
                                easing.type: Easing.OutCubic
                            }
                        }

                        // 第二阶段：缓慢落下（慢动作）
                        ParallelAnimation {
                            NumberAnimation {
                                target: splashItem
                                property: "y"
                                to: splashItem.startY + splashItem.spreadY + splashItem.fallY
                                duration: 600
                                easing.type: Easing.InQuad
                            }
                            NumberAnimation {
                                target: splashItem
                                property: "scale"
                                to: 0.5
                                duration: 600
                            }
                        }

                        // 第三阶段：淡出消失
                        NumberAnimation {
                            target: splashDrop
                            property: "opacity"
                            to: 0
                            duration: 200
                            easing.type: Easing.InQuad
                        }

                        ScriptAction {
                            script: splashItem.visible = false
                        }
                    }
                }
            }
        }
    }

    // ========== 溅射粒子索引 ==========
    property int _splashIndex: 0

    // ========== 生成溅射粒子函数 ==========
    function _spawnSplash(collisionX, collisionY) {
        var particleCount = 8 + Math.floor(Math.random() * 5)
        for (var i = 0; i < particleCount; i++) {
            var p = splashRepeater.itemAt(_splashIndex)
            if (p) {
                p.startX = collisionX - p.width / 2
                p.startY = collisionY
                p.spreadX = (Math.random() - 0.5) * 150
                p.spreadY = -40 - Math.random() * 100
                p.fallY = 50 + Math.random() * 40
                p.x = p.startX
                p.y = p.startY
                p.scale = 0.3
                p.animation.restart()
            }
            _splashIndex = (_splashIndex + 1) % splashRepeater.count
        }
    }

    // ========== 艺术字容器 ==========
    Item {
        id: textContainer
        anchors.centerIn: parent
        width: artText.paintedWidth + 60
        height: artText.paintedHeight + 40
        opacity: 0

        // ========== 文字发光层 ==========
        Text {
            id: glowText
            anchors.centerIn: parent
            text: root.text
            color: "#FFFFFF"
            font.family: root._fontFamily
            font.pixelSize: _calculateFontSize()
            font.weight: Font.Bold
            opacity: 0.5

            layer.enabled: true
            layer.effect: MultiEffect {
                blurEnabled: true
                blur: 0.6
                blurMax: 32
            }
        }

        // ========== 文字描边层 ==========
        Repeater {
            model: 8
            Text {
                anchors.centerIn: parent
                anchors.horizontalCenterOffset: Math.cos(index * Math.PI / 4) * 2
                anchors.verticalCenterOffset: Math.sin(index * Math.PI / 4) * 2
                text: root.text
                color: "#FFFFFF"
                font.family: root._fontFamily
                font.pixelSize: _calculateFontSize()
                font.weight: Font.Bold
            }
        }

        // ========== 主文字层 ==========
        Text {
            id: artText
            anchors.centerIn: parent
            text: root.text
            color: "#0A0A0A"
            font.family: root._fontFamily
            font.pixelSize: _calculateFontSize()
            font.weight: Font.Bold

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.3)
                shadowBlur: 0.3
                shadowVerticalOffset: 3
                shadowHorizontalOffset: 1
            }
        }
    }

    // ========== 入场动画 ==========
    ParallelAnimation {
        id: enterAnimation
        running: false

        NumberAnimation {
            target: textContainer
            property: "opacity"
            from: 0
            to: 1
            duration: 400
            easing.type: Easing.OutCubic
        }

        NumberAnimation {
            target: beamsContainer
            property: "opacity"
            from: 0
            to: 1
            duration: 350
            easing.type: Easing.OutCubic
        }

        NumberAnimation {
            target: backdrop
            property: "opacity"
            from: 0
            to: 1
            duration: 300
            easing.type: Easing.OutCubic
        }

        onStarted: {
            root._animating = true
            _resetAllAnimations()
            _startBeamAnimations()
        }
    }

    // ========== 退场动画 ==========
    ParallelAnimation {
        id: exitAnimation
        running: false

        NumberAnimation {
            target: textContainer
            property: "opacity"
            from: 1
            to: 0
            duration: 200
            easing.type: Easing.InCubic
        }

        NumberAnimation {
            target: beamsContainer
            property: "opacity"
            from: 1
            to: 0
            duration: 200
            easing.type: Easing.InCubic
        }

        NumberAnimation {
            target: backdrop
            property: "opacity"
            from: 1
            to: 0
            duration: 200
            easing.type: Easing.InCubic
        }

        onFinished: {
            root._animating = false
            _stopBeamAnimations()
            _resetAllAnimations()
        }
    }

    // ========== 状态切换 ==========
    onActiveChanged: {
        if (active) {
            exitAnimation.stop()
            enterAnimation.restart()
        } else {
            enterAnimation.stop()
            exitAnimation.restart()
        }
    }

    // ========== 辅助函数 ==========
    function _calculateFontSize() {
        var baseSize = Math.min(root.width, root.height) * 0.08
        return Math.max(32, Math.min(baseSize, 72))
    }

    function _resetAllAnimations() {
        // 重置雨滴位置
        for (var i = 0; i < beamRepeater.count; i++) {
            var beam = beamRepeater.itemAt(i)
            if (beam) {
                beam.x = root.width * Math.random()
                beam.y = -beam.height - Math.random() * 300
                beam.opacity = 0
                beam.dropDelay = Math.random() * 2000
                beam.dropDuration = 1800 + Math.random() * 1500
            }
        }
        // 重置溅射粒子
        for (var j = 0; j < splashRepeater.count; j++) {
            var splash = splashRepeater.itemAt(j)
            if (splash) {
                splash.visible = false
                splash.animation.stop()
            }
        }
        _splashIndex = 0
    }

    function _startBeamAnimations() {
        for (var i = 0; i < beamRepeater.count; i++) {
            var beam = beamRepeater.itemAt(i)
            if (beam && beam.animation) beam.animation.restart()
        }
    }

    function _stopBeamAnimations() {
        for (var i = 0; i < beamRepeater.count; i++) {
            var beam = beamRepeater.itemAt(i)
            if (beam && beam.animation) beam.animation.stop()
        }
    }
}
