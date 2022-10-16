#include "AudioChannel.h"

AudioChannel::AudioChannel(int index, AVCodecContext *codecContext) : BaseChannel(index, codecContext) {

}

AudioChannel::~AudioChannel() {

}

void AudioChannel::start(){}

void AudioChannel::stop() {

}
