#pragma once
#include "../include.h"
#include <string>
#include <memory>
#include <map>
#include <vector>

namespace FV
{
    class AudioSource;
    class FVCORE_API AudioDevice: public std::enable_shared_from_this<AudioDevice>
    {
    public:
        struct DeviceInfo
        {
            std::string name;
            int majorVersion;
            int minorVersion;
        };

        uint32_t format(uint16_t bits, uint16_t channels) const;

        AudioDevice(const std::string& deviceName);
        ~AudioDevice();

        std::shared_ptr<AudioSource> makeSource();
        static std::vector<DeviceInfo> availableDevices();
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
        std::string deviceName;
        int majorVersion;
        int minorVersion;
        std::map<uint32_t, uint32_t> formatTable;
    };
}