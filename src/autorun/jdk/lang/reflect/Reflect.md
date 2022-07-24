# 反射知识点

```java 
String val = System.getProperty("sun.reflect.noInflation");
if (val != null && val.equals("true")) {

sun.reflect.ReflectionFactory#newMethodAccessor
    if (noInflation) {
        // ASM 生成字节码
        sun.reflect.MethodAccessorGenerator#generate
    } else {
        sun.reflect.NativeMethodAccessorImpl#NativeMethodAccessorImpl

    }
```

> c++ invoke 实现
```c++
JVM_ENTRY(jobject, JVM_InvokeMethod(JNIEnv *env, jobject method, jobject obj, jobjectArray args0))
  Handle method_handle;
  if (thread->stack_overflow_state()->stack_available((address) &method_handle) >= JVMInvokeMethodSlack) {
    method_handle = Handle(THREAD, JNIHandles::resolve(method));
    Handle receiver(THREAD, JNIHandles::resolve(obj));
    objArrayHandle args(THREAD, objArrayOop(JNIHandles::resolve(args0)));
    oop result = Reflection::invoke_method(method_handle(), receiver, args, CHECK_NULL);
    jobject res = JNIHandles::make_local(THREAD, result);
    if (JvmtiExport::should_post_vm_object_alloc()) {
      oop ret_type = java_lang_reflect_Method::return_type(method_handle());
      assert(ret_type != NULL, "sanity check: ret_type oop must not be NULL!");
      if (java_lang_Class::is_primitive(ret_type)) {
        // Only for primitive type vm allocates memory for java object.
        // See box() method.
        JvmtiExport::post_vm_object_alloc(thread, result);
      }
    }
    return res;
  } else {
    THROW_0(vmSymbols::java_lang_StackOverflowError());
  }
JVM_END
```

> Reflection.invoke_method 实现
```c++
oop Reflection::invoke_method(oop method_mirror, Handle receiver, objArrayHandle args, TRAPS) {
  oop mirror             = java_lang_reflect_Method::clazz(method_mirror);
  int slot               = java_lang_reflect_Method::slot(method_mirror);
  bool override          = java_lang_reflect_Method::override(method_mirror) != 0;
  objArrayHandle ptypes(THREAD, objArrayOop(java_lang_reflect_Method::parameter_types(method_mirror)));

  oop return_type_mirror = java_lang_reflect_Method::return_type(method_mirror);
  BasicType rtype;
  if (java_lang_Class::is_primitive(return_type_mirror)) {
    rtype = basic_type_mirror_to_basic_type(return_type_mirror, CHECK_NULL);
  } else {
    rtype = T_OBJECT;
  }
  // 找instanceKlass
  instanceKlassHandle klass(THREAD, java_lang_Class::as_Klass(mirror));
  // 通过slot定位method指针地址
  Method* m = klass->method_with_idnum(slot);
  if (m == NULL) {
    THROW_MSG_0(vmSymbols::java_lang_InternalError(), "invoke");
  }
  // 将method指针包装成handle对象
  methodHandle method(THREAD, m);

  // 发起调用
  return invoke(klass, method, receiver, override, ptypes, rtype, args, true, THREAD);
}
```

> invoke实现
```c++
oop Reflection::invoke(instanceKlassHandle klass, methodHandle reflected_method,
                       Handle receiver, bool override, objArrayHandle ptypes,
                       BasicType rtype, objArrayHandle args, bool is_method_invoke, TRAPS) {
  ResourceMark rm(THREAD);

  methodHandle method;      // actual method to invoke
  KlassHandle target_klass; // target klass, receiver's klass for non-static

  // Ensure klass is initialized
  klass->initialize(CHECK_NULL);

  bool is_static = reflected_method->is_static();
  if (is_static) {
    // ignore receiver argument
    method = reflected_method;
    target_klass = klass;
  } else {
    // check for null receiver
    if (receiver.is_null()) {
      THROW_0(vmSymbols::java_lang_NullPointerException());
    }
    // Check class of receiver against class declaring method
    if (!receiver->is_a(klass())) {
      THROW_MSG_0(vmSymbols::java_lang_IllegalArgumentException(), "object is not an instance of declaring class");
    }
    // target klass is receiver's klass
    target_klass = KlassHandle(THREAD, receiver->klass());
    // no need to resolve if method is private or <init>
    if (reflected_method->is_private() || reflected_method->name() == vmSymbols::object_initializer_name()) {
      method = reflected_method;
    } else {
      // resolve based on the receiver
      if (reflected_method->method_holder()->is_interface()) {
        // resolve interface call
        if (ReflectionWrapResolutionErrors) {
          // new default: 6531596
          // Match resolution errors with those thrown due to reflection inlining
          // Linktime resolution & IllegalAccessCheck already done by Class.getMethod()
          method = resolve_interface_call(klass, reflected_method, target_klass, receiver, THREAD);
          if (HAS_PENDING_EXCEPTION) {
          // Method resolution threw an exception; wrap it in an InvocationTargetException
            oop resolution_exception = PENDING_EXCEPTION;
            CLEAR_PENDING_EXCEPTION;
            JavaCallArguments args(Handle(THREAD, resolution_exception));
            THROW_ARG_0(vmSymbols::java_lang_reflect_InvocationTargetException(),
                vmSymbols::throwable_void_signature(),
                &args);
          }
        } else {
          method = resolve_interface_call(klass, reflected_method, target_klass, receiver, CHECK_(NULL));
        }
      }  else {
        // if the method can be overridden, we resolve using the vtable index.
        assert(!reflected_method->has_itable_index(), "");
        int index = reflected_method->vtable_index();
        method = reflected_method;
        if (index != Method::nonvirtual_vtable_index) {
          // target_klass might be an arrayKlassOop but all vtables start at
          // the same place. The cast is to avoid virtual call and assertion.
          InstanceKlass* inst = (InstanceKlass*)target_klass();
          method = methodHandle(THREAD, inst->method_at_vtable(index));
        }
        if (!method.is_null()) {
          // Check for abstract methods as well
          if (method->is_abstract()) {
            // new default: 6531596
            if (ReflectionWrapResolutionErrors) {
              ResourceMark rm(THREAD);
              Handle h_origexception = Exceptions::new_exception(THREAD,
                     vmSymbols::java_lang_AbstractMethodError(),
                     Method::name_and_sig_as_C_string(target_klass(),
                     method->name(),
                     method->signature()));
              JavaCallArguments args(h_origexception);
              THROW_ARG_0(vmSymbols::java_lang_reflect_InvocationTargetException(),
                vmSymbols::throwable_void_signature(),
                &args);
            } else {
              ResourceMark rm(THREAD);
              THROW_MSG_0(vmSymbols::java_lang_AbstractMethodError(),
                        Method::name_and_sig_as_C_string(target_klass(),
                                                                method->name(),
                                                                method->signature()));
            }
          }
        }
      }
    }
  }

  // I believe this is a ShouldNotGetHere case which requires
  // an internal vtable bug. If you ever get this please let Karen know.
  if (method.is_null()) {
    ResourceMark rm(THREAD);
    THROW_MSG_0(vmSymbols::java_lang_NoSuchMethodError(),
                Method::name_and_sig_as_C_string(klass(),
                                                        reflected_method->name(),
                                                        reflected_method->signature()));
  }

  
  // In the JDK 1.4 reflection implementation, the security check is
  // done at the Java level
  if (!(JDK_Version::is_gte_jdk14x_version() && UseNewReflection)) {

  // Access checking (unless overridden by Method)
  if (!override) {
    if (!(klass->is_public() && reflected_method->is_public())) {
      bool access = Reflection::reflect_check_access(klass(), reflected_method->access_flags(), target_klass(), is_method_invoke, CHECK_NULL);
      if (!access) {
        return NULL; // exception
      }
    }
  }

  } // !(Universe::is_gte_jdk14x_version() && UseNewReflection)
  assert(ptypes->is_objArray(), "just checking");
  int args_len = args.is_null() ? 0 : args->length();
  // Check number of arguments
  if (ptypes->length() != args_len) {
    THROW_MSG_0(vmSymbols::java_lang_IllegalArgumentException(), "wrong number of arguments");
  }

  // Create object to contain parameters for the JavaCall
  JavaCallArguments java_args(method->size_of_parameters());

  if (!is_static) {
    java_args.push_oop(receiver);
  }

  for (int i = 0; i < args_len; i++) {
    oop type_mirror = ptypes->obj_at(i);
    oop arg = args->obj_at(i);
    if (java_lang_Class::is_primitive(type_mirror)) {
      jvalue value;
      BasicType ptype = basic_type_mirror_to_basic_type(type_mirror, CHECK_NULL);
      BasicType atype = unbox_for_primitive(arg, &value, CHECK_NULL);
      if (ptype != atype) {
        widen(&value, atype, ptype, CHECK_NULL);
      }
      switch (ptype) {
        case T_BOOLEAN:     java_args.push_int(value.z);    break;
        case T_CHAR:        java_args.push_int(value.c);    break;
        case T_BYTE:        java_args.push_int(value.b);    break;
        case T_SHORT:       java_args.push_int(value.s);    break;
        case T_INT:         java_args.push_int(value.i);    break;
        case T_LONG:        java_args.push_long(value.j);   break;
        case T_FLOAT:       java_args.push_float(value.f);  break;
        case T_DOUBLE:      java_args.push_double(value.d); break;
        default:
          THROW_MSG_0(vmSymbols::java_lang_IllegalArgumentException(), "argument type mismatch");
      }
    } else {
      if (arg != NULL) {
        Klass* k = java_lang_Class::as_Klass(type_mirror);
        if (!arg->is_a(k)) {
          THROW_MSG_0(vmSymbols::java_lang_IllegalArgumentException(), "argument type mismatch");
        }
      }
      Handle arg_handle(THREAD, arg);         // Create handle for argument
      java_args.push_oop(arg_handle); // Push handle
    }
  }

  assert(java_args.size_of_parameters() == method->size_of_parameters(), "just checking");

  // All oops (including receiver) is passed in as Handles. An potential oop is returned as an
  // oop (i.e., NOT as an handle)
  JavaValue result(rtype);
  JavaCalls::call(&result, method, &java_args, THREAD);

  if (HAS_PENDING_EXCEPTION) {
    // Method threw an exception; wrap it in an InvocationTargetException
    oop target_exception = PENDING_EXCEPTION;
    CLEAR_PENDING_EXCEPTION;
    JavaCallArguments args(Handle(THREAD, target_exception));
    THROW_ARG_0(vmSymbols::java_lang_reflect_InvocationTargetException(),
                vmSymbols::throwable_void_signature(),
                &args);
  } else {
    if (rtype == T_BOOLEAN || rtype == T_BYTE || rtype == T_CHAR || rtype == T_SHORT)
      narrow((jvalue*) result.get_value_addr(), rtype, CHECK_NULL);
    return box((jvalue*) result.get_value_addr(), rtype, CHECK_NULL);
  }
}
```



sun.reflect.NativeMethodAccessorImpl#invoke

```java
public Object invoke(Object obj, Object[] args)
        throws IllegalArgumentException, InvocationTargetException
    {
    // We can't inflate methods belonging to vm-anonymous classes because
    // that kind of class can't be referred to by name, hence can't be
    // found from the generated bytecode.
    // 计次来判断是否使用asm生成字节码。默认15次
    if (++numInvocations > ReflectionFactory.inflationThreshold()
            && !ReflectUtil.isVMAnonymousClass(method.getDeclaringClass())) {
        MethodAccessorImpl acc = (MethodAccessorImpl)
            new MethodAccessorGenerator().
                generateMethod(method.getDeclaringClass(),
                                method.getName(),
                                method.getParameterTypes(),
                                method.getReturnType(),
                                method.getExceptionTypes(),
                                method.getModifiers());
        parent.setDelegate(acc);
    }

    return invoke0(method, obj, args);
}

private static native Object invoke0(Method m, Object obj, Object[] args);
```

native好处：无需生成额外的class和类加载，减少方法区占用，直接使用klass即可
坏处：c++调用，性能相对低下

cglib好处： 纯java调用，性能相对较高
坏处：需要产生额外的class，占用方法区空间，如果量太大则可能会撑爆方法区。
