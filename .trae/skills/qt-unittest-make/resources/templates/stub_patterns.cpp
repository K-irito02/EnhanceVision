// ==================== 常用 Stub 模式 ====================

// 1. UI 显示/隐藏（QWidget, QDialog）
stub.set_lamda(&QWidget::show, [](QWidget *) {
    __DBG_STUB_INVOKE__
});

stub.set_lamda(&QWidget::hide, [](QWidget *) {
    __DBG_STUB_INVOKE__
});

// 对话框执行
stub.set_lamda(VADDR(QDialog, exec), [] {
    __DBG_STUB_INVOKE__
    return QDialog::Accepted;  // 或 QDialog::Rejected
});

// 2. QWidget 尺寸和位置
stub.set_lamda(&QWidget::height, [](QWidget *) -> int {
    __DBG_STUB_INVOKE__
    return 600;  // Mock 高度
});

stub.set_lamda(&QWidget::width, [](QWidget *) -> int {
    __DBG_STUB_INVOKE__
    return 800;  // Mock 宽度
});

stub.set_lamda(&QWidget::x, [](QWidget *) -> int {
    __DBG_STUB_INVOKE__
    return 100;  // Mock X 坐标
});

stub.set_lamda(&QWidget::y, [](QWidget *) -> int {
    __DBG_STUB_INVOKE__
    return 200;  // Mock Y 坐标
});

// 3. QWidget 边距和内容区域
stub.set_lamda(&QWidget::contentsMargins, [](QWidget *) -> QMargins {
    __DBG_STUB_INVOKE__
    return QMargins(10, 10, 10, 10);
});

stub.set_lamda(&QWidget::contentsRect, [](QWidget *) -> QRect {
    __DBG_STUB_INVOKE__
    return QRect(0, 0, 800, 600);
});

// 4. 信号监听（QSignalSpy）
QSignalSpy spy(obj, &{ClassName}::{SignalName});

// 触发信号后验证
EXPECT_EQ(spy.count(), 1);
EXPECT_EQ(spy.at(0).at(0).toInt(), expected);

// 5. 虚函数（使用 VADDR 宏）
stub.set_lamda(VADDR({ClassName}, {MethodName}), []() {
    __DBG_STUB_INVOKE__
});

// 虚函数有返回值
stub.set_lamda(VADDR({ClassName}, {MethodName}), []() -> int {
    __DBG_STUB_INVOKE__
    return 42;
});

// 虚函数有参数
stub.set_lamda(
    VADDR({ClassName}, {MethodName}),
    []({ClassName} *self, int arg1, QString arg2) -> bool {
        __DBG_STUB_INVOKE__
        EXPECT_EQ(arg1, expected);
        EXPECT_EQ(arg2, expectedStr);
        return true;
    }
);

// 6. 重载函数（使用 static_cast）
stub.set_lamda(
    static_cast<int ({ClassName}::*)(int, int)>(&{ClassName}::{MethodName}),
    []({ClassName} *self, int a, int b) -> int {
        __DBG_STUB_INVOKE__
        return a + b;
    }
);

stub.set_lamda(
    static_cast<QString ({ClassName}::*)(const QString &)>(&{ClassName}::{MethodName}),
    []({ClassName} *self, const QString &str) -> QString {
        __DBG_STUB_INVOKE__
        return "mock: " + str;
    }
);

// 7. 外部依赖
stub.set_lamda(&ExternalClass::method, [](ExternalClass *self, QString param) {
    __DBG_STUB_INVOKE__
    EXPECT_EQ(param, "expected");
    return true;
});

// 外部全局函数
stub.set_lamda(qPrintable, [](const QString &str) -> const char* {
    __DBG_STUB_INVOKE__
    static QString mockResult;
    mockResult = "mock: " + str;
    return mockResult.toLocal8Bit().constData();
});

// 8. 文件操作（QFile）
stub.set_lamda(&QFile::open, [](QFile *self, QIODevice::OpenMode mode) -> bool {
    __DBG_STUB_INVOKE__
    return true;  // Mock 打开成功
});

stub.set_lamda(&QFile::readAll, [](QFile *self) -> QByteArray {
    __DBG_STUB_INVOKE__
    return "mock content";
});

stub.set_lamda(&QFile::write, [](QFile *self, const QByteArray &data) -> qint64 {
    __DBG_STUB_INVOKE__
    return data.size();  // Mock 写入成功
});

stub.set_lamda(&QFile::close, [](QFile *self) -> void {
    __DBG_STUB_INVOKE__
});

// 9. 目录操作（QDir）
stub.set_lamda(&QDir::exists, [](QDir *self) -> bool {
    __DBG_STUB_INVOKE__
    return true;
});

stub.set_lamda(&QDir::entryList, [](QDir *self) -> QStringList {
    __DBG_STUB_INVOKE__
    return {"file1.txt", "file2.txt"};
});

// 10. 事件处理
stub.set_lamda(&QObject::eventFilter, [](QObject *self, QObject *watched, QEvent *event) -> bool {
    __DBG_STUB_INVOKE__
    return false;  // 不拦截事件
});

stub.set_lamda(&QWidget::keyPressEvent, [](QWidget *self, QKeyEvent *event) {
    __DBG_STUB_INVOKE__
    // Mock 键盘事件处理
});

stub.set_lamda(&QWidget::mousePressEvent, [](QWidget *self, QMouseEvent *event) {
    __DBG_STUB_INVOKE__
    // Mock 鼠标事件处理
});

// 11. 网络请求（QNetworkReply）
stub.set_lamda(&QNetworkReply::readAll, [](QNetworkReply *self) -> QByteArray {
    __DBG_STUB_INVOKE__
    return "mock network response";
});

stub.set_lamda(&QNetworkReply::error, [](QNetworkReply *self) -> QNetworkReply::NetworkError {
    __DBG_STUB_INVOKE__
    return QNetworkReply::NoError;
});

// 12. 定时器（QTimer）
stub.set_lamda(&QTimer::start, [](QTimer *self, int msec) {
    __DBG_STUB_INVOKE__
});

stub.set_lamda(&QTimer::stop, [](QTimer *self) {
    __DBG_STUB_INVOKE__
});

// 13. 数据库（QSqlQuery）
stub.set_lamda(&QSqlQuery::exec, [](QSqlQuery *self, const QString &query) -> bool {
    __DBG_STUB_INVOKE__
    return true;
});

stub.set_lamda(&QSqlQuery::next, [](QSqlQuery *self) -> bool {
    __DBG_STUB_INVOKE__
    return false;  // Mock 没有更多数据
});

// 14. 设置（QSettings）
stub.set_lamda(&QSettings::value, [](QSettings *self, const QString &key) -> QVariant {
    __DBG_STUB_INVOKE__
    return "mock value";
});

stub.set_lamda(&QSettings::setValue, [](QSettings *self, const QString &key, const QVariant &value) {
    __DBG_STUB_INVOKE__
});

// 15. 其他常用函数
stub.set_lamda(&QObject::deleteLater, [](QObject *self) {
    __DBG_STUB_INVOKE__
});

stub.set_lamda(&QObject::parent, [](QObject *self) -> QObject* {
    __DBG_STUB_INVOKE__
    return nullptr;
});

stub.set_lamda(&QObject::objectName, [](QObject *self) -> QString {
    __DBG_STUB_INVOKE__
    return "mock object";
});

// ==================== Stub 模式结束 ====================
