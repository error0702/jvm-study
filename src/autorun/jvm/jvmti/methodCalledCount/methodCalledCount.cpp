#include <jvmti.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <map>
using namespace std;
map<string,int> methodCount;

typedef struct {

 /* JVMTI Environment */

 jvmtiEnv *jvmti;

 jboolean vm_is_started;

 /* Data access Lock */

 jrawMonitorID lock;

 } GlobalAgentData;

 static GlobalAgentData *gdata;

static void
enter_critical_section(jvmtiEnv *jvmti)
{
      jvmtiError error;
      error = jvmti->RawMonitorEnter(gdata->lock);
 }


void ExceptionCallback
    (jvmtiEnv *jvmti_env,
     JNIEnv* jni_env,
     jthread thread,
     jmethodID method,
     jlocation location,
     jobject exception,
     jmethodID catch_method,
     jlocation catch_location) {
        char *methodName;
        char *signature;
        char *generic;
         jvmti_env->GetMethodName(method,&methodName,&signature,&generic);
        jvmtiThreadInfo threadInfo;
        jvmti_env->GetThreadInfo(thread,&threadInfo);
        //  printf("In thread %s method name is: %s, signature is %s, generic is %s\n", threadInfo->name, methodName,signature,generic);
        std::cout << "Current thread " << threadInfo.name << ", method is " << methodName << ", signature is " << signature << " throw exception;" << std::endl;

        jint threads_count_ptr;
    jthread *threads_ptr;
    jvmtiError err  = jvmti_env->GetAllThreads(&threads_count_ptr,&threads_ptr);

     if (err != JVMTI_ERROR_NONE) {
        printf("(GetAllThreads) Error expected: %d, got: %d\n", JVMTI_ERROR_NONE,  err);
        printf("\n");
    }

    jvmtiThreadInfo info1;
    if (err == JVMTI_ERROR_NONE && threads_count_ptr >= 1) {

        int i = 0;

        printf("Thread Count: %d\n", threads_count_ptr);

        for ( i=0; i < threads_count_ptr; i++) {

            /* Make sure the stack variables are garbage free */

            (void)memset(&info1,0, sizeof(info1));

            jvmtiError err1 =  jvmti_env->GetThreadInfo(threads_ptr[i], &info1);

            if (err1 != JVMTI_ERROR_NONE) {

                printf("(GetThreadInfo) Error expected: %d, got: %d\n", JVMTI_ERROR_NONE, err1);

            printf("\n");

            }
        }
        printf("Running Thread#%d: %s, Priority: %d, context class loader:%s\n", i+1,info1.name,info1.priority,(info1.context_class_loader == NULL ? ": NULL" : "Not Null"));
    }
}


void MethodEntryCallback(jvmtiEnv *jvmti_env,
     JNIEnv* jni_env,
     jthread thread,
     jmethodID method) {
        char *methodName;
        char* signature_ptr;
        char* generic_ptr;
        jvmti_env->GetMethodName(method,&methodName,&signature_ptr,&generic_ptr);
        jclass clazz;
        jvmtiError err = jvmti_env->GetMethodDeclaringClass(method,&clazz);

        char* sourceName;
        err = jvmti_env->GetSourceFileName(clazz,&sourceName);
        if(err != 0) {
            printf("GetSourceFileName error code is %d\n",err);
            return;
        }
        string tmp = string(sourceName) + string("#") + string(methodName);
        ++methodCount[tmp];
}
static void
exit_critical_section(jvmtiEnv *jvmti)

 {

    //  jvmtiError error;

    jvmti->RawMonitorExit(gdata->lock);

    //  check_jvmti_error(jvmti, error, "Cannot exit with raw monitor");

 }


JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM* vm, char* options, void* reserved) {
    static GlobalAgentData data;

    (void)memset((void*)&data, 0, sizeof(data));

    gdata = &data;
    jvmtiEnv* jvmti;
    vm->GetEnv((void**) &jvmti, JVMTI_VERSION_1_0);
    gdata->jvmti = jvmti;

    jvmtiError error; 
    error = jvmti->CreateRawMonitor("agent data",&(gdata->lock));
    printf("load agent...\n");
    jvmtiCapabilities capa;
    (void)memset(&capa, 1, sizeof(jvmtiCapabilities));
    capa.can_generate_method_entry_events = 1;
    capa.can_generate_method_exit_events = 1;
    capa.can_generate_exception_events = 1;
    capa.can_get_source_file_name = 1;
    jvmti->AddCapabilities(&capa);

    jvmtiEventCallbacks callbacks = {0};
    callbacks.MethodEntry = MethodEntryCallback;
    callbacks.Exception = ExceptionCallback;
    jvmti->SetEventCallbacks(&callbacks, sizeof(callbacks));

    jvmti->SetEventNotificationMode(JVMTI_ENABLE,JVMTI_EVENT_EXCEPTION,NULL);
    jvmti->SetEventNotificationMode(JVMTI_ENABLE,JVMTI_EVENT_METHOD_ENTRY,NULL);
    jvmti->SetEventNotificationMode(JVMTI_ENABLE,JVMTI_EVENT_METHOD_EXIT,NULL);
    
    return 0;

}

JNIEXPORT void JNICALL Agent_OnUnload(JavaVM *vm)

 {
    auto b = methodCount.cbegin(), e = methodCount.cend();
    while (b != e) {
        std::cout << "Method name: " << b->first << " called " << b->second  << " nums." << std::endl;
        ++b;
    }
 }
