#jvm start

## jvm 启动流程 (linux_x86_64)


### 1. 启动流程
#### 1.1 init SubThread call JavaMain
1. `main(int argc, char **argv) main.c`
2. -> `JLI_Launch()   java.c`
3. -> `JVMInit()  java_md_solinux.c`
4. -> `CreateExecutionEnvironment() java_md_solinux.c`
5. -> `GetJREPath() java_md_solinux.c`
6. -> `GetJVMPath() java_md_solinux.c`
7. -> `ContinueInNewThread()  java.c`
8. -> `ContinueInNewThread0() java_md_solinux.c`
9. -> `if (pthread_create(&tid, &attr, (void *(*)(void*))continuation, (void*)args) == 0)`
10. -> `pthread_join() pthread_join.c`
11. -> `JavaMain() java.c`
##### 1.1 call `JLI_Launch`
> 主要作用: 
##### 1. 进行 `libjvm.so` 加载。</br>
##### 2. 编译的 `jvm.cpp` 文件 命令类似于
> `g++ --dynamiclib src -o target`. 
参考 [jni 编译环节](../jni/README.md "#编译环节")
##### 3. `JLI_Launch` 会调用 `LoadJavaVM(jvmpath, &ifn)` 来实现`libjvm.so`的加载、
参数解析、ClassPath的获取和设置、系统属性设置以及jvm的初始化
##### 4. *ifn是一个很关键的结构体。位于 `jdk/src/share/bin/java.h` 中
#### 1.2 解析启动参数
[CreateExecutionEnvironment](https://github.com/openjdk/jdk/blob/jdk8-b120/jdk/src/share/bin/java.c#L236)
```C++
// 创建运行时环境 根据启动参数创建32或64位虚拟机, 获取JRE路径, 获取JVM路径 
CreateExecutionEnvironment(&argc, &argv,
                               jrepath, sizeof(jrepath),
                               jvmpath, sizeof(jvmpath),
                               jvmcfg,  sizeof(jvmcfg));
```
> 检查JRE环境和 `libjvm.so` (linux) 或者 `libjvm.dll` (windows) 或者 libjvm.dylib (macos) 文件是否存在。如果不存在则结束。抛出异常
> 
> JRE环境未找到错误信息模板: "Error: Could not find Java SE Runtime Environment."
> 
> libjvm动态链接库未找到错误信息模板: "Error: missing `%s' JVM at `%s'.\nPlease install or use the JRE or JDK that contains these missing components." 
```c++
/* Compute/set the name of the executable */
-> SetExecname(char **argv)
/* Find out where the JRE is that we will be using. */
if (!GetJREPath(jrepath, so_jrepath, arch, JNI_FALSE) ) 
    -> GetApplicationHome(char *buf, jint bufsize)
        const char *execname = GetExecName(); $JAVA_HOME/bin/java
        -> java.c 
        char* CheckJvmType // client server 
if (!GetJVMPath(jrepath, jvmtype, jvmpath, so_jvmpath, arch, 0 )) // 通过java命令的路径拼接libjvm.so路径
// Does `/home/autorun/openjdk8/jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/lib/amd64/server/libjvm.so' exist ... yes.
    -> if (stat(jvmpath, &s) == 0) 
        -> stat
            /* Get file attributes for FILE and put them in BUF.  */
            extern int stat (const char *__restrict __file,
                    struct stat *__restrict __buf) __THROW __nonnull ((1, 2));
/*
* we seem to have everything we need, so without further ado
* we return back, otherwise proceed to set the environment.
*/
mustsetenv = RequiresSetenv(wanted, jvmpath);
    -> llp = getenv("LD_LIBRARY_PATH");
        /* Return the value of envariable NAME, or NULL if it doesn't exist.  */
        extern char *getenv (const char *__name) __THROW __nonnull ((1)) __wur;

ifn.CreateJavaVM = 0;
ifn.GetDefaultJavaVMInitArgs = 0;
```

```c++
typedef struct {
    CreateJavaVM_t CreateJavaVM;
    GetDefaultJavaVMInitArgs_t GetDefaultJavaVMInitArgs;
    GetCreatedJavaVMs_t GetCreatedJavaVMs;
} InvocationFunctions;
```
可以看到，里面定义了3个函数指针，具体实现在 `jvm.cpp` 中。在加载完毕后jvm通过

`libjvm = dlopen(jvmpath, RTLD_NOW + RTLD_GLOBAL);` 加载 `libjvm.so` 动态链接库(linux). windows 则是dll文件
> 参考 [dlopen 函数定义](https://baike.baidu.com/item/dlopen/1967576?fr=aladdin)
##### 5. 当 `libjvm.so` 动态链接库加载完成后接下来会调用 
`dlsym(libjvm, "JNI_CreateJavaVM");`</br>
`dlsym(libjvm, "JNI_GetDefaultJavaVMInitArgs");`</br>
`dlsym(libjvm, "JNI_GetCreatedJavaVMs");`</br>
给上面提到的`InvocationFunctions` 的`CreateJavaVM`、`GetDefaultJavaVMInitArgs` 和 `GetCreatedJavaVMs` 赋值首地址。以实现方法调用

> 摘自 `java_md_solinux.c`
```c++
ifn->CreateJavaVM = (CreateJavaVM_t)
        dlsym(libjvm, "JNI_CreateJavaVM");
    if (ifn->CreateJavaVM == NULL) {
        JLI_ReportErrorMessage(DLL_ERROR2, jvmpath, dlerror());
        return JNI_FALSE;
    }

    ifn->GetDefaultJavaVMInitArgs = (GetDefaultJavaVMInitArgs_t)
        dlsym(libjvm, "JNI_GetDefaultJavaVMInitArgs");
    if (ifn->GetDefaultJavaVMInitArgs == NULL) {
        JLI_ReportErrorMessage(DLL_ERROR2, jvmpath, dlerror());
        return JNI_FALSE;
    }

    ifn->GetCreatedJavaVMs = (GetCreatedJavaVMs_t)
        dlsym(libjvm, "JNI_GetCreatedJavaVMs");
    if (ifn->GetCreatedJavaVMs == NULL) {
        JLI_ReportErrorMessage(DLL_ERROR2, jvmpath, dlerror());
        return JNI_FALSE;
    }
```
##### 6. JVMInit() 函数
> 源码位置: jdk/src/solaris/bin/java_md_solinux.c
```c++
int
JVMInit(InvocationFunctions* ifn, jlong threadStackSize,
        int argc, char **argv,
        int mode, char *what, int ret)
{
    ShowSplashScreen();
    return ContinueInNewThread(ifn, threadStackSize, argc, argv, mode, what, ret);
}
```
##### 7. ContinueInNewThread() 函数
> 源码位置: jdk/src/share/bin/java.c
```c++
int
ContinueInNewThread(InvocationFunctions* ifn, jlong threadStackSize,
                    int argc, char **argv,
                    int mode, char *what, int ret)
{

    /*
     * If user doesn't specify stack size, check if VM has a preference.
     * Note that HotSpot no longer supports JNI_VERSION_1_1 but it will
     * return its default stack size through the init args structure.
     */
    if (threadStackSize == 0) {
      struct JDK1_1InitArgs args1_1;
      memset((void*)&args1_1, 0, sizeof(args1_1));
      args1_1.version = JNI_VERSION_1_1;
      ifn->GetDefaultJavaVMInitArgs(&args1_1);  /* ignore return value */
      if (args1_1.javaStackSize > 0) {
         threadStackSize = args1_1.javaStackSize;
      }
    }

    { /* Create a new thread to create JVM and invoke main method */
      JavaMainArgs args;
      int rslt;

      args.argc = argc;
      args.argv = argv;
      args.mode = mode;
      args.what = what;
      args.ifn = *ifn;

      // 创建子线程调用Java主类的mian() 方法
      rslt = ContinueInNewThread0(JavaMain, threadStackSize, (void*)&args);
      /* If the caller has deemed there is an error we
       * simply return that, otherwise we return the value of
       * the callee
       */
      return (ret != 0) ? ret : rslt;
    }
}
```
##### 8. ContinueInNewThread0() 函数
> 源码位置: jdk/src/solaris/bin/java_md_solinux.c
```c++
    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    if (stack_size > 0) {
      pthread_attr_setstacksize(&attr, stack_size);
    }

    // 使用pthread_create系统函数创建新线程去执行JavaMain方法 如果线程创建成功 则返回值为0
    if (pthread_create(&tid, &attr, (void *(*)(void*))continuation, (void*)args) == 0) {
      void * tmp;
      // 让出当前线程执行权， 参考`java.lang.Thread.join()` 方法
      pthread_join(tid, &tmp);
      rslt = (int)tmp;
    } else {
     /*
      * Continue execution in current thread if for some reason (e.g. out of
      * memory/LWP)  a new thread can't be created. This will likely fail
      * later in continuation as JNI_CreateJavaVM needs to create quite a
      * few new threads, anyway, just give it a try..
      */
      rslt = continuation(args);
    }

    pthread_attr_destroy(&attr);
```
1. 使用pThread库创建线程。并将当前线程执行权让出。
2. 使用创建的线程去回调 `continuation`, `continuation` 为 `JavaMain`
> rslt = ContinueInNewThread0(JavaMain, threadStackSize, (void*)&args);

### 启动流程图
![流程图](img/jvm启动流程图1.png)

### 3. 附录
1. jvm `main.c` 代码(摘自 `openjdk 1.8_b120`) 
```c++

#ifdef _MSC_VER
#if _MSC_VER > 1400 && _MSC_VER < 1600

/*
 * When building for Microsoft Windows, main has a dependency on msvcr??.dll.
 *
 * When using Visual Studio 2005 or 2008, that must be recorded in
 * the [java,javaw].exe.manifest file.
 *
 * As of VS2010 (ver=1600), the runtimes again no longer need manifests.
 *
 * Reference:
 *     C:/Program Files/Microsoft SDKs/Windows/v6.1/include/crtdefs.h
 */
#include <crtassem.h>
#ifdef _M_IX86

#pragma comment(linker,"/manifestdependency:\"type='win32' "            \
        "name='" __LIBRARIES_ASSEMBLY_NAME_PREFIX ".CRT' "              \
        "version='" _CRT_ASSEMBLY_VERSION "' "                          \
        "processorArchitecture='x86' "                                  \
        "publicKeyToken='" _VC_ASSEMBLY_PUBLICKEYTOKEN "'\"")

#endif /* _M_IX86 */

//This may not be necessary yet for the Windows 64-bit build, but it
//will be when that build environment is updated.  Need to test to see
//if it is harmless:
#ifdef _M_AMD64

#pragma comment(linker,"/manifestdependency:\"type='win32' "            \
        "name='" __LIBRARIES_ASSEMBLY_NAME_PREFIX ".CRT' "              \
        "version='" _CRT_ASSEMBLY_VERSION "' "                          \
        "processorArchitecture='amd64' "                                \
        "publicKeyToken='" _VC_ASSEMBLY_PUBLICKEYTOKEN "'\"")

#endif  /* _M_AMD64 */
#endif  /* _MSC_VER > 1400 && _MSC_VER < 1600 */
#endif  /* _MSC_VER */

/*
 * Entry point.
 */
#ifdef JAVAW

char **__initenv;

int WINAPI
WinMain(HINSTANCE inst, HINSTANCE previnst, LPSTR cmdline, int cmdshow)
{
    int margc;
    char** margv;
    const jboolean const_javaw = JNI_TRUE;

    __initenv = _environ;

#else /* JAVAW */
int
main(int argc, char **argv)
{
    int margc;
    char** margv;
    const jboolean const_javaw = JNI_FALSE;
#endif /* JAVAW */
#ifdef _WIN32
    {
        int i = 0;
        if (getenv(JLDEBUG_ENV_ENTRY) != NULL) {
            printf("Windows original main args:\n");
            for (i = 0 ; i < __argc ; i++) {
                printf("wwwd_args[%d] = %s\n", i, __argv[i]);
            }
        }
    }
    JLI_CmdToArgs(GetCommandLine());
    margc = JLI_GetStdArgc();
    // add one more to mark the end
    margv = (char **)JLI_MemAlloc((margc + 1) * (sizeof(char *)));
    {
        int i = 0;
        StdArg *stdargs = JLI_GetStdArgs();
        for (i = 0 ; i < margc ; i++) {
            margv[i] = stdargs[i].arg;
        }
        margv[i] = NULL;
    }
#else /* *NIXES */
    margc = argc;
    margv = argv;
#endif /* WIN32 */
    return JLI_Launch(margc, margv,
                   sizeof(const_jargs) / sizeof(char *), const_jargs,
                   sizeof(const_appclasspath) / sizeof(char *), const_appclasspath,
                   FULL_VERSION,
                   DOT_VERSION,
                   (const_progname != NULL) ? const_progname : *margv,
                   (const_launcher != NULL) ? const_launcher : *margv,
                   (const_jargs != NULL) ? JNI_TRUE : JNI_FALSE,
                   const_cpwildcard, const_javaw, const_ergo_class);
}
```

> 通过ifdef 编译条件编译。基于Linux内核的Ubuntu系统来讲。最终编译完的代码如下
```c++
int
main(int argc, char **argv)
{
    int margc;
    char** margv;
    const jboolean const_javaw = JNI_FALSE;
    return JLI_Launch(margc, margv,
                   sizeof(const_jargs) / sizeof(char *), const_jargs,
                   sizeof(const_appclasspath) / sizeof(char *), const_appclasspath,
                   FULL_VERSION,
                   DOT_VERSION,
                   (const_progname != NULL) ? const_progname : *margv,
                   (const_launcher != NULL) ? const_launcher : *margv,
                   (const_jargs != NULL) ? JNI_TRUE : JNI_FALSE,
                   const_cpwildcard, const_javaw, const_ergo_class);
}
```

