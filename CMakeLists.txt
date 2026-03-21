cmake_minimum_required(VERSION 3.20)
project(EnhanceVision VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ========== 构建优化选项 ==========
option(USE_PRECOMPILED_HEADERS "Use precompiled headers for faster compilation" ON)
option(BUILD_NCNN_FROM_SOURCE "Build NCNN from source (disable if using pre-built)" OFF)

# ========== 编译缓存支持 ==========
# 尝试使用 ccache 或 sccache 加速编译
find_program(CCACHE_PROGRAM ccache)
find_program(SCCACHE_PROGRAM sccache)
if(CCACHE_PROGRAM)
    message(STATUS "Using ccache: ${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
elseif(SCCACHE_PROGRAM)
    message(STATUS "Using sccache: ${SCCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${SCCACHE_PROGRAM}")
endif()

# Qt 模块
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Quick
    QuickWidgets
    QuickControls2
    Concurrent
    Multimedia
    Network
    ShaderTools
    LinguistTools
    QuickEffects
)

# Qt Policy 设置（必须在 find_package(Qt6) 之后）
# QTP0001: 设置 QML 模块资源前缀
# QTP0004: 设置 qmldir 文件策略
qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

# 第三方库
set(THIRD_PARTY_DIR "${CMAKE_SOURCE_DIR}/third_party")

# 禁用 NCNN 不需要的组件（必须在 add_subdirectory 之前设置）
set(NCNN_BUILD_EXAMPLES OFF CACHE BOOL "Disable NCNN examples" FORCE)
set(NCNN_BUILD_TOOLS OFF CACHE BOOL "Disable NCNN tools" FORCE)
set(NCNN_PYTHON_API OFF CACHE BOOL "Disable NCNN Python API" FORCE)

# NCNN - 优化：使用预编译库而非从源码构建
if(BUILD_NCNN_FROM_SOURCE)
    add_subdirectory("${THIRD_PARTY_DIR}/ncnn")
else()
    # 使用预编译的 NCNN
    find_package(ncnn QUIET PATHS "${THIRD_PARTY_DIR}/ncnn/build/install" NO_DEFAULT_PATH)
    if(NOT ncnn_FOUND)
        # 如果没有预编译库，回退到从源码构建
        message(STATUS "NCNN pre-built not found, building from source...")
        add_subdirectory("${THIRD_PARTY_DIR}/ncnn")
    else()
        message(STATUS "Using pre-built NCNN")
    endif()
endif()

# FFmpeg
set(FFMPEG_INCLUDE_DIR "${THIRD_PARTY_DIR}/ffmpeg/include")
set(FFMPEG_LIB_DIR "${THIRD_PARTY_DIR}/ffmpeg/lib")
set(FFMPEG_LIBS
    avcodec avformat avutil swscale swresample
)

# 头文件搜索路径
include_directories(
    "${CMAKE_SOURCE_DIR}/include"
    "${FFMPEG_INCLUDE_DIR}"
)

# 可执行文件
qt_add_executable(EnhanceVision
    src/main.cpp
    src/app/Application.cpp
    src/app/MainWindow.cpp
    src/core/AIEngine.cpp
    src/core/ModelRegistry.cpp
    src/core/ProcessingEngine.cpp
    src/core/VideoProcessor.cpp
    src/core/ImageProcessor.cpp
    src/core/FrameCache.cpp
    src/core/TaskCoordinator.cpp
    src/core/ResourceManager.cpp
    src/models/SessionModel.cpp
    src/models/FileModel.cpp
    src/models/ProcessingModel.cpp
    src/models/MessageModel.cpp
    src/providers/PreviewProvider.cpp
    src/providers/ThumbnailProvider.cpp
    src/utils/FileUtils.cpp
    src/utils/ImageUtils.cpp
    src/utils/WindowHelper.cpp
    src/utils/SubWindowHelper.cpp
    src/controllers/SettingsController.cpp
    src/controllers/SessionController.cpp
    src/controllers/FileController.cpp
    src/controllers/ProcessingController.cpp
    src/services/ImageExportService.cpp
    include/EnhanceVision/app/Application.h
    include/EnhanceVision/app/MainWindow.h
    include/EnhanceVision/core/AIEngine.h
    include/EnhanceVision/core/ModelRegistry.h
    include/EnhanceVision/core/ProcessingEngine.h
    include/EnhanceVision/core/VideoProcessor.h
    include/EnhanceVision/core/ImageProcessor.h
    include/EnhanceVision/core/FrameCache.h
    include/EnhanceVision/core/TaskCoordinator.h
    include/EnhanceVision/core/ResourceManager.h
    include/EnhanceVision/models/SessionModel.h
    include/EnhanceVision/models/FileModel.h
    include/EnhanceVision/models/ProcessingModel.h
    include/EnhanceVision/models/MessageModel.h
    include/EnhanceVision/models/DataTypes.h
    include/EnhanceVision/providers/PreviewProvider.h
    include/EnhanceVision/providers/ThumbnailProvider.h
    include/EnhanceVision/utils/FileUtils.h
    include/EnhanceVision/utils/ImageUtils.h
    include/EnhanceVision/utils/WindowHelper.h
    include/EnhanceVision/utils/SubWindowHelper.h
    include/EnhanceVision/controllers/SettingsController.h
    include/EnhanceVision/controllers/SessionController.h
    include/EnhanceVision/controllers/FileController.h
    include/EnhanceVision/controllers/ProcessingController.h
    include/EnhanceVision/services/ImageExportService.h
    resources/qml.qrc
)

# ========== 预编译头优化 ==========
if(USE_PRECOMPILED_HEADERS AND MSVC)
    target_precompile_headers(EnhanceVision PRIVATE
        <memory>
        <string>
        <vector>
        <QObject>
        <QAbstractListModel>
        <QImage>
        <QFile>
        <QUrl>
        <QDebug>
    )
endif()

# 国际化翻译文件
set(TS_FILES
    "${CMAKE_SOURCE_DIR}/resources/i18n/app_zh_CN.ts"
    "${CMAKE_SOURCE_DIR}/resources/i18n/app_en_US.ts"
)

qt_add_translations(EnhanceVision
    TS_FILES ${TS_FILES}
    LUPDATE_OPTIONS -no-obsolete
    LRELEASE_OPTIONS -compress
)

# QML 模块
qt_add_qml_module(EnhanceVision
    URI EnhanceVision
    VERSION 1.0
    QML_FILES
        qml/main.qml
        qml/App.qml
        qml/pages/MainPage.qml
        qml/pages/SettingsPage.qml
        qml/components/Sidebar.qml
        qml/components/TitleBar.qml
        qml/components/FileList.qml
        qml/components/PreviewPane.qml
        qml/components/ControlPanel.qml
        qml/components/MediaViewer.qml
        qml/components/MediaThumbnailStrip.qml
        qml/components/MediaViewerWindow.qml
        qml/components/EmbeddedMediaViewer.qml
        qml/components/MinimizedWindowDock.qml
        qml/components/SessionList.qml
        qml/components/SessionBatchBar.qml
        qml/components/Card.qml
        qml/components/MessageItem.qml
        qml/components/MessageList.qml
        qml/components/Toast.qml
        qml/components/Dialog.qml
        qml/components/ShaderParamsPanel.qml
        qml/components/AIModelPanel.qml
        qml/components/AIParamsPanel.qml
        qml/components/DialogTitleBar.qml
        qml/components/FullShaderEffect.qml
        qml/components/OffscreenShaderRenderer.qml
        qml/controls/IconButton.qml
        qml/controls/Slider.qml
        qml/controls/ComboBox.qml
        qml/controls/Button.qml
        qml/controls/TextField.qml
        qml/controls/ColoredIcon.qml
        qml/controls/ParamSlider.qml
        qml/controls/ParamGroup.qml
        qml/controls/Tooltip.qml
        qml/controls/Switch.qml
        qml/shaders/BrightnessContrast.qml
        qml/shaders/SaturationHue.qml
        qml/shaders/SharpenBlur.qml
        qml/shaders/Hue.qml
        qml/shaders/Exposure.qml
        qml/shaders/Gamma.qml
        qml/shaders/Temperature.qml
        qml/shaders/Tint.qml
        qml/shaders/Vignette.qml
        qml/shaders/Highlights.qml
        qml/shaders/Shadows.qml
        qml/styles/Theme.qml
        qml/styles/Colors.qml
        qml/styles/Typography.qml
        qml/utils/Helpers.js
        qml/utils/NotificationManager.qml
    RESOURCES
        qml/styles/qmldir
        resources/icons/home.svg
        resources/icons/settings.svg
        resources/icons/folder.svg
        resources/icons/play.svg
        resources/icons/pause.svg
        resources/icons/upload.svg
        resources/icons/download.svg
        resources/icons/refresh.svg
        resources/icons/trash.svg
        resources/icons/image.svg
        resources/icons/video.svg
        resources/icons/chevron-left.svg
        resources/icons/chevron-right.svg
        resources/icons/check.svg
        resources/icons/x.svg
        resources/icons/sun.svg
        resources/icons/moon.svg
        resources/icons/plus.svg
        resources/icons/panel-left.svg
        resources/icons/globe.svg
        resources/icons/minus.svg
        resources/icons/square.svg
        resources/icons/copy.svg
        resources/icons/arrow-left.svg
        resources/icons/pencil.svg
        resources/icons/save.svg
        resources/icons/x-circle.svg
        resources/icons/zap.svg
        resources/icons/sliders.svg
        resources/icons/rotate-ccw.svg
        resources/icons/send.svg
        resources/icons/eye.svg
        resources/icons/eye-off.svg
        resources/icons/info.svg
        resources/icons/alert-triangle.svg
        resources/icons/check-circle.svg
        resources/icons/message-square.svg
        resources/icons/list.svg
        resources/icons/sparkles.svg
        resources/icons/file-plus.svg
        resources/icons/folder-plus.svg
        resources/icons/loader.svg
        resources/icons/layout-grid.svg
        resources/icons/monitor.svg
        resources/icons/cpu.svg
        resources/icons/help-circle.svg
        resources/icons/eraser.svg
        resources/icons/trash-2.svg
        resources/icons/pin.svg
        resources/icons/edit.svg
        resources/icons/more-vertical.svg
        resources/icons/check-square.svg
        resources/icons/list-select.svg
        resources/icons/chevron-down.svg
        resources/icons/chevron-up.svg
        resources/icons/maximize.svg
        resources/icons/minimize-2.svg
        resources/icons/volume-2.svg
        resources/icons/volume-x.svg
        resources/icons/skip-forward.svg
        resources/icons/skip-back.svg
        resources/icons/grid.svg
        # ParamGroup 图标
        resources/icons/adjust-basic.svg
        resources/icons/detail-enhance.svg
        resources/icons/light-shadow.svg
        resources/icons/color-palette.svg
        # 暗色主题蓝色图标
        resources/icons-dark/alert-triangle.svg
        resources/icons-dark/arrow-left.svg
        resources/icons-dark/check-circle.svg
        resources/icons-dark/check-square.svg
        resources/icons-dark/check.svg
        resources/icons-dark/chevron-left.svg
        resources/icons-dark/chevron-right.svg
        resources/icons-dark/copy.svg
        resources/icons-dark/cpu.svg
        resources/icons-dark/download.svg
        resources/icons-dark/eye-off.svg
        resources/icons-dark/eye.svg
        resources/icons-dark/file-plus.svg
        resources/icons-dark/folder-plus.svg
        resources/icons-dark/folder.svg
        resources/icons-dark/globe.svg
        resources/icons-dark/help-circle.svg
        resources/icons-dark/home.svg
        resources/icons-dark/image.svg
        resources/icons-dark/info.svg
        resources/icons-dark/layout-grid.svg
        resources/icons-dark/list.svg
        resources/icons-dark/loader.svg
        resources/icons-dark/message-square.svg
        resources/icons-dark/minus.svg
        resources/icons-dark/monitor.svg
        resources/icons-dark/moon.svg
        resources/icons-dark/panel-left.svg
        resources/icons-dark/pause.svg
        resources/icons-dark/pencil.svg
        resources/icons-dark/play.svg
        resources/icons-dark/plus.svg
        resources/icons-dark/refresh.svg
        resources/icons-dark/rotate-ccw.svg
        resources/icons-dark/save.svg
        resources/icons-dark/send.svg
        resources/icons-dark/settings.svg
        resources/icons-dark/sliders.svg
        resources/icons-dark/sparkles.svg
        resources/icons-dark/square.svg
        resources/icons-dark/sun.svg
        resources/icons-dark/trash.svg
        resources/icons-dark/trash-2.svg
        resources/icons-dark/pin.svg
        resources/icons-dark/edit.svg
        resources/icons-dark/more-vertical.svg
        resources/icons-dark/upload.svg
        resources/icons-dark/eraser.svg
        resources/icons-dark/list-select.svg
        resources/icons-dark/video.svg
        resources/icons-dark/x-circle.svg
        resources/icons-dark/x.svg
        resources/icons-dark/zap.svg
        resources/icons-dark/chevron-down.svg
        resources/icons-dark/chevron-up.svg
        resources/icons-dark/maximize.svg
        resources/icons-dark/minimize-2.svg
        resources/icons-dark/volume-2.svg
        resources/icons-dark/volume-x.svg
        resources/icons-dark/skip-forward.svg
        resources/icons-dark/skip-back.svg
        resources/icons-dark/grid.svg
        # ParamGroup 暗色图标
        resources/icons-dark/adjust-basic.svg
        resources/icons-dark/detail-enhance.svg
        resources/icons-dark/light-shadow.svg
        resources/icons-dark/color-palette.svg
)

# 编译 Shader 到 QSB 格式
qt6_add_shaders(EnhanceVision "fullshader"
    PRECOMPILE
    OPTIMIZED
    PREFIX
        "/"
    FILES
        resources/shaders/fullshader.vert
        resources/shaders/fullshader.frag
)

# 链接库
target_link_libraries(EnhanceVision PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Quick
    Qt6::QuickWidgets
    Qt6::QuickControls2
    Qt6::Concurrent
    Qt6::Multimedia
    Qt6::Network
    Qt6::ShaderTools
    Qt6::QuickEffects
    ncnn
)

# 链接 FFmpeg 库
target_link_directories(EnhanceVision PRIVATE
    "${FFMPEG_LIB_DIR}"
)

# 使用完整库名链接（Windows 使用 .lib，Linux 使用 .a/.so）
if(WIN32)
    foreach(lib ${FFMPEG_LIBS})
        target_link_libraries(EnhanceVision PRIVATE "${lib}.lib")
    endforeach()
else()
    foreach(lib ${FFMPEG_LIBS})
        target_link_libraries(EnhanceVision PRIVATE "${lib}")
    endforeach()
endif()

# ========== MSVC 编译优化 ==========
if(MSVC)
    # 多进程编译
    target_compile_options(EnhanceVision PRIVATE /MP)
    
    # 调试信息格式优化（减少 PDB 大小）
    target_compile_options(EnhanceVision PRIVATE /Zc:inline)
    
    # Release 模式优化
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(EnhanceVision PRIVATE /O2 /GL)
        target_link_options(EnhanceVision PRIVATE /LTCG)
    endif()
endif()

# Windows 部署
if(WIN32)
    add_custom_command(TARGET EnhanceVision POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${THIRD_PARTY_DIR}/ffmpeg/bin"
            "$<TARGET_FILE_DIR:EnhanceVision>"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/resources/models"
            "$<TARGET_FILE_DIR:EnhanceVision>/models"
    )
    
    find_program(WINDEPLOYQT windeployqt PATHS "${Qt6_DIR}/../../../bin")
    if(WINDEPLOYQT)
        add_custom_command(TARGET EnhanceVision POST_BUILD
            COMMAND "${WINDEPLOYQT}" "$<TARGET_FILE:EnhanceVision>" --qmldir "${CMAKE_SOURCE_DIR}/qml"
        )
    endif()
endif()

# ========== 单元测试 ==========
enable_testing()

find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_shader_consistency
    tests/cpp/test_shader_consistency.cpp
)

target_link_libraries(test_shader_consistency PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
)

target_include_directories(test_shader_consistency PRIVATE
    ${CMAKE_SOURCE_DIR}/include/EnhanceVision
)

add_test(NAME TestShaderConsistency COMMAND test_shader_consistency)
