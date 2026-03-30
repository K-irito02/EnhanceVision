# C++ 编译错误

## C1083: 无法打开包括文件

**原因**：头文件路径错误或模块未添加

**解决方案**：

1. 检查 CMakeLists.txt 中是否添加了对应的 Qt 模块：

```cmake
find_package(Qt6 REQUIRED COMPONENTS 
    Core 
    Gui 
    Quick 
    Widgets
    Multimedia
)
```

2. 检查头文件路径：

```cmake
target_include_directories(EnhanceVision PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EnhanceVision
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
```

3. 检查 Qt 模块链接：

```cmake
target_link_libraries(EnhanceVision PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Widgets
    Qt6::Multimedia
)
```

## C2011: 重定义

**原因**：类型重复定义，通常是由于头文件重复包含

**解决方案**：

```cpp
// 方案1：头文件保护
#ifndef CLASS_NAME_H
#define CLASS_NAME_H
// 类定义
#endif

// 方案2：#pragma once
#pragma once
// 类定义

// 方案3：前置声明解决循环依赖
class OtherClass;

class MyClass {
    OtherClass* m_other;  // 使用指针，无需完整定义
};
```

## C2027: 使用了未定义类型

**原因**：缺少头文件包含或前置声明不完整

**解决方案**：

```cpp
// MyClass.h
class OtherClass;  // 前置声明

class MyClass {
    void doSomething(OtherClass* other);
};

// MyClass.cpp
#include "OtherClass.h"  // 包含完整定义

void MyClass::doSomething(OtherClass* other) {
    other->method();
}
```

## C2039: 不是类的成员

**原因**：方法未声明或实现

**解决方案**：

```cpp
// 1. 检查头文件中是否声明
class MyClass {
public:
    void myMethod();  // 确保声明
};

// 2. 检查实现文件中是否定义
void MyClass::myMethod() {
    // 实现
}

// 3. 检查 const 修饰符是否一致
// 头文件
void myMethod() const;

// 实现文件
void MyClass::myMethod() const {  // 必须一致
    // 实现
}
```

## C2440: 类型转换错误

**原因**：类型不兼容

**解决方案**：

```cpp
// 错误：const char* 到 QString（旧式）
QString str = "hello";

// 正确方式
QString str = QStringLiteral("hello");
QString str = QLatin1String("hello");
QString str = QString::fromUtf8("hello");
```

## C2664/C2672: 函数参数不匹配

**原因**：参数类型或数量不匹配

**解决方案**：

```cpp
// 1. 检查函数签名
void process(const QString& name, int value);

process("test", 42);  // 正确
process(42, "test");  // 错误，参数顺序错误

// 2. 使用 qOverload 解决重载歧义
connect(obj, qOverload<int>(&QComboBox::currentIndexChanged), 
        this, &MyClass::onIndexChanged);
```
