# JNI 初始化篇

> 接JVM初始化篇, 如果没有看过启动篇请先阅读一下启动篇。否则直接看的话比较容易懵逼~

## JNI简介

jni相关的类文件都在 `hotspot/src/share/vm/prims` 中，后续会出一篇目录结构简介的文章。此处先不介绍， 就记住在那里就行~

[JVM启动流程](../start/README.md)

在JVM启动流程中介绍过，会赋值ifn的三个函数首地址, 分别是 `CreateJavaVM`、 `GetDefaultJavaVMInitArgs` 、`GetCreatedJavaVMs`.

然后会在 `JNI_CreateJavaVM()` 中对 JNIEnv赋值。
```c++

  result = Threads::create_vm((JavaVMInitArgs*) args, &can_try_again);
  if (result == JNI_OK) {
    JavaThread *thread = JavaThread::current();
    /* thread is thread_in_vm here */
    // jni_InvokeInterface 赋值。c语言环境中使用改变量。c++ 环境中使用JNIEnv
    *vm = (JavaVM *)(&main_vm); // main_vm定义在下面
    // JNIEnv赋值。 由此处可以看出，JNIEnv其实是线程私有的。
    *(JNIEnv**)penv = thread->jni_environment();
    ...
```

`struct JavaVM_ main_vm = {&jni_InvokeInterface};`

### `JNIEnv` 的初始化过程
1. `JavaMain() java.c`
2. -> `InitializeJVM() java.c`
3. -> `JNI_CreateJavaVM() jni.cpp`
4. -> `Threads::create_vm()       thread.cpp`
5. -> `JavaThread::JavaThread()   thread.cpp`
6. -> `JavaThread::initialize()   thread.cpp`
7. -> `jni_functions() jni.cpp`

#### 1. 使用 `pthread_create` 回调 `JavaMain()` 参考 [8. ContinueInNewThread0() 函数](../start/README.md "8. ContinueInNewThread0() 函数")
```c++
/* Initialize the virtual machine */
    start = CounterGet();
    if (!InitializeJVM(&vm, &env, &ifn)) {
        JLI_ReportErrorMessage(JVM_ERROR1);
        exit(1);
    }
    ...
```
#### 