#include "JNICallbackHelper.h"

JNICallbackHelper::JNICallbackHelper(JavaVM *vm, JNIEnv *env, jobject instance) {
    this->vm = vm;
    this->env = env;
    this->instance = env->NewGlobalRef(instance);
    jclass clazz = env->GetObjectClass(instance);
    if (!clazz) {
        return;
    }
    jmd_prepared = env->GetMethodID(clazz, "onPrepared", "()V");
    // java层处理错误的方法名为onError，参数为int类型，返回值为void
    jmd_error = env->GetMethodID(clazz, "onError", "(ILjava/lang/String;)V");
}

JNICallbackHelper::~JNICallbackHelper() {
    env->DeleteGlobalRef(instance);
    vm = nullptr;
    instance = nullptr;
    env = nullptr;
}

void JNICallbackHelper::onPrepared(int thread_mode) {
    if (thread_mode == THREAD_MAIN) {
        // 主线程
        env->CallVoidMethod(instance, jmd_prepared);
    } else {
        // 子线程
        JNIEnv *envChild;
        vm->AttachCurrentThread(&envChild, 0);
        envChild->CallVoidMethod(instance, jmd_prepared);
        vm->DetachCurrentThread();
    }
}

void JNICallbackHelper::onError(int thread_mode, int error_code, char *ffmpegErrorMsg) {

    if (thread_mode == THREAD_MAIN) {
        env->CallVoidMethod(instance, jmd_error, error_code, ffmpegErrorMsg);
    } else {
        JNIEnv *envChild;
        vm->AttachCurrentThread(&envChild, nullptr);
        jstring ffmpegErrorMsg_ = envChild->NewStringUTF(ffmpegErrorMsg);
        envChild->CallVoidMethod(instance, jmd_error, error_code, ffmpegErrorMsg_);
        vm->DetachCurrentThread();
    }
}

void JNICallbackHelper::get_pid() {
    // First, we have to find Thread class
    jclass cls = this->env->FindClass("java/lang/Thread");

    // Then, we can look for it's static method 'currentThread'
    /* Remember that you can always get method signature using javap tool
     > javap -s -p java.lang.Thread | grep -A 1 currentThread
         public static native java.lang.Thread currentThread();
           descriptor: ()Ljava/lang/Thread;
    */
    jmethodID mid = this->env->GetStaticMethodID(cls, "currentThread", "()Ljava/lang/Thread;");

    // Once you have method, you can call it. Remember that result is
    // a jobject
    jobject thread = this->env->CallStaticObjectMethod(cls, mid);
    if (thread == nullptr) {
        LOGE("Error while calling static method: currentThread\n");
    }

    // Now, we have to find another method - 'getId'
    /* Remember that you can always get method signature using javap tool
         > javap -s -p java.lang.Thread | grep -A 1 getId
             public long getId();
               descriptor: ()Jjavap -s -p java.lang.Thread | grep -A 1 currentThread
    */
    jmethodID mid_getid = this->env->GetMethodID(cls, "getId", "()J");
    if (mid_getid == nullptr) {
        LOGE("Error while calling GetMethodID for: getId\n");
    }

    // This time, we are calling instance method, note the difference
    // in Call... method
    jlong tid = this->env->CallLongMethod(thread, mid_getid);

    // Finally, let's call 'getName' of Thread object
    /* Remember that you can always get method signature using javap tool
         > javap -s -p java.lang.Thread | grep -A 1 getName
             public final java.lang.String getName();
               descriptor: ()Ljava/lang/String;
    */
    jmethodID mid_getname = this->env->GetMethodID(cls, "getName", "()Ljava/lang/String;");

    if (mid_getname == nullptr) {
        LOGE("Error while calling GetMethodID for: getName\n");
    }

    // As above, we are calling instance method
    jobject tname = this->env->CallObjectMethod(thread, mid_getname);

    // Remember to retrieve characters from String object
    const char *c_str;
    c_str = this->env->GetStringUTFChars(static_cast<jstring>(tname), NULL);
    if (c_str == nullptr) {
        return;
    }

    // display message from JNI
    LOGD("[C   ] name: %s id: %ld\n", c_str, tid);

    // and make sure to release allocated memory before leaving JNI
    this->env->ReleaseStringUTFChars(static_cast<jstring>(tname), c_str);
}
