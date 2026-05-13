# HotSpot Safepoint 安全点机制

> 本文主要以 OpenJDK 8 / HotSpot 为例。Safepoint 容易和操作系统中断、线程调度、Java `Thread.interrupt()` 混在一起, 所以这里先记录一条主线: safepoint 是 JVM 为了执行某些全局 VM 操作, 让 Java 线程在安全位置停下来的协作机制。

## 1. Safepoint是什么

`safepoint` 可以理解成 JVM 给 Java 线程设置的一些“可停靠位置”。

JVM并不是在任意一条机器指令上都能安全地暂停Java线程。比如要做GC、线程栈遍历、代码反优化时, JVM需要知道线程当前栈上哪些位置是对象引用、寄存器和栈帧处在什么状态。只有在一些JVM事先安排好的位置, 这些信息才比较完整、可恢复, 所以这些位置就叫 safepoint。

简单说:

* safepoint不是某个Java API
* safepoint也不是OS线程调度点
* safepoint是JVM内部为了执行全局操作而设计的协作检查点

这里的“安全”不是说程序逻辑安全, 而是说 JVM 此时可以安全地观察和修改运行时结构。

## 2. 为什么需要Safepoint

HotSpot执行某些VM操作时, 需要看到一个相对稳定的Java世界。例如GC要遍历对象引用, 栈遍历要读取线程栈上的oop, 代码反优化要调整栈帧。如果Java线程还在随意执行、对象引用还在不断变化, VM很难安全地完成这些操作。

典型原因:

1. GC需要知道线程栈、寄存器里的对象引用在哪里。
2. 线程栈遍历需要读取稳定的Java栈帧。
3. 代码反优化需要在可解释的frame边界处理compiled frame。
4. 类重定义、断点、单步等JVMTI操作需要目标线程或全局线程状态稳定。
5. 代码缓存、内联缓存、元数据相关操作需要避免Java线程观察到不一致的VM内部结构。

## 3. 源码入口

常见源码位置:

| 内容 | OpenJDK 8 路径 | 新版JDK路径 |
| :---: | :--- | :--- |
| Safepoint主流程 | `hotspot/src/share/vm/runtime/safepoint.cpp` | `src/hotspot/share/runtime/safepoint.cpp` |
| Safepoint定义 | `hotspot/src/share/vm/runtime/safepoint.hpp` | `src/hotspot/share/runtime/safepoint.hpp` |
| Safepoint poll机制 | `hotspot/src/share/vm/runtime/safepoint.hpp` | `src/hotspot/share/runtime/safepointMechanism.hpp` |
| VM操作 | `hotspot/src/share/vm/runtime/vm_operations.hpp` | `src/hotspot/share/runtime/vmOperation.hpp` |
| VMThread | `hotspot/src/share/vm/runtime/vmThread.cpp` | `src/hotspot/share/runtime/vmThread.cpp` |
| 线程状态切换 | `hotspot/src/share/vm/runtime/interfaceSupport.hpp` | `src/hotspot/share/runtime/interfaceSupport.inline.hpp` |

可以重点看:

```c++
SafepointSynchronize::begin()
SafepointSynchronize::end()
SafepointSynchronize::block(JavaThread* thread)
VMThread::inner_execute()
VM_Operation::evaluate_at_safepoint()
```

源码里的核心语义比较直接: `SafepointSynchronize::begin()` 会把线程推进到safepoint并暂停; `SafepointSynchronize::end()` 再恢复线程。

## 4. 执行流程

一次safepoint大致可以分成三个阶段:

1. 发起safepoint
2. 等待所有相关Java线程进入safepoint-safe状态
3. 执行VM操作并恢复线程

更具体一点:

```text
VMThread准备执行VM_Operation
        |
        v
SafepointSynchronize::begin()
        |
        v
设置全局safepoint状态
        |
        v
arm safepoint poll
        |
        v
Java线程在poll点或状态切换点停下
        |
        v
等待所有线程变成safepoint-safe
        |
        v
执行GC、栈遍历、反优化等VM操作
        |
        v
SafepointSynchronize::end()
        |
        v
唤醒线程继续执行
```

HotSpot不是在任意机器指令中间直接把Java线程强行停住。它通常依赖解释器检查、编译代码中的safepoint poll、线程状态切换检查等机制, 让线程在JVM能理解的位置停下来。

这也是为什么有一个指标叫 `time to safepoint`: JVM已经发起safepoint, 但还在等某些线程跑到可停的位置。

## 5. 线程状态和Safepoint的关系

HotSpot内部线程状态比Java层 `Thread.State` 更细。和safepoint关系比较大的状态有:

| 状态 | 大概含义 | Safepoint处理 |
| :---: | :--- | :--- |
| `_thread_in_Java` | 正在执行Java代码或JIT编译后的Java代码 | 需要跑到safepoint poll后停下 |
| `_thread_in_vm` | 正在执行JVM内部代码 | 通常由VM代码自己保证安全点检查 |
| `_thread_in_native` | 正在执行native/JNI代码 | 一般认为暂时不在Java执行流里, 返回Java前需要检查safepoint |
| `_thread_blocked` | 阻塞等待, 如monitor/park等 | 通常可以被认为处于安全状态或在唤醒前检查 |

这里要注意: Java层看到的 `RUNNABLE` 不代表它一定正在CPU上运行, 也不代表它一定能马上响应safepoint。一个线程可能是 `RUNNABLE`, 但正在OS run queue里排队; 也可能正在执行native/OS调用。

所以safepoint延迟可能来自两类原因:

1. JVM层: 线程迟迟没有运行到safepoint poll。
2. OS层: 线程一直没有被调度到CPU上执行, 也就没有机会响应safepoint。

## 6. native状态和Safepoint

native状态不是“必须马上停住”, 而是HotSpot特别处理的一种safepoint-safe状态。

线程运行在普通native code时, VMThread看到它在native, 通常不等待它block。原因是它暂时不在执行Java bytecode, 不会继续改动Java执行栈帧。但这个线程从native返回Java前必须检查safepoint状态, 必要时阻塞等待。

可以先这样理解:

1. native线程不会继续执行Java bytecode, 所以通常可被VM视为safepoint-safe。
2. native线程回到Java前, 要通过状态切换检查safepoint请求。
3. JNI critical region 是特殊情况。

JNI官方规范要求 `GetPrimitiveArrayCritical` / `ReleasePrimitiveArrayCritical` 之间的native代码不能长时间运行、不能阻塞、不能随意JNI调用。因为VM可能为了这类critical区域临时限制GC移动对象。

所以不要简单说“native一定会卡住safepoint”, 更准确是:

> 普通native执行通常可以被JVM视为安全状态; 但JNI critical、长时间不返回或持有VM相关资源的native逻辑, 仍然可能影响GC或其它VM操作。

## 7. 常见触发场景

从源码看, 触发safepoint的直接机制通常是某个 `VM_Operation` 被提交给 `VMThread`, 且 `evaluate_at_safepoint()` 为 true。

典型场景:

| 场景 | 为什么需要safepoint |
| :---: | :--- |
| Stop-The-World GC | 需要稳定地遍历线程栈和对象引用 |
| 线程栈遍历 | 如 `jstack`、JVMTI、诊断命令需要读取栈帧 |
| 类卸载 | 需要确认类元数据、对象引用、代码引用状态 |
| 偏向锁撤销 | 需要检查对象和线程持锁状态 |
| 代码反优化 | 编译代码需要回退到解释执行或重建栈帧 |
| 代码缓存清理 | 需要处理正在执行或即将执行的compiled code |
| monitor deflation | 清理不再使用的monitor结构 |
| 类重定义 | JVMTI redefine classes 需要稳定的类元数据视图 |

不是所有GC都全程STW, 但很多GC在某些阶段仍然需要safepoint。并发GC只是把一部分工作放到Java线程并发执行期间做, 不代表完全没有安全点停顿。

## 8. 性能影响

Safepoint对性能的影响主要看三段时间:

```text
进入safepoint的等待时间
        +
VM操作真正执行时间
        +
线程恢复时间
```

其中最容易被忽略的是第一段: `time to safepoint`。

如果日志里看到safepoint总时间很长, 要区分到底是VM操作本身慢, 还是线程进入safepoint慢。

常见原因:

1. 某个线程长时间没有运行到safepoint poll。
2. 某个线程长时间没有被OS调度。
3. 编译代码中safepoint poll位置太少, 例如大循环没有合适的安全点检查。
4. CPU被打满, VMThread或业务线程抢不到CPU。
5. 容器CPU配额太小, 导致safepoint协调和GC线程竞争激烈。
6. 频繁触发需要safepoint的VM操作, 导致业务线程反复停顿。
7. JNI critical或复杂native逻辑影响GC/VM操作。

Safepoint本身不是坏事。问题通常出在safepoint太频繁、单次VM操作太慢, 或进入safepoint的等待时间异常长。

## 9. 排查命令

### 9.1 JDK 8

查看safepoint统计:

```shell
java \
  -XX:+UnlockDiagnosticVMOptions \
  -XX:+PrintSafepointStatistics \
  -XX:PrintSafepointStatisticsCount=1 \
  -XX:+PrintGCApplicationStoppedTime \
  -jar app.jar
```

常见关注点:

```text
vmop                 触发safepoint的VM操作
sync time            等待线程进入safepoint的时间
cleanup time         safepoint内部清理时间
vmop time            VM操作执行时间
total                safepoint总耗时
```

配合线程栈:

```shell
jstack <pid>
jcmd <pid> Thread.print
```

配合OS线程观察:

```shell
top -H -p <pid>
pidstat -t -p <pid> 1
ps -eLf | grep java
```

### 9.2 JDK 9+

统一日志可以这样看:

```shell
java -Xlog:safepoint=info -jar app.jar
java -Xlog:safepoint*=debug -jar app.jar
```

也可以运行中打开:

```shell
jcmd <pid> VM.log what=safepoint=debug
jcmd <pid> VM.log output=safepoint.log what=safepoint*=debug
```

其他辅助命令:

```shell
jcmd <pid> VM.command_line
jcmd <pid> VM.flags
jcmd <pid> Thread.print
jcmd <pid> JFR.start name=sp duration=60s filename=safepoint.jfr
```

如果怀疑是OS调度或CPU饱和, 还要看:

```shell
top -H -p <pid>
pidstat -t -p <pid> 1
perf top -p <pid>
cat /proc/<pid>/task/<tid>/status
```

排查safepoint问题时不要只看Java线程状态。最好同时看safepoint日志、线程栈、OS线程CPU、容器CPU限制和GC日志。

## 10. Safepoint和OS中断的区别

Safepoint不是操作系统中断。

| 对比项 | Safepoint | OS中断 |
| :---: | :--- | :--- |
| 所属层次 | JVM运行时机制 | 操作系统/硬件机制 |
| 发起者 | VMThread / VM operation | CPU、设备、内核事件 |
| 目的 | 让JVM执行全局安全操作 | 响应硬件、时钟、IO、异常或系统事件 |
| 停顿方式 | Java线程运行到可检查点后协作停下 | CPU/内核可以异步打断当前执行流 |
| 可见对象 | JVM线程、栈、oop、元数据 | CPU寄存器、内核线程、设备、调度实体 |
| 典型场景 | GC、栈遍历、反优化、类卸载 | 时钟中断、IO中断、缺页异常、系统调用 |
| 是否等同于 `Thread.interrupt()` | 否 | 否 |

OS中断可以发生在很底层的位置, 例如时钟中断让内核有机会重新调度线程。Safepoint则是JVM自己定义的运行时协议, 它关心的是Java线程能不能停在JVM可以理解的状态上。

有些HotSpot实现细节可能会借助内存页保护、信号或trap来实现safepoint poll, 但这只是实现手段。语义上safepoint仍然是JVM的安全点协议, 不是OS中断机制本身。

另外, Java的 `Thread.interrupt()` 也不是safepoint。`interrupt()` 只是设置线程中断标记, 并唤醒部分可中断阻塞操作; 它不会让JVM进入全局安全点, 也不会暂停所有Java线程。

## 11. 几个容易误解的点

1. `RUNNABLE` 不等于正在CPU上执行。
2. STW不是“瞬间按暂停键”, 而是相关线程陆续到达safepoint或进入可认为安全的状态。
3. native里的线程不等于一定卡住safepoint。
4. `Thread.sleep()` / `yield()` 不是safepoint控制手段。
5. `Thread.interrupt()` 不是safepoint。

## 12. 总结

1. Safepoint是HotSpot为全局VM操作准备的安全停顿机制。
2. Java线程不是在任意机器指令处被JVM粗暴暂停, 而是在safepoint poll、状态切换等JVM可控位置停下。
3. `_thread_in_Java`、`_thread_in_native`、`_thread_blocked` 等内部状态会影响safepoint协调方式。
4. GC、栈遍历、类卸载、偏向锁撤销、反优化等都可能触发safepoint。
5. 分析safepoint性能时, 要区分 `time to safepoint` 和 `vm operation time`。
6. Safepoint属于JVM协作协议, OS中断属于操作系统/硬件机制, 二者不是一个层面的东西。

## 13. 参考资料

* [OpenJDK safepoint.cpp](https://github.com/openjdk/jdk/blob/master/src/hotspot/share/runtime/safepoint.cpp)
* [OpenJDK safepointMechanism.hpp](https://github.com/openjdk/jdk/blob/master/src/hotspot/share/runtime/safepointMechanism.hpp)
* [OpenJDK vmThread.cpp](https://github.com/openjdk/jdk/blob/master/src/hotspot/share/runtime/vmThread.cpp)
* [OpenJDK vmOperation.hpp](https://github.com/openjdk/jdk/blob/master/src/hotspot/share/runtime/vmOperation.hpp)
* [OpenJDK interfaceSupport.inline.hpp](https://github.com/openjdk/jdk/blob/master/src/hotspot/share/runtime/interfaceSupport.inline.hpp)
* [OpenJDK javaThread.hpp](https://github.com/openjdk/jdk/blob/master/src/hotspot/share/runtime/javaThread.hpp)
* [Oracle JNI Functions - GetPrimitiveArrayCritical](https://docs.oracle.com/en/java/javase/24/docs/specs/jni/functions.html)
* [Oracle JVM TI Specification](https://docs.oracle.com/en/java/javase/18/docs/specs/jvmti.html)
* [Linux Kernel Generic IRQ Handling](https://www.kernel.org/doc/html/v6.7/core-api/genericirq.html)
