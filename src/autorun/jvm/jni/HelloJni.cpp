//
// Created by autorun on 2021/11/12.
//
#include <iostream>
#include "HelloJni.h"
using namespace std;

JNIEXPORT void JNICALL Java_HelloJni_hello(JNIEnv *env, jobject obj)
{
    // c语言写法
//    jclass jclass1 = (*env).GetObjectClass(obj);
//    // c++写法
//    jclass jclass2 = env->GetObjectClass(obj);
//
//    cout << jclass1 << endl;
//    cout << jclass2 << endl;


    jclass mapKlass = env->FindClass("java/util/HashMap");
    jmethodID constructor = env->GetMethodID(mapKlass, "<init>", "()V");
    jobject mapObj2 = env->NewObject(mapKlass, constructor);

    // toString()Ljava/lang/String;
    jobject toStringRst = env->CallObjectMethod(mapObj2, env->GetMethodID(mapKlass, "toString", "()Ljava/lang/String;"));
    cout << toStringRst << endl;

    // call hashmap put
    // put(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;
//    jint* value = new jint;
//    value = 5;
//    env->CallDoubleMethodA(mapObj2, env->GetMethodID(mapKlass, "put",
//                                                     "<(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;>"),
//                           value);
//    jobject putObj = env->CallObjectMethod(mapObj2,
//                                           env->GetMethodID(mapKlass, "put",
//                                                            "<(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;>"));

    // 获取 hashmap的类
//    jclass mapObjKlass = env->GetObjectClass(obj); // "java/util/HashMap"
//    jobject mapObj1 = env->AllocObject(mapObjKlass); // 只申请内存
//    jmethodID constructor = env->GetMethodID(mapObjKlass, "<init>", "()V ");
//    jobject mapObj2 = env->NewObject(mapObjKlass, constructor);
// TODO 尝试抛异常

env->ThrowNew(env->FindClass("java/lang/Error"), "c++ throw exception!");
    cout << "hello JNI!" << endl;
}
