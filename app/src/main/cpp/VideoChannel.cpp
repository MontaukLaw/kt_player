#include "VideoChannel.h"

void drop_video_frame(queue<AVFrame *> &q) {
    if (!q.empty()) {
        AVFrame *frame = q.front();
        // 不是关键帧
        if (frame->key_frame == 0) {
            BaseChannel::release_av_frame(&frame);
            q.pop();
        }
    }
}

void drop_video_packet(queue<AVPacket *> &q) {
    if (!q.empty()) {
        AVPacket *packet = q.front();
        if (packet->flags != AV_PKT_FLAG_KEY) {
            BaseChannel::release_av_packet(&packet);
            q.pop();
        }
    }
}

VideoChannel::VideoChannel(int streamIndex, AVCodecContext *codecContext, AVRational timeBase, int fps)
        : BaseChannel(streamIndex, codecContext, timeBase), fps(fps) {

    frames.set_sync_handle(drop_video_frame);
    packets.set_sync_handle(drop_video_packet);
}

VideoChannel::~VideoChannel() {

}


// read file的消费者, 同时是play的生产者, 双重身份.
// 本质上, 这里应该是同步的, 不存在异步过程
void VideoChannel::video_decode() {
    AVPacket *packet = nullptr;

    while (isPlaying) {

        // 当数据包读取的速度过快, 而来不及进行解码的时候, 这里需要放慢解码的速度.
        // 注意这里要观察的是frames队列, 而不是packets队列
        if (isPlaying && frames.if_queue_full()) {
            av_usleep(10 * 1000);  // 10ms
            continue;
        }

        // LOGD("video_decode\n");
        int ret = packets.pop_from_queue(packet);  // 阻塞式获取队列中的数据包
        // 当外界要停止这个线程的时候，会把isPlaying设置为false，这个时候就会跳出循环，释放packet
        if (!isPlaying) {
            break;
        }
        if (!ret) {
            continue;
        }
        // 发送到ffmpeg的缓冲区
        ret = avcodec_send_packet(codecContext, packet);

        if (ret) {
            break;
        }

        AVFrame *frame = av_frame_alloc();
        ret = avcodec_receive_frame(codecContext, frame);
        if (ret == AVERROR(EAGAIN)) {
            // 如果B帧参考后面的数据失败, 可能是P帧还没有出来, 就会出现这个错误
            continue;
        } else if (ret != 0) {
            if (frame) {
                av_frame_free(&frame);
            }
            break;
        }

        // LOGD("video_decode success\n");
        // 将这一帧放入frames队列中
        frames.insert_to_queue(frame);

        av_packet_unref(packet);
        // 把自己的packet释放掉, 避免内存泄漏
        release_av_packet(&packet);
        // av_packet_unref(packet);
        // release_av_packet(&packet);
    }

    av_packet_unref(packet);
    release_av_packet(&packet);
}

// 解码后就进行播放
void VideoChannel::video_play() {
    AVFrame *frame = nullptr;
    uint8_t *dst_data[4];   // 原始yuv数据, 非常大的一个数组
    int dst_linesize[4];

    // 解码器上下文是BaseChannel的一个成员变量
    av_image_alloc(dst_data, dst_linesize, codecContext->width, codecContext->height,
                   AV_PIX_FMT_RGBA, 1);

    SwsContext *swsContext = sws_getContext(codecContext->width, codecContext->height,
                                            codecContext->pix_fmt,  // 输入的格式
                                            codecContext->width, codecContext->height,
                                            AV_PIX_FMT_RGBA,       // 输出的格式 RGBA, 安卓只能支持RGBA
                                            SWS_BILINEAR, nullptr, nullptr, nullptr);
    while (isPlaying) {
        // LOGD("video_play\n");
        int ret = frames.pop_from_queue(frame);
        if (!isPlaying) {
            break;
        }
        if (!ret) {
            continue;
        }
        // 格式转换, 将yuv420p转换成rgba
        sws_scale(
                // 输入
                swsContext, frame->data, frame->linesize, 0, codecContext->height,
                // 输出
                dst_data, dst_linesize);

        // 在渲染之前, 做音视频同步
        // 视频要根据音频的时间戳决定是否需要延迟渲染.

        // 用于防止有人编码的时候加了东西
        double extraDelay = frame->repeat_pict / (2 * fps);  // 重复的帧数

        // LOGD("extraDelay: %f", extraDelay);  // extraDelay: 0.000000

        double fpsDelay = 1.0 / fps;  // 1s/fps
        double realDelay = fpsDelay + extraDelay;

        // LOGD("realDelay: %f", realDelay);  // realDelay: 0.043478
        // 直接休眠是死的
        // av_usleep(realDelay * 1000000);

        // 实际上需要根据音频解码的时间戳来进行延迟
        double videoTimeStamp = frame->best_effort_timestamp * av_q2d(timeBase);
        double audioTimeStamp = audioChannel->audioTimeStamp;

        double timeDiff = videoTimeStamp - audioTimeStamp;
        if (timeDiff > 0) {
            // 表示video比较快
            if (timeDiff > 1) {
                // 如果差值大于1s, 就直接休眠1s
                av_usleep(realDelay * 2 * 1000000);
            } else {
                double usleepTime = (realDelay + timeDiff) * 1000000;
                LOGD("usleepTime: %f us", usleepTime);
                av_usleep(usleepTime);
            }

        } else if (timeDiff < 0) {
            // 即音频速度太慢, 需要视频通过丢弃帧来提高渲染速度
            // 丢弃一些帧, 但是不能丢太多, 否则会出现画面卡顿的情况

            if (timeDiff <= -0.05) {
                // 如果差值小于-0.05s, 就直接丢弃这一帧
                frames.sync();
                // release_av_frame(&frame);
                continue;
            }
        } else {
            // 如果差值为0, 就不需要延迟, 表示已经同步
        }

        // native window
        // 开始渲染
        renderCallback(dst_data[0], dst_linesize[0], codecContext->width, codecContext->height);

        // 渲染完之后, 释放frame内存
        av_frame_unref(frame);
        release_av_frame(&frame);
    }

    // 全部释放, 道德所致
    av_frame_unref(frame);
    release_av_frame(&frame);
    isPlaying = false;
    av_freep(&dst_data[0]);
    sws_freeContext(swsContext);

}

void *task_video_decode(void *args) {
    auto *videoChannel = static_cast<VideoChannel *>(args);
    videoChannel->video_decode();
    return nullptr;
}

void *task_video_play(void *args) {
    auto *videoChannel = static_cast<VideoChannel *>(args);
    videoChannel->video_play();
    return nullptr;
}


void VideoChannel::start() {

    isPlaying = true;
    // 1. 解码
    packets.set_work(true);

    // 第一个线程， 取出队列的数据包，解码成数据帧
    pthread_create(&pid_video_decode, nullptr, task_video_decode, this);
    // LOGD("decode pid:%d", pid);
    // getprocname(pid_video_decode, process_name, sizeof(process_name));

    frames.set_work(true);
    // LOGD("video pid:%s", process_name);
    // 第二个线程, 取出解码后的数据进行播放
    pthread_create(&pid_video_play, nullptr, task_video_play, this);
    // LOGD("play pid:%d", pid);

}

void VideoChannel::stop() {

}

void VideoChannel::setRenderCallback(RenderCallback func) {
    this->renderCallback = func;
}

void VideoChannel::setAudioChannel(AudioChannel *pChannel) {
    this->audioChannel = pChannel;
}


