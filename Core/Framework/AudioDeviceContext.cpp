#include "AudioDeviceContext.h"

using namespace FV;

AudioDeviceContext::AudioDeviceContext(std::shared_ptr<AudioDevice> dev)
    : device(dev)
    , listener(std::make_shared<AudioListener>(dev))
{
}

AudioDeviceContext::~AudioDeviceContext()
{
}

std::shared_ptr<AudioPlayer> AudioDeviceContext::makePlayer(std::shared_ptr<AudioStream> stream)
{
    return nullptr;
}