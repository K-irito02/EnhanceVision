# FFmpeg Post-Build Copy 失败修复笔记

## 概述

**创建日期**: 2026-03-24  
**问题类型**: 构建错误（MSB3073 post-build 失败）

---

## 一、问题现象

构建时 C++ 编译全部通过，`EnhanceVision.exe` 成功生成，但 post-build 阶段报错：

```
Error copying directory from
  "E:/QtAudio-VideoLearning/EnhanceVision/third_party/ffmpeg/bin"
  to
  "E:/QtAudio-VideoLearning/EnhanceVision/build/msvc2022/Release/Release"
MSB3073: exit code -1
```

---

## 二、根因分析

`third_party/ffmpeg/` 目录存在但完全为空（无 `bin/`、`include/`、`lib/` 子目录）。  
CMake `copy_directory` 命令对不存在的源目录直接报错退出，导致整个 post-build 步骤失败。

项目期望在 `third_party/ffmpeg/` 下放置 FFmpeg 7.1 Windows 预编译库，但该库未随仓库提交（体积大，通常通过外部途径手动放置）。

---

## 三、修复方案

### 1. 创建空目录占位

创建 `third_party/ffmpeg/bin/`、`include/`、`lib/` 三个空目录，确保路径合法。

### 2. 新增 CMake 辅助脚本

新建 `cmake/copy_if_exists.cmake`：

```cmake
# 仅在源目录存在且非空时才执行 copy
if(EXISTS "${SRC_DIR}")
    file(GLOB _contents "${SRC_DIR}/*")
    if(_contents)
        file(COPY "${SRC_DIR}/" DESTINATION "${DST_DIR}")
    else()
        message(STATUS "Skipping copy: ${SRC_DIR} is empty")
    endif()
else()
    message(STATUS "Skipping copy: ${SRC_DIR} does not exist")
endif()
```

### 3. 修改 CMakeLists.txt post-build 命令

将原来的硬拷贝：
```cmake
COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${THIRD_PARTY_DIR}/ffmpeg/bin"
    "$<TARGET_FILE_DIR:EnhanceVision>"
```

改为调用辅助脚本（目录为空时静默跳过）：
```cmake
COMMAND ${CMAKE_COMMAND}
    -DSRC_DIR="${THIRD_PARTY_DIR}/ffmpeg/bin"
    -DDST_DIR="$<TARGET_FILE_DIR:EnhanceVision>"
    -P "${CMAKE_SOURCE_DIR}/cmake/copy_if_exists.cmake"
```

---

## 四、修改文件清单

| 文件 | 修改内容 |
|------|----------|
| `CMakeLists.txt` | post-build ffmpeg copy 改为条件拷贝 |
| `cmake/copy_if_exists.cmake` | 新增：条件拷贝辅助脚本 |
| `third_party/ffmpeg/bin/` | 新增空目录（占位） |
| `third_party/ffmpeg/include/` | 新增空目录（占位） |
| `third_party/ffmpeg/lib/` | 新增空目录（占位） |

---

## 五、构建验证

- 构建结果：✅ exit code 0
- 唯一警告：`windeployqt` 找不到 VCINSTALLDIR（无害，不影响运行）
- 程序启动：✅ 正常

---

## 六、后续事项

- [ ] 若需要实际的 FFmpeg 视频处理功能，需手动下载 FFmpeg 7.1 Windows 预编译包并解压到 `third_party/ffmpeg/`
  - 推荐来源：https://github.com/BtbN/FFmpeg-Builds/releases（ffmpeg-n7.1-latest-win64-gpl-shared）
  - 解压后确保 `bin/` 含 `.dll`，`lib/` 含 `.lib`，`include/` 含头文件
- [ ] 考虑在 README 中补充 FFmpeg 手动部署说明
