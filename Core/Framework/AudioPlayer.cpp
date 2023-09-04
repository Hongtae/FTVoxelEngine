#include "AudioPlayer.h"

using namespace FV;

AudioPlayer::AudioPlayer(std::shared_ptr<AudioSource> src, std::shared_ptr<AudioStream> st)
    : playing(false)
    , buffering(false)
    , bufferedPosition(0)
    , playbackPosition(0), playLoopCount(0)
    , maxBufferingTime(1.0)
    , source(src), stream(st) {
}

AudioPlayer::~AudioPlayer() {
    source->stop();
    source->dequeueBuffers();
}

void AudioPlayer::play() {
    if (playing = false) {
        playing = true;
        buffering = true;
        playLoopCount = 1;
    }
}

void AudioPlayer::play(double start, int loopCount) {
    if (playing == false) {
        source->stop();
        source->dequeueBuffers();

        playing = true;
        buffering = true;
        playLoopCount = loopCount;
        stream->seekTime(start);
        playbackPosition = stream->timePosition();
    }
}

void AudioPlayer::stop() {
    stream->seekPcm(0);
    source->stop();
    source->dequeueBuffers();

    playing = false;
    playbackPosition = 0;
    bufferedPosition = 0;
}

void AudioPlayer::pause() {
    if (playing)
        source->pause();
}
