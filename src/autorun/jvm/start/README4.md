
## `init_globals()`

```c++
// ...
// Initialize Java-Level synchronization subsystem
ObjectMonitor::Initialize();
ObjectSynchronizer::initialize();

// Initialize global modules
jint status = init_globals();
// ...
```
