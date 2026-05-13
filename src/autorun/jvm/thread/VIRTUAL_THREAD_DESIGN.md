# Java Virtual Thread设计与调度机制

> 本文不把virtual thread当成普通线程模型的一个小补丁, 而是把它当成一种运行时设计来看: JDK把异步事件分发、continuation保存/恢复、carrier线程调度封装在 `Thread` API 后面, 让应用层继续写同步阻塞代码。

## 1. 先用应用层思路类比

如果自己设计一个类似机制, 很容易想到:

```text
业务任务
  |
  v
提交到线程池
  |
  v
任务遇到IO等待
  |
  v
登记到调度线程 / NIO selector
  |
  v
worker线程释放
  |
  v
IO完成后selector通知
  |
  v
把任务重新投递到线程池
  |
  v
继续消费结果
```

这个方向和virtual thread的设计思想很接近。差别在于:

```text
普通异步框架:
    恢复的是callback / Future后续阶段

virtual thread:
    恢复的是原来的continuation / 调用栈
```

所以它底层像异步事件驱动, 但表层仍然是同步线程模型。

可以先记住一句话:

> virtual thread不是把阻塞消灭了, 而是把“等待期间占住OS线程”变成“等待期间挂起continuation”。

## 2. 和固定线程池最大的区别

固定线程池:

```text
fixed pool size = 6

worker-1: task-1 调远程接口, 阻塞中
worker-2: task-2 调远程接口, 阻塞中
worker-3: task-3 调远程接口, 阻塞中
worker-4: task-4 调远程接口, 阻塞中
worker-5: task-5 调远程接口, 阻塞中
worker-6: task-6 调远程接口, 阻塞中

task-7..task-N:
    在线程池队列里等待
```

这时CPU可能很空, 但6个worker都被阻塞任务占住。

virtual thread:

```text
carrier-1: vthread-1 运行到IO等待 -> vthread-1 unmount
carrier-1: 继续运行 vthread-7

carrier-2: vthread-2 运行到IO等待 -> vthread-2 unmount
carrier-2: 继续运行 vthread-8
```

区别是:

| 对比项 | 固定线程池 | virtual thread |
| :---: | :--- | :--- |
| 等待中的任务 | 占住worker线程 | 保存continuation后卸载 |
| 后续任务 | 在线程池队列里等worker空闲 | carrier可以继续跑其它virtual thread |
| 恢复方式 | OS唤醒阻塞的worker | JDK把virtual thread重新放回调度队列 |
| 适合场景 | 控制CPU并行度、控制资源并发 | 大量阻塞式IO任务 |

所以:

```text
线程池主要解决:
    线程创建太贵, 复用worker

virtual thread主要解决:
    阻塞等待太浪费, 释放carrier
```

## 3. 核心组件

从JDK实现看, virtual thread大致涉及这些角色:

| 角色 | 作用 |
| :---: | :--- |
| `VirtualThread` | Java层轻量线程对象, 仍然是 `Thread` 的一种 |
| `Continuation` | 保存和恢复virtual thread执行状态的核心机制 |
| scheduler | 调度virtual thread continuation的执行器 |
| carrier thread | 真正承载virtual thread运行的平台线程 |
| poller | 等待网络IO事件, 事件就绪后唤醒对应virtual thread |
| parker / monitor | 处理 `park`、`wait`、monitor阻塞等等待场景 |

JDK源码中可以重点看:

| 内容 | 源码 |
| :---: | :--- |
| virtual thread主实现 | [VirtualThread.java](https://github.com/openjdk/jdk/blob/master/src/java.base/share/classes/java/lang/VirtualThread.java) |
| continuation抽象 | [Continuation.java](https://github.com/openjdk/jdk/blob/master/src/java.base/share/classes/jdk/internal/vm/Continuation.java) |
| Thread构建入口 | [ThreadBuilders.java](https://github.com/openjdk/jdk/blob/master/src/java.base/share/classes/java/lang/ThreadBuilders.java) |
| JEP说明 | [JEP 444](https://openjdk.org/jeps/444) |
| JDK 24 pinning改进 | [JEP 491](https://openjdk.org/jeps/491) |

源码里能看到几个关键字段/概念:

```java
// VirtualThread.java中的核心方向
DEFAULT_SCHEDULER
Continuation cont
Runnable runContinuation
Thread carrierThread
```

这些名字已经把机制说得很直白:

```text
virtual thread自己不是OS线程
它的执行体是continuation
continuation被包装成runContinuation
runContinuation提交给scheduler
真正执行时挂载到carrierThread
```

## 4. carrier一般怎么配置

JDK的默认virtual thread scheduler是一个专用的 `ForkJoinPool`。它不是普通业务线程池, 也不建议把它理解成业务线程池的 `corePoolSize`。

默认并行度大致是:

```text
parallelism = Runtime.availableProcessors()
```

也就是JVM认为当前进程可用的CPU数。容器CPU限制、`-XX:ActiveProcessorCount` 都可能影响这个值。

常见可调系统属性:

```shell
-Djdk.virtualThreadScheduler.parallelism=8
-Djdk.virtualThreadScheduler.maxPoolSize=256
```

含义:

| 参数 | 含义 |
| :---: | :--- |
| `jdk.virtualThreadScheduler.parallelism` | 正常并行执行virtual thread的carrier数量 |
| `jdk.virtualThreadScheduler.maxPoolSize` | 某些阻塞不能unmount时, scheduler可补偿扩展的平台线程上限 |

注意:

1. 这不是业务线程池的 `corePoolSize * n`。
2. IO阻塞场景的伸缩性主要来自unmount, 不是无限增加carrier。
3. CPU密集任务仍然受CPU核心数限制。
4. pinning场景下carrier被占住, scheduler不一定补偿。

可以这样理解:

```text
可协作阻塞:
    virtual thread卸载
    carrier释放
    不需要增加carrier

部分不能卸载的JDK阻塞:
    可能临时扩carrier
    受maxPoolSize限制

pinning:
    carrier被占住
    扩展性下降
```

## 5. 一次virtual thread执行流程

创建:

```java
Thread.startVirtualThread(() -> {
    handleRequest();
});
```

粗略流程:

```text
创建VirtualThread
        |
        v
创建Continuation
        |
        v
封装成runContinuation
        |
        v
提交给scheduler
        |
        v
carrier取到任务
        |
        v
mount virtual thread
        |
        v
执行用户代码
```

如果用户代码执行完成:

```text
用户代码返回
        |
        v
Continuation done
        |
        v
unmount
        |
        v
VirtualThread进入TERMINATED
```

如果用户代码中途阻塞, 就进入下一节。

## 6. 阻塞时任务怎么关联回去

这是virtual thread的核心。

以网络读取为例:

```java
String body = readFromSocket();
process(body);
```

表面是同步阻塞代码, 底层可以拆成:

```text
vthread-1在carrier-3上运行
        |
        v
调用socket read
        |
        v
数据未就绪
        |
        v
把fd和vthread-1登记到poller
        |
        v
保存continuation
        |
        v
vthread-1 unmount
        |
        v
carrier-3释放, 去跑其它vthread
```

事件完成:

```text
OS通知fd可读
        |
        v
poller查表: fd -> vthread-1
        |
        v
unpark / submit vthread-1
        |
        v
vthread-1进入scheduler ready queue
        |
        v
某个carrier重新mount它
        |
        v
从readFromSocket之后继续执行process(body)
```

所以“任务怎么再关联回去”的答案是:

```text
挂起时:
    等待事件 -> virtual thread

完成时:
    事件 -> 找到virtual thread -> 放回调度队列
```

这和NIO的分发机制很像:

```text
selector注册:
    fd -> attachment/callback

virtual thread注册:
    fd -> virtual thread continuation
```

区别在于NIO通常恢复callback, virtual thread恢复原调用栈。

Inside Java的文章也用poller解释过网络IO: poller维护文件描述符到virtual thread的映射, IO事件到达后查回对应virtual thread并unpark它。见 [Networking I/O with Virtual Threads](https://inside.java/2021/05/10/networking-io-with-virtual-threads/)。

## 7. park / unpark场景

`LockSupport.park()` 更容易理解成“运行时级别的挂起”。

```text
virtual thread调用park
        |
        v
如果没有permit
        |
        v
设置PARKING状态
        |
        v
yield continuation
        |
        v
进入PARKED状态
        |
        v
carrier释放
```

别的线程调用:

```java
LockSupport.unpark(vthread);
```

恢复:

```text
设置permit
        |
        v
PARKED -> UNPARKED
        |
        v
submit runContinuation
        |
        v
scheduler重新调度
```

所以 `park/unpark` 不是“阻塞某个carrier线程”的语义, 而是尽量让virtual thread自己挂起, carrier去做别的工作。

如果continuation不能yield, 例如pinning场景, 才会退化成carrier被park。

## 8. monitor / wait / pinning

JDK 21里, pinning是理解virtual thread的关键限制。

pinning可以理解成:

```text
virtual thread本该unmount
但因为当前状态不能安全卸载
所以carrier也被一起占住
```

JDK 21常见场景:

1. 在 `synchronized` 中执行长时间阻塞。
2. native / foreign function 调用阻塞。

示意:

```java
synchronized void read() throws IOException {
    socket.getInputStream().read();
}
```

这段在JDK 21里可能pin住carrier。JDK 24的 [JEP 491](https://openjdk.org/jeps/491) 改进了 `synchronized` 相关pinning, 让更多场景可以在阻塞时释放carrier。

排查参数:

```shell
-Djdk.tracePinnedThreads=full
```

JFR也有 `jdk.VirtualThreadPinned` 事件。JEP 444说明了virtual thread相关JFR事件, 包括 start/end、pinned、submit failed。

## 9. 和线程池+调度线程设计的异同

你设想的设计:

```text
固定线程池
    +
调度线程
    +
NIO selector
    +
完成后通知调用端消费结果
```

这可以工作, 也是很多异步框架的大方向。

和virtual thread的相同点:

1. 都需要把等待事件登记起来。
2. 都需要有一个或多个调度/事件分发线程。
3. 都需要事件完成后把任务重新放回可执行队列。
4. 都能避免大量OS线程阻塞在IO上。

不同点:

| 对比项 | 线程池+调度线程 | Java virtual thread |
| :---: | :--- | :--- |
| 应用代码 | 通常要拆callback/Future/状态机 | 继续写同步阻塞代码 |
| 恢复对象 | callback、Future后续阶段、消息 | 原来的continuation和调用栈 |
| 局部变量 | 需要捕获到闭包/对象里 | 保留在虚拟线程栈中 |
| 异常栈 | 容易被异步边界切断 | 更接近普通同步调用栈 |
| 调试工具 | 框架自己建设 | JDK/JFR/thread dump支持 |
| 生态兼容 | 需要API异步化 | 复用很多阻塞式Java API |

所以virtual thread可以被看成:

```text
运行时托管的异步回调
        +
有栈continuation恢复
        +
Thread API外观
```

它不是“没有异步”, 而是把异步复杂度藏到了JDK里。

## 10. 和Go goroutine对比

Go goroutine和Java virtual thread很像:

```text
goroutine -> Go runtime scheduler -> OS thread
virtual thread -> JDK scheduler -> carrier platform thread -> OS thread
```

相似点:

1. 都是轻量并发执行体。
2. 都不是每个任务固定绑定一个OS线程。
3. 都有运行时调度器。
4. 都支持用近似同步的代码写大量并发任务。
5. 都需要处理阻塞、唤醒、栈增长、调度公平性。

差异:

| 对比项 | Go goroutine | Java virtual thread |
| :---: | :--- | :--- |
| 语言位置 | Go语言核心并发模型 | Java `Thread` API的一种实现 |
| 启动方式 | `go f()` | `Thread.startVirtualThread()` / virtual thread executor |
| 通信习惯 | channel是核心抽象 | 仍复用Thread、Executor、Queue、Lock等Java生态 |
| 调度模型术语 | G/M/P | virtual thread / carrier / scheduler |
| 生态目标 | 从语言设计初期围绕goroutine | 兼容既有Java阻塞式代码 |
| API颜色 | 无 `async` / `await` 颜色 | 同样不引入 `async` / `await` 颜色 |

Go规范把 `go` 语句描述为启动一个独立并发控制流。官方Go Wiki也说明goroutine是用户级轻量线程, 会复用多个OS线程。参考 [Go spec - Go statements](https://go.dev/ref/spec#Go_statements) 和 [Go Wiki - Go for C++ Programmers](https://tip.golang.org/wiki/GoForCPPProgrammers)。

一个直观区别是: Go把这个模型放在语言核心语法里; Java把它放在标准库 `Thread` 抽象里。

## 11. 和Kotlin coroutine对比

Kotlin coroutine更像“语言/编译器参与的无栈协程”。

Kotlin:

```kotlin
suspend fun fetch(): String {
    delay(1000)
    return "ok"
}
```

Java virtual thread:

```java
Thread.startVirtualThread(() -> {
    var data = blockingFetch();
});
```

对比:

| 对比项 | Kotlin coroutine | Java virtual thread |
| :---: | :--- | :--- |
| 挂起标记 | `suspend` 显式标记 | 普通阻塞调用 |
| 实现方式 | 编译器改写状态机 | JDK continuation保存/恢复调用栈 |
| 调度器 | `CoroutineDispatcher` | virtual thread scheduler |
| 调用链 | suspend语义会传播 | 方法签名不需要改成async/suspend |
| 恢复对象 | continuation/state machine | virtual thread continuation |
| 典型问题 | 函数颜色、scope管理、dispatcher选择 | pinning、carrier阻塞、ThreadLocal滥用 |

Kotlin官方文档把coroutine称为可挂起的计算, 并说明它可以挂起而不阻塞线程, 之后在同一或不同线程恢复。参考 [Kotlin Coroutines basics](https://kotlinlang.org/docs/coroutines-basics.html)。

所以:

```text
Kotlin coroutine:
    编译器+库把异步控制流显式建模

Java virtual thread:
    JDK运行时把阻塞控制流隐式挂起/恢复
```

## 12. 和fiber/纤程对比

fiber通常泛指用户态轻量执行单元, 经常和“有栈协程”接近。

可以这样理解:

```text
fiber:
    用户态调度
    有自己的执行上下文/栈
    可以显式yield/resume

virtual thread:
    JDK管理
    有continuation和可增长栈块
    阻塞点由JDK库/VM协作触发yield
```

区别:

| 对比项 | fiber/纤程 | Java virtual thread |
| :---: | :--- | :--- |
| 概念范围 | 泛称, 不同系统实现差异很大 | Java平台具体实现 |
| 调度控制 | 常见为显式yield/resume | 应用层不手动yield给调度器 |
| API形态 | 可能是独立fiber API | 仍然是 `Thread` |
| 工具支持 | 取决于运行时/框架 | JDK thread dump、JFR、debugger逐步支持 |

所以说virtual thread“像fiber”可以, 但它不是任意意义上的fiber。它更准确是Java平台上的有栈轻量线程实现。

## 13. 新版本JDK源码阅读路线

建议按下面顺序读:

```text
Thread.startVirtualThread
        |
        v
ThreadBuilders
        |
        v
VirtualThread构造
        |
        v
Continuation创建
        |
        v
submitRunContinuation
        |
        v
runContinuation
        |
        v
mount / cont.run / unmount
        |
        v
park / unpark / afterYield
```

源码入口:

| 关注点 | 文件 | 重点标识 |
| :---: | :--- | :--- |
| 创建virtual thread | `ThreadBuilders.java` | `OfVirtual`、`start` |
| virtual thread状态机 | `VirtualThread.java` | `NEW`、`RUNNING`、`PARKED`、`PINNED`、`TERMINATED` |
| 默认scheduler | `VirtualThread.java` | `DEFAULT_SCHEDULER`、`createDefaultScheduler` |
| continuation | `Continuation.java` | `yield`、`run`、pinned reason |
| mount/unmount | `VirtualThread.java` | `mount`、`unmount`、`runContinuation` |
| park/unpark | `VirtualThread.java` | `park`、`parkNanos`、`unpark` |
| 网络IO唤醒 | `sun.nio.ch` | `Poller`、平台epoll/kqueue/wepoll实现 |

读源码时可以带着这几个问题:

1. virtual thread什么时候进入 `RUNNING`?
2. 什么情况下 `yieldContinuation()` 成功?
3. 什么情况下退化成 `PINNED`?
4. `unpark()` 怎么把virtual thread重新提交给scheduler?
5. 网络IO里 fd 和 virtual thread 的映射在哪里维护?
6. timeout task 怎么防止唤醒旧的一次等待?

不要只看 `Thread` API。真正的设计重点在 `VirtualThread` 的状态机、`Continuation` 和 `sun.nio.ch` 的poller协作。

## 14. 这套设计的本质

从应用层看:

```text
同步阻塞代码
```

从运行时看:

```text
异步事件注册
        +
continuation保存
        +
carrier复用
        +
事件完成后重新调度
```

从设计目标看:

```text
让Thread-per-request模型重新变得可扩展
```

它不是替代所有线程池, 也不是让CPU计算变快。它主要解决:

```text
大量任务阻塞等待IO时
平台线程被白白占住的问题
```

如果任务是CPU密集型, 仍然应该控制并行度。如果资源有限, 仍然应该用连接池、`Semaphore`、限流器控制资源并发。virtual thread解决的是执行模型, 不是资源无限化。

## 15. 极简源码对应

下面不是完整源码摘录, 而是把OpenJDK源码里的关键形态压缩成便于记忆的伪代码。

`VirtualThread` 里有几个核心成员:

```java
Continuation cont;
Runnable runContinuation;
Thread carrierThread;
```

执行时大概是:

```java
mount();
cont.run();
unmount();
```

挂起时大概是:

```java
yieldContinuation();
```

重新调度时大概是:

```java
submitRunContinuation();
```

所以可以把源码理解成:

```text
VirtualThread:
    保存状态机
    保存continuation
    知道自己的scheduler
    mount到carrier上运行
    yield时卸载
    unpark时重新提交runContinuation
```

这比“线程池跑任务”多出来的关键能力是 `Continuation`: 它让JDK可以恢复原来的执行栈, 而不是只调用一个新的callback。

## 16. 参考资料

* [OpenJDK VirtualThread.java](https://github.com/openjdk/jdk/blob/master/src/java.base/share/classes/java/lang/VirtualThread.java)
* [OpenJDK Continuation.java](https://github.com/openjdk/jdk/blob/master/src/java.base/share/classes/jdk/internal/vm/Continuation.java)
* [OpenJDK ThreadBuilders.java](https://github.com/openjdk/jdk/blob/master/src/java.base/share/classes/java/lang/ThreadBuilders.java)
* [OpenJDK JEP 444 - Virtual Threads](https://openjdk.org/jeps/444)
* [OpenJDK JEP 491 - Synchronize Virtual Threads without Pinning](https://openjdk.org/jeps/491)
* [Inside Java - Networking I/O with Virtual Threads](https://inside.java/2021/05/10/networking-io-with-virtual-threads/)
* [Go Specification - Go statements](https://go.dev/ref/spec#Go_statements)
* [Go Wiki - Go for C++ Programmers](https://tip.golang.org/wiki/GoForCPPProgrammers)
* [Kotlin Coroutines basics](https://kotlinlang.org/docs/coroutines-basics.html)
