---
name: "axiom-memory-debugging"
description: "Memory debugging specialist — leak detection, retain cycles, Instruments profiling, systematic debugging workflow, and common leak patterns."
source: "charleswiltgen/axiom"
---

# Memory Debugging

## Quick Leak Check

Add `deinit` / destructor logging to suspect classes:

```cpp
// C++
~MyClass() { qDebug() << "MyClass deallocated"; }
```

```python
# Python
def __del__(self):
    print(f"✅ {self.__class__.__name__} deallocated")
```

Navigate to view, navigate away. See "deallocated"? Yes = no leak. No = retained somewhere.

## Common Memory Leak Patterns (With Fixes)

### Pattern 1: Timer Leaks (Most Common — 50% of leaks)

The RunLoop/event loop retains scheduled timers. You must explicitly stop/invalidate to break the retention.

**C++ / Qt:**
```cpp
// BAD — timer never stopped
m_timer = new QTimer(this);
m_timer->start(1000);
// Never stopped → keeps firing even after logical end of use

// GOOD — stop timer when done
m_timer->stop();
m_timer->deleteLater();
m_timer = nullptr;
```

**Python / PySide6:**
```python
# BAD
self._timer = QTimer()
self._timer.start(1000)

# GOOD — stop in cleanup
self._timer.stop()
self._timer.deleteLater()
```

### Pattern 2: Observer/Notification Leaks (25% of leaks)

```cpp
// BAD — No disconnect
connect(sender, &Sender::signal, this, &MyClass::slot);
// Never disconnected → accumulates listeners

// GOOD — disconnect in destructor or when done
disconnect(sender, &Sender::signal, this, &MyClass::slot);
```

### Pattern 3: Closure Capture Leaks (15% of leaks)

```cpp
// BAD — lambda captures 'this' strongly in a stored callback
callbacks.push_back([this]() { this->refresh(); });

// GOOD — use weak_ptr pattern
auto weak = weak_from_this();
callbacks.push_back([weak]() {
    if (auto self = weak.lock()) {
        self->refresh();
    }
});
```

### Pattern 4: Strong Reference Cycles

```cpp
// BAD — parent → child → parent (cycle)
class Parent {
    std::shared_ptr<Child> child;
};
class Child {
    std::shared_ptr<Parent> parent; // cycle!
};

// GOOD — break cycle with weak_ptr
class Child {
    std::weak_ptr<Parent> parent; // no cycle
};
```

## Systematic Debugging Workflow

### Phase 1: Confirm Leak (5 min)

Profile with memory tools, repeat action 10 times.
- **Flat** = not a leak (stop)
- **Steady climb** = leak (continue)

### Phase 2: Locate Leak (10-15 min)

Use memory profiler or sanitizer to identify leaked objects.

Common locations:
- Timers (50%)
- Notifications/KVO/signals (25%)
- Closures in collections (15%)
- Delegate/parent cycles (10%)

### Phase 3: Fix and Verify (5 min)

Apply fix from patterns above. Add destructor logging. Run profiler again — memory should stay flat.

### Compound Leaks

Real apps often have 2-3 leaks stacking. Fix the largest first, re-run profiler, repeat until flat.

## C++ Specific Tools

### AddressSanitizer (ASAN)

```cmake
# CMake
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -g")
set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fsanitize=address")
```

### Valgrind (Linux)

```bash
valgrind --leak-check=full --show-leak-kinds=all ./my_app
```

### Visual Studio Memory Diagnostics

```cpp
// Windows / MSVC
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>

// At program start
_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

// At program end (or use atexit)
_CrtDumpMemoryLeaks();
```

### Qt-Specific Memory Debugging

```cpp
// Check QObject tree
qDebug() << object->children().count() << "children";

// Track QObject creation/destruction
qDebug() << "Creating" << metaObject()->className();
```

## Real-World Impact

**Before**: 50+ leaked objects with uncleared timers → 50MB → 200MB → Crash (13min)
**After**: Timer properly stopped → 50MB stable for hours

**Key insight**: 90% of leaks come from forgetting to stop timers, observers, or subscriptions. Always clean up in destructors or use RAII patterns that auto-cleanup.
