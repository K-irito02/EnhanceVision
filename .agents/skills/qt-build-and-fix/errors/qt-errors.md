# Qt 错误

## MOC 错误

### Q_ENUM 宏错误

**原因**：宏不在 Q_OBJECT 类内

**解决方案**：

```cpp
// 错误
class MyClass {
public:
    enum class Status { OK, Error };
};
Q_ENUM(MyClass::Status)  // 错误位置

// 正确
class MyClass : public QObject {
    Q_OBJECT
public:
    enum class Status { OK, Error };
    Q_ENUM(Status)  // 必须在类内
};
```

### 未定义的 vtable

**原因**：Q_OBJECT 宏但未运行 MOC

**解决方案**：

```cmake
# 方案1：确保头文件包含在构建中
set(HEADERS
    include/EnhanceVision/core/MyClass.h
)

target_sources(EnhanceVision PRIVATE
    ${HEADERS}
    ${SOURCES}
)

# 方案2：使用 AUTOMOC
set(CMAKE_AUTOMOC ON)
```

### 信号槽连接失败

**原因**：签名不匹配或对象问题

**解决方案**：

```cpp
// 检查连接返回值
bool connected = connect(sender, &Sender::signal, 
                         receiver, &Receiver::slot);
if (!connected) {
    qWarning() << "连接失败";
}

// 使用新式连接（编译时检查）
connect(sender, &Sender::signal, 
        receiver, &Receiver::slot);

// 处理重载信号
connect(comboBox, qOverload<int>(&QComboBox::currentIndexChanged),
        this, &MyClass::onIndexChanged);

// Lambda 连接
connect(button, &QPushButton::clicked, this, [this]() {
    // 处理
});
```

## QML 错误

### QML 模块未找到

**原因**：QML 文件未注册

**解决方案**：

```cmake
qt_add_qml_module(EnhanceVision
    URI EnhanceVision
    VERSION 1.0
    QML_FILES
        qml/main.qml
        qml/App.qml
        qml/pages/MainPage.qml
        qml/components/Sidebar.qml
    RESOURCES
        resources/icons/...
)
```

### QML 类型未定义

**原因**：缺少 import 或注册

**解决方案**：

```qml
// 检查 import
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EnhanceVision  // 自定义模块
```

```cpp
// C++ 注册类型
qmlRegisterType<MyClass>("EnhanceVision", 1, 0, "MyClass");

// 或使用 QML_ELEMENT
class MyClass : public QObject {
    Q_OBJECT
    QML_ELEMENT
    // ...
};
```

### 属性绑定失败

**原因**：属性未正确暴露

**解决方案**：

```cpp
// C++ 暴露属性
class MyClass : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    
public:
    QString name() const { return m_name; }
    void setName(const QString& name) {
        if (m_name != name) {
            m_name = name;
            emit nameChanged();
        }
    }
    
signals:
    void nameChanged();
    
private:
    QString m_name;
};
```

```qml
// QML 使用
MyClass {
    name: "test"  // 绑定
}
```
