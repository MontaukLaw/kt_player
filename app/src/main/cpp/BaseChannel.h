#ifndef MY_APPLICATION_FFMPEG_PLAYER_KT_BASECHANNEL_H
#define MY_APPLICATION_FFMPEG_PLAYER_KT_BASECHANNEL_H

extern "C" {
#include <libavcodec/avcodec.h>
};

#include "safe_queue.h"

class BaseChannel {

public:
    int streamIndex = -1;  // 音频或者视频的下标
    SafeQueue<AVPacket *> packets;   // 压缩的数据包, 包含音频或者视频
    SafeQueue<AVFrame *> frames;     // 解码后的数据帧, 可能是音频也可能是视频
    bool isPlaying;                  // 音频或者视频是否正在播放
    AVCodecContext *codecContext = 0;  // 解码器上下文

    BaseChannel(int index, AVCodecContext *codecContext) : streamIndex(streamIndex), codecContext(codecContext) {
        packets.set_release_callback(release_av_packet);
        frames.set_release_callback(release_av_frame);
    }

    virtual ~BaseChannel() {
        packets.clear();
        frames.clear();
    }

    // 释放队列中的AVPacket
    static void release_av_packet(AVPacket **packet) {
        if (packet) {
            av_packet_free(packet);
            *packet = 0;
        }
    }

    // 释放队列中的AVFrame
    static void release_av_frame(AVFrame **frame) {
        if (frame) {
            av_frame_free(frame);
            *frame = 0;
        }
    }

    void set_playing(bool playing) {
        this->isPlaying = playing;
    }
};

#endif //MY_APPLICATION_FFMPEG_PLAYER_KT_BASECHANNEL_H
