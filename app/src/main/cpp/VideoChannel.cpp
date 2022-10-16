#include "VideoChannel.h"

VideoChannel::VideoChannel(int index, AVCodecContext *codecContext) : BaseChannel(index, codecContext) {

}

VideoChannel::~VideoChannel() {

}

// 消费者
void VideoChannel::video_decode() {
    AVPacket *packet = nullptr;

    while (isPlaying) {

        LOGD("video_decode\n");
        int ret = packets.get_queue_and_pop(packet);  // 阻塞式获取队列中的数据包
        // 当外界要停止这个线程的时候，会把isPlaying设置为false，这个时候就会跳出循环，释放packet
        if (!isPlaying) {
            break;
        }
        if (!ret) {
            continue;
        }
        // 发送到ffmpeg的缓冲区
        ret = avcodec_send_packet(codecContext, packet);

        // 把自己的packet释放掉
        release_av_packet(&packet);
        if (ret) {
            break;
        }

        AVFrame *frame = av_frame_alloc();
        ret = avcodec_receive_frame(codecContext, frame);
        if (ret == AVERROR(EAGAIN)) {
            // 如果B帧参考后面的数据失败, 可能是P帧还没有出来, 就会出现这个错误
            continue;
        } else if (ret != 0) {
            break;
        }

        // 将这一帧放回到队列中
        frames.insert_to_queue(frame);
    }

    release_av_packet(&packet);

}

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
        LOGD("video_play\n");
        int ret = frames.get_queue_and_pop(frame);
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

        // native window
        // 开始渲染
        renderCallback(dst_data[0], dst_linesize[0], codecContext->width, codecContext->height);

        // 释放frame
        release_av_frame(&frame);
    }

    release_av_frame(&frame);
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
    frames.set_work(true);

    // 第一个线程， 取出队列的数据包，解码成数据帧
    pthread_create(&pid_video_decode, nullptr, task_video_decode, this);

    // 第二个线程, 取出解码后的数据进行播放
    pthread_create(&pid_video_play, nullptr, task_video_play, this);
}

void VideoChannel::stop() {

}

void VideoChannel::setRenderCallback(RenderCallback renderCallback) {
    this->renderCallback = renderCallback;
}


