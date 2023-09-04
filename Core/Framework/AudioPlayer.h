#pragma once
#include "../include.h"
#include "AudioSource.h"
#include "AudioStream.h"

namespace FV {
    class FVCORE_API AudioPlayer {
    public:
        AudioPlayer(std::shared_ptr<AudioSource>, std::shared_ptr<AudioStream>);
        virtual ~AudioPlayer();

        uint32_t sampleRate() const { stream->sampleRate(); }
        uint32_t channels() const { stream->channels(); }
        uint32_t bits() const { stream->bits(); }
        double duration() const { stream->timeTotal(); }
        double position() const { stream->timePosition(); }

        bool retainedWhilePlaying = false;

        void play();
        void play(double start, int loops);
        void stop();
        void pause();

        AudioSource::State state() const { source->state(); }

        const std::shared_ptr<AudioSource> source;
        const std::shared_ptr<AudioStream> stream;

    protected:
        virtual void bufferingStateChanged(bool, double timestamp) {}
        virtual void playbackStateChanged(bool, double  position) {}
        virtual void processStream(void* data, size_t byteCount, double timestamp) {}
    private:
        bool playing;
        bool buffering;
        double bufferedPosition;
        double playbackPosition;
        int playLoopCount;
        double maxBufferingTime;
        friend class AudioDeviceContext;
    };
}
