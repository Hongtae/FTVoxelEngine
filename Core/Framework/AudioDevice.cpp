#include "../Libs/OpenAL/include/AL/al.h"
#include "../Libs/OpenAL/include/AL/alc.h"
#include "../Libs/OpenAL/include/AL/alext.h"

#include "AudioDevice.h"

using namespace FV;

AudioDevice::AudioDevice()
    : device(nullptr), context(nullptr)
{
}

AudioDevice::~AudioDevice()
{
    if (alcGetCurrentContext() == context)
        alcMakeContextCurrent(nullptr);
    if (context)
        alcDestroyContext((ALCcontext*)context);
    if (device)
        alcCloseDevice((ALCdevice*)device);
}

uint32_t AudioDevice::format(uint16_t bits, uint16_t channels) const
{
    BitsChannels bc = { bits, channels };
    if (auto it = formatTable.find(bc.value); it != formatTable.end())
        return it->second;
    return 0;
}

std::shared_ptr<AudioSource> AudioDevice::makeSource()
{
    return nullptr;
}
