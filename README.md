# jvm-study

> 这是一个学习JVM源码的仓库。
> 
> 如果想要贡献文章请提pr (Pull Request)，谢谢
> 
> 关于什么是`pr`， 请参考这篇文章 [文章1](https://www.jianshu.com/p/a31a888ac46b)
>  [文章2](https://blog.csdn.net/qq_33429968/article/details/62219783)
> 
> 如果是命令行玩家请参考此文章: [命令行如何提交pr](http://www.ruanyifeng.com/blog/2017/07/pull_request.html)
<hr>

## jvm 学习大纲

<hr>

### 1. 编译JVM
* [x] [1.1 OpenJDK编译教程](src/autorun/jvm/enviment/ENVIMENT_INIT.md)
* [x] [1.2 配置调试环境](src/autorun/jvm/enviment/IDE_DEBUG.md) </br>

### 2. JNI
- [x] [2.1 初识JNI](src/autorun/jvm/jni/README.md) </br>
- [x] [2.2 JNIEnv相关API](src/autorun/jvm/jni/JNIEnvAPI.md) </br>
- [x] [2.3 JNI初始化过程](src/autorun/jvm/jni/JNI_INIT.md)


### 3. jvm启动流程剖析
- [x] [3.1 jvm启动流程1](src/autorun/jvm/start/README.md) </br>
- [ ] [3.2 jvm启动流程2](src/autorun/jvm/start/README2.md) </br>

### 4. 面向对象OOP模型
- [ ] [4.1 OOP KLASS模型](src/autorun/jvm/oop/OOP.md) </br>
- [ ] [4.2 指针压缩](src/autorun/jvm/oop/Compressed_Oops.md) </br>
- [ ] [4.3 内存编织](src/autorun/jvm/oop/Memory_Weave.md) </br>

### 5. 方法调用
- [ ] [5.1 CallStub栈帧的创建](src/autorun/jvm/method/CALL_STUB.md) </br>
- [ ] [5.2 Java方法调用过程](src/autorun/jvm/method/JAVA_CALLS.md) </br>
