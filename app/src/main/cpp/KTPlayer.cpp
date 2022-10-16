#include "KTPlayer.h"
#include "JNICallbackHelper.h"

extern "C" {
#include <libavformat/avformat.h>
}

KTPlayer::KTPlayer(const char *data_source, JNICallbackHelper *helper) {
    // 需要对data_source进行拷贝，因为data_source是局部变量，会被释放
    this->data_source = new char[strlen(data_source) + 1];

    // 拷贝data_source
    strcpy(this->data_source, data_source);

    this->helper = helper;
}

KTPlayer::~KTPlayer() {
    if (data_source) {
        delete data_source;
        data_source = nullptr;
    }
    if (helper) {
        delete helper;
        helper = nullptr;
    }
}

// 线程5 把原始包
void *play_thread(void *args) {
    return nullptr;
}

// 读取队列中的AVPacket*, 解码成AVFrame* , 放入另一个队列
// 线程4
void *read_queue_thread(void *args) {
    return nullptr;

}

// 第3个线程， 把mp4文件里里面的数据包读出来， 放入队列
void *read_package_thread(void *args) {
    return nullptr;
}

// 第2个线程, 用于启动解码器进行解码
void *start_thread(void *args) {
    // 强转回KTPlayer对象
    auto *player = static_cast<KTPlayer *>(args);
    player->_start();

    return nullptr;
}

void KTPlayer::_start() {

    while (isPlaying) {
        // 从队列中取出一个AVPacket 可能是音频也可能是视频
        AVPacket *packet = av_packet_alloc();
        // 从队列中取出一个AVPacket
        int ret = av_read_frame(formatContext, packet);
        if (!ret) {
            // 读取成功
            if (audioChannel && packet->stream_index == audioChannel->streamIndex) {
                // 音频包
                audioChannel->packets.insert_to_queue(packet);
            } else if (videoChannel && packet->stream_index == videoChannel->streamIndex) {
                // 视频包
                videoChannel->packets.insert_to_queue(packet);
            }
        } else if (ret == AVERROR_EOF) {
            // 读取完毕
            if (audioChannel->packets.empty() && audioChannel->frames.empty() &&
                videoChannel->packets.empty() && videoChannel->frames.empty()) {
                // 音频和视频都播放完毕
                break;
            }
        } else {
            // 读取失败
            break;
        }
    }

    isPlaying = false;

    videoChannel->stop();
    audioChannel->stop();
}

void KTPlayer::start() {
    isPlaying = true;

    if (videoChannel) {
        videoChannel->set_playing(true);
        videoChannel->start();
    }
    if (audioChannel) {
        audioChannel->set_playing(true);
        audioChannel->start();
    }

    // 创建子线程
    pthread_create(&pid_start, nullptr, start_thread, this);
}

// 子线程的执行函数
// 第1个线程
void *task_prepare(void *args) {
    // 强转回KTPlayer对象
    auto *player = static_cast<KTPlayer *>(args);
    player->_prepare();

    // 必须给返回值，否则运行会报错
    return nullptr;
}

// 这个函数是给java调用的。在java层调用这个函数，会开启一个子线程，子线程会调用_prepare()函数
void KTPlayer::prepare() {
    // 问题：当前的prepare函数，子线程，还是，主线程？
    // 此函数是被MainActivity的onResume调用下来的（安卓的主线程）

    // 解封装 FFMpeg来解析 data_source 可以直接解析吗？
    // 答：data_source == 文件IO流, 直播网络rtmp, 所以按道理来说，会耗时，所以必须使用子线程

    // 创建子线程 pthread
    pthread_create(&pid_prepare, nullptr, task_prepare, this); // this == DerryPlayer的实例
}

void KTPlayer::_prepare() {

    formatContext = avformat_alloc_context();

    AVDictionary *opts = nullptr;
    av_dict_set(&opts, "timeout", "5000000", 0);

    /* 1. AVFormatContext
     * 2. url: 文件路径或者直播地址
     * 3. 输入的封装格式
     * 4. 参数
     */
    int ret = avformat_open_input(&formatContext, this->data_source, 0, &opts);

    // 释放字典
    av_dict_free(&opts);

    if (ret) {
        if (helper) {
            // 获取ffmpeg的错误提示str
            char *errInfo = av_err2str(ret);
            helper->onError(THREAD_CHILD, FFMPEG_CAN_NOT_OPEN_URL, errInfo);
        }
        return;
    }

    // 读取媒体信息
    ret = avformat_find_stream_info(formatContext, 0);
    if (ret < 0) {
        if (helper) {
            // 获取ffmpeg的错误提示str
            char *errInfo = av_err2str(ret);
            helper->onError(THREAD_CHILD, FFMPEG_CAN_NOT_FIND_STREAMS, errInfo);
        }
        return;
    }

    // 遍历视频流的个数
    for (int i = 0; i < this->formatContext->nb_streams; ++i) {

        // 获取流
        AVStream *stream = this->formatContext->streams[i];

        // 获取解码器的参数
        AVCodecParameters *codecpar = stream->codecpar;

        // 获取解码器
        AVCodec *codec = avcodec_find_decoder(codecpar->codec_id);

        // 创建解码器上下文
        AVCodecContext *codecContext = avcodec_alloc_context3(codec);
        if (!codecContext) {
            if (helper) {
                helper->onError(THREAD_CHILD, FFMPEG_ALLOC_CODEC_CONTEXT_FAIL, "avcodec_alloc_context3 fail");
            }
            return;
        }

        // 将视频流的信息赋值给解码器上下文
        ret = avcodec_parameters_to_context(codecContext, codecpar);
        if (ret < 0) {
            if (helper) {
                // 获取ffmpeg的错误提示str
                char *errInfo = av_err2str(ret);
                helper->onError(THREAD_CHILD, FFMPEG_CODEC_CONTEXT_PARAMETERS_FAIL, errInfo);
            }
            return;
        }

        // 打开解码器
        ret = avcodec_open2(codecContext, codec, 0);
        if (ret) {
            if (helper) {
                char *errInfo = av_err2str(ret);
                helper->onError(THREAD_CHILD, FFMPEG_OPEN_DECODER_FAIL, errInfo);
            }

            return;
        }

        // 获取流的类型
        if (codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioChannel = new AudioChannel(0, nullptr);

        } else if (codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoChannel = new VideoChannel(0, nullptr);
            videoChannel->setRenderCallback(this->renderCallback);
        }

        // 准备成功, 把消息传到到Activity
        if (helper) {
            helper->onPrepared(THREAD_CHILD);
        }

    } // for end ----------------------------

    // 如果音频和视频都不存在
    if (!audioChannel && !videoChannel) {
        if (helper) {
            helper->onError(THREAD_CHILD, FFMPEG_NO_MEDIA, "no media");
        }
        return;
    }

    if (helper) {
        helper->onPrepared(THREAD_CHILD);
    }
}

void KTPlayer::setRenderCallback(RenderCallback callback) {
    this->renderCallback = callback;
}
