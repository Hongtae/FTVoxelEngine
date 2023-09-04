#include "../Libs/OpenAL/include/AL/al.h"
#include "../Libs/OpenAL/include/AL/alc.h"
#include "../Libs/OpenAL/include/AL/alext.h"

#include "AudioListener.h"

using namespace FV;

AudioListener::AudioListener(std::shared_ptr<AudioDevice> dev)
    : device(dev) {
}

AudioListener::~AudioListener() {
}

float AudioListener::gain() const {
    float v = 1.0;
    alGetListenerf(AL_GAIN, &v);
    return v;
}

void AudioListener::setGain(float v) {
    alListenerf(AL_GAIN, std::max(v, 0.0f));
}

Vector3 AudioListener::position() const {
    Vector3 pos = {};
    alGetListener3f(AL_POSITION, &pos.x, &pos.y, &pos.z);
    return pos;
}

void AudioListener::setPosition(const Vector3& v) {
    alListener3f(AL_POSITION, v.x, v.y, v.z);
}

Vector3 AudioListener::velocity() const {
    Vector3 v = {};
    alGetListener3f(AL_VELOCITY, &v.x, &v.y, &v.z);
    return v;
}

void AudioListener::setVelocity(const Vector3& v) {
    alListener3f(AL_VELOCITY, v.x, v.y, v.z);
}

Vector3 AudioListener::forward() const {
    ALfloat v[6] = {
    0.0, 0.0, -1.0, // forward
    0.0, 1.0, 0.0,  // up
    };
    alGetListenerfv(AL_ORIENTATION, v);
    return Vector3(v[0], v[1], v[2]);
}

void AudioListener::setForward(const Vector3& fw) {
    ALfloat v[6] = {
    0.0, 0.0, -1.0, // forward
    0.0, 1.0, 0.0,  // up
    };
    alGetListenerfv(AL_ORIENTATION, v);
    v[0] = fw.x;
    v[1] = fw.y;
    v[2] = fw.z;
    alListenerfv(AL_ORIENTATION, v);
}

Vector3 AudioListener::up() const {
    ALfloat v[6] = {
        0.0, 0.0, -1.0, // forward
        0.0, 1.0, 0.0,  // up
    };
    alGetListenerfv(AL_ORIENTATION, v);
    return Vector3(v[3], v[4], v[5]);
}

void AudioListener::setUp(const Vector3& up) {
    ALfloat v[6] = {
    0.0, 0.0, -1.0, // forward
    0.0, 1.0, 0.0,  // up
    };
    alGetListenerfv(AL_ORIENTATION, v);
    v[3] = up.x;
    v[4] = up.y;
    v[5] = up.z;
    alListenerfv(AL_ORIENTATION, v);
}

void AudioListener::setOrientation(const Matrix3& m) {
    setOrientation(m.row3(), m.row2());
}

void AudioListener::setOrientation(const Vector3& forward, const Vector3& up) {
    Vector3 f = forward.normalized();
    Vector3 u = up.normalized();
    ALfloat v[6] = { f.x, f.y, f.z, u.x, u.y, u.z };
    alListenerfv(AL_ORIENTATION, v);
}
