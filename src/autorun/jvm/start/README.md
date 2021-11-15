#jvm start

## jvm 启动流程 (linux_x86_64)


### 1. 启动流程
#### 1.1 init SubThread call JavaMain
1. `main(int argc, char **argv) main.c`
2. -> `JLI_Launch()   java.c`
3. -> `JVMInit()  java_md_solinux.c`
4. -> `ContinueInNewThread()  java.c`
5. -> `ContinueInNewThread0() java_md_solinux.c`
6. -> `if (pthread_create(&tid, &attr, (void *(*)(void*))continuation, (void*)args) == 0)`
7. -> `pthread_join() pthread_join.c`

#### 1.2 call `JLI_Launch`
> 主要作用: 
1. 进行 `libjvm.so` 加载。</br>
2. 编译的 `jvm.cpp` 文件 命令类似于
> `g++ --dynamiclib src -o target`. 
参考 [jni 编译环节](../jni/README.md "编译环节")
3. `JLI_Launch` 会调用 `LoadJavaVM(jvmpath, &ifn)` 来实现`libjvm.so`的加载、
参数解析、ClassPath的获取和设置、系统属性设置以及jvm的初始化
4. *ifn是一个很关键的结构体。位于 `jdk/src/share/bin/java.h` 中
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
5. 当 `libjvm.so` 动态链接库加载完成后接下来会调用 
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

### 4. 附录
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

