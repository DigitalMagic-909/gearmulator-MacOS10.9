#pragma once

// MAC-LEGACY: std::shared_mutex is implemented in the system libc++ of macOS 10.12 and newer only. For older
// deployment targets (OS X 10.9 - 10.11) baseLib::SharedMutex is a small replacement with the same interface,
// the same algorithm as libc++ (a write-entered bit plus a reader count, two gates), so std::shared_lock,
// std::unique_lock and std::lock_guard work with it unchanged. Everywhere else it is std::shared_mutex.

#if defined(__APPLE__) && defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__) && __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 101200

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <shared_mutex>	// std::shared_lock (a header-only template) for the users of this class

namespace baseLib
{
	class SharedMutex
	{
	public:
		SharedMutex() = default;
		SharedMutex(const SharedMutex&) = delete;
		SharedMutex& operator=(const SharedMutex&) = delete;

		void lock()
		{
			std::unique_lock<std::mutex> l(m_mutex);
			m_gate1.wait(l, [this] { return (m_state & WriteEntered) == 0; });
			m_state |= WriteEntered;
			m_gate2.wait(l, [this] { return (m_state & ReadersMask) == 0; });
		}

		bool try_lock()
		{
			std::unique_lock<std::mutex> l(m_mutex, std::try_to_lock);
			if (!l.owns_lock() || m_state != 0)
				return false;
			m_state = WriteEntered;
			return true;
		}

		void unlock()
		{
			{
				std::lock_guard<std::mutex> l(m_mutex);
				m_state = 0;
			}
			m_gate1.notify_all();
		}

		void lock_shared()
		{
			std::unique_lock<std::mutex> l(m_mutex);
			m_gate1.wait(l, [this] { return (m_state & WriteEntered) == 0 && (m_state & ReadersMask) < ReadersMask; });
			++m_state;
		}

		bool try_lock_shared()
		{
			std::unique_lock<std::mutex> l(m_mutex, std::try_to_lock);
			if (!l.owns_lock() || (m_state & WriteEntered) != 0 || (m_state & ReadersMask) == ReadersMask)
				return false;
			++m_state;
			return true;
		}

		void unlock_shared()
		{
			std::lock_guard<std::mutex> l(m_mutex);
			const uint32_t readers = (m_state & ReadersMask) - 1;
			m_state = (m_state & WriteEntered) | readers;
			if (m_state & WriteEntered)
			{
				if (readers == 0)
					m_gate2.notify_one();	// the waiting writer
			}
			else if (readers == ReadersMask - 1)
			{
				m_gate1.notify_one();		// a reader blocked by the reader limit
			}
		}

	private:
		static constexpr uint32_t WriteEntered = 1u << 31;
		static constexpr uint32_t ReadersMask = ~WriteEntered;

		std::mutex m_mutex;
		std::condition_variable m_gate1;
		std::condition_variable m_gate2;
		uint32_t m_state = 0;
	};
}

#else

#include <shared_mutex>

namespace baseLib
{
	using SharedMutex = std::shared_mutex;
}

#endif
