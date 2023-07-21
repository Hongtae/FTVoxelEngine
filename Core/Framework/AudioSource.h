#pragma once
#include "../include.h"
#include <vector>
#include <mutex>
#include "Vector3.h"

namespace FV
{
	class AudioDevice;
    class FVCORE_API AudioSource
    {
    public:
        AudioSource(std::shared_ptr<AudioDevice>, uint32_t);
        ~AudioSource();

		enum State 
		{
			StateStopped,
			StatePlaying,
			StatePaused,
		};
		State state() const;
		bool enqueueBuffer(int sampleRate, int bits, int channels, const void* data, size_t bytes, double timeStamp);
		void dequeueBuffers() const;
		size_t numberOfBuffersInQueue() const;

		double timePosition() const;
		void setTimePosition(double);
		double timeOffset() const;
		void setTimeOffset(double);

		void play();
		void pause();
		void stop();

		// pitch control
		float pitch() const;
		void setPitch(float f);

		// audio gain
		float gain() const;
		void setGain(float f);

		// min gain
		float minGain() const;
		void setMinGain(float f);

		// max gain
		float maxGain() const;
		void setMaxGain(float f);

		// max distance
		float maxDistance() const;
		void setMaxDistance(float f);

		// rolloff factor
		float rolloffFactor() const;
		void setRolloffFactor(float f);

		// cone outer gain
		float coneOuterGain() const;
		void setConeOuterGain(float f);

		// cone inner angle
		float coneInnerAngle() const;
		void setConeInnerAngle(float f);

		// cone outer angle
		float coneOuterAngle() const;
		void setConeOuterAngle(float f);

		// reference distance
		float referenceDistance() const;
		void setReferenceDistance(float f);

		// source position
		Vector3 position() const;
		void setPosition(const Vector3& v);

		// source velocity
		Vector3 velocity() const;
		void setVelocity(const Vector3& v);

		// source direction
		Vector3 direction() const;
		void setDirection(const Vector3& v);

	private:
		uint32_t sourceID;
		mutable std::mutex bufferLock;
		struct Buffer
		{
			double timestamp;
			size_t bytes;
			size_t bytesSecond;
			uint32_t bufferID;
		};
		mutable std::vector<Buffer> buffers;
		std::shared_ptr<AudioDevice> device;
    };
}
