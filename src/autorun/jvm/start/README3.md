# 本期主题 `vm_init_globals();`

## 1. 主流程
1. 源码位置: `hotspot/src/share/vm/runtime/init.cpp`
```c++
check_ThreadShadow();
basic_types_init();
eventlog_init();
mutex_init();
chunkpool_init();
perfMemory_init();
```
`basic_types_init`主要用于java初始化基础类型
源码位置：hotspot/src/share/vm/utilities/globalDefinitions.hpp

> `void basic_types_init()`
```c++
enum BasicType {
  T_BOOLEAN     =  4,
  T_CHAR        =  5,
  T_FLOAT       =  6,
  T_DOUBLE      =  7,
  T_BYTE        =  8,
  T_SHORT       =  9,
  T_INT         = 10,
  T_LONG        = 11,
  T_OBJECT      = 12,
  T_ARRAY       = 13,
  T_VOID        = 14,
  T_ADDRESS     = 15,
  T_NARROWOOP   = 16,
  T_METADATA    = 17,
  T_NARROWKLASS = 18,
  T_CONFLICT    = 19, // for stack value type with conflicting contents
  T_ILLEGAL     = 99
};
```

```c++
for (int i = T_BOOLEAN; i <= T_CONFLICT; i++) {
      BasicType vt = (BasicType)i;
      BasicType ft = type2field[vt];
      switch (vt) {
      // the following types might plausibly show up in memory layouts:
      case T_BOOLEAN:
      case T_BYTE:
      case T_CHAR:
      case T_SHORT:
      case T_INT:
      case T_FLOAT:
      case T_DOUBLE:
      case T_LONG:
      case T_OBJECT:
      case T_ADDRESS:     // random raw pointer
      case T_METADATA:    // metadata pointer
      case T_NARROWOOP:   // compressed pointer
      case T_NARROWKLASS: // compressed klass pointer
      case T_CONFLICT:    // might as well support a bottom type
      case T_VOID:        // padding or other unaddressed word
        // layout type must map to itself
        assert(vt == ft, "");
        break;
      default:
        // non-layout type must map to a (different) layout type
        assert(vt != ft, "");
        assert(ft == type2field[ft], "");
      }
      // every type must map to same-sized layout type:
      assert(type2size[vt] == type2size[ft], "");
}
```
```c++
if (UseCompressedOops) {
    // Size info for oops within java objects is fixed
    heapOopSize        = jintSize;
    LogBytesPerHeapOop = LogBytesPerInt;
    LogBitsPerHeapOop  = LogBitsPerInt;
    BytesPerHeapOop    = BytesPerInt;
    BitsPerHeapOop     = BitsPerInt;
  } else {
    heapOopSize        = oopSize;
    LogBytesPerHeapOop = LogBytesPerWord;
    LogBitsPerHeapOop  = LogBitsPerWord;
    BytesPerHeapOop    = BytesPerWord;
    BitsPerHeapOop     = BitsPerWord;
  }
  _type2aelembytes[T_OBJECT] = heapOopSize;
  _type2aelembytes[T_ARRAY]  = heapOopSize;
```
3. eventlog_init();


4. 锁类型初始化 `mutex_init();`
```c++
def(tty_lock                     , Mutex  , event,       true ); // allow to lock in VM

  def(CGC_lock                   , Monitor, special,     true ); // coordinate between fore- and background GC
  def(STS_init_lock              , Mutex,   leaf,        true );
  if (UseConcMarkSweepGC) {
    def(iCMS_lock                  , Monitor, special,     true ); // CMS incremental mode start/stop notification
  }
  if (UseConcMarkSweepGC || UseG1GC) {
    def(FullGCCount_lock           , Monitor, leaf,        true ); // in support of ExplicitGCInvokesConcurrent
  }
  if (UseG1GC) {
    def(CMark_lock                 , Monitor, nonleaf,     true ); // coordinate concurrent mark thread
    def(CMRegionStack_lock         , Mutex,   leaf,        true );
    def(SATB_Q_FL_lock             , Mutex  , special,     true );
    def(SATB_Q_CBL_mon             , Monitor, nonleaf,     true );
    def(Shared_SATB_Q_lock         , Mutex,   nonleaf,     true );

    def(DirtyCardQ_FL_lock         , Mutex  , special,     true );
    def(DirtyCardQ_CBL_mon         , Monitor, nonleaf,     true );
    def(Shared_DirtyCardQ_lock     , Mutex,   nonleaf,     true );

    def(FreeList_lock              , Mutex,   leaf     ,   true );
    def(SecondaryFreeList_lock     , Monitor, leaf     ,   true );
    def(OldSets_lock               , Mutex  , leaf     ,   true );
    def(RootRegionScan_lock        , Monitor, leaf     ,   true );
    def(MMUTracker_lock            , Mutex  , leaf     ,   true );
    def(HotCardCache_lock          , Mutex  , special  ,   true );
    def(EvacFailureStack_lock      , Mutex  , nonleaf  ,   true );
  }
  def(ParGCRareEvent_lock          , Mutex  , leaf     ,   true );
  def(DerivedPointerTableGC_lock   , Mutex,   leaf,        true );
  def(CodeCache_lock               , Mutex  , special,     true );
  def(Interrupt_lock               , Monitor, special,     true ); // used for interrupt processing
  def(RawMonitor_lock              , Mutex,   special,     true );
  def(OopMapCacheAlloc_lock        , Mutex,   leaf,        true ); // used for oop_map_cache allocation.

  def(Patching_lock                , Mutex  , special,     true ); // used for safepointing and code patching.
  def(ObjAllocPost_lock            , Monitor, special,     false);
  def(Service_lock                 , Monitor, special,     true ); // used for service thread operations
  def(JmethodIdCreation_lock       , Mutex  , leaf,        true ); // used for creating jmethodIDs.

  def(SystemDictionary_lock        , Monitor, leaf,        true ); // lookups done by VM thread
  def(PackageTable_lock            , Mutex  , leaf,        false);
  def(InlineCacheBuffer_lock       , Mutex  , leaf,        true );
  def(VMStatistic_lock             , Mutex  , leaf,        false);
  def(ExpandHeap_lock              , Mutex  , leaf,        true ); // Used during compilation by VM thread
  def(JNIHandleBlockFreeList_lock  , Mutex  , leaf,        true ); // handles are used by VM thread
  def(SignatureHandlerLibrary_lock , Mutex  , leaf,        false);
  def(SymbolTable_lock             , Mutex  , leaf+2,      true );
  def(StringTable_lock             , Mutex  , leaf,        true );
  def(ProfilePrint_lock            , Mutex  , leaf,        false); // serial profile printing
  def(ExceptionCache_lock          , Mutex  , leaf,        false); // serial profile printing
  def(OsrList_lock                 , Mutex  , leaf,        true );
  def(Debug1_lock                  , Mutex  , leaf,        true );
#ifndef PRODUCT
  def(FullGCALot_lock              , Mutex  , leaf,        false); // a lock to make FullGCALot MT safe
#endif
  def(BeforeExit_lock              , Monitor, leaf,        true );
  def(PerfDataMemAlloc_lock        , Mutex  , leaf,        true ); // used for allocating PerfData memory for performance data
  def(PerfDataManager_lock         , Mutex  , leaf,        true ); // used for synchronized access to PerfDataManager resources

  // CMS_modUnionTable_lock                   leaf
  // CMS_bitMap_lock                          leaf + 1
  // CMS_freeList_lock                        leaf + 2

  def(Safepoint_lock               , Monitor, safepoint,   true ); // locks SnippetCache_lock/Threads_lock

  def(Threads_lock                 , Monitor, barrier,     true );

  def(VMOperationQueue_lock        , Monitor, nonleaf,     true ); // VM_thread allowed to block on these
  def(VMOperationRequest_lock      , Monitor, nonleaf,     true );
  def(RetData_lock                 , Mutex  , nonleaf,     false);
  def(Terminator_lock              , Monitor, nonleaf,     true );
  def(VtableStubs_lock             , Mutex  , nonleaf,     true );
  def(Notify_lock                  , Monitor, nonleaf,     true );
  def(JNIGlobalHandle_lock         , Mutex  , nonleaf,     true ); // locks JNIHandleBlockFreeList_lock
  def(JNICritical_lock             , Monitor, nonleaf,     true ); // used for JNI critical regions
  def(AdapterHandlerLibrary_lock   , Mutex  , nonleaf,     true);
  if (UseConcMarkSweepGC) {
    def(SLT_lock                   , Monitor, nonleaf,     false );
                    // used in CMS GC for locking PLL lock
  }
  def(Heap_lock                    , Monitor, nonleaf+1,   false);
  def(JfieldIdCreation_lock        , Mutex  , nonleaf+1,   true ); // jfieldID, Used in VM_Operation
  def(MemberNameTable_lock         , Mutex  , nonleaf+1,   false); // Used to protect MemberNameTable

  def(CompiledIC_lock              , Mutex  , nonleaf+2,   false); // locks VtableStubs_lock, InlineCacheBuffer_lock
  def(CompileTaskAlloc_lock        , Mutex  , nonleaf+2,   true );
  def(CompileStatistics_lock       , Mutex  , nonleaf+2,   false);
  def(MultiArray_lock              , Mutex  , nonleaf+2,   false); // locks SymbolTable_lock

  def(JvmtiThreadState_lock        , Mutex  , nonleaf+2,   false); // Used by JvmtiThreadState/JvmtiEventController
  def(JvmtiPendingEvent_lock       , Monitor, nonleaf,     false); // Used by JvmtiCodeBlobEvents
  def(Management_lock              , Mutex  , nonleaf+2,   false); // used for JVM management

  def(Compile_lock                 , Mutex  , nonleaf+3,   true );
  def(MethodData_lock              , Mutex  , nonleaf+3,   false);

  def(MethodCompileQueue_lock      , Monitor, nonleaf+4,   true );
  def(Debug2_lock                  , Mutex  , nonleaf+4,   true );
  def(Debug3_lock                  , Mutex  , nonleaf+4,   true );
  def(ProfileVM_lock               , Monitor, special,   false); // used for profiling of the VMThread
  def(CompileThread_lock           , Monitor, nonleaf+5,   false );
  def(PeriodicTask_lock            , Monitor, nonleaf+5,   true);

#ifdef INCLUDE_TRACE
  def(JfrMsg_lock                  , Monitor, leaf,        true);
  def(JfrBuffer_lock               , Mutex,   nonleaf+1,   true);
  def(JfrThreadGroups_lock         , Mutex,   nonleaf+1,   true);
  def(JfrStream_lock               , Mutex,   nonleaf+2,   true);
  def(JfrStacktrace_lock           , Mutex,   special,     true );
#endif
```

内存池初始化 `chunkpool_init()` 
