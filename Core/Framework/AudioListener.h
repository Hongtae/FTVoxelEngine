#pragma once
#include "../include.h"
#include "Vector3.h"
#include "Matrix3.h"

namespace FV
{
    class AudioDevice;
    class FVCORE_API AudioListener
    {
    public:
        AudioListener(std::shared_ptr<AudioDevice>);
        ~AudioListener();

        float gain() const;
        void setGain(float f);

        Vector3 position() const;
        void setPosition(const Vector3& v);

        Vector3 velocity() const;
        void setVelocity(const Vector3& v);

        Vector3 forward() const;
        void setForward(const Vector3& v);

        Vector3 up() const;
        void setUp(const Vector3& v);

        void setOrientation(const Matrix3& m);
        void setOrientation(const Vector3& forward, const Vector3& up);

    private:
        std::shared_ptr<AudioDevice> device;
    };
}
