#pragma once
#include "../include.h"
#include <string>
#include <map>

namespace FV
{
    class AudioSource;
    class FVCORE_API AudioDevice
    {
    public:
        struct DeviceInfo
        {
            std::string name;
            int majorVersion;
            int minorVersion;
        };

        uint32_t format(uint16_t bits, uint16_t channels) const;

        AudioDevice();
        ~AudioDevice();

        std::shared_ptr<AudioSource> makeSource();
    private:
        union BitsChannels
        {
            struct {
                uint16_t bits;
                uint16_t channels;
            };
            uint32_t value;
        };

    private:
        void* device;
        void* context;
        std::map<uint32_t, uint32_t> formatTable;
    };
}