#ifndef MY_APPLICATION_FFMPEG_PLAYER_KT_AUDIOCHANNEL_H
#define MY_APPLICATION_FFMPEG_PLAYER_KT_AUDIOCHANNEL_H

#include "BaseChannel.h"

// cpp 继承
class AudioChannel : public BaseChannel {

public:
    AudioChannel(int index, AVCodecContext *codecContext);

    virtual ~AudioChannel();

    void start();

    void stop();
};

#endif //MY_APPLICATION_FFMPEG_PLAYER_KT_AUDIOCHANNEL_H
