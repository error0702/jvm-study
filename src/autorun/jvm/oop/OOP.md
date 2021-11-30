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

```c++
class oopDesc {
private:
  volatile markOop  _mark; // markOopDesc* markOop
  
  union _metadata {
    Klass*      _klass;
    narrowKlass _compressed_klass;
  } _metadata;
};
```


### 2. KLASS 模型