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