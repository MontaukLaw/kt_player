#ifndef MY_APPLICATION_FFMPEG_PLAYER_KT_JNICALLBACKHELPER_H
#define MY_APPLICATION_FFMPEG_PLAYER_KT_JNICALLBACKHELPER_H

#include <jni.h>
#include "util.h"

class JNICallbackHelper{

public:
    JNICallbackHelper(JavaVM *vm, JNIEnv *env, jobject instance);
    ~JNICallbackHelper();

    void onPrepared(int thread_mode);
    void onError(int thread_mode, int error_code, char * ffmpegErrorMsg);

private:
    JavaVM *vm = 0;
    JNIEnv *env = 0;
    jobject instance = 0;
    jmethodID jmd_prepared = 0;
    jmethodID jmd_error = 0;
};

#endif //MY_APPLICATION_FFMPEG_PLAYER_KT_JNICALLBACKHELPER_H
