#pragma once
#include "../include.h"
#include <atomic>
#include <mutex>

namespace FV
{
	class SpinLock
	{
		mutable std::atomic<bool> _lock; // true for unlocked
		enum { Unlocked = false, Locked = true };
	public:
		void lock() const {
			bool expected = Unlocked;
			while (_lock.compare_exchange_strong(
				expected, Locked,
				std::memory_order_release,
				std::memory_order_relaxed) == false) {
			}
		}

		bool try_lock() const {
			bool expected = Unlocked;
			return _lock.compare_exchange_strong(expected, Locked,
												 std::memory_order_release,
												 std::memory_order_relaxed);
		}

		void unlock() const {
			FVASSERT_DEBUG(_lock.load() == Locked); // must be locked before release
			_lock.store(Unlocked);
		}
	};
}
