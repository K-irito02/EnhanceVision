# 链接错误

## LNK2019: 无法解析的外部符号

**原因**：库未链接或实现缺失

**解决方案**：

1. 检查库链接：

```cmake
target_link_libraries(EnhanceVision PRIVATE
    Qt6::Core
    Qt6::Gui
    # 添加缺失的库
)
```

2. 检查实现文件是否包含在构建中：

```cmake
set(SOURCES
    src/core/MyClass.cpp  # 确保包含
)
```

3. 检查第三方库路径：

```cmake
target_link_directories(EnhanceVision PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/lib
)
```

## LNK2001: 无法解析的外部符号

**原因**：静态成员未定义

**解决方案**：

```cpp
// 头文件
class MyClass {
    static int s_count;  // 声明
};

// 实现文件
int MyClass::s_count = 0;  // 定义
```

## LNK1112: 模块计算机类型冲突

**原因**：32位和64位库混用

**解决方案**：

```cmake
# 确保使用 x64 架构
set(CMAKE_GENERATOR_PLATFORM x64)

# 检查库路径
# 32位库：lib/x86
# 64位库：lib/x64
```

## LNK1181: 无法打开输入文件

**原因**：库文件路径错误

**解决方案**：

```cmake
# 检查库文件是否存在
if(NOT EXISTS "${LIB_PATH}/mylib.lib")
    message(FATAL_ERROR "库文件未找到: ${LIB_PATH}/mylib.lib")
endif()

# 使用完整路径
target_link_libraries(EnhanceVision PRIVATE
    "${LIB_PATH}/mylib.lib"
)
```

## FFmpeg 链接问题

```cmake
# FFmpeg 链接配置
set(FFMPEG_LIB_DIR "${THIRD_PARTY_DIR}/ffmpeg/lib")
set(FFMPEG_LIBS avcodec avformat avutil swscale swresample)

target_link_directories(EnhanceVision PRIVATE
    "${FFMPEG_LIB_DIR}"
)

if(WIN32)
    foreach(lib ${FFMPEG_LIBS})
        target_link_libraries(EnhanceVision PRIVATE "${lib}.lib")
    endforeach()
endif()
```
