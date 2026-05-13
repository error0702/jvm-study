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

## 3. 为什么不是任何位置都能停

理解safepoint之前, 先要理解一个关键问题: JVM为什么不能在任意机器指令处把Java线程停下来?

原因是JVM在做GC、栈遍历、反优化时, 需要解释当前线程的执行现场。比如:

1. 当前PC对应的是哪个Java方法、哪个字节码位置。
2. 当前栈帧里哪些slot是对象引用, 哪些只是普通整数。
3. 当前寄存器里哪些值是oop。
4. 当前compiled frame能不能还原成解释器frame。
5. 当前线程的frame anchor是否可遍历。

这些信息不是每条机器指令都有。HotSpot会在一些特定位置保存或能推导出这些信息, 这些位置通常就可以作为safepoint。

可以先这样理解:

```text
普通机器指令位置:
    JVM未必知道所有寄存器/栈slot的含义
    不适合GC根扫描或反优化

safepoint位置:
    JVM能通过OopMap、frame信息、pc位置解释线程现场
    可以安全做栈遍历、GC、反优化等操作
```

### 3.1 OopMap的作用

`OopMap` 可以先理解为“这条执行位置上, 哪些寄存器/栈位置保存的是对象引用”的地图。

GC扫描线程栈时, 不能把栈上的所有值都当成对象地址。比如一个 `int` 值刚好看起来像某个堆地址, 如果误认为oop, GC就会出错。所以HotSpot需要准确知道哪些位置是oop。

编译器在生成代码时, 会在safepoint位置关联对应的 `OopMap`。这样当线程停在safepoint时, GC就能沿着OopMap找到根引用。

### 3.2 frame anchor的作用

线程从Java进入VM或native时, HotSpot需要记录最后一个Java frame的位置, 这个信息通常和 `JavaFrameAnchor` 相关。

它的意义是:

```text
Java代码
  |
  v
进入VM/native
  |
  v
记录last Java frame
  |
  v
GC/栈遍历时仍然能从这个anchor找到Java栈
```

所以native线程是否能被视为safepoint-safe, 也会看它是否有可遍历的Java frame anchor。不是所有“人在native里”的情况都可以无脑处理, 关键是VM能不能正确找到并解释它离开Java时的现场。

## 4. 源码入口

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
SafepointSynchronize::arm_safepoint()
ThreadSafepointState::examine_state_of_thread()
VMThread::inner_execute()
VM_Operation::evaluate_at_safepoint()
```

源码里的核心语义比较直接: `SafepointSynchronize::begin()` 会把线程推进到safepoint并暂停; `SafepointSynchronize::end()` 再恢复线程。

可以先记住几个核心对象:

| 对象/结构 | 作用 |
| :---: | :--- |
| `SafepointSynchronize` | safepoint全局协调入口 |
| `ThreadSafepointState` | 每个JavaThread对应的safepoint状态记录 |
| `SafepointMechanism` | safepoint poll的抽象, 新版JDK更明显 |
| `VMThread` | 执行VM operation并协调全局safepoint |
| `VM_Operation` | 需要VMThread执行的操作, 很多默认在safepoint中执行 |
| `Threads_lock` | 进入safepoint时用于稳定线程集合 |

源码阅读顺序可以按这条线走:

```text
VMThread::inner_execute()
        |
        v
VM_Operation::evaluate_at_safepoint()
        |
        v
SafepointSynchronize::begin()
        |
        v
ThreadSafepointState::examine_state_of_thread()
        |
        v
SafepointSynchronize::block()
        |
        v
SafepointSynchronize::end()
```

如果只看 `safepoint.cpp`, 容易以为所有线程都是在同一个函数里被停住的。实际上源码会分散在三类地方:

1. VMThread发起和结束全局safepoint。
2. JavaThread自己在poll点进入 `block()`。
3. 状态转换代码在“返回Java之前”补一次检查。

## 5. 执行流程

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

### 5.1 begin阶段大概做什么

`SafepointSynchronize::begin()` 可以粗略拆成下面几步:

1. VMThread准备进入safepoint, 检查当前确实不是已经处于safepoint。
2. 获取 `Threads_lock`, 稳定当前Java线程集合, 避免线程正在启动/退出时集合变化。
3. 修改全局safepoint状态, 通知运行中的线程需要响应safepoint。
4. arm safepoint poll, 让解释器/编译代码/状态转换路径能观察到safepoint请求。
5. 遍历JavaThread, 根据每个线程当前状态判断它是否已经safepoint-safe。
6. 对仍在Java中运行的线程等待其自我阻塞。
7. 所有相关线程都进入安全状态后, VMThread开始执行真正的VM operation。

其中最关键的是第5步: HotSpot不是只看Java层 `Thread.State`, 而是看内部线程状态和frame anchor等信息。

### 5.2 end阶段大概做什么

`SafepointSynchronize::end()` 做的是反向恢复:

1. 执行safepoint期间的清理任务。
2. disarm safepoint poll, 让线程之后不再因为这次safepoint请求阻塞。
3. 释放等待在safepoint barrier上的Java线程。
4. 清理本次safepoint统计信息。
5. 释放 `Threads_lock`, 允许线程集合继续变化。

也就是说, safepoint不是只有“停住”这一个动作。它还包括发起、统计、poll布防、线程汇合、VM操作、清理、恢复这一整套协议。

## 6. Safepoint poll机制

Java线程能够响应safepoint, 是因为HotSpot在它会经过的位置插入了检查。

常见检查位置:

1. 解释器执行字节码时的分支、方法返回等位置。
2. JIT编译后的代码中的safepoint poll。
3. 从native/VM/blocked状态返回Java之前的状态转换路径。
4. 某些运行时调用、异常处理、反优化入口。

### 6.1 编译代码里的poll

编译后的Java代码一般不会每条机器指令都检查safepoint。HotSpot会在相对合适的位置放poll, 典型位置包括:

1. 方法返回前后
2. 循环回边
3. 运行时调用附近
4. 可能需要GC map的安全位置

为什么循环回边重要?

```java
while (true) {
    i++;
}
```

如果一个长循环里没有合适的safepoint poll, JVM发起safepoint后, 这个线程可能长时间不响应。现代HotSpot会尽量在循环里放poll, 但如果编译优化把某些检查消掉或生成了很特殊的代码, 仍然可能出现 `time to safepoint` 异常。

### 6.2 polling page / trap

HotSpot的一种实现方式是使用 polling page。

大概思路:

```text
平时:
compiled code 读取 polling page
        |
        v
页面可读, 什么都不发生

发起safepoint:
VMThread把polling page设置成不可访问
        |
        v
compiled code 再读polling page时触发page fault/trap
        |
        v
HotSpot异常处理逻辑识别这是safepoint poll
        |
        v
当前JavaThread进入SafepointSynchronize::block()
```

这里要特别注意: 这个page fault/trap只是HotSpot实现poll的一种手段。它看起来会经过OS异常处理路径, 但语义上仍然是JVM safepoint协议, 不是操作系统中断在“主动帮JVM停线程”。

新版JDK里 `SafepointMechanism` 把poll机制抽象得更清楚, 可以有global poll、thread-local poll等实现差异。学习时先抓住主线: 线程会在poll点观察到safepoint请求, 然后自己进入阻塞。

### 6.3 三类poll入口不要混在一起

学习时容易把所有poll都想成“编译代码读polling page”。这个理解不完整。HotSpot让线程响应safepoint大致有三条入口:

| 入口 | 大概位置 | 解决什么问题 |
| :---: | :--- | :--- |
| 解释器检查 | 解释器执行分支、返回、调用等位置 | 解释执行的Java线程也要能响应safepoint |
| compiled poll | JIT编译代码中的poll点, 常见于循环回边、返回点 | 编译执行的Java线程需要在OopMap完整的位置停下 |
| state transition check | native/VM/blocked返回Java之前 | 已经离开Java执行流的线程不能在safepoint期间直接回到Java |

所以“某个线程为什么没响应safepoint”要先判断它卡在哪一类路径上:

1. 如果它在Java编译代码里跑, 重点看有没有机会执行到compiled poll。
2. 如果它在解释器里跑, 重点看解释器检查点是否能被执行到。
3. 如果它在native/blocked里, 重点不是让它立刻poll, 而是保证它返回Java之前被挡住。

### 6.4 新老版本SafepointMechanism的差异

如果以OpenJDK 8和JDK 10+做一个粗略分界, safepoint机制有一个比较关键的变化: JEP 312 引入了 Thread-Local Handshakes。

OpenJDK 8里主要是全局safepoint思路:

```text
VMThread发起全局safepoint
        |
        v
所有相关JavaThread都需要进入safepoint-safe状态
        |
        v
VMThread执行全局VM_Operation
        |
        v
所有线程一起恢复
```

JDK 10+ 引入 thread-local handshake 后, JVM可以在不进入全局safepoint的情况下, 让某个线程或一批线程执行一个回调:

```text
发起handshake
        |
        v
只要求目标JavaThread响应poll/状态转换
        |
        v
目标线程执行handshake closure
        |
        v
其它无关JavaThread可以继续运行
```

二者关系可以这样理解:

| 对比项 | OpenJDK 8 全局safepoint为主 | JDK 10+ thread-local handshake |
| :---: | :--- | :--- |
| 停顿范围 | 通常要求所有Java线程进入安全状态 | 可以只要求目标线程响应 |
| 典型用途 | STW GC、全局栈遍历、类卸载、反优化等 | 单线程/局部线程操作, 降低全局停顿影响 |
| poll形式 | 以全局poll/page armed思路为主 | 支持thread-local poll/handshake请求 |
| VM操作 | 很多 `VM_Operation` 默认 `evaluate_at_safepoint() == true` | 新增不需要全局safepoint的handshake类操作 |
| 目标 | 获得整个Java世界的一致视图 | 在目标线程上执行安全回调, 避免无关线程停顿 |

需要注意:

1. Thread-local handshake 不是替代所有全局safepoint。STW GC这类需要全局一致性的操作仍然需要全局safepoint。
2. 它也不是Java层API, 普通业务代码不能直接调用它来“暂停某个线程”。
3. 它的意义是降低一些VM内部操作的停顿范围, 尤其是原本没必要停全世界的操作。
4. 从学习角度看, 老版本先理解 `SafepointSynchronize`; 新版本再看 `SafepointMechanism` 和 handshake 相关代码会更顺。

JDK 10 当时还增加过 `ThreadLocalHandshakes` 选项, 用来控制是否使用JEP 312引入的新机制。官方CSR里描述它的含义是使用 thread-local polls 替代 global poll 来支持safepoint/handshake机制。后续JDK版本中实现细节继续演进, 所以读源码时一定要先确认自己看的JDK版本。

### 6.5 除了SafepointMechanism, JDK 8之后还有哪些相关变化

从JDK 8看到更高版本时, 不要只找 `SafepointMechanism` 这个类。Safepoint相关变化分散在runtime、GC、logging、synchronization、serviceability等模块里。

可以按版本粗略看:

| 版本 | JEP或变更 | 主要变化 | 和safepoint的关系 |
| :---: | :--- | :--- | :--- |
| JDK 8 | 基线版本 | 全局safepoint是主要理解模型; 常见背景包括Parallel GC、CMS、G1、偏向锁、`PrintSafepointStatistics` | 学习源码时重点看 `SafepointSynchronize`、`VMThread`、`ThreadSafepointState`; 日志和停顿来源也多以JDK 8参数和GC组合为背景 |
| JDK 9 | [JEP 158: Unified JVM Logging](https://openjdk.org/jeps/158) | JVM日志统一到 `-Xlog` 框架, 引入tag、level、decorator | safepoint观察方式从零散 `PrintXXX` 参数逐步转向 `-Xlog:safepoint` |
| JDK 9 | [JEP 271: Unified GC Logging](https://openjdk.org/jeps/271) | GC日志迁移到统一日志框架 | 分析停顿时通常组合看 `-Xlog:safepoint` 和 `-Xlog:gc` |
| JDK 9 | [JEP 248: Make G1 the Default Garbage Collector](https://openjdk.org/jeps/248) | Server默认GC从Parallel GC切到G1 | 默认GC变了, safepoint日志里常见GC阶段、停顿形态、调优入口也会变 |
| JDK 10 | [JEP 312: Thread-Local Handshakes](https://openjdk.org/jeps/312) | 支持不进入全局safepoint, 只让目标JavaThread执行回调 | 一些单线程/局部线程操作可以缩小停顿范围; 这是新版和JDK 8的重要区别 |
| JDK 11 | [JEP 333: ZGC Experimental](https://openjdk.org/jeps/333) | 引入实验版ZGC, 目标是低延迟、大堆、并发整理 | safepoint仍存在, 但低延迟GC开始把更多GC工作并发化, 减少STW阶段负担 |
| JDK 12 | [JEP 189: Shenandoah Experimental](https://openjdk.org/jeps/189) | 引入实验版Shenandoah低暂停GC | 和ZGC类似, 重点是把标记、转移等工作尽量并发化, 降低safepoint停顿压力 |
| JDK 14 | [JEP 363: Remove CMS](https://openjdk.org/jeps/363) | CMS被移除 | JDK 8里常见的CMS相关停顿和参数在新版中不再适用 |
| JDK 15 | [JEP 374: Deprecate and Disable Biased Locking](https://openjdk.org/jeps/374) | 偏向锁默认禁用, 相关参数废弃 | JDK 8里偏向锁撤销可能触发safepoint; 新版本默认减少这类来源 |
| JDK 15 | [JEP 377: ZGC Production](https://openjdk.org/jeps/377) | ZGC转正 | 低延迟GC成为生产可选项, 分析safepoint时要结合ZGC自己的并发阶段和短暂停顿 |
| JDK 15 | [JEP 379: Shenandoah Production](https://openjdk.org/jeps/379) | Shenandoah转正 | 同样强调低暂停, 不能套用JDK 8 CMS/Parallel的停顿直觉 |
| JDK 16 | [JEP 376: ZGC Concurrent Thread-Stack Processing](https://openjdk.org/jeps/376) | ZGC把线程栈处理从safepoint移到并发阶段 | 这是“减少safepoint里必须做的事”的典型例子 |
| JDK 17 | LTS整合版本 | G1、ZGC、Shenandoah、JFR、handshake等能力在LTS中更常见 | 排查时通常不再只按JDK 8经验看 `PrintXXX`、CMS、偏向锁 |
| JDK 21 | [JEP 444: Virtual Threads](https://openjdk.org/jeps/444) | Virtual Threads正式引入 | Java层线程和OS线程不再能简单一一对应; 线程dump、调度、safepoint成本都要区分virtual/platform/carrier thread |

这些变化可以分成几类理解。

#### 6.5.1 日志体系变化: 从PrintXXX到Xlog

JDK 8里常见写法是:

```shell
-XX:+PrintSafepointStatistics
-XX:+PrintGCApplicationStoppedTime
-XX:+PrintGCDetails
```

JDK 9之后更多使用统一日志:

```shell
-Xlog:safepoint=info
-Xlog:safepoint*=debug
-Xlog:gc*
```

这不只是参数名字变化。统一日志引入tag、level、decorator之后, 同一个问题可以用更细的tag组合观察。比如safepoint只看 `safepoint` tag, GC只看 `gc` tag, 或者把二者结合起来看停顿来源。

所以读老文章时要注意:

1. JDK 8文章里的 `PrintGCApplicationStoppedTime` 大多对应“应用因为VM操作暂停了多久”的观察口径。
2. JDK 9+里要看 `-Xlog:safepoint` 和 `-Xlog:gc` 的组合。
3. 字段名和格式可能不同, 但仍然要分清 `sync`、`cleanup`、`vmop` 这几个阶段。

#### 6.5.2 GC变化: safepoint还在, 但停顿边界变了

JDK 8的常见组合是 Parallel GC、CMS、G1。后续版本中:

1. JDK 9开始G1成为Server默认GC。
2. JDK 14移除了CMS。
3. ZGC、Shenandoah这类低延迟GC逐渐成熟。
4. ZGC在JDK 16引入并发线程栈处理, 把原来需要在safepoint里做的线程栈处理挪到并发阶段。

这会影响你看safepoint的方式:

```text
JDK 8:
    很多场景仍然容易从“STW GC / CMS / Parallel / G1”角度看停顿

更高版本:
    需要区分GC是否把部分root扫描、标记、转移、栈处理并发化
    safepoint仍然存在, 但GC设计会努力缩短必须STW的部分
```

不要误解成“低延迟GC没有safepoint”。更准确是: 低延迟GC尽量把原本必须停全世界的工作拆出去, 让safepoint阶段只保留更小、更必要的工作。

#### 6.5.3 偏向锁变化: JDK 8里常见, 新版本默认淡出

JDK 8里偏向锁是一个需要关注的safepoint来源。偏向锁撤销、批量重偏向、批量撤销都可能和safepoint有关。

在JDK 15里, 偏向锁默认禁用并废弃相关命令行选项。这个变化的影响是:

1. 老版本里 `RevokeBias`、bulk rebias、bulk revoke 这类停顿更常见。
2. 新版本默认不再启用偏向锁, 相关safepoint来源减少。
3. 学习对象头、锁升级、monitor时, 需要标注自己讨论的是JDK 8语境还是更高版本语境。

也就是说, 如果你在JDK 8文档里写“偏向锁撤销会触发safepoint”, 这句话在JDK 8语境下没问题; 但写成“现代JDK一定会频繁因为偏向锁撤销进入safepoint”就不准确。

#### 6.5.4 Serviceability变化: 工具观察方式越来越细

JDK 8里排查常用 `jstack`、`jcmd`、GC日志、safepoint统计。更高版本中, JFR、统一日志、jcmd动态日志能力更强。

这带来两个变化:

1. 观察safepoint不一定只靠启动参数, 可以运行中用 `jcmd VM.log` 打开日志。
2. 一些原本依赖全局safepoint的服务性操作, 在handshake机制之后可以缩小影响范围。

但要注意: thread dump、heap dump、JVMTI、JFR事件采样的具体实现会随版本变化。学习时最好把结论写成:

> 某些服务性操作可能触发全局safepoint, 新版本也可能用handshake或其它机制缩小停顿范围。具体要看JDK版本和对应源码。

不要写成“所有jstack都一定全局safepoint”或者“新版本jstack完全不会safepoint”。

#### 6.5.5 Virtual Threads带来的理解差异

JDK 21正式引入Virtual Threads后, Java层看到的 `Thread` 数量可能远大于平台线程数量。它和safepoint的关系可以先这样理解:

1. virtual thread是Java层轻量线程, 不等于一个固定OS线程。
2. 真正在CPU上执行Java代码时, virtual thread会挂载到carrier/platform thread上。
3. safepoint协调仍然是HotSpot运行时层面的事情, 不能简单按“Java层有多少个Thread对象”理解成本。
4. 线程dump、栈追踪、阻塞原因分析时, 要区分virtual thread、carrier thread、platform thread。

所以JDK 8里的“一条Java Thread基本对应一条OS thread”的直觉, 到JDK 21虚拟线程场景下就不够用了。分析safepoint、线程dump和调度时, 需要额外问一句: 我现在看到的是virtual thread的业务栈, 还是carrier/platform thread的执行实体?

#### 6.5.6 总结成一句话

JDK 8之后的变化可以这样记:

```text
JDK 8:
    全局safepoint是主线
    日志以PrintXXX参数为主
    Parallel/CMS/G1、偏向锁都是常见分析背景

JDK 9+:
    统一日志改变观察方式
    G1默认改变GC停顿背景

JDK 10+:
    thread-local handshake降低部分操作的全局停顿范围

JDK 14/15+:
    CMS移除、偏向锁默认禁用, 老版本常见停顿来源发生变化

JDK 16+:
    ZGC等低延迟GC继续把部分工作从safepoint移到并发阶段

JDK 21+:
    virtual thread让“Java线程”和“OS线程”的对应关系更不能简单等同
```

## 7. 哪些线程需要响应poll是怎么维护的

这个问题是safepoint的核心: JVM发起safepoint时, 并不是“所有线程凭空知道要停”。HotSpot会维护一组Java线程, 给需要响应的线程布防poll, 再根据每个线程的状态判断它是否已经安全。

### 7.1 线程集合在哪里维护

HotSpot内部有全局线程列表, Java线程创建、启动、退出时会加入或移出这个集合。进入safepoint时, VMThread会拿 `Threads_lock` 来稳定这组线程。

可以先这样理解:

```text
JavaThread创建
        |
        v
加入Threads全局线程列表
        |
        v
VMThread发起safepoint
        |
        v
拿Threads_lock稳定线程集合
        |
        v
遍历当前JavaThread列表
        |
        v
给需要响应的线程设置poll/检查状态
```

为什么要拿 `Threads_lock`?

因为safepoint要求VMThread知道“这次我要等哪些线程”。如果一边遍历线程列表, 一边有线程启动或退出, 线程集合就不稳定。`Threads_lock` 的作用就是让这次safepoint看到一个稳定的参与者集合。

### 7.2 每个线程自己的Safepoint状态

除了全局线程列表, 每个 `JavaThread` 还会有关联的safepoint状态, 常见名字是 `ThreadSafepointState`。

它大概负责记录:

1. 当前线程是否已经到达safepoint-safe状态。
2. 当前线程是否仍在Java里运行, 需要继续等待。
3. 当前线程是否已经阻塞、native、退出等, 可以认为安全。
4. 当前线程是否需要在poll点或状态转换点处理safepoint请求。

所以VMThread不是只靠一个全局标志判断全部线程, 而是:

```text
全局safepoint状态
        +
每个JavaThread自己的ThreadSafepointState
        +
JavaThread当前_thread_state
        +
frame anchor / 是否可遍历
```

一起决定“这个线程现在算不算safe”。

### 7.3 发起时如何给线程布防poll

发起safepoint时, VMThread会把全局safepoint状态切到同步阶段, 然后arm poll。

老版本可以先理解成“全局poll布防”:

```text
VMThread设置全局safepoint请求
        |
        v
让polling page / poll标志进入armed状态
        |
        v
所有正在Java里跑的线程经过poll点时都会看到请求
```

新版JDK里 `SafepointMechanism` 引入了更清楚的抽象, 可以支持thread-local poll:

```text
全局safepoint:
    arm所有相关JavaThread的poll

thread-local handshake:
    只arm目标JavaThread的poll
```

这就是新老机制的关键差异之一: 新版可以只让目标线程响应, 不一定要让所有Java线程都进全局safepoint。

### 7.4 哪些线程必须等, 哪些线程不用等

VMThread遍历JavaThread时, 会按线程状态分类处理:

| 线程状态 | 是否需要继续等它响应poll | 原因 |
| :---: | :---: | :--- |
| `_thread_in_Java` | 通常需要 | 正在执行Java代码, 必须跑到poll点停下 |
| `_thread_in_vm` | 视情况 | 正在VM内部, 需要通过VM路径/状态转换保证安全 |
| `_thread_in_native` | 通常不等它立即block | 不在执行Java bytecode, 返回Java前会检查 |
| `_thread_blocked` | 通常不等它主动poll | 已阻塞, 唤醒/返回前会检查 |
| 正在启动/退出 | 通过线程列表和锁协调 | 避免半初始化或已退出线程参与错误统计 |

所以“需要响应poll”的主要是正在Java执行流中的线程。已经在native或blocked里的线程, 通常可以被认为处于safepoint-safe状态, 但它们之后不能绕过检查直接回到Java继续跑。

### 7.5 线程如何从“需要等待”变成“已安全”

一个 `_thread_in_Java` 线程响应safepoint大概是这样:

```text
JavaThread继续执行
        |
        v
经过解释器检查点 / compiled safepoint poll
        |
        v
发现poll armed或全局safepoint请求
        |
        v
进入SafepointSynchronize::block()
        |
        v
更新自己的ThreadSafepointState
        |
        v
通知/唤醒VMThread: 我已经safe
        |
        v
等待safepoint结束
```

VMThread这边会持续检查还剩多少线程没有进入safe状态。等计数归零或全部线程都被判断为safe后, 才能执行真正的VM operation。

### 7.6 native/blocked线程为什么返回前要检查

如果一个线程在safepoint发起时已经是native或blocked, VMThread通常不会等它跑到Java层poll点。问题是: safepoint还没结束时, 它可能被唤醒或从native返回。

所以状态转换路径必须检查:

```text
native / blocked
        |
        v
准备返回Java
        |
        v
检查是否有safepoint/handshake请求
        |
        v
如果有, 先阻塞等待
        |
        v
safepoint结束后再进入Java执行
```

这就是为什么safepoint不仅靠编译代码里的poll, 还要靠线程状态转换检查。否则native线程可能在全局safepoint期间突然回到Java继续执行, 破坏VM操作需要的稳定视图。

### 7.7 一句话总结

哪些线程该响应poll, 不是由Java层 `Thread` 对象决定的, 而是由HotSpot内部三类信息共同维护:

1. `Threads` 全局JavaThread列表: 决定这次safepoint涉及哪些线程。
2. `JavaThread` 当前 `_thread_state`: 决定线程现在是否需要主动poll。
3. `ThreadSafepointState` / `SafepointMechanism`: 记录线程是否已safe, 以及poll请求是否armed。

全局safepoint时, 目标通常是让所有相关JavaThread进入safe状态; thread-local handshake时, 则可以只让目标线程响应。

### 7.8 维护关系按时间线再串一次

把上面的机制串成一条时间线, 会更容易理解“哪些线程该响应poll是怎么维护的”:

```text
线程创建:
    JavaThread加入Threads全局列表
    初始化线程自己的状态和safepoint状态

线程运行:
    _thread_state不断在Java / VM / native / blocked之间转换
    编译器/解释器/状态转换路径保留safepoint检查点

发起全局safepoint:
    VMThread稳定Threads列表
    设置全局同步状态
    arm全局poll
    遍历JavaThread并记录哪些线程仍需等待

线程响应:
    in Java的线程在poll点进入block
    native/blocked线程返回Java前检查并等待
    VMThread看到所有目标线程都safe

结束safepoint:
    VMThread disarm poll
    唤醒等待线程
    线程恢复原执行流
```

这里有两个容易忽略的点:

1. 线程是否参与这次safepoint, 由VMThread看到的JavaThread集合决定。
2. 线程是否必须主动跑到poll点, 由它当时的内部状态决定。

## 8. 线程状态和Safepoint的关系

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

### 8.1 不同状态怎么处理

可以先按下面这张表理解:

| 当前状态 | VMThread通常怎么处理 | 关键点 |
| :---: | :--- | :--- |
| `_thread_in_Java` | 等它运行到poll点自我阻塞 | 最典型的 `time to safepoint` 来源 |
| `_thread_in_vm` | 要求它回调或在VM路径中检查 | VM代码要遵守safepoint规则 |
| `_thread_in_native` | 通常视为safe, 返回Java前再检查 | native不继续执行Java bytecode |
| `_thread_blocked` | 通常视为safe, 唤醒/返回前检查 | 不允许绕过safepoint继续跑Java |
| 新启动/退出中 | 通过 `Threads_lock` 协调 | 避免线程集合不稳定 |

所谓“safe”, 不是说线程消失了, 而是说它不会继续并发执行Java代码并改动JVM需要稳定观察的状态。

### 8.2 block到底做什么

线程响应safepoint时会进入 `SafepointSynchronize::block(JavaThread* thread)` 这类路径。可以粗略理解为:

1. 当前JavaThread发现safepoint请求。
2. 把自己的状态切换到blocked或对应的safepoint等待状态。
3. 通知VMThread: 我已经到达safepoint-safe状态。
4. 在barrier/monitor上等待。
5. VMThread执行完VM operation后唤醒它。
6. 线程恢复之前的执行状态, 继续跑Java代码。

这说明safepoint是协作式的: 线程不是被JVM随便打断后冻结, 而是在JVM安排的位置进入一段等待逻辑。

## 9. native状态和Safepoint

native状态不是“必须马上停住”, 而是HotSpot特别处理的一种safepoint-safe状态。

线程运行在普通native code时, VMThread看到它在native, 通常不等待它block。原因是它暂时不在执行Java bytecode, 不会继续改动Java执行栈帧。但这个线程从native返回Java前必须检查safepoint状态, 必要时阻塞等待。

可以先这样理解:

1. native线程不会继续执行Java bytecode, 所以通常可被VM视为safepoint-safe。
2. native线程回到Java前, 要通过状态切换检查safepoint请求。
3. JNI critical region 是特殊情况。

JNI官方规范要求 `GetPrimitiveArrayCritical` / `ReleasePrimitiveArrayCritical` 之间的native代码不能长时间运行、不能阻塞、不能随意JNI调用。因为VM可能为了这类critical区域临时限制GC移动对象。

所以不要简单说“native一定会卡住safepoint”, 更准确是:

> 普通native执行通常可以被JVM视为安全状态; 但JNI critical、长时间不返回或持有VM相关资源的native逻辑, 仍然可能影响GC或其它VM操作。

### 9.1 为什么JNI critical特殊

JNI提供了 `GetPrimitiveArrayCritical` 这类API, 允许native代码尽量直接访问Java数组底层数据。为了做到这一点, VM可能需要临时限制GC移动这个数组。

所以JNI规范要求critical区域内的代码要非常短:

```c++
jint* p = env->GetPrimitiveArrayCritical(array, NULL);
// 这里不要长时间阻塞, 不要做复杂JNI调用
env->ReleasePrimitiveArrayCritical(array, p, 0);
```

如果critical区域太长, 可能导致GC等待对象移动条件满足, 进而拖慢某些VM操作。这个问题不是“native状态天然阻塞safepoint”, 而是“这段native持有了VM需要特别对待的资源”。

### 9.2 Java/native边界为什么重要

Safepoint依赖“线程状态是可信的”。所以HotSpot在Java、VM、native之间切换时, 会通过一批状态转换辅助类维护线程状态。

可以简化理解为:

```text
Java -> VM/native:
    先把JavaThread状态从_thread_in_Java切到对应状态
    保存必要的last Java frame / frame anchor
    之后VMThread可以把它按native或VM状态分类

VM/native -> Java:
    准备恢复Java执行前, 先检查safepoint/handshake
    如果此时全局safepoint未结束, 不能直接回Java
    必须先等待, 再恢复Java执行
```

这层边界是safepoint正确性的兜底。否则一个线程在safepoint开始时处于native, VMThread认为它暂时安全; 但它马上返回Java继续修改对象图或栈状态, 就会破坏GC、栈遍历或反优化需要的一致视图。

## 10. 常见触发场景

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

### 10.1 VM_Operation和Safepoint的关系

HotSpot里很多全局动作会包装成 `VM_Operation` 交给 `VMThread` 执行。`VM_Operation::evaluate_at_safepoint()` 默认倾向于在safepoint中执行, 具体操作可以覆盖这个行为。

可以先这么理解:

```text
业务线程/工具线程/GC逻辑
        |
        v
创建VM_Operation
        |
        v
提交给VMThread
        |
        v
VMThread判断是否需要safepoint
        |
        v
需要: begin safepoint -> evaluate operation -> end safepoint
不需要: 直接evaluate operation
```

所以排查safepoint日志里的 `vmop` 字段时, 本质上是在看“这次全局停顿是哪个VM_Operation触发的”。

### 10.2 几条典型链路

#### 10.2.1 `System.gc()` 到 safepoint

以HotSpot为例, `System.gc()` 不是语言层面保证马上GC, 但在默认实现里通常会走到VM内部的GC请求。大致链路可以先这样理解:

```text
Java: System.gc()
        |
        v
JVM native入口
        |
        v
Universe::heap()->collect(...)
        |
        v
创建/提交GC相关VM_Operation
        |
        v
VMThread执行该VM_Operation
        |
        v
需要全局一致视图 -> 进入safepoint
        |
        v
执行STW阶段GC
```

这里重点不是 `System.gc()` 本身, 而是: 只要某个VM操作需要稳定遍历Java堆和线程栈, 就可能通过VMThread协调safepoint。

#### 10.2.2 `jstack` / `jcmd Thread.print` 到 safepoint

线程栈遍历也可能需要safepoint。因为要读取多个Java线程的栈帧, JVM需要这些栈帧处在可解释状态。

大致链路:

```text
jstack / jcmd Thread.print
        |
        v
Attach Listener / Diagnostic Command
        |
        v
创建ThreadDump或GetStackTrace相关VM_Operation
        |
        v
VMThread协调线程状态
        |
        v
遍历JavaThread栈帧
```

不同JDK版本和不同命令实现细节会有差异, 但核心点是: 栈遍历不是简单从OS层把线程栈内存扒出来就完事, JVM还要知道Java frame、method、bci、oop等运行时信息。

#### 10.2.3 反优化到 safepoint

JIT编译后的代码有时需要回退到解释执行, 这就是反优化。比如类层次变化、profile信息变化、某些乐观假设失效时, JVM需要把compiled frame还原成解释器能理解的frame。

大致链路:

```text
compiled code中的假设失效
        |
        v
触发deoptimization相关VM操作
        |
        v
需要稳定线程栈和compiled frame
        |
        v
进入safepoint或使用局部handshake
        |
        v
重建/修补frame, 回退到解释执行
```

这类操作解释了为什么safepoint不只和GC有关。GC最常见, 但不是唯一使用者。

### 10.3 不是所有VM操作都一样重

看到safepoint日志里的 `vmop` 时, 不要只看“发生了safepoint”, 还要看这次VM操作本身是什么。

可以粗略分成三类:

| 类型 | 例子 | 关注点 |
| :---: | :--- | :--- |
| GC类 | full GC、young GC的STW阶段、显式GC | `vmop`时间、GC日志、堆大小、晋升/压缩成本 |
| 诊断类 | thread dump、heap dump、JFR、JVMTI查询 | 是否被工具频繁触发, 是否在高峰期执行 |
| 运行时维护类 | deoptimization、class unloading、code cache清理、monitor deflation | 是否和编译、类加载、代码缓存压力相关 |

同样是safepoint, 如果 `sync` 很短而 `vmop` 很长, 说明线程很快停住了, 真正慢的是VM操作本身。如果 `sync` 很长, 才更像是某些线程迟迟没有到达安全状态。

## 11. 性能影响

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

### 11.1 日志指标怎么读

JDK8 `PrintSafepointStatistics` 输出里常见字段可以先这样理解:

| 字段 | 含义 |
| :---: | :--- |
| `vmop` | 触发本次safepoint的VM操作 |
| `total` | 参与统计的线程总数 |
| `initially_running` | safepoint开始时仍在运行、需要等待响应的线程数 |
| `wait_to_block` | 等待线程自我阻塞的数量 |
| `spin` | VMThread自旋等待线程到达safepoint的时间 |
| `block` | VMThread阻塞等待线程到达safepoint的时间 |
| `sync` | 让线程全部进入safepoint-safe状态的总时间 |
| `cleanup` | safepoint内部清理任务耗时 |
| `vmop` time | 真正执行VM操作的耗时 |

粗略判断:

1. `sync` 很长: 重点找哪个线程迟迟没到safepoint。
2. `vmop` time 很长: 重点看具体VM操作, 比如GC、heap dump、class redefine。
3. safepoint次数很多但每次很短: 关注触发频率, 看是否有工具、日志、诊断、显式GC在频繁制造VM operation。

### 11.2 sync长和vmop长的排查方向

可以按下面的方式先分流:

| 现象 | 优先怀疑 | 下一步看什么 |
| :---: | :--- | :--- |
| `sync` 长, `vmop` 短 | 线程迟迟没有进入safepoint | 线程CPU、是否长循环、native调用、OS调度 |
| `sync` 短, `vmop` 长 | VM操作本身慢 | GC日志、heap dump、class redefine、JFR/诊断命令 |
| 次数很多, 每次很短 | 触发频率过高 | 是否频繁 `System.gc()`、监控工具、JVMTI/JFR、偏向锁/诊断操作 |
| `cleanup` 长 | safepoint清理任务耗时 | 具体JDK日志字段、清理项、版本差异 |

一个常见误判是: 看到应用停顿就说“线程到不了safepoint”。实际上如果 `vmop` 很长, 线程可能早就都停好了, 慢的是GC、dump、类重定义等真正的VM操作。

### 11.3 常见慢Safepoint场景

1. 大循环迟迟不到poll点。
2. 线程没有被OS调度, 一直没机会执行poll。
3. JNI critical区域过长。
4. 大量线程导致safepoint协调成本上升。
5. CPU打满, VMThread抢不到CPU。
6. 容器CPU quota太小, 导致VMThread/GC/业务线程互相抢。
7. 诊断命令、heap dump、thread dump、JFR等工具操作本身比较重。

## 12. 排查命令

### 12.1 JDK 8

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

### 12.2 JDK 9+

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

## 13. Safepoint和OS中断的区别

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

### 13.1 和异常、信号、page fault的关系

这里再细拆一下:

| 名称 | 层次 | 和safepoint的关系 |
| :---: | :--- | :--- |
| OS中断/IRQ | 硬件/内核 | 和safepoint不是同一层机制 |
| page fault | CPU异常/OS处理 | HotSpot可能利用它实现polling page |
| Unix signal | OS进程信号 | HotSpot可能用于处理某些异常/信号, 但不是safepoint语义本身 |
| Java exception | Java语言/运行时 | 普通异常抛出不是safepoint |
| `Thread.interrupt()` | Java线程协作标记 | 不是safepoint, 也不会全局停顿 |

所以最准确的说法是:

> safepoint的语义属于JVM; 它的某些实现细节可能借助OS异常/信号机制, 但不能把safepoint等同于OS中断。

## 14. 几个容易误解的点

1. `RUNNABLE` 不等于正在CPU上执行。
2. STW不是“瞬间按暂停键”, 而是相关线程陆续到达safepoint或进入可认为安全的状态。
3. native里的线程不等于一定卡住safepoint。
4. `Thread.sleep()` / `yield()` 不是safepoint控制手段。
5. `Thread.interrupt()` 不是safepoint。
6. 看到page fault/trap不代表safepoint就是OS中断。
7. 并发GC不代表完全没有safepoint。
8. safepoint不是只服务GC, 反优化、栈遍历、类元数据维护等也会用到。
9. thread-local handshake不是“Java层暂停线程API”, 它是HotSpot内部减少全局停顿范围的机制。
10. `time to safepoint` 长不一定是VMThread慢, 也可能是目标JavaThread没有机会执行poll。

## 15. 总结

1. Safepoint是HotSpot为全局VM操作准备的安全停顿机制。
2. Java线程不是在任意机器指令处被JVM粗暴暂停, 而是在safepoint poll、状态切换等JVM可控位置停下。
3. `_thread_in_Java`、`_thread_in_native`、`_thread_blocked` 等内部状态会影响safepoint协调方式。
4. GC、栈遍历、类卸载、偏向锁撤销、反优化等都可能触发safepoint。
5. 新版 `SafepointMechanism` 和 thread-local handshake 的重点是缩小部分VM操作的停顿范围, 不是取消全局safepoint。
6. 分析safepoint性能时, 要区分 `time to safepoint`、`vm operation time` 和触发频率。
7. Safepoint属于JVM协作协议, OS中断属于操作系统/硬件机制, 二者不是一个层面的东西。

## 16. 参考资料

* [OpenJDK safepoint.cpp](https://github.com/openjdk/jdk/blob/master/src/hotspot/share/runtime/safepoint.cpp)
* [OpenJDK safepointMechanism.hpp](https://github.com/openjdk/jdk/blob/master/src/hotspot/share/runtime/safepointMechanism.hpp)
* [OpenJDK JEP 312 - Thread-Local Handshakes](https://bugs.openjdk.org/browse/JDK-8185640)
* [OpenJDK JEP 158 - Unified JVM Logging](https://openjdk.org/jeps/158)
* [OpenJDK JEP 189 - Shenandoah Low-Pause-Time GC Experimental](https://openjdk.org/jeps/189)
* [OpenJDK JEP 248 - Make G1 the Default Garbage Collector](https://openjdk.org/jeps/248)
* [OpenJDK JEP 271 - Unified GC Logging](https://openjdk.org/jeps/271)
* [OpenJDK JEP 333 - ZGC Experimental](https://openjdk.org/jeps/333)
* [OpenJDK JEP 363 - Remove the Concurrent Mark Sweep Garbage Collector](https://openjdk.org/jeps/363)
* [OpenJDK JEP 374 - Deprecate and Disable Biased Locking](https://openjdk.org/jeps/374)
* [OpenJDK JEP 376 - ZGC Concurrent Thread-Stack Processing](https://openjdk.org/jeps/376)
* [OpenJDK JEP 377 - ZGC Production](https://openjdk.org/jeps/377)
* [OpenJDK JEP 379 - Shenandoah Production](https://openjdk.org/jeps/379)
* [OpenJDK JEP 444 - Virtual Threads](https://openjdk.org/jeps/444)
* [OpenJDK CSR JDK-8189942 - ThreadLocalHandshakes option](https://bugs.openjdk.org/browse/JDK-8189942)
* [OpenJDK vmThread.cpp](https://github.com/openjdk/jdk/blob/master/src/hotspot/share/runtime/vmThread.cpp)
* [OpenJDK vmOperation.hpp](https://github.com/openjdk/jdk/blob/master/src/hotspot/share/runtime/vmOperation.hpp)
* [OpenJDK interfaceSupport.inline.hpp](https://github.com/openjdk/jdk/blob/master/src/hotspot/share/runtime/interfaceSupport.inline.hpp)
* [OpenJDK javaThread.hpp](https://github.com/openjdk/jdk/blob/master/src/hotspot/share/runtime/javaThread.hpp)
* [Oracle JNI Functions - GetPrimitiveArrayCritical](https://docs.oracle.com/en/java/javase/24/docs/specs/jni/functions.html)
* [Oracle JVM TI Specification](https://docs.oracle.com/en/java/javase/18/docs/specs/jvmti.html)
* [Linux Kernel Generic IRQ Handling](https://www.kernel.org/doc/html/v6.7/core-api/genericirq.html)
