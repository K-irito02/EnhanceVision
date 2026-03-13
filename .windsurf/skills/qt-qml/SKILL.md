---
name: "qt-qml"
description: "QML and Qt Quick — QML vs Widgets decision guide, official best practices, exposing objects to QML, signals and properties."
source: "l3digital-net/claude-code-plugins"
---

# QML and Qt Quick

### QML vs Widgets: When to Choose QML

| Use QML when... | Use Widgets when... |
| --- | --- |
| Building modern, animated, fluid UIs | Building traditional desktop tools |
| Targeting mobile or embedded | Heavy data tables and forms |
| Designers are involved in the UI | Rich text editing required |
| GPU-accelerated rendering needed | Complex platform widget integration |
| Writing a new app from scratch | Extending an existing widget app |

For new Python/PySide6 desktop applications, QML offers better visual results with less code. For data-heavy enterprise tools, widgets remain the pragmatic choice.

### Official Best Practices (Qt Quick)

**1. Type-safe property declarations** — Always use explicit types, not `var`:

```qml
// WRONG — prevents static analysis, unclear errors
property var name

// CORRECT
property string name
property int count
property MyModel optionsModel
```

**2. Prefer declarative bindings over imperative assignments:**

```qml
// WRONG — imperative assignment overwrites bindings, breaks Qt Design Studio
Rectangle {
    Component.onCompleted: color = "red"
}

// CORRECT — declarative binding, evaluates once at load
Rectangle {
    color: "red"
}
```

**3. Interaction signals over value-change signals:**

```qml
// WRONG — valueChanged fires on clamping/rounding, causes event cascades
Slider { onValueChanged: model.update(value) }

// CORRECT — moved only fires on user interaction
Slider { onMoved: model.update(value) }
```

**4. Don't anchor the immediate children of Layouts:**

```qml
// WRONG — anchors on direct Layout children cause binding loops
RowLayout {
    Rectangle { anchors.fill: parent }
}

// CORRECT — use Layout attached properties
RowLayout {
    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 40
    }
}
```

**5. Don't customize native styles** — Windows and macOS native styles ignore QSS. Base all custom styling on cross-platform styles: `Basic`, `Fusion`, `Material`, or `Universal`:

```qml
// In main() — must be set before QGuiApplication
QQuickStyle.setStyle("Material")
```

**6. Make all user-visible strings translatable from the start:**

```qml
Label { text: qsTr("Save File") }
Button { text: qsTr("Cancel") }
```

### Exposing Python Objects to QML

Three methods: Required Properties (preferred), Context Property, Registered QML Type.

**Key rule: `@Slot` is mandatory for any Python method callable from QML.** Missing it causes `TypeError` at runtime.

### QML Signals and Connections

Use `signal` keyword for custom signals, `Connections` component for cross-component connections, and `on<Signal>` handlers for direct connections.
