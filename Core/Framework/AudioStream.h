#pragma once
#include "../include.h"
#include <filesystem>

namespace FV
{
    enum class AudioStreamEncodingFormat
    {
        unknown,
        oggVorbis,
        oggFLAC,
        flac,
        mp3,
        wave,
    };

    class FVCORE_API AudioStream
    {
    public:
        AudioStream();
        AudioStream(const std::filesystem::path& path);
        AudioStream(const void* data, size_t size);
        ~AudioStream();

         size_t read(void*, size_t);
         uint64_t seekRaw(uint64_t); ///< seek by stream pos
         uint64_t seekPcm(uint64_t); ///< seek by PCM
         double seekTime(double);    ///< seek by time

        uint64_t rawPosition() const;
        uint64_t pcmPosition() const;
        double timePosition() const;

        uint64_t rawTotal() const;
        uint64_t pcmTotal() const;
        double timeTotal() const;

        uint32_t sampleRate() const;
        uint32_t channels() const;
        uint32_t bits() const;

        AudioStreamEncodingFormat mediaType() const { return format; }

    private:
        AudioStreamEncodingFormat format;
        void* stream;
    };
}
