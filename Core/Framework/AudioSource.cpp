#include <numbers>
#include <algorithm>

#include "../Libs/OpenAL/include/AL/al.h"
#include "../Libs/OpenAL/include/AL/alc.h"
#include "../Libs/OpenAL/include/AL/alext.h"

#include "AudioSource.h"
#include "AudioDevice.h"
#include "Logger.h"
#include "Quaternion.h"

using namespace FV;

AudioSource::AudioSource(std::shared_ptr<AudioDevice> dev, uint32_t sID)
    : sourceID(sID)
    , device(dev) {
    FVASSERT_DEBUG(sourceID != 0);
}

AudioSource::~AudioSource() {
    FVASSERT_DEBUG(sourceID != 0);
    FVASSERT_DEBUG(alIsSource(sourceID));

    stop();
    FVASSERT_DEBUG(buffers.empty());

    alDeleteSources(1, &sourceID);

    // check error
    auto err = alGetError();
    if (err != AL_NO_ERROR) {
        Log::error(std::format("AudioSource error: {}, {}",
                               err, alGetString(err)));
    }
}

AudioSource::State AudioSource::state() const {
    ALint st = 0;
    alGetSourcei(sourceID, AL_SOURCE_STATE, &st);

    switch (st) {
    case AL_PLAYING:	return StatePlaying;
    case AL_PAUSED:		return StatePaused;
    }
    return StateStopped;
}

void AudioSource::play() {
    std::scoped_lock guard(bufferLock);
    alSourcePlay(sourceID);
}

void AudioSource::pause() {
    std::scoped_lock guard(bufferLock);
    alSourcePause(sourceID);
}

void AudioSource::stop() {
    std::scoped_lock guard(bufferLock);
    alSourceStop(sourceID);
    ALint buffersQueued = 0;
    ALint buffersProcessed = 0;
    alGetSourcei(sourceID, AL_BUFFERS_QUEUED, &buffersQueued);		// entire buffers
    alGetSourcei(sourceID, AL_BUFFERS_PROCESSED, &buffersProcessed);// finished buffers

    for (int i = 0; i < buffersProcessed; ++i) {
        ALuint bufferID = 0;
        alSourceUnqueueBuffers(sourceID, 1, &bufferID);
    }

    if (buffersProcessed != buffers.size()) {
        Log::error(std::format("Buffer mismatch! {}, {}",
                               buffers.size(), buffersProcessed));
    }

    alSourcei(sourceID, AL_LOOPING, 0);
    alSourcei(sourceID, AL_BUFFER, 0);
    alSourceRewind(sourceID);

    for (auto& buffer : buffers) {
        ALuint bufferID = buffer.bufferID;
        alDeleteBuffers(1, &bufferID);
    }
    buffers.clear();

    // check error!
    if (ALenum err = alGetError(); err != AL_NO_ERROR) {
        Log::error(std::format("AudioSource error: {}, {}",
                               err, alGetString(err)));
    }
}

size_t AudioSource::numberOfBuffersInQueue() const {
    dequeueBuffers();

    std::scoped_lock guard(bufferLock);

    ALint queuedBuffers = 0;
    alGetSourcei(sourceID, AL_BUFFERS_QUEUED, &queuedBuffers);
    if (queuedBuffers != buffers.size()) {
        Log::error(std::format("AudioBuffer mismatch! {}, {}",
                               buffers.size(), queuedBuffers));
    }
    return buffers.size();
}

void AudioSource::dequeueBuffers() const {
    std::scoped_lock guard(bufferLock);
    ALint bufferProcessed = 0;
    alGetSourcei(sourceID, AL_BUFFERS_PROCESSED, &bufferProcessed);
    for (int i = 0; i < bufferProcessed; ++i) {
        ALuint bufferID = 0;
        alSourceUnqueueBuffers(sourceID, 1, &bufferID);
        if (bufferID) {
            if (auto it = std::find_if(
                buffers.begin(), buffers.end(), [bufferID](const Buffer& buffer) {
                    return buffer.bufferID == bufferID;
                }); it != this->buffers.end())
                this->buffers.erase(it);
                alDeleteBuffers(1, &bufferID);
        } else {
            Log::error(
                std::format("AudioSource failed to dequeue buffer! source:{}",
                            sourceID));
        }

        // check error
        if (ALenum err = alGetError(); err != AL_NO_ERROR) {
            Log::error(std::format("AudioSource error: {}, {}",
                                   err, alGetString(err)));
        }
    }
}

bool AudioSource::enqueueBuffer(int sampleRate,
                                int bits,
                                int channels,
                                const void* data,
                                size_t byteCount,
                                double timeStamp) {
    if (data && byteCount > 0 && sampleRate > 0) {
        uint32_t format = device->format(bits, channels);
        if (format) {
            std::scoped_lock guard(bufferLock);

            std::vector<ALuint> finishedBuffers;
            ALint numBuffersProcessed = 0;
            alGetSourcei(sourceID, AL_BUFFERS_PROCESSED, &numBuffersProcessed);
            finishedBuffers.reserve(numBuffersProcessed);

            while (numBuffersProcessed > 0) {
                ALuint bufferID = 0;
                alSourceUnqueueBuffers(sourceID, 1, &bufferID);
                if (bufferID)
                    finishedBuffers.push_back(bufferID);
                numBuffersProcessed--;
            }

            ALuint bufferID = 0;
            if (finishedBuffers.empty() == false) {
                for (ALuint id : finishedBuffers) {
                    if (auto it = std::find_if(
                        buffers.begin(), buffers.end(),
                        [id](const Buffer& buffer) {
                            return buffer.bufferID == id;
                        }); it != this->buffers.end())
                        this->buffers.erase(it);
                }
                bufferID = finishedBuffers[0];
                ALsizei numBuffers = (ALsizei)finishedBuffers.size();
                if (numBuffers > 1) {
                    ALuint* buff = finishedBuffers.data();
                    alDeleteBuffers(numBuffers - 1, &buff[1]);
                }
            }
            if (bufferID == 0)
                alGenBuffers(1, &bufferID);
            // enqueue buffer.
            alBufferData(bufferID, format, data, (ALsizei)byteCount, sampleRate);
            alSourceQueueBuffers(sourceID, 1, &bufferID);

            size_t bytesSecond = sampleRate * channels * (bits >> 3);
            Buffer bufferInfo = { timeStamp, byteCount, bytesSecond, bufferID };
            this->buffers.push_back(bufferInfo);

            // check error
            if (ALenum err = alGetError(); err != AL_NO_ERROR) {
                Log::error(std::format("AudioSource error: {}, {}",
                                       err, alGetString(err)));
            }
            return true;
        } else {
            Log::error(std::format("Unsupported autio format, bits:{}, channels:{}",
                                   bits, channels));
        }
    }
    dequeueBuffers();
    return false;
}

double AudioSource::timePosition() const {
    dequeueBuffers();
    FVASSERT_DEBUG(sourceID != 0);
    std::scoped_lock guard(bufferLock);
    if (buffers.empty())
        return 0.0;

    const Buffer& buffInfo = buffers.at(0);
    FVASSERT_DEBUG(buffInfo.bufferID != 0);
    FVASSERT_DEBUG(buffInfo.bytes != 0);
    FVASSERT_DEBUG(buffInfo.bytesSecond != 0);

    ALint bytesOffset = 0;
    alGetSourcei(sourceID, AL_BYTE_OFFSET, &bytesOffset);
    // If last buffer is too small, playing over next buffer before unqueue.
    // This can be time accuracy problem.
    bytesOffset = std::clamp(bytesOffset, 0, (ALint)buffInfo.bytes);

    double position = buffInfo.timestamp + (static_cast<double>(bytesOffset) / static_cast<double>(buffInfo.bytesSecond));
    return position;
}

void AudioSource::setTimePosition(double t) {
    dequeueBuffers();
    FVASSERT_DEBUG(sourceID != 0);
    std::scoped_lock guard(bufferLock);

    if (this->buffers.empty() == false) {
        const Buffer& buffInfo = buffers.at(0);
        FVASSERT_DEBUG(buffInfo.bufferID != 0);
        FVASSERT_DEBUG(buffInfo.bytes != 0);
        FVASSERT_DEBUG(buffInfo.bytesSecond != 0);

        if (t > buffInfo.timestamp) {
            t -= buffInfo.timestamp;
            ALint bytesOffset = std::clamp(ALint(buffInfo.bytesSecond * t), 0, ALint(buffInfo.bytes));

            alSourcei(sourceID, AL_BYTE_OFFSET, bytesOffset);

            // check error.
            if (ALenum err = alGetError(); err != AL_NO_ERROR) {
                Log::error(std::format("AudioSource error: {}, {}",
                                       err, alGetString(err)));
            }
        }
    }
}

double AudioSource::timeOffset() const {
    dequeueBuffers();
    FVASSERT_DEBUG(sourceID != 0);
    std::scoped_lock guard(bufferLock);

    if (this->buffers.empty() == false) {
        const Buffer& buffInfo = buffers.at(0);
        FVASSERT_DEBUG(buffInfo.bufferID != 0);
        FVASSERT_DEBUG(buffInfo.bytes != 0);
        FVASSERT_DEBUG(buffInfo.bytesSecond != 0);

        ALint bytesOffset = 0;
        alGetSourcei(sourceID, AL_BYTE_OFFSET, &bytesOffset);
        // If last buffer is too small, playing over next buffer before unqueue.
        // This can be time accuracy problem.
        bytesOffset = std::clamp(bytesOffset, 0, (ALint)buffInfo.bytes);

        return static_cast<double>(bytesOffset) / static_cast<double>(buffInfo.bytesSecond);
    }
    return 0;
}

void AudioSource::setTimeOffset(double t) {
    dequeueBuffers();
    FVASSERT_DEBUG(sourceID != 0);
    std::scoped_lock guard(bufferLock);

    if (this->buffers.empty() == false) {
        const Buffer& buffInfo = buffers.at(0);
        FVASSERT_DEBUG(buffInfo.bufferID != 0);
        FVASSERT_DEBUG(buffInfo.bytes != 0);
        FVASSERT_DEBUG(buffInfo.bytesSecond != 0);

        ALint bytesOffset = std::clamp<ALint>(ALint(t * buffInfo.bytesSecond), 0, ALint(buffInfo.bytes));
        alSourcei(sourceID, AL_BYTE_OFFSET, bytesOffset);

        if (ALenum err = alGetError(); err != AL_NO_ERROR) {
            Log::error(std::format("AudioSource error: {}, {}",
                                   err, alGetString(err)));
        }
    }
}

float AudioSource::pitch() const {
    FVASSERT_DEBUG(sourceID != 0);
    float f = 1.0;
    alGetSourcef(sourceID, AL_PITCH, &f);
    return f;
}

void AudioSource::setPitch(float f) {
    FVASSERT_DEBUG(sourceID != 0);
    f = std::max(f, 0.0f);
    alSourcef(sourceID, AL_PITCH, f);
}

float AudioSource::gain() const {
    FVASSERT_DEBUG(sourceID != 0);
    float f = 1.0;
    alGetSourcef(sourceID, AL_GAIN, &f);
    return f;
}

void AudioSource::setGain(float f) {
    FVASSERT_DEBUG(sourceID != 0);
    f = std::max(f, 0.0f);
    alSourcef(sourceID, AL_GAIN, f);
}

float AudioSource::minGain() const {
    FVASSERT_DEBUG(sourceID != 0);
    float f = 0.0;
    alGetSourcef(sourceID, AL_MIN_GAIN, &f);
    return f;
}

void AudioSource::setMinGain(float f) {
    FVASSERT_DEBUG(sourceID != 0);
    f = std::clamp(f, 0.0f, 1.0f);
    alSourcef(sourceID, AL_MIN_GAIN, f);
}

float AudioSource::maxGain() const {
    FVASSERT_DEBUG(sourceID != 0);
    float f = 1.0;
    alGetSourcef(sourceID, AL_MAX_GAIN, &f);
    return f;
}

void AudioSource::setMaxGain(float f) {
    FVASSERT_DEBUG(sourceID != 0);
    f = std::clamp(f, 0.0f, 1.0f);
    alSourcef(sourceID, AL_MAX_GAIN, f);
}

float AudioSource::maxDistance() const {
    FVASSERT_DEBUG(sourceID != 0);
    float f = FLT_MAX;
    alGetSourcef(sourceID, AL_MAX_DISTANCE, &f);
    return f;
}

void AudioSource::setMaxDistance(float f) {
    FVASSERT_DEBUG(sourceID != 0);
    f = std::max(f, 0.0f);
    alSourcef(sourceID, AL_MAX_DISTANCE, f);
}

float AudioSource::rolloffFactor() const {
    FVASSERT_DEBUG(sourceID != 0);
    float f = 1.0;
    alGetSourcef(sourceID, AL_ROLLOFF_FACTOR, &f);
    return f;
}

void AudioSource::setRolloffFactor(float f) {
    FVASSERT_DEBUG(sourceID != 0);
    f = std::max(f, 0.0f);
    alSourcef(sourceID, AL_ROLLOFF_FACTOR, f);
}

float AudioSource::coneOuterGain() const {
    FVASSERT_DEBUG(sourceID != 0);
    float f = 0.0;
    alGetSourcef(sourceID, AL_CONE_OUTER_GAIN, &f);
    return f;
}

void AudioSource::setConeOuterGain(float f) {
    FVASSERT_DEBUG(sourceID != 0);
    f = std::clamp(f, 0.0f, 1.0f);
    alSourcef(sourceID, AL_CONE_OUTER_GAIN, f);
}

float AudioSource::coneInnerAngle() const {
    FVASSERT_DEBUG(sourceID != 0);
    float f = 360.0;
    alGetSourcef(sourceID, AL_CONE_INNER_ANGLE, &f);
    return degreeToRadian(f);
}

void AudioSource::setConeInnerAngle(float f) {
    FVASSERT_DEBUG(sourceID != 0);
    f = std::clamp(radianToDegree(f), 0.0f, 360.0f);
    alSourcef(sourceID, AL_CONE_INNER_ANGLE, f);
}

float AudioSource::coneOuterAngle() const {
    FVASSERT_DEBUG(sourceID != 0);
    float f = 360.0;
    alGetSourcef(sourceID, AL_CONE_OUTER_ANGLE, &f);
    return degreeToRadian(f);
}

void AudioSource::setConeOuterAngle(float f) {
    FVASSERT_DEBUG(sourceID != 0);
    f = std::clamp(radianToDegree(f), 0.0f, 360.0f);
    alSourcef(sourceID, AL_CONE_OUTER_ANGLE, f);
}

float AudioSource::referenceDistance() const {
    FVASSERT_DEBUG(sourceID != 0);
    float f = 1.0;
    alGetSourcef(sourceID, AL_REFERENCE_DISTANCE, &f);
    return f;
}

void AudioSource::setReferenceDistance(float f) {
    FVASSERT_DEBUG(sourceID != 0);
    f = std::max(f, 0.0f);
    alSourcef(sourceID, AL_REFERENCE_DISTANCE, f);
}

Vector3 AudioSource::position() const {
    FVASSERT_DEBUG(sourceID != 0);
    Vector3 v(0, 0, 0);
    alGetSource3f(sourceID, AL_POSITION, &v.x, &v.y, &v.z);
    return v;
}

void AudioSource::setPosition(const Vector3& v) {
    FVASSERT_DEBUG(sourceID != 0);
    alSource3f(sourceID, AL_POSITION, v.x, v.y, v.z);
}

Vector3 AudioSource::velocity() const {
    FVASSERT_DEBUG(sourceID != 0);
    Vector3 v(0, 0, 0);
    alGetSource3f(sourceID, AL_VELOCITY, &v.x, &v.y, &v.z);
    return v;
}

void AudioSource::setVelocity(const Vector3& v) {
    FVASSERT_DEBUG(sourceID != 0);
    alSource3f(sourceID, AL_VELOCITY, v.x, v.y, v.z);
}

Vector3 AudioSource::direction() const {
    FVASSERT_DEBUG(sourceID != 0);
    Vector3 v(0, 0, 0);
    alGetSource3f(sourceID, AL_DIRECTION, &v.x, &v.y, &v.z);
    return v;
}

void AudioSource::setDirection(const Vector3& v) {
    FVASSERT_DEBUG(sourceID != 0);
    alSource3f(sourceID, AL_DIRECTION, v.x, v.y, v.z);
}
