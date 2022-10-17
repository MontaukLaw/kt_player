#include "AudioChannel.h"

AudioChannel::AudioChannel(int streamIndex, AVCodecContext *codecContext) : BaseChannel(streamIndex, codecContext) {

}

AudioChannel::~AudioChannel() {

}

void AudioChannel::start(){}

void AudioChannel::stop() {

}
