#ifndef MY_APPLICATION_FFMPEG_PLAYER_KT_VIDEOCHANNEL_H
#define MY_APPLICATION_FFMPEG_PLAYER_KT_VIDEOCHANNEL_H

#include "util.h"
extern "C" {
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}
#include "BaseChannel.h"

// 定义函数指针
typedef void (*RenderCallback)(uint8_t *, int, int, int);

// 继承
class VideoChannel:public BaseChannel {

private :
    pthread_t pid_video_decode;
    pthread_t pid_video_play;
    RenderCallback renderCallback;

public:
    VideoChannel(int index, AVCodecContext *codecContext);

    ~VideoChannel();

    void start();

    void stop();

    void video_decode();

    void video_play();

    void setRenderCallback(RenderCallback renderCallback);
};

#endif //MY_APPLICATION_FFMPEG_PLAYER_KT_VIDEOCHANNEL_H
