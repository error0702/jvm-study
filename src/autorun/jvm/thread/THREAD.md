# JVM线程模型

> 本文主要以 OpenJDK 8 / HotSpot 为例。线程这块容易和操作系统实现混在一起, 所以这里先记录一条主线: Java线程在HotSpot内部怎么表示, 又是怎么落到不同操作系统线程上的。更高版本引入Virtual Threads之后, Java层线程和OS线程的关系会发生变化, 这部分单独放在专题文档中说明。

## 1. Java线程和OS线程的关系

早期JVM曾经出现过绿色线程模型, 但是HotSpot主流平台线程实现中, Java线程基本采用一对一模型:

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

这里要把两个概念分开:

1. **平台线程**: JDK 8里普通 `java.lang.Thread` 的主流实现, 基本一条Java线程对应一条OS线程。
2. **虚拟线程**: JDK 21正式引入的轻量线程, Java层可以创建大量 `Thread`, 但它们不会长期绑定一条固定OS线程。

本文主要讨论平台线程。虚拟线程见 [Java Virtual Thread设计与调度机制](VIRTUAL_THREAD_DESIGN.md)。

## 2. 源码入口

常见源码位置:

| 内容 | OpenJDK 8 路径 | 新版JDK路径 |
| :---: | :--- | :--- |
| 线程公共逻辑 | `hotspot/src/share/vm/runtime/thread.cpp` | `src/hotspot/share/runtime/thread.cpp` |
| 线程定义 | `hotspot/src/share/vm/runtime/thread.hpp` | `src/hotspot/share/runtime/thread.hpp` |
| Linux实现 | `hotspot/src/os/linux/vm/os_linux.cpp` | `src/hotspot/os/linux/os_linux.cpp` |
| macOS实现 | `hotspot/src/os/bsd/vm/os_bsd.cpp` | `src/hotspot/os/bsd/os_bsd.cpp` |
| Windows实现 | `hotspot/src/os/windows/vm/os_windows.cpp` | `src/hotspot/os/windows/os_windows.cpp` |
| Java Thread native入口 | `hotspot/src/share/vm/prims/jvm.cpp` | `src/hotspot/share/prims/jvm.cpp` |
| Parker/park实现 | `hotspot/src/share/vm/runtime/park.hpp` | `src/hotspot/share/runtime/park.hpp` |
| ObjectMonitor | `hotspot/src/share/vm/runtime/objectMonitor.cpp` | `src/hotspot/share/runtime/objectMonitor.cpp` |

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

可以把调用链粗略画成:

```text
Java: Thread.start()
        |
        v
JVM_StartThread
        |
        v
new JavaThread(...)
        |
        v
os::create_thread(thread, ...)
        |
        v
pthread_create / CreateThread
        |
        v
os::start_thread(thread)
        |
        v
JavaThread::run()
        |
        v
调用Java层Thread.run()
```

这里容易误解的一点是: `Thread.start()` 不是直接在当前线程里调用 `run()`。它会进入VM, 创建或绑定底层执行实体, 然后由新线程回调Java层 `run()`。

### 2.1 从start到退出的生命周期

平台线程从创建到退出, 可以按下面几段理解:

```text
Java对象创建:
    new Thread(...)
    只有Java对象, 还没有真正OS线程

start:
    JVM_StartThread
    创建JavaThread / OSThread / native thread

运行:
    native thread进入JavaThread::run()
    回调Java层Thread.run()

退出:
    run()正常返回或抛出未捕获异常
    HotSpot做线程退出清理
    Java层Thread进入TERMINATED
```

几个注意点:

1. 同一个 `Thread` 对象只能 `start()` 一次。线程结束后不能再次启动。
2. `run()` 抛出未捕获异常时, 当前线程会结束, 异常会交给 `UncaughtExceptionHandler`。
3. 线程退出时, JVM需要清理 `JavaThread`、释放native thread相关资源、从线程列表中移除。
4. 其它线程调用 `join()` 等待的是目标线程终止这个事实, 不是等待某个返回值。

所以 `Thread` 不是可重复使用的任务对象。如果要反复提交任务, 应该使用线程池、Executor或虚拟线程per-task executor。

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
| Attach Listener | 处理 `jcmd`、`jstack` 等Attach请求 |
| Signal Dispatcher | 处理部分信号相关逻辑 |
| Reference Handler / Finalizer | Java层引用处理、终结逻辑相关线程 |

这些线程最终也会落到操作系统线程上。区别在于它们在HotSpot内部承担的职责不同。

从排查角度看, `jstack` 里看到的不一定都是业务线程。像 `VM Thread`、`GC Thread`、`C2 CompilerThread`、`Attach Listener` 这些线程, 更应该从JVM内部职责理解, 不要误判成业务线程池泄漏。

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

三层状态不要混在一起:

| 层次 | 例子 | 说明 |
| :---: | :--- | :--- |
| Java层状态 | `RUNNABLE`、`WAITING`、`BLOCKED` | `Thread.getState()` 或 `jstack` 展示给Java开发者的抽象 |
| HotSpot内部状态 | `_thread_in_Java`、`_thread_in_native`、`_thread_blocked` | JVM内部用于safepoint、栈遍历、状态转换判断 |
| OS调度状态 | running、runnable、sleeping、blocked | 内核调度器看到的线程状态 |

这里要注意一个容易误解的点:

> Java层的 `RUNNABLE` 不等于操作系统层面一定正在CPU上运行。

Java里的 `RUNNABLE` 通常包含“正在运行”和“就绪等待CPU调度”。也就是说, 一个线程处于 `RUNNABLE`, 可能正在CPU上执行, 也可能只是已经具备运行条件, 但还在OS run queue里排队。

### 4.1 Java状态和常见阻塞点

| Java状态 | 常见来源 | 是否还在消耗CPU |
| :---: | :--- | :---: |
| `NEW` | 创建了 `Thread` 但还没 `start()` | 否 |
| `RUNNABLE` | 正在Java/native中运行, 或已就绪等待OS调度 | 可能 |
| `BLOCKED` | 等待进入 `synchronized` monitor | 否, 通常在等待锁 |
| `WAITING` | `Object.wait()`、`Thread.join()`、`LockSupport.park()` | 否, 等待唤醒 |
| `TIMED_WAITING` | `sleep()`、限时 `wait()`、限时 `park()` | 否, 等待超时或唤醒 |
| `TERMINATED` | `run()` 结束或异常退出 | 否 |

`RUNNABLE` 最容易误判。比如线程卡在socket read的native调用里, Java层也可能看到 `RUNNABLE`。所以排查CPU问题时, 不能只靠Java状态, 还要结合OS线程CPU占用。

### 4.2 HotSpot状态和safepoint的关系

HotSpot内部状态更关心“线程现在能不能被VM安全观察”:

| HotSpot状态 | 大概含义 | 对safepoint的影响 |
| :---: | :--- | :--- |
| `_thread_in_Java` | 正在执行Java解释器或JIT代码 | 通常需要跑到poll点响应safepoint |
| `_thread_in_vm` | 正在执行VM内部代码 | VM代码路径需要遵守safepoint检查协议 |
| `_thread_in_native` | 正在执行JNI/native代码 | 通常暂时视为safe, 返回Java前要检查 |
| `_thread_blocked` | 阻塞等待monitor、park等 | 通常可认为safe, 唤醒/返回前检查 |

这也是为什么 [HotSpot Safepoint安全点机制](SAFEPOINT.md) 里强调: safepoint不能只看Java层 `Thread.State`。

### 4.3 线程栈、-Xss和StackOverflowError

平台线程有自己的native stack。Java方法调用、解释器/JIT栈帧、JNI调用等都会消耗线程栈空间。

`-Xss` 用来设置每个线程的栈大小:

```shell
java -Xss1m -jar app.jar
```

需要注意:

1. `-Xss` 越大, 单线程能承载的调用深度越深, 但同样内存下能创建的平台线程数量越少。
2. `-Xss` 越小, 能创建更多平台线程, 但递归、深调用链、复杂框架栈更容易触发 `StackOverflowError`。
3. 平台线程栈通常还有guard page, 用于在栈接近耗尽时触发保护逻辑。
4. 虚拟线程的栈模型不同, 它的栈以stack chunk形式放在Java堆里, 不应该简单套用平台线程的固定native stack直觉。

所以线上看到“无法创建新线程”时, 不只要看Java堆, 还要看:

```text
进程可用内存
每线程栈大小(-Xss)
OS线程数限制
容器内存限制
ulimit / pid limit
```

### 4.4 daemon、join和JVM退出

Java线程分为daemon线程和非daemon线程。

JVM退出的大致规则是:

```text
只要还有非daemon Java线程存活:
    JVM继续运行

所有非daemon Java线程都结束:
    JVM可以退出
    daemon线程不会阻止JVM退出
```

几个常见点:

1. `main` 线程是非daemon线程。
2. 后台监控、清理、心跳线程常被设置成daemon, 但不能把关键业务写入只放在daemon线程里。
3. `join()` 是当前线程等待目标线程结束, 不是目标线程等待当前线程。
4. `join()` 内部语义可以理解为等待目标线程终止并建立可见性关系: 目标线程结束前的动作, 对成功检测到它终止的线程可见。

示例:

```java
Thread worker = new Thread(() -> {
    // do work
});
worker.start();
worker.join(); // 当前线程等待worker结束
```

如果调用 `join()` 的线程被中断, `join()` 会抛出 `InterruptedException`。这不是目标线程被中断导致的, 而是等待者自己被中断。

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

### 5.5 容器、CPU拓扑和调度抖动

现代Java应用经常跑在容器里, 线程调度还会叠加cgroup限制。

需要关注:

| 因素 | 影响 |
| :---: | :--- |
| CPU quota | 容器可用CPU时间被限制, 线程可能频繁等待配额恢复 |
| cpuset | 进程只能在指定CPU集合上运行, 影响并行度和亲和性 |
| memory limit | 线程栈、Java堆、native memory共同挤占容器内存 |
| pid limit | 平台线程过多可能先碰到进程/线程数量限制 |
| NUMA | 跨NUMA节点访问内存可能增加延迟 |
| 大小核 | macOS/部分Linux平台上, 性能核/能效核调度会影响延迟稳定性 |

这也是为什么同一份Java代码在本机、容器、物理机、云主机上的线程表现可能不同。

排查时可以补充看:

```shell
jcmd <pid> VM.flags
jcmd <pid> VM.info
cat /sys/fs/cgroup/cpu.max
cat /sys/fs/cgroup/cpuset.cpus
ulimit -a
```

JDK 10之后容器感知能力逐步增强, JVM会更准确地读取容器CPU/内存限制。但这不代表调度问题消失了: 当CPU quota很小、线程数很多、GC/JIT/业务线程同时竞争时, 抖动仍然会放大。

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

### 7.1 monitor、park、interrupt的关系

这几个概念容易混:

| 机制 | Java入口 | HotSpot/底层关注点 | 备注 |
| :---: | :--- | :--- | :--- |
| monitor | `synchronized`、`Object.wait()` | `ObjectMonitor`、monitor enter/exit、wait set | `wait()` 必须持有monitor, 调用后释放monitor |
| park | `LockSupport.park()` | `Parker`、permit、条件变量/事件 | AQS、线程池、并发包常用 |
| interrupt | `Thread.interrupt()` | 中断标记 + 唤醒部分可中断阻塞 | 不是强制停止线程 |

`interrupt()` 不会直接杀死线程。它主要做两件事:

1. 设置线程的中断标记。
2. 如果线程正阻塞在 `sleep()`、`wait()`、`join()` 或部分park路径上, 让它有机会被唤醒并处理 `InterruptedException` 或中断状态。

所以业务代码要想“响应中断”, 必须自己检查中断标记或正确处理中断异常。JVM不会因为调用了 `interrupt()` 就把目标线程从任意机器指令处停下来。

### 7.2 为什么不建议依赖sleep做同步

`Thread.sleep(100)` 只能表达“至少让当前线程在一段时间内不主动运行”, 不能表达“另一个线程一定已经完成某件事”。

原因是:

1. sleep结束后线程只是变成可运行, 什么时候再次拿到CPU仍由OS决定。
2. OS计时器精度、负载、电源策略都会影响唤醒时间。
3. 另一个线程可能因为锁竞争、GC、safepoint、CPU配额没有运行。

正确表达线程协作, 应该优先用:

1. `join()` 等待线程结束。
2. `CountDownLatch`、`CyclicBarrier`、`Semaphore` 等同步器。
3. `Future` / `CompletableFuture` 表达任务完成。
4. `BlockingQueue` 表达生产消费。

### 7.3 线程协作和内存可见性

线程调度只解决“谁什么时候运行”, 不解决“一个线程写的值什么时候对另一个线程可见”。可见性要靠Java内存模型里的同步关系。

常见 happens-before 关系:

| 关系 | 含义 |
| :---: | :--- |
| 程序顺序 | 同一线程内, 前面的动作 happens-before 后面的动作 |
| `Thread.start()` | 调用 `start()` 之前的动作, 对新线程里的动作可见 |
| 线程终止 / `join()` | 目标线程结束前的动作, 对成功 `join()` 的线程可见 |
| monitor unlock/lock | 对同一个monitor的解锁 happens-before 后续加锁 |
| volatile写/读 | 对某个volatile变量的写 happens-before 后续读 |
| interrupt检测 | 一个线程调用 `interrupt()` happens-before 另一个线程检测到中断 |

所以:

```java
ready = true;       // 普通变量写
thread.start();    // start建立可见性边界
```

新线程能看到 `start()` 之前已经发生的写入。

但下面这种写法没有可靠同步:

```java
boolean done = false;

new Thread(() -> {
    while (!done) {
        // 可能一直看不到done变成true
    }
}).start();

done = true;
```

正确做法是使用 `volatile`、锁、并发工具类或其它明确同步机制:

```java
volatile boolean done = false;
```

这里要区分:

1. OS调度决定线程有没有机会运行。
2. JMM决定线程之间的读写是否有可见性保证。
3. safepoint决定JVM内部操作能否在安全位置观察线程状态。

这三件事相关, 但不是同一个层面。

## 8. 线程问题怎么定位

分析Java线程问题时, 最好同时拿三类信息:

1. JVM视角: `jstack` / `jcmd Thread.print`。
2. OS视角: 每个native线程的CPU、状态、调度情况。
3. JVM日志: GC、safepoint、JIT、容器CPU等背景。

### 8.1 Java线程和OS线程ID怎么对应

`jstack` 中常见字段:

```text
"worker-1" #31 prio=5 os_prio=0 tid=0x00007f... nid=0x1234 runnable
```

可以先这样理解:

| 字段 | 含义 |
| :---: | :--- |
| Java线程名 | `"worker-1"` |
| `#31` | JVM内部展示的Java线程序号 |
| `prio` | Java层优先级 |
| `os_prio` | OS层优先级映射结果 |
| `tid` | HotSpot内部 `JavaThread*` 地址一类的标识 |
| `nid` | native thread id, 通常用于和OS工具对齐 |

在Linux上常用做法:

```shell
top -H -p <pid>
printf '%x\n' <tid>
jstack <pid> | grep -i <hex-nid>
```

`top -H` 里看到的是十进制线程ID, `jstack` 的 `nid` 常以十六进制展示, 所以需要转换。

### 8.2 常见现象怎么分流

| 现象 | 优先看什么 | 可能方向 |
| :---: | :--- | :--- |
| CPU很高 | `top -H` + `jstack` 对齐高CPU线程 | 死循环、忙等、频繁GC、JIT编译、锁自旋 |
| 大量 `BLOCKED` | `jstack` monitor owner | synchronized锁竞争 |
| 大量 `WAITING` / `TIMED_WAITING` | 等待栈顶方法 | 线程池空闲、队列等待、park、sleep |
| 响应突然卡顿 | GC日志 + safepoint日志 + 线程栈 | STW GC、长safepoint、锁竞争、IO阻塞 |
| 容器内抖动 | cgroup CPU、`top -H`、JVM flags | CPU quota太小、线程数过多、GC线程竞争 |

不要只看线程状态下结论。例如大量 `WAITING` 不一定是问题, 线程池工作线程空闲时本来就会等待队列任务。

## 9. Safepoint与线程调度

线程调度是OS层面的事情, safepoint是JVM为了执行某些全局操作而设置的安全点机制。更完整的说明见 [HotSpot Safepoint 安全点机制](SAFEPOINT.md)。

典型需要safepoint的场景:

1. Stop-The-World GC
2. 类卸载
3. 偏向锁撤销
4. 线程栈遍历
5. 代码反优化

当JVM发起safepoint时, Java线程需要运行到可检查的位置并停下来。这里要注意:

* OS调度决定线程什么时候获得CPU继续跑
* JVM safepoint决定线程跑到某些点时是否需要停下配合VM操作

所以如果某个线程长时间没有被OS调度到, 就可能迟迟跑不到下一个safepoint检查位置。至于native代码, 要看具体状态: 普通native执行通常可以被JVM视为安全状态, 但JNI critical、长时间不返回或持有VM相关资源的native逻辑, 仍然可能影响GC或其它VM操作。

## 10. Virtual Threads专题入口

JDK 21之后, virtual thread改变了Java线程和OS线程一对一的直觉。它更像JDK把异步事件分发、continuation恢复、carrier线程调度封装到了 `Thread` API 后面。

这部分不再放在线程模型主文档里展开, 详见 [Java Virtual Thread设计与调度机制](VIRTUAL_THREAD_DESIGN.md)。

## 11. 总结

1. HotSpot主流Java线程模型是一对一映射到OS线程。
2. JVM负责创建、管理、记录线程状态, 但CPU调度最终由OS完成。
3. Java层状态、HotSpot内部状态、OS调度状态是三套不同抽象, 不能直接等同。
4. Linux、macOS、Windows在线程优先级、阻塞唤醒、计时器精度、电源策略上都有差异。
5. `Thread.setPriority()`、`yield()`、`sleep()` 都不能作为严格调度保证。
6. 分析线程问题时要结合 `jstack`、OS线程ID、CPU使用率和系统调度信息一起看。
7. JDK 21虚拟线程改变了Java线程和OS线程的一对一直觉, 需要单独看 [Java Virtual Thread设计与调度机制](VIRTUAL_THREAD_DESIGN.md)。
8. 线程协作不仅要考虑调度, 还要考虑Java内存模型的可见性边界。

## 12. 参考资料

* [OpenJDK jvm.cpp](https://github.com/openjdk/jdk/blob/master/src/hotspot/share/prims/jvm.cpp)
* [OpenJDK thread.cpp](https://github.com/openjdk/jdk/blob/master/src/hotspot/share/runtime/thread.cpp)
* [OpenJDK objectMonitor.cpp](https://github.com/openjdk/jdk/blob/master/src/hotspot/share/runtime/objectMonitor.cpp)
* [OpenJDK park.hpp](https://github.com/openjdk/jdk/blob/master/src/hotspot/share/runtime/park.hpp)
* [Java Language Specification 17 - Threads and Locks](https://docs.oracle.com/javase/specs/jls/se21/html/jls-17.html)
