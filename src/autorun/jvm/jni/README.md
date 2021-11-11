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
5. 在 `jdk` 的 `$JAVA_HOME` 中将 `jni.h` 拷贝到工程项目中，并且按照第四步将 `jni.h` 文件增加到 `CMakeLists.txt` 中
6. 同时 `jni.h` 中用到了 `jni_md.h` 的头文件。按照第五步的操作方式
7. 编写 `.h` 头文件的实现。