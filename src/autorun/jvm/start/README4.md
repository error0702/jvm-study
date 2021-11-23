
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
  if (status != JNI_OK)
    return status;

  interpreter_init();  // before any methods loaded
  invocationCounter_init();  // before any methods loaded
  marksweep_init();
  accessFlags_init();
  templateTable_init();
  InterfaceSupport_init();
  SharedRuntime::generate_stubs();
  universe2_init();  // dependent on codeCache_init and stubRoutines_init1
  referenceProcessor_init();
  jni_handles_init();
  vmStructs_init();
  vtableStubs_init();
  InlineCacheBuffer_init();
  compilerOracle_init();
  compilationPolicy_init();
  compileBroker_init();
  VMRegImpl::set_regName();
    if (!universe_post_init()) {
    return JNI_ERR;
  }
  javaClasses_init();   // must happen after vtable initialization
  stubRoutines_init2(); // note: StubRoutines need 2-phase init

  // All the flags that get adjusted by VM_Version_init and os::init_2
  // have been set so dump the flags now.
  if (PrintFlagsFinal) {
    CommandLineFlags::printFlags(tty, false);
  }
}
```
HandleMark hm;

### `management_init()`
### `bytecodes_init()`
### `classLoader_init()`
### `codeCache_init()`
### `VM_Version_init()`
### `os_init_globals()`
stubRoutines_init1();