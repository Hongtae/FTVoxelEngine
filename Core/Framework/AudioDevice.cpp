#include <format>

#include "../Libs/OpenAL/include/AL/al.h"
#include "../Libs/OpenAL/include/AL/alc.h"
#include "../Libs/OpenAL/include/AL/alext.h"

#include "AudioDevice.h"
#include "AudioSource.h"
#include "Logger.h"

using namespace FV;

std::vector<AudioDevice::DeviceInfo> AudioDevice::availableDevices() {
    std::vector<DeviceInfo> deviceList;
    if (alcIsExtensionPresent(nullptr, "ALC_ENUMERATION_EXT") == AL_TRUE) {
        // defaultDeviceName contains the name of the default device 
        std::string defaultDeviceName = alcGetString(nullptr, ALC_DEFAULT_DEVICE_SPECIFIER);

        // Pass in NULL device handle to get list of devices 
        const char* devices = (char*)alcGetString(nullptr, ALC_DEVICE_SPECIFIER);
        // devices contains the device names, separated by NULL  
        // and terminated by two consecutive NULLs. 
        while (*devices != 0) {
            ALCdevice* device = alcOpenDevice(devices);
            if (device) {
                DeviceInfo info = {};
                info.name = alcGetString(device, ALC_DEVICE_SPECIFIER);

                ALCint majorVersion = 0;
                ALCint minorVersion = 0;
                alcGetIntegerv(device, ALC_MAJOR_VERSION, sizeof(ALCint), &majorVersion);
                alcGetIntegerv(device, ALC_MINOR_VERSION, sizeof(ALCint), &minorVersion);

                info.majorVersion = majorVersion;
                info.minorVersion = minorVersion;

                if (info.name == defaultDeviceName)
                    deviceList.insert(deviceList.begin(), info);
                else
                    deviceList.push_back(info);

                alcCloseDevice(device);
            }
            devices += strlen(devices) + 1;
        }
    }
    return deviceList;
}

AudioDevice::AudioDevice(const std::string& deviceName)
    : device(nullptr), context(nullptr)
    , majorVersion(0), minorVersion(0) {
    ALCdevice* device = alcOpenDevice(deviceName.c_str());
    FVASSERT_THROW(device != nullptr);

    ALCcontext* context = alcCreateContext(device, nullptr);
    FVASSERT_THROW(context != nullptr);

    alcMakeContextCurrent(context);
    this->device = device;
    this->context = context;
    this->deviceName = alcGetString(device, ALC_DEVICE_SPECIFIER);
    ALCint majorVersion = 0;
    ALCint minorVersion = 0;
    alcGetIntegerv(device, ALC_MAJOR_VERSION, sizeof(ALCint), &majorVersion);
    alcGetIntegerv(device, ALC_MINOR_VERSION, sizeof(ALCint), &minorVersion);

    this->majorVersion = majorVersion;
    this->minorVersion = minorVersion;

    Log::info("OpenAL device: {} Version: {:d}.{:d}.", deviceName, majorVersion, minorVersion);

    // update format table
    formatTable[BitsChannels{4, 1}.value] = alGetEnumValue("AL_FORMAT_MONO_IMA4");
    formatTable[BitsChannels{4, 2}.value] = alGetEnumValue("AL_FORMAT_STEREO_IMA4");

    formatTable[BitsChannels{8, 1}.value] = AL_FORMAT_MONO8;
    formatTable[BitsChannels{8, 2}.value] = AL_FORMAT_STEREO8;
    formatTable[BitsChannels{8, 4}.value] = alGetEnumValue("AL_FORMAT_QUAD8");
    formatTable[BitsChannels{8, 6}.value] = alGetEnumValue("AL_FORMAT_51CHN8");
    formatTable[BitsChannels{8, 8}.value] = alGetEnumValue("AL_FORMAT_71CHN8");

    formatTable[BitsChannels{16, 1}.value] = AL_FORMAT_MONO16;
    formatTable[BitsChannels{16, 2}.value] = AL_FORMAT_STEREO16;
    formatTable[BitsChannels{16, 4}.value] = alGetEnumValue("AL_FORMAT_QUAD16");
    formatTable[BitsChannels{16, 6}.value] = alGetEnumValue("AL_FORMAT_51CHN16");
    formatTable[BitsChannels{16, 8}.value] = alGetEnumValue("AL_FORMAT_71CHN16");

    formatTable[BitsChannels{32, 1}.value] = alGetEnumValue("AL_FORMAT_MONO_FLOAT32");
    formatTable[BitsChannels{32, 2}.value] = alGetEnumValue("AL_FORMAT_STEREO_FLOAT32");
    formatTable[BitsChannels{32, 4}.value] = alGetEnumValue("AL_FORMAT_QUAD32");
    formatTable[BitsChannels{32, 6}.value] = alGetEnumValue("AL_FORMAT_51CHN32");
    formatTable[BitsChannels{32, 8}.value] = alGetEnumValue("AL_FORMAT_71CHN32");
}

AudioDevice::~AudioDevice() {
    if (alcGetCurrentContext() == context)
        alcMakeContextCurrent(nullptr);
    if (context)
        alcDestroyContext((ALCcontext*)context);
    if (device)
        alcCloseDevice((ALCdevice*)device);
}

uint32_t AudioDevice::format(uint16_t bits, uint16_t channels) const {
    BitsChannels bc = { bits, channels };
    if (auto it = formatTable.find(bc.value); it != formatTable.end())
        return it->second;
    return 0;
}

std::shared_ptr<AudioSource> AudioDevice::makeSource() {
    ALuint sourceID = 0;
    alGenSources(1, &sourceID);
    alSourcei(sourceID, AL_LOOPING, 0);
    alSourcei(sourceID, AL_BUFFER, 0);
    alSourceStop(sourceID);

    return std::make_shared<AudioSource>(shared_from_this(),
                                         (uint32_t)sourceID);
}
