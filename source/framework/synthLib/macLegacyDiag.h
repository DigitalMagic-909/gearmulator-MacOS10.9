#pragma once

// MAC-LEGACY: diagnostics for builds that target OS X 10.9 - 10.11. They write a small text file next to the
// plugin's ROM folder (diagnose.txt) that tells whether the device was created, whether the host calls the plugin,
// whether MIDI arrives and how loud the board, the device and the host output are. Compiled out everywhere else.

#if defined(__APPLE__) && defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__) && __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 101200
#define TUS_MAC_LEGACY_DIAG 1
#endif

#ifdef TUS_MAC_LEGACY_DIAG

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

namespace synthLib::diag
{
	struct State
	{
		std::mutex mutex;
		std::string file;
		bool reporterStarted = false;

		std::atomic<uint64_t> blocks{0};
		std::atomic<uint64_t> samples{0};
		std::atomic<uint64_t> midiIn{0};
		std::atomic<uint64_t> boardBlocks{0};
		std::atomic<uint64_t> invalidBlocks{0};

		std::atomic<int> hostRate{0};
		std::atomic<int> blockSize{0};
		std::atomic<int> inChannels{-1};
		std::atomic<int> outChannels{-1};
		std::atomic<int> bufferChannels{-1};
		std::atomic<int> mapped{-1};
		std::atomic<int> latency{-1};

		std::atomic<float> boardPeak{0.0f};
		std::atomic<float> devicePeak{0.0f};
		std::atomic<float> hostPeak{0.0f};
		std::atomic<float> masterVolume{-99.0f};
	};

	inline State& state()
	{
		static State s;
		return s;
	}

	inline std::string now()
	{
		const auto t = std::time(nullptr);
		char buf[32];
		std::strftime(buf, sizeof(buf), "%H:%M:%S", std::localtime(&t));
		return buf;
	}

	inline void writeLine(const std::string& _line)
	{
		auto& s = state();
		std::lock_guard lock(s.mutex);
		if (s.file.empty())
			return;
		std::ofstream o(s.file, std::ios::app);
		if (o.is_open())
			o << now() << "  " << _line << '\n';
	}

	inline void peak(std::atomic<float>& _a, float _v)
	{
		_v = std::fabs(_v);
		if (!(_v == _v))	// NaN shows as a huge peak
			_v = 1e30f;
		float cur = _a.load(std::memory_order_relaxed);
		while (_v > cur && !_a.compare_exchange_weak(cur, _v, std::memory_order_relaxed)) {}
	}

	inline void report()
	{
		auto& s = state();
		std::ostringstream ss;
		ss << std::setprecision(4)
			<< "host blocks=" << s.blocks.load() << " samples=" << s.samples.load()
			<< " rate=" << s.hostRate.load() << " blocksize=" << s.blockSize.load()
			<< " ch in/out/buffer=" << s.inChannels.load() << '/' << s.outChannels.load() << '/' << s.bufferChannels.load()
			<< " mapped=" << s.mapped.load() << " latency=" << s.latency.load()
			<< " | midi in=" << s.midiIn.load()
			<< " | invalid device blocks=" << s.invalidBlocks.load()
			<< " | board blocks=" << s.boardBlocks.load()
			<< " | peaks (last 2 s): board=" << s.boardPeak.exchange(0.0f)
			<< " device=" << s.devicePeak.exchange(0.0f)
			<< " output=" << s.hostPeak.exchange(0.0f)
			<< " | master volume=" << s.masterVolume.load();
		writeLine(ss.str());
	}

	// first call wins: starts the file and a reporter that writes a status line every 2 seconds for 5 minutes
	inline void start(const std::string& _file, const std::string& _header)
	{
		auto& s = state();
		{
			std::lock_guard lock(s.mutex);
			if (s.reporterStarted)
				return;
			s.reporterStarted = true;
			s.file = _file;
			std::ofstream o(s.file, std::ios::trunc);
			if (o.is_open())
				o << "JE8086 diagnose (OS X 10.9 build)\n" << _header << '\n';
		}

		std::thread([]
		{
			for (int i = 0; i < 150; ++i)
			{
				std::this_thread::sleep_for(std::chrono::seconds(2));
				report();
			}
			writeLine("reporter stopped after 5 minutes");
		}).detach();
	}
}

#define TUS_DIAG(_x) do { std::ostringstream tusDiagSs_; tusDiagSs_ << _x; synthLib::diag::writeLine(tusDiagSs_.str()); } while (0)

#else

#define TUS_DIAG(_x) do {} while (0)

#endif
