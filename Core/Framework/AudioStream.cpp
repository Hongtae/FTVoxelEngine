#include <istream>
#include <fstream>
#include <vector>
#include <algorithm>

#include "../Libs/dkwrapper/DKAudioStream.h"
#include "AudioStream.h"

using namespace FV;

namespace {
    class StreamSource
    {
    public:
        StreamSource() {}
        virtual ~StreamSource() {}

        virtual uint64_t setPosition(uint64_t) = 0;
        virtual uint64_t getPosition() = 0;
        virtual uint64_t remainLength() = 0;
        virtual uint64_t totalLength() = 0;
        virtual uint64_t read(void* buff, size_t) = 0;

        DKStream* proxy;
    };

    class FileStreamSource : public StreamSource
    {
    public:
        FileStreamSource(const std::string& path)
            : file(path, std::ios::binary | std::ios::in | std::ios::ate)
        {
            if (file.good())
            {
                file.seekg(0, std::ios::beg);
                this->begin = file.tellg();
                file.seekg(0, std::ios::end);
                this->end = file.tellg();
                file.clear();
                file.seekg(0, std::ios::beg);
            }
        }
        uint64_t setPosition(uint64_t pos) override 
        {
            if (file.good())
                file.seekg(pos, std::ios::beg);
            return file.tellg();
        }
        uint64_t getPosition() override
        {
            return file.tellg() - this->begin;
        }
        uint64_t remainLength() override
        {
            return this->end - file.tellg();
        }
        uint64_t totalLength() override
        {
            return this->end - this->begin;
        }
        uint64_t read(void* buff, size_t s) override 
        {
            if (file.good())
            {
                file.read((std::ifstream::char_type*)buff, s);
                return file.gcount();
            }
            return uint64_t(-1); //error
        }
        std::ifstream file;
        std::streampos begin;
        std::streampos end;
    };
    class DataStreamSource : public StreamSource
    {
    public:
        DataStreamSource(const uint8_t* p, size_t s)
            : data(&p[0], &p[s]), position(0)
        {
        }
        uint64_t setPosition(uint64_t pos) override
        {
            position = std::clamp(pos, 0ULL, uint64_t(data.size()));
            return position;
        }
        uint64_t getPosition() override
        {
            return position;
        }
        uint64_t remainLength() override
        {
            return data.size() - position;
        }
        uint64_t totalLength() override 
        {
            return data.size();
        }
        uint64_t read(void* buff, size_t size) override
        {
            uint64_t s = std::min(size, data.size() - position);
            if (s > 0)
                memcpy(buff, &(data.data()[position]), s);
            return s;
        }
        std::vector<uint8_t> data;
        uint64_t position;
    };

    DKAudioStream* allocStream(StreamSource* source)
    {
        if (source)
        {
            DKStream* proxy = new DKStream{};
            proxy->userContext = source;
            proxy->read = [](void* p, void* buffer, size_t length)->uint64_t
            {
                StreamSource* source = (StreamSource*)p;
                return source->read(buffer, length);
            };
            proxy->write = nullptr;
            proxy->setPosition = [](void* p, uint64_t off)->uint64_t
            {
                StreamSource* source = (StreamSource*)p;
                return source->setPosition(off);
            };
            proxy->getPosition = [](void* p)->uint64_t
            {
                StreamSource* source = (StreamSource*)p;
                return source->getPosition();
            };
            proxy->remainLength = [](void* p)->uint64_t
            {
                StreamSource* source = (StreamSource*)p;
                return source->remainLength();
            };
            proxy->totalLength = [](void* p)->uint64_t
            {
                StreamSource* source = (StreamSource*)p;
                return source->totalLength();
            };

            DKAudioStream* stream = DKAudioStreamCreate(source->proxy);
            if (stream)
            {
                source->proxy = proxy;
                return stream;
            }
            delete proxy;
        }
        return nullptr;
    }
    StreamSource* deallocStream(DKAudioStream* stream)
    {
        if (stream)
        {
            StreamSource* src = (StreamSource*)(stream->userContext);
            DKStream* proxy = src->proxy;
            DKAudioStreamDestroy(stream);
            delete stream;
            delete proxy;
            src->proxy = nullptr;
            return src;
        }
        return nullptr;
    }
}

AudioStream::AudioStream()
    : stream(nullptr)
{
    this->stream = allocStream(nullptr);
}

AudioStream::AudioStream(const std::string& path)
    : stream(nullptr)
{
    this->stream = allocStream(new FileStreamSource(path));
}

AudioStream::AudioStream(const void* data, size_t size)
    : stream(nullptr)
{
    this->stream = allocStream(new DataStreamSource((const uint8_t*)data, size));
}

AudioStream::~AudioStream()
{
    auto source = deallocStream((DKAudioStream*)stream);
    delete source;
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
