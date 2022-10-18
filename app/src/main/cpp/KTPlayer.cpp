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

// 第2个子线程, 用于启动解码器进行解码
void *start_thread(void *args) {
    // 强转回KTPlayer对象
    auto *player = static_cast<KTPlayer *>(args);
    player->read_media_file();

    return nullptr;
}

void KTPlayer::read_media_file() {

    while (isPlaying) {

        // 等待读出来的数据被解码器消费
        if (videoChannel && videoChannel->packets.if_queue_full()) {
            av_usleep(10 * 1000); // 10ms
            continue;
        }

        if (audioChannel && audioChannel->packets.if_queue_full()) {
            av_usleep(10 * 1000);
            continue;
        }

        // 从队列中取出一个AVPacket 可能是音频也可能是视频
        // 这个packet是从mp4文件中读出来的, 放入队列, 暂时不可以释放
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
            bool ifAudioFinished = false;
            bool ifVideoFinished = false;
            if (audioChannel) {
                if (audioChannel->packets.empty() && audioChannel->frames.empty()) {
                    ifAudioFinished = true;
                }
            } else {
                ifAudioFinished = true;
            }

            if (videoChannel) {

                if (videoChannel->packets.empty() && videoChannel->frames.empty()) {
                    ifVideoFinished = true;
                }
            } else {
                ifVideoFinished = true;
            }

            if (ifAudioFinished && ifVideoFinished) {
                // 音频和视频都播放完毕
                LOGI("all media data been played");
                break;
            }
        } else {
            // 读取失败
            LOGE("read media file fail");
            break;
        }
    }

    LOGD("读取完毕");
    isPlaying = false;

    if (videoChannel) {
        videoChannel->stop();
    }
    if (audioChannel) {
        audioChannel->stop();
    }
}

void KTPlayer::start() {
    isPlaying = true;

    if (videoChannel) {
        if(audioChannel){
            // 让video channel可以持有audio channel的指针
            videoChannel->setAudioChannel(audioChannel);
        }
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
    player->get_media_format();

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

// 对媒体文件进行解封装
void KTPlayer::get_media_format() {

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
        ret = avcodec_open2(codecContext, codec, nullptr);
        if (ret) {
            if (helper) {
                char *errInfo = av_err2str(ret);
                helper->onError(THREAD_CHILD, FFMPEG_OPEN_DECODER_FAIL, errInfo);
            }

            return;
        }

        AVRational timeBase = stream->time_base;

        // 获取流的类型
        if (codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            // 音频流
            audioChannel = new AudioChannel(i, codecContext, timeBase);
            // LOGD("audioChannel index %d\n", audioChannel->streamIndex);
        } else if (codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            // 视频流
            AVRational frameRate = stream->avg_frame_rate;
            int fps = av_q2d(frameRate);
            videoChannel = new VideoChannel(i, codecContext, timeBase, fps);
            videoChannel->setRenderCallback(this->renderCallback);
            // LOGD("videoChannel index %d\n", videoChannel->streamIndex);
        }
    }  // for end ----------------------------

    // 如果音频和视频都不存在
    if (!audioChannel && !videoChannel) {
        if (helper) {
            helper->onError(THREAD_CHILD, FFMPEG_NO_MEDIA, "no media");
        }
        return;
    }

    // 准备成功, 把消息传到到Activity
    if (helper) {
        // helper->get_pid();
        helper->onPrepared(THREAD_CHILD);
    }
}

void KTPlayer::setRenderCallback(RenderCallback callback) {
    this->renderCallback = callback;
}
