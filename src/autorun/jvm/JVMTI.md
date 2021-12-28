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

      
  
## see also
[JVM事件处理](https://github.com/openjdk/jdk/blob/jdk8-b120/jdk/src/share/back/eventHandler.c)<br/>
[JVMTI](https://github.com/openjdk/jdk/blob/jdk8-b120/jdk/src/share/javavm/export/jvmti.h)
