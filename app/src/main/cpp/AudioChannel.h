#ifndef MY_APPLICATION_FFMPEG_PLAYER_KT_AUDIOCHANNEL_H
#define MY_APPLICATION_FFMPEG_PLAYER_KT_AUDIOCHANNEL_H

#include "BaseChannel.h"
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

extern "C" {
#include "libswresample/swresample.h"  // 重采样
};

// cpp 继承
class AudioChannel : public BaseChannel {


public:

    int out_channels;
    int out_sample_size;
    int out_sample_rate;
    int out_buffers_size;
    uint8_t *out_buffers = 0;
    int bufferSize = 0;
    SwrContext *swr_ctx;
    double audioTimeStamp;    // 音频时间戳

public:
    AudioChannel(int index, AVCodecContext *codecContext, AVRational timeBase);

    virtual ~AudioChannel();

    void start();

    void stop();

    void audio_decode();

    void audio_play();

    int get_pcm_size(void);

private:

    pthread_t pid_audio_decode;
    pthread_t pid_audio_play;

    // 引擎
    SLObjectItf engineObject = nullptr;
    // 引擎接口
    SLEngineItf engineInterface = nullptr;

    // 混音器
    SLObjectItf outputMixObject = nullptr;
    // 播放器
    SLObjectItf bqPlayerObject = nullptr;
    // 播放器接口
    SLPlayItf bqPlayerPlay = nullptr;

    // 播放器队列接口
    SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue = nullptr;

};

#endif //MY_APPLICATION_FFMPEG_PLAYER_KT_AUDIOCHANNEL_H
