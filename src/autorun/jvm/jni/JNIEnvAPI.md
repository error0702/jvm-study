# JNIEnv的一些API

## 数据类型映射
<hr/>

> java的数据类型和C++的对应关系（大部分可用，小部分需要根据实际情况调整）

| Java类型 | 本地类型 | 描述 |
| :---: | :---: | :---: |
| boolean | jboolean | C/C++8位整型|
| byte | jbyte | C/C++带符号的8位整型|
| char | jchar | C/C++无符号的16位整型|
| short | jshort | C/C++带符号的16位整型|
| int | jint | C/C++带符号的32位整型|
| long | jlong | C/C++带符号的64位整型|
| float | jfloat | C/C++32位浮点型|
| double | jdouble | C/C++64位浮点型|
| Object | jobject | 任何Java对象，或者没有对应java类型的对象|
| Class | jclass | Class对象|
| String | jstring | 字符串对象|
| Object[] | jobjectArray | 任何对象的数组|
| boolean[] | jbooleanArray | 布尔型数组|
| byte[] | jbyteArray | 比特型数组|
| char[] | jcharArray | 字符型数组|
| short[] | jshortArray | 短整型数组|
| int[] | jintArray | 整型数组|
| long[] | jlongArray | 长整型数组|
| float[] | jfloatArray | 浮点型数组|
| double[] | jdoubleArray | 双浮点型数组|

> jni 对应的据类型

|函数 |	Java 数组类型 | 本地类型 |
| :---: | :---: | :---: |
|GetBooleanArrayElements |	jbooleanArray | jboolean |
|GetByteArrayElements |	jbyteArray | jbyte |
|GetCharArrayElements |	jcharArray | jchar |
|GetShortArrayElements |	jshortArray | jshort |
|GetIntArrayElements |	jintArray | jint |
|GetLongArrayElements |	jlongArray | jlong |
|GetFloatArrayElements |	jfloatArray | jfloat |
|GetDoubleArrayElements |	jdoubleArray | jdouble |
 
### JNI相关函数
> 参照 [jni.h](base/jni.h) `struct JNIEnv_` 部分，或者jdk安装目录中 `include` 目录下的 `jni.h` 头文件

| 函数名 | 返回值 | 描述 |
| :---: | :---: | :---: |
| `GetVersion()`| `jint` | 获取版本号 |
| `DefineClass()`| `jclass` | 加载类 |
| `FindClass()`| `jclass` | 根据类名获取已加载的类 |
| `FromReflectedMethod()`| `jmethodID` | 通过反射获取方法 |
| `FromReflectedField()`| `jfieldID` | 通过反射获取字段 |
| `ToReflectedMethod()`| `jobject` | 通过class,methodId,是否是静态获取方法 |
| `GetSuperclass()`| `jclass` | 获取父类 |
| `IsAssignableFrom()`| `jboolean` | 确定传入类的继承结构是否被包含在调用方内。参照 `java.lang.Class.isAssignableFrom()` |
| `ToReflectedField()`| `jobject` | 反射获取字段 |
| `Throw()`| `jint` | 抛出异常 |
| `ThrowNew()`| `jint` | 抛出异常, 参数不同 |
| `ExceptionOccurred()`| `jthrowable` | 获取发生异常的信息 |
| `ExceptionDescribe()`| `void` | 订阅异常, jvm内部会使用标准输出打印异常信息 |
| `FatalError()`| `void` | 致命错误, jvm会调用os dump现场 |
| `PushLocalFrame()`| `jint` | //TODO ... |
| `PopLocalFrame()`| `jint` | //TODO ... |
| `NewGlobalRef()`| `jobject` | 在全局区创建引用 |
| `DeleteGlobalRef()`| `jobject` | 删除全局区引用 |
| `DeleteLocalRef()`| `jobject` | 删除局部区引用 |
| `IsSameObject()`| `jboolean` | 比较俩对象是否相等 |
| `NewLocalRef()`| `jobject` | 创建本地引用 |
| `EnsureLocalCapacity()`| `jint` | 确保本地容量在参数capacity区间 |
| `AllocObject()`| `jobject` | 根据jclass 分配对象空间 |
| `NewObject()`| `jobject` | 根据jclass和method创建对象 |
| `NewObjectV()`| `jobject` | 根据jclass和method创建对象, 通过`va_list` 传参 |
| `NewObjectA()`| `jobject` | 根据jclass和method创建对象, 通过 `jvalue *args` 传参 |
| `GetObjectClass()`| `jclass` | 通过对象获取其`jklass` 对象 |
| `IsInstanceOf()`| `jboolean` | 比较实例 `Klass` 是否相同 |
| `GetMethodID()`| `jmethodID` | 获取 `methodId` |
| `CallObjectMethod()`| `jobject` | 根据 `methodId` 调用method |
| `CallObjectMethodV()`| `jobject` | 根据 `methodId` 调用method, 含义同 `NewObjectV()` |
| `CallObjectMethodA()`| `jobject` | 根据 `methodId` 调用method, 含义同 `NewObjectA()` |
| `CallBooleanMethod()`| `jboolean` | 根据 `methodId` 调用boolean method |
| `CallBooleanMethodV()`| `jboolean` | 根据 `methodId` 调用boolean method 含义同 `NewObjectV()`|
| `CallBooleanMethodA()`| `jboolean` | 根据 `methodId` 调用boolean method 含义同 `NewObjectA()`|
| `Call*Method()`| `jboolean` | 根据 `methodId` 调用 各种类型的method |
| `CallNonvirtual*Method()`| `*` | 根据 `methodId` 调用 各种类型的非虚method |
| `GetFieldID()`| `jfieldID` | 根据 `field` 获取field信息 |
| `Get*FieldID()`| `*` | 根据 `field` 获取各种类型的field信息 |
| `Set*Field()`| `*` | 根据 `field` 设置各种类型的field值 |
| `CallStatic*Method()`| `*` | 调用各种类型的静态java方法 |
| `NewString()`| `jstring` | 创建`string`对象 |
| `GetStringLength()`| `jsize` | 获取`string`对象的长度, `jsize` 等同于 `jint` |
| `GetStringChars()`| `const jchar*` | 获取`string`对象的值, 转为char数组类型 |
| `ReleaseStringChars()`| `void` | 释放字符串空间 |
| `NewStringUTF()`| `jstring` | 创建utf的 `string` 对象 |
| `GetStringUTFLength()`| `jsize` | 获取 `stringUTF` 字符串对象的长度 |











## 附录
### 参考文献</br>
[Java和C或C++的数据类型对照表](https://www.cnblogs.com/jkguo/p/11262741.html)
