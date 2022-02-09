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

```c++
void JavaCalls::call_helper(JavaValue* result, methodHandle* m, JavaCallArguments* args, TRAPS) {
  methodHandle method = *m;
  JavaThread* thread = (JavaThread*)THREAD;
  // 判断是否是java线程
  assert(thread->is_Java_thread(), "must be called by a java thread");
  assert(method.not_null(), "must have a method to call");
  // 是否进入safepoint
  assert(!SafepointSynchronize::is_at_safepoint(), "call to Java code during VM operation");
  assert(!thread->handle_area()->no_handle_mark_active(), "cannot call out to Java here");


  CHECK_UNHANDLED_OOPS_ONLY(thread->clear_unhandled_oops();)

  // Verify the arguments
  // 是否检查jni参数
  if (CheckJNICalls)  {
    args->verify(method, result->get_type(), thread);
  }
  else debug_only(args->verify(method, result->get_type(), thread));

  // Ignore call if method is empty
  if (method->is_empty_method()) {
    assert(result->get_type() == T_VOID, "an empty method must return a void value");
    return;
  }


#ifdef ASSERT
  { InstanceKlass* holder = method->method_holder();
    // A klass might not be initialized since JavaCall's might be used during the executing of
    // the <clinit>. For example, a Thread.start might start executing on an object that is
    // not fully initialized! (bad Java programming style)
    assert(holder->is_linked(), "rewritting must have taken place");
  }
#endif
  // 检查是否是编译线程
  assert(!thread->is_Compiler_thread(), "cannot compile from the compiler");
  // 是否需要编译此方法
  if (CompilationPolicy::must_be_compiled(method)) {
      // 编译方法
    CompileBroker::compile_method(method, InvocationEntryBci,
                                  CompilationPolicy::policy()->initial_compile_level(),
                                  methodHandle(), 0, "must_be_compiled", CHECK);
  }

  // Since the call stub sets up like the interpreter we call the from_interpreted_entry
  // so we can go compiled via a i2c. Otherwise initial entry method will always
  // run interpreted.
  address entry_point = method->from_interpreted_entry();
  if (JvmtiExport::can_post_interpreter_events() && thread->is_interp_only_mode()) {
    entry_point = method->interpreter_entry();
  }

  // Figure out if the result value is an oop or not (Note: This is a different value
  // than result_type. result_type will be T_INT of oops. (it is about size)
  // 获取方法返回值
  BasicType result_type = runtime_type_from(result);
  // 判断返回值是否是对象数组或者oop对象
  bool oop_result_flag = (result->get_type() == T_OBJECT || result->get_type() == T_ARRAY);

  // NOTE: if we move the computation of the result_val_address inside
  // the call to call_stub, the optimizer produces wrong code.
  // 获取到保存方法所调用的结果指针
  intptr_t* result_val_address = (intptr_t*)(result->get_value_addr());

  // Find receiver
  // 获取调用端，如果是静态则从args里取，否则使用handle代替
  Handle receiver = (!method->is_static()) ? args->receiver() : Handle();

  // When we reenter Java, we need to reenable the yellow zone which
  // might already be disabled when we are in VM.
  // 获取当前调用栈是否处于overflow, 如果是则抛出异常
  if (thread->stack_yellow_zone_disabled()) {
    thread->reguard_stack();
  }

  // Check that there are shadow pages available before changing thread state
  // to Java
  // 判断当前调用栈是否有足够的内存，如果不够则抛出异常
  if (!os::stack_shadow_pages_available(THREAD, method)) {
    // Throw stack overflow exception with preinitialized exception.
    Exceptions::throw_stack_overflow_exception(THREAD, __FILE__, __LINE__, method);
    return;
  } else {
    // Touch pages checked if the OS needs them to be touched to be mapped.
    // 申请内存
    os::bang_stack_shadow_pages();
  }

  // do call
  // 执行方法调用。
  { JavaCallWrapper link(method, receiver, result, CHECK);
    { HandleMark hm(thread);  // HandleMark used by HandleMarkCleaner

      StubRoutines::call_stub()(
        (address)&link,
        // (intptr_t*)&(result->_value), // see NOTE above (compiler problem)
        result_val_address,          // see NOTE above (compiler problem)
        result_type,
        method(),
        entry_point,
        args->parameters(),
        args->size_of_parameters(),
        CHECK
      );

      // 处理调用结果
      result = link.result();  // circumvent MS C++ 5.0 compiler bug (result is clobbered across call)
      // Preserve oop return value across possible gc points
      // 将结果设置到当前线程的vm_result中，
      if (oop_result_flag) {
        thread->set_vm_result((oop) result->get_jobject());
      }
    }
  } // Exit JavaCallWrapper (can block - potential return oop must be preserved)

  // Check if a thread stop or suspend should be executed
  // The following assert was not realistic.  Thread.stop can set that bit at any moment.
  //assert(!thread->has_special_runtime_exit_condition(), "no async. exceptions should be installed");

  // Restore possible oop return
  // 设置返回结果，将vm_result清空
  if (oop_result_flag) {
    result->set_jobject((jobject)thread->vm_result());
    thread->set_vm_result(NULL);
  }
}

void JavaCalls::call(JavaValue* result, methodHandle method, JavaCallArguments* args, TRAPS) {
  // Check if we need to wrap a potential OS exception handler around thread
  // This is used for e.g. Win32 structured exception handlers
  assert(THREAD->is_Java_thread(), "only JavaThreads can make JavaCalls");
  // Need to wrap each and everytime, since there might be native code down the
  // stack that has installed its own exception handlers
  os::os_exception_wrapper(call_helper, result, &method, args, THREAD);
}

// os_linux.cpp
void
os::os_exception_wrapper(java_call_t f, JavaValue* value, const methodHandle& method,
JavaCallArguments* args, JavaThread* thread) {
f(value, method, args, thread);
}
```

`call_helper`

参考文献: https://blog.csdn.net/qq_31865983/article/details/102877069