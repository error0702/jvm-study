# 内存池初始化

## 1. 内存池初始化步骤
1. `Threads::create_vm() thread.cpp` </br>
2. `init_globals() init.cpp` </br>
3. `universe_init() universe.cpp` </br>
4. `Universe::initialize_heap() universe.cpp` </br>
5. `Universe::heap()->initialize() universe.cpp` </br>

### 1. `Threads::create_vm()`

### 2. `init_globals()`

### 3. `universe_init()`

### 4. `Universe::initialize_heap()`

这里判断使用什么类型的gc，同时初始化gc策略。 

如果启用了(默认启用) `PerfData` (参考jvm参数 `-XX:+UsePerfData`)， 则开启jvm内部数据收集。
作用是动态的调整gc， 对象和jit相关的指标做垃圾回收

> jdk8 默认使用 ParallelGC， 所以它会调用 
```c++
Universe::_collectedHeap = new ParallelScavengeHeap();
Universe::heap()->initialize();
```

// check and init gc policy
```c++
  if (UseParallelGC) {
#if INCLUDE_ALL_GCS
    Universe::_collectedHeap = new ParallelScavengeHeap();
#else  // INCLUDE_ALL_GCS
    fatal("UseParallelGC not supported in this VM.");
#endif // INCLUDE_ALL_GCS

  } else if (UseG1GC) {
#if INCLUDE_ALL_GCS
    G1CollectorPolicy* g1p = new G1CollectorPolicy();
    g1p->initialize_all();
    G1CollectedHeap* g1h = new G1CollectedHeap(g1p);
    Universe::_collectedHeap = g1h;
#else  // INCLUDE_ALL_GCS
    fatal("UseG1GC not supported in java kernel vm.");
#endif // INCLUDE_ALL_GCS

  } else {
    GenCollectorPolicy *gc_policy;

    if (UseSerialGC) {
      gc_policy = new MarkSweepPolicy();
    } else if (UseConcMarkSweepGC) {
#if INCLUDE_ALL_GCS
      if (UseAdaptiveSizePolicy) {
        gc_policy = new ASConcurrentMarkSweepPolicy();
      } else {
        gc_policy = new ConcurrentMarkSweepPolicy();
      }
#else  // INCLUDE_ALL_GCS
    fatal("UseConcMarkSweepGC not supported in this VM.");
#endif // INCLUDE_ALL_GCS
    } else { // default old generation
      gc_policy = new MarkSweepPolicy();
    }
    gc_policy->initialize_all();

    Universe::_collectedHeap = new GenCollectedHeap(gc_policy);
  }

  jint status = Universe::heap()->initialize();
  if (status != JNI_OK) {
    return status;
  }
```

### 5. `Universe::heap()->initialize()`
