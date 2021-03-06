# OOP-KLASS 二分模型

## 1. OOP概念
> 摘自`《Hotspot实战》`
> 
> 设计一个面向对象系统，应当支特面向对象的几个主要特征：封装、继承和至态。在JVM
中必须能够支特这些特征。我们不禁会问：HotSpot 基于 C++实现，而 C++就是一门面向对象
话吉，它本身就具有上述面向对象基本特征，那么只需要在HotSpot 内部为每个 Java 类生成
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
1. 类结构图</br>
jdk 1.7及以下版本结构图(摘自 `Hotspot` 实战)
![](img/lt1.7.png)

jdk1.8及以上版本结构图
![](img/gt1.8.jpg)

2. OOP模型对应代码

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
3. 代码详解

`union` 关键字含义:
* `union` 即为联合，它是一种特殊的类。通过关键字`union` 进行定义，一个`union` 可以有多个数据成员。
* 联合体是一种互斥的结构体，也就是说在任意时刻，联合中只能有一个数据成员可以有值。当给联合中某个成员赋值之后，该联合中的其它成员就变成未定义状态了。

简单解释一下
* `markOop` 实际上是markOopDesc指针类型，言外之意就是会随着操作系统的位数决定它的大小。
如x86架构下，指针大小是4字节(Byte), 而64位系统则是8字节。 所以这也就是为什么对象头无法压缩的原因
* 联合体 `_metadata`: 上面也说过联合体是排他的，也就是说同一时刻，只要联合体中的其中一个字段有值，则其它字段都是未定义的状态
* 关于对象指针压缩， 使用 `-XX:(+/-)UseCompressedOops` 来控制是否开启指针压缩。 控制指针压缩的参数如下表格：

|数据类型|参数名称|默认值|参数类型和支持环境|
|   ---|    ---|    ---|    ---|
|bool|UseCompressedOops|true|{lp64_product}|

通过上面的表格可以看出来，这个参数是64位jdk，并且是`product`版本的Hotspot级别的参数(当然，自己编译的debug级别的jdk也是可以的。这里使用级别这个词不太准确，请自行脑补)

指针压缩完后的效果其实是把8字节大小的指针使用int类型(4字节)的结构体来存储。这样的话其实存储的是内存地址，而且是需要往右偏移3位的一段空间地址。
所以取地址时，需要左移3位就可以拿到真正的偏移地址了。这里不再赘述。详情移步到 [4.2 指针压缩](Compressed_Oops.md) 章节

4. 总结

`JVM` 在描述 `JAVA` 类型和类型指针 `JAVA` 方法类型和方法指针，常量池缓存类型指针，基本数据类型和数组类型指针。 `HotSpot` 认为以上这几种模型已经足够描述 `JAVA` 程序的全部： 数据、方法、类型、数组和实例。

前面我们说过，`OOP` 是描述对象类型的承载主体， Klass是描述类类型的承载主体。


`JAVA` 代码中出现 `Entity entity = new Entity(); ` 
这段代码时，`Hotspot` 会先将 `Entity` 这个类类型加载到方法区[^Method_Area](永久代/元空间。不同的 `JVM` 有不同的实现)， 然后再 `Hotspot` 堆中为其实例对象 `entity` 对象开辟一块内存空间，存放对象实例数据。
在 `JVM` 加载 `Entity` 到方法区时，`JVM` 会创建一个 `instanceKlass`，其中保存了 `Entity` 这个类中定义的所有信息，包括变量、方法、父类、接口、构造、属性等， 所以 `instanceKlass` 就是 `Entity` 这个 `JAVA` 类类型结构的对等体。
而 `instanceOop` 这个普通对象指针 对象中包含了一个指针，这个指针就指向这个 `instanceKlass` 实例。而 `JVM` 在实例化 `Entity` 时会会创建一个 `instanceOop` 实例，该实例便是 `Entity` 对象实例 `entity` 在内存中的对等体。主要存储 `Entity` 实例对象的成员变量。
其中， instanceOop中有一个指针指向 `instanceKlass`， 通过这个指针， `JVM` 便可以在运行时获取这个类的元信息(反射特性)。


## 附录
[^Method_Area]: https://docs.oracle.com/javase/specs/jvms/se8/html/jvms-2.html#jvms-2.5.4 