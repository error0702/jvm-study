# Klass-Java类的描述信息

源码位置:
* OpenJDK 8: `hotspot/src/share/vm/oops/instanceKlass.hpp`
* 新版JDK: `src/hotspot/share/oops/instanceKlass.hpp`

> openjdk8 instanceKlass
```c++
// An InstanceKlass is the VM level representation of a Java class.
// It contains all information needed for at class at execution runtime.

//  InstanceKlass layout:
//    [C++ vtbl pointer           ] Klass
//    [subtype cache              ] Klass
//    [instance size              ] Klass
//    [java mirror                ] Klass
//    [super                      ] Klass
//    [access_flags               ] Klass
//    [name                       ] Klass
//    [first subklass             ] Klass
//    [next sibling               ] Klass
//    [array klasses              ]
//    [methods                    ]
//    [local interfaces           ]
//    [transitive interfaces      ]
//    [fields                     ]
//    [constants                  ]
//    [class loader               ]
//    [source file name           ]
//    [inner classes              ]
//    [static field size          ]
//    [nonstatic field size       ]
//    [static oop fields size     ]
//    [nonstatic oop maps size    ]
//    [has finalize method        ]
//    [deoptimization mark bit    ]
//    [initialization state       ]
//    [initializing thread        ]
//    [Java vtable length         ]
//    [oop map cache (stack maps) ]
//    [EMBEDDED Java vtable             ] size in words = vtable_len
//    [EMBEDDED nonstatic oop-map blocks] size in words = nonstatic_oop_map_size
//      The embedded nonstatic oop-map blocks are short pairs (offset, length)
//      indicating where oops are located in instances of this klass.
//    [EMBEDDED implementor of the interface] only exist for interface
//    [EMBEDDED host klass        ] only exist for an anonymous class (JSR 292 enabled)
```

主要标注 `klass` 的几种状态。
```c++
  // See "The Java Virtual Machine Specification" section 2.16.2-5 for a detailed description
  // of the class loading & initialization procedure, and the use of the states.
  enum ClassState {
    allocated,                          // allocated (but not yet linked)
    loaded,                             // loaded and inserted in class hierarchy (but not linked yet)
    linked,                             // successfully linked/verified (but not initialized yet)
    being_initialized,                  // currently running class initializer
    fully_initialized,                  // initialized (successfull final state)
    initialization_error                // error happened during initialization
  };
```


`InstanceKlass::initialize -> InstanceKlass::initialize_impl`

## Klass 与 Java镜像类

Java代码中拿到的 `Class<?>` 对象, 在HotSpot内部通常称为 mirror。也就是说:

```java
Entity.class
entity.getClass()
```

拿到的是 Java 堆里的 `java.lang.Class` 对象。而 `InstanceKlass` 是 VM 元数据, 不直接暴露给 Java 代码。二者之间会建立关联:

```text
Java堆对象 instanceOop
        |
        | klass pointer
        v
InstanceKlass 元数据  <---->  java.lang.Class mirror
```

简单理解:

* `instanceOop`: Java对象实例, 存字段值
* `InstanceKlass`: Java类的运行时元数据, 存字段布局、方法、父类、接口、常量池等
* `java.lang.Class`: Java层能看到的类型镜像对象

所以反射并不是直接把 `InstanceKlass` 暴露给 Java 代码, 而是通过 `java.lang.Class` 这个 mirror 作为入口。

## Klass家族

`Klass` 是一组 VM 元数据类型的基类视角。不同 Java 类型会对应不同的 Klass 结构:

| Klass类型 | 描述 |
| :---: | :--- |
| `InstanceKlass` | 普通Java类 |
| `InstanceMirrorKlass` | `java.lang.Class` 对象本身的 Klass |
| `InstanceRefKlass` | 引用类型相关, 如 `java.lang.ref.Reference` 子类 |
| `ArrayKlass` | 数组Klass基类 |
| `ObjArrayKlass` | 引用类型数组 |
| `TypeArrayKlass` | 基本类型数组 |

普通对象通过对象头里的 klass pointer 找到 `InstanceKlass`; 数组对象则会找到对应的 `ArrayKlass`。这也是为什么数组对象可以在运行时知道自己的元素类型和长度。

## Klass 中存什么

`InstanceKlass` 里保存的是类级别的信息, 不是某一个对象的字段值。可以先按下面几类理解:

1. 类型关系: 父类、接口、子类链
2. 符号信息: 类名、源码文件名、访问标志
3. 方法信息: methods、vtable、itable
4. 字段布局: static field size、nonstatic field size、字段偏移
5. 运行时状态: 是否 loaded、linked、initialized
6. 运行时入口: class loader、常量池、mirror

其中 vtable 和 itable 是方法分派非常关键的结构:

* `vtable`: 虚方法分派, 主要服务于普通虚方法调用
* `itable`: 接口方法分派, 主要服务于接口调用

## 类状态说明

上面列出的 `ClassState` 可以和类生命周期大概对应起来:

| 状态 | 含义 |
| :---: | :--- |
| `allocated` | 已分配元数据结构, 但还没有完成加载 |
| `loaded` | 已加载并加入类层次结构, 但还没有完成链接 |
| `linked` | 已完成验证、准备、解析等链接过程, 但还未初始化 |
| `being_initialized` | 正在执行 `<clinit>` |
| `fully_initialized` | 类初始化完成 |
| `initialization_error` | 初始化失败 |

这里需要注意: 对象执行构造方法 `<init>` 和类执行静态初始化方法 `<clinit>` 不是一回事。

* `<clinit>`: 类初始化, 处理静态变量和静态代码块
* `<init>`: 对象初始化, 处理构造方法
