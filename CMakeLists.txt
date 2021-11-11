cmake_minimum_required(VERSION 3.20)
project(jvm_study)

set(CMAKE_CXX_STANDARD 11)

add_executable(jvm_study main.cpp
        src/autorun/jvm/jni/HelloJni.h
        src/autorun/jvm/jni/base/jni.h)
