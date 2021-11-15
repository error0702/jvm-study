# jvm 启动流程2
> 上一篇讲了 jvm 在启动时发生的调用关系。此篇讲解回调JVM后又发生了什么
## 1. jvm call main()
接上文 `*ifn` 的 `CreateJavaVM_t CreateJavaVM` 
实际调用的是`JNI_CreateJavaVM`
> `JNI_CreateJavaVM` 实现在 `hotspot/src/share/vm/prims/jni.cpp` 中

`#define _JNI_IMPORT_OR_EXPORT_ JNIEXPORT`

`_JNI_IMPORT_OR_EXPORT_ jint JNICALL JNI_CreateJavaVM(JavaVM **vm, void **penv, void *args)`

1. 使用 `result = Threads::create_vm((JavaVMInitArgs*) args, &can_try_again);` 创建jvm应用线程

可以看到， 实际调用了 `thread.cpp` 中的 `Threads::create_vm()` 方法

2. `Threads::create_vm()`
> 源码位置: thread.cpp
```c++
// Initialize library-based TLS
  ThreadLocalStorage::init();

  // Initialize the output stream module
  ostream_init();

  // Process java launcher properties.
  Arguments::process_sun_java_launcher_properties(args);

  // Initialize the os module
  os::init();

  MACOS_AARCH64_ONLY(os::current_thread_enable_wx(WXWrite));

  // Record VM creation timing statistics
  TraceVmCreationTime create_vm_timer;
  create_vm_timer.start();

  // Initialize system properties.
  Arguments::init_system_properties();

  // So that JDK version can be used as a discriminator when parsing arguments
  JDK_Version_init();

  // Update/Initialize System properties after JDK version number is known
  Arguments::init_version_specific_system_properties();

  // Make sure to initialize log configuration *before* parsing arguments
  LogConfiguration::initialize(create_vm_timer.begin_time());
```
总体来说是初始化了系统的环境变量、线程私有存储，设置了标准输入和输出等。

`MACOS_AARCH64_ONLY(os::current_thread_enable_wx(WXWrite));` 对m1的支持


源码位置: hotspot/src/share/vm/runtime/arguments.cpp
Arguments::init_system_properties();
```c++
PropertyList_add(&_system_properties, new SystemProperty("java.vm.specification.name",
                                                                 "Java Virtual Machine Specification",  false));
  PropertyList_add(&_system_properties, new SystemProperty("java.vm.version", VM_Version::vm_release(),  false));
  PropertyList_add(&_system_properties, new SystemProperty("java.vm.name", VM_Version::vm_name(),  false));
  PropertyList_add(&_system_properties, new SystemProperty("java.vm.info", VM_Version::vm_info_string(),  true));

  // following are JVMTI agent writeable properties.
  // Properties values are set to NULL and they are
  // os specific they are initialized in os::init_system_properties_values().
  _java_ext_dirs = new SystemProperty("java.ext.dirs", NULL,  true);
  _java_endorsed_dirs = new SystemProperty("java.endorsed.dirs", NULL,  true);
  _sun_boot_library_path = new SystemProperty("sun.boot.library.path", NULL,  true);
  _java_library_path = new SystemProperty("java.library.path", NULL,  true);
  _java_home =  new SystemProperty("java.home", NULL,  true);
  _sun_boot_class_path = new SystemProperty("sun.boot.class.path", NULL,  true);

  _java_class_path = new SystemProperty("java.class.path", "",  true);
```
规范版本号: `Arguments::init_version_specific_system_properties()`
```c++

  if (JDK_Version::is_gte_jdk17x_version()) {
    spec_vendor = "Oracle Corporation";
    spec_version = JDK_Version::current().major_version();
  }
  jio_snprintf(buffer, bufsz, "1." UINT32_FORMAT, spec_version);

  PropertyList_add(&_system_properties,
      new SystemProperty("java.vm.specification.vendor",  spec_vendor, false));
  PropertyList_add(&_system_properties,
      new SystemProperty("java.vm.specification.version", buffer, false));
  PropertyList_add(&_system_properties,
      new SystemProperty("java.vm.vendor", VM_Version::vm_vendor(),  false));
```