#!/bin/bash

if [[ ! -f $(which g++) ]]; then
  echo 'not found in path'
  exit
fi

SOURCE_FILE=HelloJni.cpp
TARGET_PATH=../../../../test/autorun/jvm/jni/lib
TARGET_FILE=HelloJni.so
# use g++ in build cpp file
g++ -dynamiclib $SOURCE_FILE -o $TARGET_PATH/$TARGET_FILE

# shellcheck disable=SC2181
if [ 0 == $? ]; then
    echo 'build successfully'
else
  echo 'build fail!'
fi