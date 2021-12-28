# JVMTI 

## 什么是JVMTI

JVMTI[^1] 全称是`Java Virtual Machine Tool Interfece`是开发和监控工具使用的编程接口。 它为正在运行中的Java应用提供了检查和控制其状态的方法。它是`JVMDI`(JVM Debug Interface)和`JVMPI`(JVM Profiling Interface)的一种整合.
  




[^1]: https://docs.oracle.com/javase/8/docs/platform/jvmti/jvmti.html

## 部署代理

代理一般是以动态链接库的形式部署,如在`windows`上是`DLL`,在`Solaris`上是`so`,在`mac`上则是`dylib` <br/>
命令行参数: <br/>
* -agentlib:\<agent-lib-name\>=\<options\>
  <p>
  -agentlib: 后面的名称是要加载的库的名称. <agent-lib-name> 扩展为操作系统特定的文件名。 <options> 将在启动时传递给代理。 例如，如果指定选项-agentlib:foo=opt1,opt2，则VM会尝试从Windows下的系统PATH加载共享库foo.dll，或者Solaris运行环境下从<b>LD_LIBRARY_PATH</b>加载libfoo.so。在`mac`操作系统下不需要设置<b>LD_LIBRARY_PATH</b> ,java 二进制文件将通过它自己的运行路径查找它所依赖的库blifoo.dylib, 如果代理库静态链接到可执行文件中，则不会发生实际加载.
    </p>
  
* -agentpath:\<path-to-agent>=\<options>
    <p>
  -agentpath: 后面的路径是加载库的绝对路径。 不会发生库名扩展。 <options> 将在启动时传递给代理。 例如，如果指定选项 
      -agentpath:c:\myLibs\foo.dll=opt1,opt2，VM 将尝试加载共享库 c:\myLibs\foo.dll。 如果代理库静态链接到可执行文件中，则不会发生实际加载。</p>
 
* -agentlib:jdwp=\<options>
      <p>
        如果JVMTI代理需要特定的库，例如 jdwp，则可以在启动时指定路径.
      </p>
## 生命周期 
代理有两种启动方式,每个代理只调用一次启动函数。
 * 代理跟随目标JVM一起启动 Agent_OnLoad函数将会被调用 
 * 附加到已启动的JVM上 Agent_OnAttach函数将会被调用 <br/>
      
代理结束时会执行Agent_OnUnload函数(可选)<br/>

> 代理必须包含jvmti.h带有以下语句的文件：`#include <jvmti.h>`,并且必须包含一个`Agent_OnLoad`函数.

      
## JVMTI的事件处理
      
JVMTI依赖于每个事件的回调.一般会在`Agent_OnLoad`函数中添加事件的回调,例如要使用`InterruptThread`功能,则`can_signal_thread`功能必须打开.<br/>
```c++
// JVMTI所支持的功能 
typedef struct {
    unsigned int can_tag_objects : 1;
    unsigned int can_generate_field_modification_events : 1;
    unsigned int can_generate_field_access_events : 1;
    unsigned int can_get_bytecodes : 1;
    unsigned int can_get_synthetic_attribute : 1;
    unsigned int can_get_owned_monitor_info : 1;
    unsigned int can_get_current_contended_monitor : 1;
    unsigned int can_get_monitor_info : 1;
    unsigned int can_pop_frame : 1;
    unsigned int can_redefine_classes : 1;
    unsigned int can_signal_thread : 1;
    unsigned int can_get_source_file_name : 1;
    unsigned int can_get_line_numbers : 1;
    unsigned int can_get_source_debug_extension : 1;
    unsigned int can_access_local_variables : 1;
    unsigned int can_maintain_original_method_order : 1;
    unsigned int can_generate_single_step_events : 1;
    unsigned int can_generate_exception_events : 1;
    unsigned int can_generate_frame_pop_events : 1;
    unsigned int can_generate_breakpoint_events : 1;
    unsigned int can_suspend : 1;
    unsigned int can_redefine_any_class : 1;
    unsigned int can_get_current_thread_cpu_time : 1;
    unsigned int can_get_thread_cpu_time : 1;
    unsigned int can_generate_method_entry_events : 1;
    unsigned int can_generate_method_exit_events : 1;
    unsigned int can_generate_all_class_hook_events : 1;
    unsigned int can_generate_compiled_method_load_events : 1;
    unsigned int can_generate_monitor_events : 1;
    unsigned int can_generate_vm_object_alloc_events : 1;
    unsigned int can_generate_native_method_bind_events : 1;
    unsigned int can_generate_garbage_collection_events : 1;
    unsigned int can_generate_object_free_events : 1;
    unsigned int can_force_early_return : 1;
    unsigned int can_get_owned_monitor_stack_depth_info : 1;
    unsigned int can_get_constant_pool : 1;
    unsigned int can_set_native_method_prefix : 1;
    unsigned int can_retransform_classes : 1;
    unsigned int can_retransform_any_class : 1;
    unsigned int can_generate_resource_exhaustion_heap_events : 1;
    unsigned int can_generate_resource_exhaustion_threads_events : 1;
    unsigned int : 7;
    unsigned int : 16;
    unsigned int : 16;
    unsigned int : 16;
    unsigned int : 16;
    unsigned int : 16;
} jvmtiCapabilities;

// JVMTI 所支持的事件类型
typedef enum {
    JVMTI_MIN_EVENT_TYPE_VAL = 50,
    JVMTI_EVENT_VM_INIT = 50,
    JVMTI_EVENT_VM_DEATH = 51,
    JVMTI_EVENT_THREAD_START = 52,
    JVMTI_EVENT_THREAD_END = 53,
    JVMTI_EVENT_CLASS_FILE_LOAD_HOOK = 54,
    JVMTI_EVENT_CLASS_LOAD = 55,
    JVMTI_EVENT_CLASS_PREPARE = 56,
    JVMTI_EVENT_VM_START = 57,
    JVMTI_EVENT_EXCEPTION = 58,
    JVMTI_EVENT_EXCEPTION_CATCH = 59,
    JVMTI_EVENT_SINGLE_STEP = 60,
    JVMTI_EVENT_FRAME_POP = 61,
    JVMTI_EVENT_BREAKPOINT = 62,
    JVMTI_EVENT_FIELD_ACCESS = 63,
    JVMTI_EVENT_FIELD_MODIFICATION = 64,
    JVMTI_EVENT_METHOD_ENTRY = 65,
    JVMTI_EVENT_METHOD_EXIT = 66,
    JVMTI_EVENT_NATIVE_METHOD_BIND = 67,
    JVMTI_EVENT_COMPILED_METHOD_LOAD = 68,
    JVMTI_EVENT_COMPILED_METHOD_UNLOAD = 69,
    JVMTI_EVENT_DYNAMIC_CODE_GENERATED = 70,
    JVMTI_EVENT_DATA_DUMP_REQUEST = 71,
    JVMTI_EVENT_MONITOR_WAIT = 73,
    JVMTI_EVENT_MONITOR_WAITED = 74,
    JVMTI_EVENT_MONITOR_CONTENDED_ENTER = 75,
    JVMTI_EVENT_MONITOR_CONTENDED_ENTERED = 76,
    JVMTI_EVENT_RESOURCE_EXHAUSTED = 80,
    JVMTI_EVENT_GARBAGE_COLLECTION_START = 81,
    JVMTI_EVENT_GARBAGE_COLLECTION_FINISH = 82,
    JVMTI_EVENT_OBJECT_FREE = 83,
    JVMTI_EVENT_VM_OBJECT_ALLOC = 84,
    JVMTI_MAX_EVENT_TYPE_VAL = 84
} jvmtiEvent;
```
 
      
```C++
jvmtiCapabilities capabilities = {0};
// 设置所支持的功能 
capabilities.can_generate_exception_events = 1;
capabilities.can_get_bytecodes = 1;
capabilities.can_get_constant_pool = 1;
// 使用AddCapabilities函数将其添加到JVMTI环境中
jvmti->AddCapabilities(&capabilities);

// 注册事件通知 启用了VM初始化,异常,线程启动和结束等几个事件,注册的每个事件都必须有一个指定的回调函数, 如当生类型为Exception的事件时,会调用ExceptionCallback
jvmtiEventCallbacks callbacks = {0};
callbacks.VMInit = VMInit;
callbacks.Exception = ExceptionCallback;
callbacks.ThreadStart = ThreadStartCallback;
callbacks.ThreadEnd = ThreadEndCallback;
jvmti->SetEventCallbacks(&callbacks, sizeof(callbacks));
jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, NULL);
jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_EXCEPTION, NULL);
jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_THREAD_START, NULL);
jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_THREAD_END, NULL);
```
 
## see also
[JVM事件处理](https://github.com/openjdk/jdk/blob/jdk8-b120/jdk/src/share/back/eventHandler.c) <br/>
[JVMTI](https://github.com/openjdk/jdk/blob/jdk8-b120/jdk/src/share/javavm/export/jvmti.h)
