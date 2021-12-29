# jvm-study
## jvm-study是什么 

 这是一个学习JVM源码的仓库. 通过这个仓库,可以学习到JVM相关的知识. 
## jvm 学习大纲

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
- [ ] [3.3 jvm启动流程3](src/autorun/jvm/start/README3.md) </br>
- [ ] [3.4 jvm启动流程4](src/autorun/jvm/start/README4.md) </br>

### 4. 面向对象OOP模型
- [ ] [4.1 OOP KLASS模型](src/autorun/jvm/oop/OOP.md) </br>
- [ ] [4.2 指针压缩](src/autorun/jvm/oop/Compressed_Oops.md) </br>
- [ ] [4.3 内存编织](src/autorun/jvm/oop/Memory_Weave.md) </br>

### 5. 方法调用
- [ ] [5.1 CallStub栈帧的创建](src/autorun/jvm/method/CALL_STUB.md) </br>
- [ ] [5.2 Java方法调用过程](src/autorun/jvm/method/JAVA_CALLS.md) </br>

### 6. JVMTI
- [x] [6.1 初识JVMTI](src/autorun/jvm/jvmti/README.md)
- [x] [6.2 使用JVMTI扩展NPE(JEP 358)](src/autorun/jvm/jvmti/richNPE/richNPE.cpp)
- [x] [6.3 使用JVMTI统计每个方法的调用次数](src/autorun/jvm/jvmti/methodCalledCount/methodCalledCount.cpp)
## 工具篇
### 1. JDK自带工具
- [X] [1.1 JINFO查看及修改运行时参数](src/autorun/jdk/tools/JINFO.md) </br>
- [ ] [1.2 JSTACK线程相关都靠我](src/autorun/jdk/tools/JSTACK.md) </br>

## Contributing

  如果您希望为本项目做出贡献,您可以在<a href='docs/CONTRIBUTING.md'>这里</a>找到更多信息. 我们感谢您做的出任何贡献.

