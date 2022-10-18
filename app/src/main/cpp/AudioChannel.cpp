#include "AudioChannel.h"


AudioChannel::AudioChannel(int streamIndex, AVCodecContext *codecContext, AVRational timeBase)
        : BaseChannel(streamIndex, codecContext, timeBase) {
    // 音频三要素
    // 采样率: 44100
    // 声道数: 2
    // 样本格式: 16bit
    // 即每秒钟数据有 44100 * 2 * 16 / 8 = 176400 176k字节

    // 音频压缩包AAC
    // 采样率: 44100
    // 声道数: 2
    // 样本格式: 32bit, 浮点运算效率更高
    // 即每秒钟数据有 44100 * 2 * 32 / 8 = 352800 352k字节

    // 手机的DAC仅仅支持
    // 采样率: 44100
    // 声道数: 2
    // 样本格式: 16bit
    out_channels = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);
    out_sample_size = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
    out_sample_rate = 44100;


    out_buffers_size = out_sample_rate * out_sample_size * out_channels;  // 三要素相乘

    // 堆区开辟
    out_buffers = static_cast<uint8_t *>(malloc(out_buffers_size));

    swr_ctx = swr_alloc_set_opts(nullptr,
            // 输出参数
                                 AV_CH_LAYOUT_STEREO,        // 双声道
                                 AV_SAMPLE_FMT_S16,        // 采样大小 16bit
                                 out_sample_rate,                        // 采样率44100

            // 输入参数
                                 codecContext->channel_layout,   // 声道数
                                 codecContext->sample_fmt,      // 采样大小
                                 codecContext->sample_rate,     // 采样率
                                 0, 0);

    // 初始化重采样上下文
    swr_init(swr_ctx);


}

AudioChannel::~AudioChannel() {

}

void *task_audio_decode(void *args) {
    auto *audioChannel = static_cast<AudioChannel *>(args);
    audioChannel->audio_decode();
    return nullptr;
}

void *task_audio_play(void *args) {
    auto *audioChannel = static_cast<AudioChannel *>(args);
    audioChannel->audio_play();
    return nullptr;
}

void AudioChannel::start() {
    isPlaying = true;
    packets.set_work(true);
    pthread_create(&pid_audio_decode, nullptr, task_audio_decode, this);

    frames.set_work(true);
    pthread_create(&pid_audio_play, nullptr, task_audio_play, this);

}

void AudioChannel::stop() {

}

// 对音频做解码
void AudioChannel::audio_decode() {
    AVPacket *packet = nullptr;
    while (isPlaying) {

        // 当数据包读取的速度过快, 而来不及进行解码的时候, 这里需要放慢解码的速度.
        // 注意这里要观察的是frames队列, 而不是packets队列
        if (isPlaying && frames.if_queue_full()) {
            av_usleep(10 * 1000);  // 10ms
            continue;
        }

        // 从队列中取出一个AVPacket
        int ret = packets.pop_from_queue(packet);
        if (!isPlaying) {
            break;
        }
        if (!ret) {
            continue;
        }

        // 把AVPacket发送给解码器
        ret = avcodec_send_packet(codecContext, packet);

        if (ret) {
            break;
        }

        // 从解码器中取出一个AVFrame
        AVFrame *frame = av_frame_alloc();
        ret = avcodec_receive_frame(codecContext, frame);

        // 音频也有帧的概念, 但是音频的帧是不完整的, 需要进行重采样
        if (ret == AVERROR(EAGAIN)) {
            // 重新获取AVPacket
            continue;
        } else if (ret != 0) {
            break;
        }

        // 把AVFrame放入队列
        frames.insert_to_queue(frame);

        av_packet_unref(packet);
        release_av_packet(&packet);
    }

    av_packet_unref(packet);
    release_av_packet(&packet);
}

int AudioChannel::get_pcm_size(void) {

    // 三要素
    int pcm_data_size = 0;
    // 待重采样的PCM 44100 * 2 * 32 / 8 = 352800 352k字节

    AVFrame *avFrame = nullptr;
    // 取出队列中的音频frame
    if (isPlaying) {
        int ret = frames.pop_from_queue(avFrame);
        if (!ret) {
            return 0;
        }

        // 音频重采样的工作
        int dst_nb_samples = av_rescale_rnd(swr_get_delay(swr_ctx, avFrame->sample_rate) + avFrame->nb_samples,
                                            out_sample_rate,
                                            avFrame->sample_rate,
                                            AV_ROUND_UP);


        // pcm的处理逻辑
        int samples_per_channel = swr_convert(swr_ctx,
                // 输出
                                              &out_buffers,          // 重采样后的buff
                                              dst_nb_samples,   // 成功的单通道样本数

                // 输入
                                              (const uint8_t **) avFrame->data,    // 队列的AVFrame的原始数据
                                              avFrame->nb_samples);   // 输入的样本数

        // 这个是最终的PCM数据大小
        pcm_data_size = samples_per_channel * out_sample_size * out_channels;

        // 这个值长得像0.00000 或者0.00234 0.03444354
        audioTimeStamp = avFrame->best_effort_timestamp * av_q2d(timeBase);
        // this->audioTimeStamp
    }

    av_frame_unref(avFrame);
    release_av_frame(&avFrame);

    return pcm_data_size;
}

// 重复回调
void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
    auto *audioChannel = static_cast<AudioChannel *>(context);

    audioChannel->bufferSize = audioChannel->get_pcm_size();

    if (audioChannel->bufferSize > 0) {
        // 把PCM数据放入队列
        (*bq)->Enqueue(bq, audioChannel->out_buffers, audioChannel->bufferSize);
    }
}

// 播放wave裸数据
void AudioChannel::audio_play() {

    SLresult sLresult;

    // STEP 1 创建引擎对象并获取接口
    sLresult = slCreateEngine(&engineObject, 0, nullptr, 0, nullptr, nullptr);
    if (SL_RESULT_SUCCESS != sLresult) {
        LOGE("slCreateEngine failed");
        return;
    }

    // SL_BOOLEAN_FALSE 表示等待引擎接口创建成功
    sLresult = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != sLresult) {
        LOGE("engineObject Realize failed");
        return;
    }

    sLresult = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineInterface);
    if (SL_RESULT_SUCCESS != sLresult) {
        LOGE("engineObject GetInterface failed");
        return;
    }

    // STEP 2 设置混音器
    sLresult = (*engineInterface)->CreateOutputMix(engineInterface, &outputMixObject, 0, 0, 0);
    if (SL_RESULT_SUCCESS != sLresult) {
        LOGE("engineInterface CreateOutputMix failed");
        return;
    }

    // SL_BOOLEAN_FALSE 表示等待混音器接口创建成功
    sLresult = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != sLresult) {
        LOGE("outputMixObject Realize failed");
        return;
    }

    // 获取混音器接口
    /*
    sLresult = (*outputMixObject)->GetInterface(outputMixObject, SL_IID_ENVIRONMENTALREVERB, &outputMixEnvironmentalReverb);
    if(SL_RESULT_SUCCESS == sLresult) {
        // 设置混音器的环境混响效果
        sLresult = (*outputMixEnvironmentalReverb)->SetEnvironmentalReverbProperties(outputMixEnvironmentalReverb, &reverbSettings);
        if(SL_RESULT_SUCCESS != sLresult) {
            LOGE("outputMixEnvironmentalReverb SetEnvironmentalReverbProperties failed");
            return;
        }
    } else {
        LOGE("outputMixObject GetInterface failed");
        return;
    }
    */

    // STEP 3 创建播放器
    SLDataLocator_AndroidSimpleBufferQueue android_queue = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 10};

    // 解码后的数据时PCM格式, 而不再是mp3
    SLDataFormat_PCM format_pcm = {
            SL_DATAFORMAT_PCM, // 播放pcm格式的数据
            2, // 双声道
            SL_SAMPLINGRATE_44_1, // 采样率
            SL_PCMSAMPLEFORMAT_FIXED_16, // 位数
            SL_PCMSAMPLEFORMAT_FIXED_16, // 和位数一样
            SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT, // 立体声(前左前右)
            SL_BYTEORDER_LITTLEENDIAN // 小端
    };

    // 将上面的信息放到数据源中
    SLDataSource audioSrc = {&android_queue, &format_pcm};

    // 设置混音器
    SLDataLocator_OutputMix locator_outputMix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
    SLDataSink audioSnk = {&locator_outputMix, nullptr};

    const SLInterfaceID ids[] = {SL_IID_BUFFERQUEUE};
    const SLboolean req[1] = {SL_BOOLEAN_TRUE};

    // 创建播放器
    sLresult = (*engineInterface)->CreateAudioPlayer(engineInterface, &bqPlayerObject, &audioSrc, &audioSnk,
            // 打开队列
                                                     1, ids, req);
    if (SL_RESULT_SUCCESS != sLresult) {
        LOGE("engineInterface CreateAudioPlayer failed");
        return;
    }

    // 初始化播放器
    sLresult = (*bqPlayerObject)->Realize(bqPlayerObject, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != sLresult) {
        LOGE("bqPlayerObject Realize failed");
        return;
    }

    // 获取播放器接口
    sLresult = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_PLAY, &bqPlayerPlay);
    if (SL_RESULT_SUCCESS != sLresult) {
        LOGE("bqPlayerObject GetInterface failed");
        return;
    }

    // STEP 4 设置播放回调
    sLresult = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_BUFFERQUEUE, &bqPlayerBufferQueue);
    if (SL_RESULT_SUCCESS != sLresult) {
        LOGE("bqPlayerObject GetInterface failed");
        return;
    }

    // 这个播放回调会被不断调用
    (*bqPlayerBufferQueue)->RegisterCallback(bqPlayerBufferQueue, bqPlayerCallback, this);

    // STEP 5 设置播放状态
    sLresult = (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PLAYING);

    // STEP 6 手动激活回调函数
    bqPlayerCallback(bqPlayerBufferQueue, this);

    // STEP 7

}
