#include <jni.h>
#include <string>
#include "KTPlayer.h"
#include "JNICallbackHelper.h"

JavaVM *vm = 0;
jint JNI_OnLoad(JavaVM *vm, void *args) {
    ::vm = vm;
    return JNI_VERSION_1_6;
}

void render_callback(uint8_t *data, int linesize, int w, int h) {
    // 渲染
}

extern "C"
JNIEXPORT jlong JNICALL
Java_com_example_myapplicationffmpegplayerkt_KTPlayer_prepareNative(JNIEnv *env, jobject thiz, jstring data_source) {
    const char * data_source_ = env->GetStringUTFChars(data_source, nullptr);

    auto *helper = new JNICallbackHelper(vm ,env, thiz);

    auto *player = new KTPlayer(data_source_, helper);      // 有意为之的，开辟堆空间，不能释放

    // 将本地的函数指针, 传给player的成员变量
    player->setRenderCallback(render_callback);
    player->prepare();

    env->ReleaseStringUTFChars(data_source, data_source_);

    // 将cpp对象的指针传到java层
    return reinterpret_cast<jlong>(player);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_myapplicationffmpegplayerkt_KTPlayer_startNative(JNIEnv *env, jobject thiz, jlong native_obj) {
     auto *player = reinterpret_cast<KTPlayer *>(native_obj);
     player->start();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_myapplicationffmpegplayerkt_KTPlayer_stopNative(JNIEnv *env, jobject thiz, jlong native_obj) {
    // TODO: implement stopNative()
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_myapplicationffmpegplayerkt_KTPlayer_releaseNative(JNIEnv *env, jobject thiz, jlong native_obj) {
    // TODO: implement releaseNative()
}


extern "C"
JNIEXPORT void JNICALL
Java_com_example_myapplicationffmpegplayerkt_KTPlayer_setSurfaceNative(JNIEnv *env, jobject thiz, jlong native_obj, jobject surface) {

    // 要将surface传递到VideoChannel的渲染部分

}