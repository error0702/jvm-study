
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

```c++
jint init_globals() {
  HandleMark hm;
  management_init();
  bytecodes_init();
  classLoader_init();
  codeCache_init();
  VM_Version_init();
  os_init_globals();
  stubRoutines_init1();
  jint status = universe_init();  // dependent on codeCache_init and
                                  // stubRoutines_init1 and metaspace_init.
  
```