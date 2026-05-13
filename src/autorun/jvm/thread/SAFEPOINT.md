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

### 4.1 begin阶段大概做什么

`SafepointSynchronize::begin()` 可以粗略拆成下面几步:

1. VMThread准备进入safepoint, 检查当前确实不是已经处于safepoint。
2. 获取 `Threads_lock`, 稳定当前Java线程集合, 避免线程正在启动/退出时集合变化。
3. 修改全局safepoint状态, 通知运行中的线程需要响应safepoint。
4. arm safepoint poll, 让解释器/编译代码/状态转换路径能观察到safepoint请求。
5. 遍历JavaThread, 根据每个线程当前状态判断它是否已经safepoint-safe。
6. 对仍在Java中运行的线程等待其自我阻塞。
7. 所有相关线程都进入安全状态后, VMThread开始执行真正的VM operation。

其中最关键的是第5步: HotSpot不是只看Java层 `Thread.State`, 而是看内部线程状态和frame anchor等信息。

### 4.2 end阶段大概做什么

`SafepointSynchronize::end()` 做的是反向恢复:

1. 执行safepoint期间的清理任务。
2. disarm safepoint poll, 让线程之后不再因为这次safepoint请求阻塞。
3. 释放等待在safepoint barrier上的Java线程。
4. 清理本次safepoint统计信息。
5. 释放 `Threads_lock`, 允许线程集合继续变化。

也就是说, safepoint不是只有“停住”这一个动作。它还包括发起、统计、poll布防、线程汇合、VM操作、清理、恢复这一整套协议。

## 5. Safepoint poll机制

Java线程能够响应safepoint, 是因为HotSpot在它会经过的位置插入了检查。

常见检查位置:

1. 解释器执行字节码时的分支、方法返回等位置。
2. JIT编译后的代码中的safepoint poll。
3. 从native/VM/blocked状态返回Java之前的状态转换路径。
4. 某些运行时调用、异常处理、反优化入口。

### 5.1 编译代码里的poll

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

### 5.2 polling page / trap

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

### 5.3 新老版本SafepointMechanism的差异

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

## 6. 哪些线程需要响应poll是怎么维护的

这个问题是safepoint的核心: JVM发起safepoint时, 并不是“所有线程凭空知道要停”。HotSpot会维护一组Java线程, 给需要响应的线程布防poll, 再根据每个线程的状态判断它是否已经安全。

### 6.1 线程集合在哪里维护

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

### 6.2 每个线程自己的Safepoint状态

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

### 6.3 发起时如何给线程布防poll

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

### 6.4 哪些线程必须等, 哪些线程不用等

VMThread遍历JavaThread时, 会按线程状态分类处理:

| 线程状态 | 是否需要继续等它响应poll | 原因 |
| :---: | :---: | :--- |
| `_thread_in_Java` | 通常需要 | 正在执行Java代码, 必须跑到poll点停下 |
| `_thread_in_vm` | 视情况 | 正在VM内部, 需要通过VM路径/状态转换保证安全 |
| `_thread_in_native` | 通常不等它立即block | 不在执行Java bytecode, 返回Java前会检查 |
| `_thread_blocked` | 通常不等它主动poll | 已阻塞, 唤醒/返回前会检查 |
| 正在启动/退出 | 通过线程列表和锁协调 | 避免半初始化或已退出线程参与错误统计 |

所以“需要响应poll”的主要是正在Java执行流中的线程。已经在native或blocked里的线程, 通常可以被认为处于safepoint-safe状态, 但它们之后不能绕过检查直接回到Java继续跑。

### 6.5 线程如何从“需要等待”变成“已安全”

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

### 6.6 native/blocked线程为什么返回前要检查

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

### 6.7 一句话总结

哪些线程该响应poll, 不是由Java层 `Thread` 对象决定的, 而是由HotSpot内部三类信息共同维护:

1. `Threads` 全局JavaThread列表: 决定这次safepoint涉及哪些线程。
2. `JavaThread` 当前 `_thread_state`: 决定线程现在是否需要主动poll。
3. `ThreadSafepointState` / `SafepointMechanism`: 记录线程是否已safe, 以及poll请求是否armed。

全局safepoint时, 目标通常是让所有相关JavaThread进入safe状态; thread-local handshake时, 则可以只让目标线程响应。

## 7. 线程状态和Safepoint的关系

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

### 7.1 不同状态怎么处理

可以先按下面这张表理解:

| 当前状态 | VMThread通常怎么处理 | 关键点 |
| :---: | :--- | :--- |
| `_thread_in_Java` | 等它运行到poll点自我阻塞 | 最典型的 `time to safepoint` 来源 |
| `_thread_in_vm` | 要求它回调或在VM路径中检查 | VM代码要遵守safepoint规则 |
| `_thread_in_native` | 通常视为safe, 返回Java前再检查 | native不继续执行Java bytecode |
| `_thread_blocked` | 通常视为safe, 唤醒/返回前检查 | 不允许绕过safepoint继续跑Java |
| 新启动/退出中 | 通过 `Threads_lock` 协调 | 避免线程集合不稳定 |

所谓“safe”, 不是说线程消失了, 而是说它不会继续并发执行Java代码并改动JVM需要稳定观察的状态。

### 7.2 block到底做什么

线程响应safepoint时会进入 `SafepointSynchronize::block(JavaThread* thread)` 这类路径。可以粗略理解为:

1. 当前JavaThread发现safepoint请求。
2. 把自己的状态切换到blocked或对应的safepoint等待状态。
3. 通知VMThread: 我已经到达safepoint-safe状态。
4. 在barrier/monitor上等待。
5. VMThread执行完VM operation后唤醒它。
6. 线程恢复之前的执行状态, 继续跑Java代码。

这说明safepoint是协作式的: 线程不是被JVM随便打断后冻结, 而是在JVM安排的位置进入一段等待逻辑。

## 8. native状态和Safepoint

native状态不是“必须马上停住”, 而是HotSpot特别处理的一种safepoint-safe状态。

线程运行在普通native code时, VMThread看到它在native, 通常不等待它block。原因是它暂时不在执行Java bytecode, 不会继续改动Java执行栈帧。但这个线程从native返回Java前必须检查safepoint状态, 必要时阻塞等待。

可以先这样理解:

1. native线程不会继续执行Java bytecode, 所以通常可被VM视为safepoint-safe。
2. native线程回到Java前, 要通过状态切换检查safepoint请求。
3. JNI critical region 是特殊情况。

JNI官方规范要求 `GetPrimitiveArrayCritical` / `ReleasePrimitiveArrayCritical` 之间的native代码不能长时间运行、不能阻塞、不能随意JNI调用。因为VM可能为了这类critical区域临时限制GC移动对象。

所以不要简单说“native一定会卡住safepoint”, 更准确是:

> 普通native执行通常可以被JVM视为安全状态; 但JNI critical、长时间不返回或持有VM相关资源的native逻辑, 仍然可能影响GC或其它VM操作。

### 8.1 为什么JNI critical特殊

JNI提供了 `GetPrimitiveArrayCritical` 这类API, 允许native代码尽量直接访问Java数组底层数据。为了做到这一点, VM可能需要临时限制GC移动这个数组。

所以JNI规范要求critical区域内的代码要非常短:

```c++
jint* p = env->GetPrimitiveArrayCritical(array, NULL);
// 这里不要长时间阻塞, 不要做复杂JNI调用
env->ReleasePrimitiveArrayCritical(array, p, 0);
```

如果critical区域太长, 可能导致GC等待对象移动条件满足, 进而拖慢某些VM操作。这个问题不是“native状态天然阻塞safepoint”, 而是“这段native持有了VM需要特别对待的资源”。

## 9. 常见触发场景

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

### 9.1 VM_Operation和Safepoint的关系

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

## 10. 性能影响

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

### 10.1 日志指标怎么读

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

### 10.2 常见慢Safepoint场景

1. 大循环迟迟不到poll点。
2. 线程没有被OS调度, 一直没机会执行poll。
3. JNI critical区域过长。
4. 大量线程导致safepoint协调成本上升。
5. CPU打满, VMThread抢不到CPU。
6. 容器CPU quota太小, 导致VMThread/GC/业务线程互相抢。
7. 诊断命令、heap dump、thread dump、JFR等工具操作本身比较重。

## 11. 排查命令

### 11.1 JDK 8

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

### 11.2 JDK 9+

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

## 12. Safepoint和OS中断的区别

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

### 12.1 和异常、信号、page fault的关系

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

## 13. 几个容易误解的点

1. `RUNNABLE` 不等于正在CPU上执行。
2. STW不是“瞬间按暂停键”, 而是相关线程陆续到达safepoint或进入可认为安全的状态。
3. native里的线程不等于一定卡住safepoint。
4. `Thread.sleep()` / `yield()` 不是safepoint控制手段。
5. `Thread.interrupt()` 不是safepoint。
6. 看到page fault/trap不代表safepoint就是OS中断。
7. 并发GC不代表完全没有safepoint。

## 14. 总结

1. Safepoint是HotSpot为全局VM操作准备的安全停顿机制。
2. Java线程不是在任意机器指令处被JVM粗暴暂停, 而是在safepoint poll、状态切换等JVM可控位置停下。
3. `_thread_in_Java`、`_thread_in_native`、`_thread_blocked` 等内部状态会影响safepoint协调方式。
4. GC、栈遍历、类卸载、偏向锁撤销、反优化等都可能触发safepoint。
5. 分析safepoint性能时, 要区分 `time to safepoint` 和 `vm operation time`。
6. Safepoint属于JVM协作协议, OS中断属于操作系统/硬件机制, 二者不是一个层面的东西。

## 15. 参考资料

* [OpenJDK safepoint.cpp](https://github.com/openjdk/jdk/blob/master/src/hotspot/share/runtime/safepoint.cpp)
* [OpenJDK safepointMechanism.hpp](https://github.com/openjdk/jdk/blob/master/src/hotspot/share/runtime/safepointMechanism.hpp)
* [OpenJDK JEP 312 - Thread-Local Handshakes](https://bugs.openjdk.org/browse/JDK-8185640)
* [OpenJDK CSR JDK-8189942 - ThreadLocalHandshakes option](https://bugs.openjdk.org/browse/JDK-8189942)
* [OpenJDK vmThread.cpp](https://github.com/openjdk/jdk/blob/master/src/hotspot/share/runtime/vmThread.cpp)
* [OpenJDK vmOperation.hpp](https://github.com/openjdk/jdk/blob/master/src/hotspot/share/runtime/vmOperation.hpp)
* [OpenJDK interfaceSupport.inline.hpp](https://github.com/openjdk/jdk/blob/master/src/hotspot/share/runtime/interfaceSupport.inline.hpp)
* [OpenJDK javaThread.hpp](https://github.com/openjdk/jdk/blob/master/src/hotspot/share/runtime/javaThread.hpp)
* [Oracle JNI Functions - GetPrimitiveArrayCritical](https://docs.oracle.com/en/java/javase/24/docs/specs/jni/functions.html)
* [Oracle JVM TI Specification](https://docs.oracle.com/en/java/javase/18/docs/specs/jvmti.html)
* [Linux Kernel Generic IRQ Handling](https://www.kernel.org/doc/html/v6.7/core-api/genericirq.html)
