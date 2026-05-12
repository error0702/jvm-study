# 内存编织技术

> 这里的“内存编织”不是JVM规范中的正式术语。可以先理解为 HotSpot 在类加载时对字段进行布局, 尽量让对象实例字段在内存中排列得更紧凑, 同时记录哪些位置是对象引用, 方便GC扫描。

## 1. 为什么需要字段布局

Java源码中字段的声明顺序不一定等于对象在内存中的最终排列顺序。HotSpot在加载类时需要计算:

1. 每个实例字段在对象中的偏移量
2. 静态字段在 mirror 或相关存储区域中的位置
3. 对象实例最终大小
4. 哪些偏移位置是 oop 引用, 供GC扫描使用
5. 是否需要填充字段来满足对象对齐

举个例子:

```java
class Entity {
    byte b;
    long l;
    int i;
    Object ref;
}
```

如果完全按照声明顺序排布, `byte` 后面可能需要插入较多 padding 才能让 `long` 对齐。HotSpot会结合字段大小、对象对齐、父类字段、引用字段等因素计算最终布局。

## 2. 源码入口

源码位置:
* OpenJDK 8: `hotspot/src/share/vm/classfile/classFileParser.cpp`
* 新版JDK: `src/hotspot/share/classfile/classFileParser.cpp`

可以重点看字段布局相关逻辑:

```c++
// openjdk8 中可以搜索 layout_fields
ClassFileParser::layout_fields(...)
```

这个阶段会处理静态字段、非静态字段、字段偏移、字段大小、对象对齐等信息。最终这些布局信息会进入 `InstanceKlass`。

## 3. 字段大概怎么分组

可以先粗略理解成几类:

| 类型 | 说明 |
| :---: | :--- |
| `oop`字段 | Java引用类型字段, 如 `Object ref` |
| `long/double` | 8字节字段 |
| `int/float` | 4字节字段 |
| `short/char` | 2字节字段 |
| `byte/boolean` | 1字节字段 |

HotSpot会尽量利用空洞减少浪费。但这个布局不是Java语言规范承诺的行为, 不同JDK版本、不同参数下都有可能变化。

## 4. oop map

字段布局不仅是为了省内存, 还服务于GC。

GC扫描对象时需要知道对象的哪些位置是引用。`InstanceKlass` 中会保存 nonstatic oop map, 用来描述实例对象里引用字段的位置。

在 `InstanceKlass` 的布局注释里也能看到:

```c++
// [EMBEDDED nonstatic oop-map blocks]
// The embedded nonstatic oop-map blocks are short pairs (offset, length)
// indicating where oops are located in instances of this klass.
```

也就是说, 对象里的普通数据字段和引用字段虽然都在一块对象内存中, 但GC并不是盲扫整块内存, 而是依赖 Klass 中记录的 oop map 找到引用。

## 5. 和父类字段的关系

子类对象中会包含父类定义的实例字段。对象布局时一般先放父类字段, 再布局子类自己的字段。

可以先这样理解:

```text
+--------------------+
| object header      |
+--------------------+
| super fields       |
+--------------------+
| current fields     |
+--------------------+
| padding            |
+--------------------+
```

因此一个子类对象的大小, 不只是当前类字段大小, 还包括父类字段和对象头。

## 6. 注意点

1. Java语言层面不要依赖字段在内存中的顺序。
2. 字段布局是HotSpot实现细节, 不是JVM规范保证。
3. 压缩指针会影响引用字段大小, 从而影响对象布局。
4. 对象对齐会影响最终对象大小。
5. GC识别引用字段依赖 `InstanceKlass` 中记录的 oop map。
