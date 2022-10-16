#ifndef MY_APPLICATION_FFMPEG_PLAYER_KT_KTPLAYER_H
#define MY_APPLICATION_FFMPEG_PLAYER_KT_KTPLAYER_H

#include <cstring>
#include <pthread.h>
#include "JNICallbackHelper.h"
#include "AudioChannel.h"
#include "VideoChannel.h"
#include "util.h"

extern "C" {
#include <libavformat/avformat.h>
}

class KTPlayer {
private :
    char *data_source = 0;   // ！！！指针一定需要初始化
    pthread_t pid_prepare;
    pthread_t pid_start;
    AVFormatContext *formatContext = 0;
    AudioChannel *audioChannel = 0;
    VideoChannel *videoChannel = 0;
    JNICallbackHelper *helper = 0;
    bool isPlaying = 0;

    RenderCallback renderCallback;

public :
    KTPlayer(const char *data_source, JNICallbackHelper *helper);

    ~KTPlayer();

    void prepare();

    void _prepare();

    void start();

    void _start();

    void stop();

    void release();

    void setRenderCallback(RenderCallback callback);
    // void setRenderCallback(void *renderCallback);

};

#endif //MY_APPLICATION_FFMPEG_PLAYER_KT_KTPLAYER_H
