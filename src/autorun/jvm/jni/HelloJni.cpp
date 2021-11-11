//
// Created by autorun on 2021/11/12.
//
#include <iostream>
#include "HelloJni.h"
using namespace std;

JNIEXPORT void JNICALL Java_HelloJni_hello(JNIEnv *env, jobject obj)
{
    cout << "hello JNI!" << endl;
}
