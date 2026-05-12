# JVM线程模型

> 本文主要以 OpenJDK 8 / HotSpot 为例。线程这块容易和操作系统实现混在一起, 所以这里先记录一条主线: Java线程在HotSpot内部怎么表示, 又是怎么落到不同操作系统线程上的。

## 1. Java线程和OS线程的关系

早期JVM曾经出现过绿色线程模型, 但是HotSpot主流实现中, Java线程基本采用一对一模型:

```text
java.lang.Thread
        |
        v
JavaThread
        |
        v
OSThread
        |
        v
native thread(pthread / Windows thread)
```

简单理解:

* `java.lang.Thread`: Java层看到的线程对象
* `JavaThread`: HotSpot内部表示Java线程的C++对象
* `OSThread`: HotSpot封装的操作系统线程信息
* native thread: 真正由操作系统调度的线程

所以 Java 线程什么时候运行、运行多久、在哪个CPU上运行, 最终还是由操作系统调度器决定。JVM可以设置线程优先级、创建线程、阻塞唤醒线程, 但它不是最终的CPU调度者。

## 2. 源码入口

常见源码位置:

| 内容 | OpenJDK 8 路径 | 新版JDK路径 |
| :---: | :--- | :--- |
| 线程公共逻辑 | `hotspot/src/share/vm/runtime/thread.cpp` | `src/hotspot/share/runtime/thread.cpp` |
| 线程定义 | `hotspot/src/share/vm/runtime/thread.hpp` | `src/hotspot/share/runtime/thread.hpp` |
| Linux实现 | `hotspot/src/os/linux/vm/os_linux.cpp` | `src/hotspot/os/linux/os_linux.cpp` |
| macOS实现 | `hotspot/src/os/bsd/vm/os_bsd.cpp` | `src/hotspot/os/bsd/os_bsd.cpp` |
| Windows实现 | `hotspot/src/os/windows/vm/os_windows.cpp` | `src/hotspot/os/windows/os_windows.cpp` |

在OpenJDK 8中, Java线程创建可以重点看:

```c++
JavaThread::JavaThread(...)
os::create_thread(...)
os::start_thread(...)
```

大致流程:

1. Java层调用 `Thread.start()`
2. JVM进入 native 方法 `JVM_StartThread`
3. HotSpot创建 `JavaThread`
4. 平台相关的 `os::create_thread` 创建native thread
5. `os::start_thread` 唤醒线程真正开始执行

## 3. HotSpot中的线程类型

HotSpot内部不只有Java业务线程, 还有很多VM自己的线程:

| 类型 | 说明 |
| :---: | :--- |
| `JavaThread` | 执行Java代码的线程, 包括普通业务线程 |
| `VMThread` | 执行VM操作, 如safepoint相关任务 |
| `CompilerThread` | JIT编译线程 |
| `WatcherThread` | 周期性任务线程 |
| GC线程 | 执行串行、并行或并发GC相关工作 |
| Service线程 | JVM内部服务线程, 例如低频事件处理 |

这些线程最终也会落到操作系统线程上。区别在于它们在HotSpot内部承担的职责不同。

## 4. 线程状态

Java层有 `java.lang.Thread.State`:

```text
NEW
RUNNABLE
BLOCKED
WAITING
TIMED_WAITING
TERMINATED
```

HotSpot内部还会维护更细的执行状态, 例如:

```c++
_thread_new
_thread_in_native
_thread_in_vm
_thread_in_Java
_thread_blocked
```

这里要注意一个容易误解的点:

> Java层的 `RUNNABLE` 不等于操作系统层面一定正在CPU上运行。

Java里的 `RUNNABLE` 通常包含“正在运行”和“就绪等待CPU调度”。也就是说, 一个线程处于 `RUNNABLE`, 可能正在CPU上执行, 也可能只是已经具备运行条件, 但还在OS run queue里排队。

## 5. 操作系统调度差异

### 5.1 总体差异

JVM屏蔽了很多平台差异, 但调度行为最终还是会受到OS影响:

| 维度 | Linux | macOS | Windows |
| :---: | :--- | :--- | :--- |
| native线程API | pthread | pthread(BSD/Mach之上) | Windows Thread API |
| 调度核心 | CFS等Linux调度器 | XNU调度器 | Windows Scheduler |
| Java优先级映射 | 通常映射较弱, 常规用户权限下影响有限 | 映射更保守, JVM优先级不应过度依赖 | 映射到Windows线程优先级, 影响相对更明显 |
| 阻塞/唤醒 | futex/pthread cond等 | pthread cond/Mach原语等 | Event/Semaphore/ConditionVariable等 |
| 栈管理 | pthread栈 + guard page | pthread栈 + guard page | Windows线程栈 + guard page |

JVM层面同样是 `Thread.sleep()`、`Object.wait()`、`LockSupport.park()`、`synchronized` 阻塞, 到不同OS上会变成不同的系统调用或内核同步原语。

### 5.2 Linux调度

Linux下HotSpot一般通过 `pthread_create` 创建线程。普通Java线程最终就是一个Linux native thread, 可以通过下面命令观察:

```shell
ps -eLf | grep java
top -H -p <pid>
jstack <pid>
```

几个重点:

1. Linux的普通线程调度主要由CFS负责, JVM不会自己分配CPU时间片。
2. Java线程优先级会尝试映射到OS优先级或nice值, 但普通用户权限下能产生的影响有限。
3. `Thread.yield()` 只是提示调度器让出执行机会, 不保证其它线程一定马上运行。
4. `LockSupport.park()`、monitor阻塞等最终会进入OS阻塞等待, 常见底层会用到 futex 或 pthread 条件变量一类机制。
5. CPU亲和性、cgroup、容器CPU配额会明显影响Java线程实际调度。

所以在Linux上看Java线程性能问题时, 不能只看Java线程状态, 还要看:

```shell
top -H
pidstat -t
perf top
cat /proc/<pid>/task/<tid>/status
```

尤其在容器里, JVM看到的CPU数量和cgroup限制之间如果不一致, 可能会影响GC线程数、JIT线程数和业务线程争抢CPU的情况。

### 5.3 macOS调度

macOS的HotSpot在OpenJDK 8老目录中通常归在 `bsd` 平台实现里, 底层也是 pthread API, 再落到 XNU 内核调度。

几个重点:

1. macOS对线程优先级和调度策略的控制比Linux/Windows更保守, Java层 `setPriority` 不适合当成强调度手段。
2. macOS桌面系统会受到App Nap、电源策略、前后台状态影响, 压测结果可能和Linux服务器差异明显。
3. Apple Silicon 下还会涉及性能核/能效核调度, 同样的Java线程可能被OS放到不同类型核心上执行。
4. HotSpot里可以看到一些 `MACOS_AARCH64_ONLY` 之类的宏, 这类代码用于处理 Apple Silicon 平台特有的约束。

因此, macOS更适合作为开发调试环境, 不建议直接用macOS线程调度表现推断Linux生产环境表现。

### 5.4 Windows调度

Windows下HotSpot使用Windows线程API创建线程。Windows线程优先级体系和Linux不同, Java线程优先级映射到Windows后通常更容易体现出差异。

几个重点:

1. Windows有进程优先级类和线程相对优先级两个维度。
2. Java的 `Thread.setPriority()` 在Windows上可能比Linux更容易影响调度, 但仍然不建议依赖它做业务正确性保证。
3. Windows的等待/唤醒会使用Windows内核对象或条件变量等机制。
4. Windows下计时器精度、sleep唤醒精度与系统配置有关, `Thread.sleep(1)` 不一定精确睡1ms。

所以跨平台Java程序里, 和线程调度相关的逻辑应该尽量避免依赖优先级、yield、sleep精度。

## 6. Java优先级为什么不可靠

Java层优先级范围是1到10:

```java
Thread.MIN_PRIORITY  // 1
Thread.NORM_PRIORITY // 5
Thread.MAX_PRIORITY  // 10
```

但是不同OS支持的优先级模型不一样, JVM只能做映射:

```text
Java priority 1..10  ->  OS priority / nice / policy
```

这个映射有几个限制:

1. OS优先级档位不一定正好是10档。
2. 普通用户权限可能没有权限提高线程优先级。
3. OS调度器还会考虑公平性、交互性、CPU拓扑、电源策略。
4. 容器环境下还会叠加cgroup调度限制。

因此, Java优先级只能作为调度提示, 不能作为严格执行顺序的保证。

## 7. sleep、yield、park、wait的区别

| 方法 | 大概含义 | 是否释放monitor | 调度特点 |
| :---: | :--- | :---: | :--- |
| `Thread.sleep()` | 当前线程睡眠一段时间 | 否 | 到时间后进入可运行状态, 何时运行仍由OS决定 |
| `Thread.yield()` | 提示让出CPU | 否 | 只是提示, 不保证让给谁 |
| `Object.wait()` | 在对象monitor上等待 | 是 | 需要持有monitor, 被notify/超时/中断唤醒 |
| `LockSupport.park()` | 挂起当前线程 | 否 | 基于permit模型, 常用于并发包 |

这些API到了HotSpot内部会经过不同路径, 但最终阻塞和唤醒都离不开OS提供的等待机制。

## 8. Safepoint与线程调度

线程调度是OS层面的事情, safepoint是JVM为了执行某些全局操作而设置的安全点机制。

典型需要safepoint的场景:

1. Stop-The-World GC
2. 类卸载
3. 偏向锁撤销
4. 线程栈遍历
5. 代码反优化

当JVM发起safepoint时, Java线程需要运行到可检查的位置并停下来。这里要注意:

* OS调度决定线程什么时候获得CPU继续跑
* JVM safepoint决定线程跑到某些点时是否需要停下配合VM操作

所以如果某个线程长时间没有被OS调度到, 或者长时间处在native代码里, 都可能影响JVM进入safepoint的速度。

## 9. 总结

1. HotSpot主流Java线程模型是一对一映射到OS线程。
2. JVM负责创建、管理、记录线程状态, 但CPU调度最终由OS完成。
3. Linux、macOS、Windows在线程优先级、阻塞唤醒、计时器精度、电源策略上都有差异。
4. `Thread.setPriority()`、`yield()`、`sleep()` 都不能作为严格调度保证。
5. 分析线程问题时要结合 `jstack`、OS线程ID、CPU使用率和系统调度信息一起看。

