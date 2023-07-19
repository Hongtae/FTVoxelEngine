#include "../Libs/dkwrapper/DKAudioStream.h"
#include "AudioStream.h"

namespace {
    struct DataStream
    {
        DKStream stream;
        uint64_t position;
    };
}
using namespace FV;

AudioStream::AudioStream()
    : source(nullptr), stream(nullptr)
{
    DKAudioStream* audioStream = new DKAudioStream();
    this->stream = audioStream;

    //TODO: setup DataStream..
}

AudioStream::~AudioStream()
{
    if (stream)
    {
        DKAudioStreamDestroy(reinterpret_cast<DKAudioStream*>(stream));
        delete reinterpret_cast<DKAudioStream*>(stream);
    }
}

size_t AudioStream::read(void* buffer, size_t length) 
{
    DKAudioStream* stream = reinterpret_cast<DKAudioStream*>(this->stream);
    uint64_t read = stream->read(stream, buffer, length);
    if (read == ~uint64_t(0))
        return -1;
    return 0;
}

uint64_t AudioStream::seekRaw(uint64_t raw)
{
    DKAudioStream* stream = reinterpret_cast<DKAudioStream*>(this->stream);
    return stream->seekRaw(stream, raw);
}

uint64_t AudioStream::seekPcm(uint64_t pcm)
{
    DKAudioStream* stream = reinterpret_cast<DKAudioStream*>(this->stream);
    return stream->seekPcm(stream, pcm);
}

double AudioStream::seekTime(double t)
{
    DKAudioStream* stream = reinterpret_cast<DKAudioStream*>(this->stream);
    return stream->seekTime(stream, t);
}

uint64_t AudioStream::rawPosition() const
{
    DKAudioStream* stream = reinterpret_cast<DKAudioStream*>(this->stream);
    return stream->rawPosition(stream);
}

uint64_t AudioStream::pcmPosition() const
{
    DKAudioStream* stream = reinterpret_cast<DKAudioStream*>(this->stream);
    return stream->pcmPosition(stream);
}

double AudioStream::timePosition() const
{
    DKAudioStream* stream = reinterpret_cast<DKAudioStream*>(this->stream);
    return stream->timePosition(stream);
}

uint64_t AudioStream::rawTotal() const
{
    DKAudioStream* stream = reinterpret_cast<DKAudioStream*>(this->stream);
    return stream->rawTotal(stream);
}

uint64_t AudioStream::pcmTotal() const
{
    DKAudioStream* stream = reinterpret_cast<DKAudioStream*>(this->stream);
    return stream->pcmTotal(stream);
}

double AudioStream::timeTotal() const
{
    DKAudioStream* stream = reinterpret_cast<DKAudioStream*>(this->stream);
    return stream->timeTotal(stream);
}

uint32_t AudioStream::sampleRate() const
{
    DKAudioStream* stream = reinterpret_cast<DKAudioStream*>(this->stream);
    return stream->sampleRate;
}

uint32_t AudioStream::channels() const
{
    DKAudioStream* stream = reinterpret_cast<DKAudioStream*>(this->stream);
    return stream->channels;
}

uint32_t AudioStream::bits() const
{
    DKAudioStream* stream = reinterpret_cast<DKAudioStream*>(this->stream);
    return stream->bits;
}
