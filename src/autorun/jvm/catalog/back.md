`utilities/globalDefinitions_visCPP.hpp` Windows相关头文件 </br>
`utilities/globalDefinitions_sparcWorks.hpp` 非Windows相关头文件

主要体现在 `globalDefinitions.hpp` 文件中，使用来决定引入哪一个文件。
```c++
#ifdef TARGET_COMPILER_visCPP
# include "utilities/globalDefinitions_visCPP.hpp"
#endif
#ifdef TARGET_COMPILER_sparcWorks
# include "utilities/globalDefinitions_sparcWorks.hpp"
#endif
``` 
如果使用 `TARGET_COMPILER_visCPP` 相关属性则会使用 `globalDefinitions_visCPP.hpp` 文件，否则会使用默认的 `globalDefinitions_sparcWorks.cpp` 文件
```cmake
# Used for platform dispatching
CXX_FLAGS=$(CXX_FLAGS) /D TARGET_OS_FAMILY_windows
CXX_FLAGS=$(CXX_FLAGS) /D TARGET_ARCH_$(Platform_arch)
CXX_FLAGS=$(CXX_FLAGS) /D TARGET_ARCH_MODEL_$(Platform_arch_model)
CXX_FLAGS=$(CXX_FLAGS) /D TARGET_OS_ARCH_windows_$(Platform_arch)
CXX_FLAGS=$(CXX_FLAGS) /D TARGET_OS_ARCH_MODEL_windows_$(Platform_arch_model)
CXX_FLAGS=$(CXX_FLAGS) /D TARGET_COMPILER_visCPP
```
## 主控线程
`runtime/thread.cpp`  `VMThread::create()`

## JVM 线程类型
```c++
enum ThreadType {
    vm_thread,
    cgc_thread,        // Concurrent GC thread
    pgc_thread,        // Parallel GC thread
    java_thread,       // Java, CodeCacheSweeper, JVMTIAgent and Service threads.
    compiler_thread,
    watcher_thread,
    asynclog_thread,   // dedicated to flushing logs
    os_thread
};
```