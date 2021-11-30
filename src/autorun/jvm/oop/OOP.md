# OOP-KLASS 二分模型

## 1. OOP概念
> 摘自`《Hotspot实战》`
> 
> 设计一个面向对象系统，应当支特面向对象的几个主要特征：封装、继承和至态。在JVM
中必须能够支特这些特征。我们不禁会问：HotSpot 基于 C++实现，而 C++就是一门面向对象
话吉，它本身就具有上述面向对象基木特征，那么只需要在HotSpot 内部为每个 Java 类生成
个C++类，不就行了吗？换句话说，虚拟机行加载一个 Java 类，就在内部创建一个域和方法与
之相同的C++类对等体。当Java 程序需要创建文例对象时，反映到虚拟机中，就在内部创建相
应的 C++对象。
> 
> 
> 事实上，Hotspot 的设计者并没有按照上述思路设计对象表示系统，而是专门设计了OOP-Klass
分模型：

* OOP: ordinary object pointer， 或 OOPS. 即普通对象指针，用来描述对象实例信息
* Klass: Java 类的 C++对等体，用来描达 Java 类

### 1. OOP 模型
源码位置: `hotspot/src/share/vm/oops/oop.hpp`
```c++
class oopDesc {
private:
  volatile markOop  _mark; // markOopDesc* markOop
  
  union _metadata {
    Klass*      _klass;
    narrowKlass _compressed_klass; // juint  narrowKlass
  } _metadata;
};
```

`union` 关键字含义:
* `union` 即为联合，它是一种特殊的类。通过关键字`union` 进行定义，一个`union` 可以有多个数据成员。
* 联合体是一种互斥的结构体，也就是说在任意时刻，联合中只能有一个数据成员可以有值。当给联合中某个成员赋值之后，该联合中的其它成员就变成未定义状态了。

简单解释一下
* `markOop` 实际上是markOopDesc指针类型，言外之意就是会随着操作系统的位数决定它的大小。
如x86架构下，指针大小是4字节(Byte), 而64位系统则是8字节。 所以这也就是为什么对象头无法压缩的原因
* 联合体 `_metadata`: 上面也说过联合体是排他的，也就是说同一时刻，只要联合体中的其中一个字段有值，则其它字段都是未定义的状态
* 关于对象指针压缩， 使用 `-XX:(+/-)UseCompressedOops` 来控制是否开启指针压缩。 指针压缩的 


     bool UseCompressedOops                        := true                                {lp64_product}
### 2. KLASS 模型