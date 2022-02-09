# Java方法调用过程

## 简介
> JavaCalls是本地代码执行Java方法调用的一个工具类，会创建一个新的栈帧，做必要的栈帧切换工作，保证新的栈帧与原有的栈帧被正确的链接起来。
> JavaCalls继承自AllStatic类，其定义位于hotspot/src/share/vm/runtime/javaCalls.hpp中，它的方法都是可以直接调用的静态方法，如下图：</br>

![](imgs/java_calls_1.png)

众所周知，`jvm` 在调用方法时有如下字节码指令

| 指令名             | 指令含义                 | 对应方法           |
|-----------------|----------------------|----------------|
| invokeinterface | 调用接口方法               | `call_virtual` | 
| invokevirtual   | 调用多态方法               | `call_virtual` |
| invokestatic | 调用静态方法               | `call_static`  |
| invokespecial | 调用私有方法、父类方法以及构造方法    | `call_special` |
| invokedynamic | 动态语言支持调用, `lambda` 表达式 | `call_virtual` |

其中, `invokedynamic` 比较特殊，在这里暂不赘述，如果有兴趣可以看看 `jdk` 的源码 
`java.lang.invoke.LambdaMetafactory` </br>
`java.lang.invoke.MethodHandle` </br>
`java.lang.invoke.CallSite`

## JavaCalls
源码位置: `hotspot/src/share/vm/runtime/javaCalls.cpp`
### call_special

```c++
void JavaCalls::call_special(JavaValue* result, KlassHandle klass, Symbol* name, Symbol* signature, 
                             JavaCallArguments* args, TRAPS) {
CallInfo callinfo;
LinkResolver::resolve_special_call(callinfo, klass, name, signature, KlassHandle(), false, CHECK);
methodHandle method = callinfo.selected_method();
assert(method.not_null(), "should have thrown exception");

// Invoke the method
JavaCalls::call(result, method, args, CHECK);
}
```

### call_static
```c++
void JavaCalls::call_static(JavaValue* result, KlassHandle klass, Symbol* name, 
                            Symbol* signature, JavaCallArguments* args, TRAPS) {
  CallInfo callinfo;
  LinkResolver::resolve_static_call(callinfo, klass, name, signature, KlassHandle(), false, true, CHECK);
  methodHandle method = callinfo.selected_method();
  assert(method.not_null(), "should have thrown exception");

  // Invoke the method
  JavaCalls::call(result, method, args, CHECK);
}
```

### call_virtual
```c++
void JavaCalls::call_virtual(JavaValue* result, KlassHandle spec_klass, Symbol* name, Symbol* signature, 
                             JavaCallArguments* args, TRAPS) {
  CallInfo callinfo;
  Handle receiver = args->receiver();
  KlassHandle recvrKlass(THREAD, receiver.is_null() ? (Klass*)NULL : receiver->klass());
  LinkResolver::resolve_virtual_call(
          callinfo, receiver, recvrKlass, spec_klass, name, signature,
          KlassHandle(), false, true, CHECK);
  methodHandle method = callinfo.selected_method();
  assert(method.not_null(), "should have thrown exception");

  // Invoke the method
  JavaCalls::call(result, method, args, CHECK);
}
```

### call



参考文献: https://blog.csdn.net/qq_31865983/article/details/102877069