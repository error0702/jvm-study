# 类解析

> hotspot/src/share/vm/classfile/classFileParser.cpp

`ClassFileParser::parseClassFile` 

### 解析常量池
`constantPoolHandle cp = parse_constant_pool(CHECK_(nullHandle));`


```c++
  AccessFlags access_flags;
  jint flags = cfs->get_u2_fast() & JVM_RECOGNIZED_CLASS_MODIFIERS;
```

