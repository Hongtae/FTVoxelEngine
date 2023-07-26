#pragma once
#include <mutex>
#include <thread>
#include <memory>
#include <vector>

#include "../include.h"
#include "AudioDevice.h"
#include "AudioListener.h"
#include "AudioPlayer.h"
#include "AudioStream.h"

namespace FV
{
    class FVCORE_API AudioDeviceContext
    {
    public:
        AudioDeviceContext(std::shared_ptr<AudioDevice>);
        ~AudioDeviceContext();

        const std::shared_ptr<AudioDevice> device;
        const std::shared_ptr<AudioListener> listener;

        std::shared_ptr<AudioPlayer> makePlayer(std::shared_ptr<AudioStream>);

        static std::shared_ptr<AudioDeviceContext> makeDefault();
    private:
        std::vector<std::weak_ptr<AudioPlayer>> players;
        std::mutex lock;
        std::jthread thread;    

        int maxBufferCount;
        double minBufferTime;
        double maxBufferTime;
    };
}
