# JNI学习笔记

## JNI头文件生成
1. 创建 `.java` 文件
2. 增加 `native` 的相关方法
3. 使用 `javac` 生成 `.java` 文件的 `class` 文件和对应的 `.h` 头文件
    `javac -h jni_target_dictory jni.java file`
    > case:</br>
   >  `javac -h src/autorun/jvm/jni/ src/autorun/jvm/jni/HelloJni.java`
4. 如果使用clion类似工具开发时则需要在 `CMakeLists.txt` 增加生成好的 `.h` 文件方便编译
> 在 `CMakeLists.txt` 的 `add_executable` 步骤中增加对应的 `.h` 文件。使IDE找到对应的头文件
5. 编译时建议直接通过 include 参数引用 `$JAVA_HOME/include` 以及对应平台目录, 例如 Linux 的 `$JAVA_HOME/include/linux`、macOS 的 `$JAVA_HOME/include/darwin`。不建议把 JDK 自带的 `jni.h`、`jni_md.h` 拷贝到工程中维护。
6. 编写 `.h` 头文件的实现。

## jvm 在生成 static 与 实例对象的方法时的差异
1. 首先如果是 `static` 的 `native` 方法, 参数名为 `jclass`. 换句话说 `jclass` 是拿到的被 `JNIHandles::make_local(klass)` 包装过的 `klass` 实例。
更准确地说, JNI 层拿到的是声明该 native 方法的 `java.lang.Class` 对象的 JNI 引用。HotSpot 内部会通过 mirror 和 Klass 建立关联, 但在 JNI API 视角下不应直接把 `jclass` 理解为 Klass 实例。

## 编译环节
1. 编译 `so` 文件. 如果是c语言也就是.c文件编译时使用
`gcc -fPIC -shared sourceFile -o libxxx.so`
如果是c++ 则将上面命令中的 `gcc` 替换为 `g++` 即可。macOS 生成 `.dylib` 时可使用 `-dynamiclib`。

## 加载环节
1. 编写java代码，加载时可以使用 `System.load(file)` 或者使用 `System.loadLibrary(libname)` 加载对应的本地包

`System.load(file)` 和 `System.loadLibrary(libname)` 差别。参考如下链接</br>
[Java System.load() 与 System.loadLibrary() 区别解析](https://www.jianshu.com/p/c8a575ad344f)
