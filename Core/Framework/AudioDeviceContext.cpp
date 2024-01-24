#include <format>
#include <chrono>
#include <algorithm>
#include "Logger.h"
#include "AudioDeviceContext.h"

using namespace FV;

AudioDeviceContext::AudioDeviceContext(std::shared_ptr<AudioDevice> dev)
    : device(dev)
    , listener(std::make_shared<AudioListener>(dev))
    , maxBufferCount(3)
    , minBufferTime(0.4)
    , maxBufferTime(10.0) {
    this->thread = std::jthread(
        [this](std::stop_token stopToken) {
            Log::info("AudioDeviceContext playback task is started.)");

            std::vector<uint8_t> buffer(1024, 0);
            std::vector<std::shared_ptr<AudioPlayer>> retainedPlayers;

            while (stopToken.stop_requested() == false) {
                std::vector<std::shared_ptr<AudioPlayer>> activePlayers;
                if (true) {
                    std::scoped_lock guard(lock);
                    activePlayers.reserve(this->players.size());
                    for (auto& wpl : this->players) {
                        if (auto player = wpl.lock(); player)
                            activePlayers.push_back(player);
                    }
                }
                retainedPlayers.clear();
                for (auto& player : activePlayers) {
                    if (player->playing) {
                        auto& source = player->source;
                        source->dequeueBuffers();

                        if (player->buffering && source->numberOfBuffersInQueue() < maxBufferCount) {
                            // buffering!
                            auto& stream = player->stream;
                            double bufferingTime = std::clamp(player->maxBufferingTime,
                                                              minBufferTime, maxBufferTime);
                            uint32_t sampleAlignment = stream->channels() * stream->bits() >> 3;
                            size_t bufferSize = size_t(bufferingTime * double(stream->sampleRate())) * sampleAlignment;

                            if (bufferSize > buffer.size()) // resize buffer
                                buffer.resize(bufferSize);

                            double bufferPos = stream->timePosition();
                            uint64_t bytesRead = stream->read(buffer.data(), bufferSize);
                            if (bytesRead > 0 && bytesRead != uint64_t(-1)) {
                                player->processStream(buffer.data(), bytesRead, bufferPos);
                                if (source->enqueueBuffer(stream->sampleRate(),
                                                          stream->bits(),
                                                          stream->channels(),
                                                          buffer.data(),
                                                          bufferSize,
                                                          bufferPos)) {
                                    player->playing = true;
                                    player->bufferedPosition = stream->timePosition();
                                    player->bufferingStateChanged(true, bufferPos);
                                } else // error
                                {
                                    Log::error("AudioSource::enqueueBuffer failed.");
                                    player->buffering = false;
                                    player->playing = false;
                                }
                            } else if (bytesRead == 0) // eof
                            {
                                if (player->playLoopCount > 1) {
                                    player->playLoopCount -= 1;
                                    stream->seekPcm(0); // rewind
                                } else {
                                    player->buffering = false;
                                }
                            } else // error (-1)
                            {
                                Log::error("AudioStream::read failed.");
                                player->playing = false;
                                player->buffering = false;
                                source->stop();
                                source->dequeueBuffers();
                            }

                            if (player->buffering == false)
                                player->bufferingStateChanged(false, bufferPos);
                        }

                        // update state
                        if (player->playing) {
                            if (source->state() == AudioSource::StateStopped) {
                                if (source->numberOfBuffersInQueue() > 0)
                                    source->play();          // resume.
                                else
                                    player->playing = false; // done.
                            }
                        }

                        if (player->playing) {
                            double pos = source->timePosition();
                            if (player->playbackPosition != pos) {
                                player->playbackPosition = pos;
                                player->playbackStateChanged(true, pos);
                            }
                            if (player->retainedWhilePlaying)
                                retainedPlayers.push_back(player);
                        } else {
                            player->playbackStateChanged(false, player->playbackPosition);
                        }
                    }
                }
                // sleep 200ms
                std::this_thread::sleep_for(std::chrono::duration<int, std::milli>(200));
            };
            Log::info("AudioDeviceContext playback task is finished.");
        }
    );
}

AudioDeviceContext::~AudioDeviceContext() {
    this->thread.request_stop();
    this->thread.join();
}

std::shared_ptr<AudioPlayer> AudioDeviceContext::makePlayer(std::shared_ptr<AudioStream> stream) {
    if (auto source = device->makeSource(); source) {
        auto player = std::make_shared<AudioPlayer>(source, stream);

        std::scoped_lock guard(lock);
        this->players.push_back(player);
        return player;
    }
    return nullptr;
}

std::shared_ptr<AudioDeviceContext> AudioDeviceContext::makeDefault() {
    static std::weak_ptr<AudioDeviceContext> defaultCtxt;
    static std::mutex lock;

    if (auto ctxt = defaultCtxt.lock(); ctxt)
        return ctxt;

    std::scoped_lock guard(lock);
    if (auto ctxt = defaultCtxt.lock(); ctxt)
        return ctxt;

    if (auto devices = AudioDevice::availableDevices(); devices.empty() == false) {
        for (auto& deviceInfo : devices) {
            try {
                auto device = std::make_shared<AudioDevice>(deviceInfo.name);
                auto ctxt = std::make_shared<AudioDeviceContext>(device);
                defaultCtxt = ctxt; // store weak-ptr
                return ctxt;
            } catch (const std::runtime_error& err) {
                Log::error("Runtime-error: {}", err.what());
            }
        }
    }
    return nullptr;
}
