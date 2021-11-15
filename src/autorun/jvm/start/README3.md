# 本期主题 `vm_init_globals();`

源码位置: `hotspot/src/share/vm/runtime/init.cpp`
```c++
check_ThreadShadow();
basic_types_init();
eventlog_init();
mutex_init();
chunkpool_init();
perfMemory_init();
```

1. `basic_types_init`主要用于java初始化基础类型
源码位置：hotspot/src/share/vm/utilities/globalDefinitions.hpp
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

