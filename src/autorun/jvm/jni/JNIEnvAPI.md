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
| `FindClass()`| `jclass` | 根据symbol name获取已加载的类 |
| `DefineClass()`| `jclass` | 加载类 |


## 附录
### 参考文献</br>
[Java和C或C++的数据类型对照表](https://www.cnblogs.com/jkguo/p/11262741.html)
