#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>

enum class SRLoggerLevel : uint8_t {
	Trace,
	Debug,
	Info,
	Warn,
	Error,
	Critical,
	Off
};

#ifndef SRLOG_COMPILED_LEVEL
	#ifdef _DEBUG
		#define SRLOG_COMPILED_LEVEL SRLoggerLevel::Trace
	#else
		#define SRLOG_COMPILED_LEVEL SRLoggerLevel::Error
	#endif
#endif

#define SRLOG_CAT_VULKAN "VK"
#define SRLOG_CAT_DX12   "DX12"
#define SRLOG_CAT_APP    "APP"

#define SRLOG_COLOR_RESET       "\033[0m"
#define SRLOG_COLOR_TRACE       "\033[90m"  // bright black / gray
#define SRLOG_COLOR_DEBUG       "\033[37m"  // white
#define SRLOG_COLOR_INFO        "\033[32m"  // green
#define SRLOG_COLOR_WARN        "\033[33m"  // yellow
#define SRLOG_COLOR_ERROR       "\033[31m"  // red
#define SRLOG_COLOR_CRITICAL    "\033[1;31m"// bold red

#define SRLOG_COLOR_CAT_VULKAN  "\033[36m"   // cyan
#define SRLOG_COLOR_CAT_DX12    "\033[34m"   // blue
#define SRLOG_COLOR_CAT_APP     "\033[35m"   // magenta (default/app)

class SRLogger {
public:
	static SRLogger& get() {
		static SRLogger instance;
		return instance;
	}

	void set_level(SRLoggerLevel level);

	template <class... Args>
	void log(SRLoggerLevel level, const char* fmt, Args&&... args) {
		if (!should_log(level)) {
			return;
		}

		char msg[4096];
		int n = std::snprintf(msg, sizeof(msg), fmt, to_cstr(args)...);

		if (n < 0) {
			return;
		}

		if (n >= (int)sizeof(msg)) {
			n = sizeof(msg) - 1;
		}

		char line[4600];
		auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		std::tm tm = {};
		localtime_s(&tm, &now);

		int m = std::snprintf(
			line,
			sizeof(line),
			"[%02d:%02d:%02d] %s[%s]%s %s\n",
			tm.tm_hour,
			tm.tm_min,
			tm.tm_sec,
			level_to_color(level),      // start color
			level_to_str(level),        // level name
			SRLOG_COLOR_RESET,          // reset after
			msg
		);

		if (m < 0) {
			return;
		}

		std::scoped_lock lock{ m_Mutex };
		if (m_ConsoleEnabled) {
			std::fwrite(line, 1, (size_t)m, stdout);
		}
		if (m_File) {
			std::fwrite(line, 1, (size_t)m, m_File);
		}

		// NOTE: The idea here is that if a message is logged with a level of
		// SRLoggerLevel::Error it indicates that a crash might be imminent in
		// the part of the code that the log function is called from. Meaning
		// that we must flush the buffer into the OS in order for the log to
		// appear before the program crash.
		if (level >= SRLoggerLevel::Error) {
			if (m_ConsoleEnabled) {
				std::fflush(stdout);
			}
			if (m_File) {
				std::fflush(m_File);
			}
		}
	}

	template <class... Args>
	void log_cat(SRLoggerLevel level, const char* cat, const char* fmt, Args&&... args) {
		if (!should_log(level)) {
			return;
		}

		char msg[4096];
		int n = std::snprintf(msg, sizeof(msg), fmt, to_cstr(args)...);

		if (n < 0) {
			return;
		}

		if (n >= (int)sizeof(msg)) {
			n = sizeof(msg) - 1;
		}

		char line[4600];
		auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		std::tm tm = {};
		localtime_s(&tm, &now);

		int m = std::snprintf(
			line,
			sizeof(line),
			"[%02d:%02d:%02d] %s[%s]%s%s[%s]%s %s\n",
			tm.tm_hour,
			tm.tm_min,
			tm.tm_sec,
			level_to_color(level),
			level_to_str(level),
			SRLOG_COLOR_RESET,
			category_to_color(cat),
			(cat && *cat) ? cat : SRLOG_CAT_APP,
			SRLOG_COLOR_RESET,
			msg
		);

		if (m < 0) {
			return;
		}

		std::scoped_lock lock{ m_Mutex };
		if (m_ConsoleEnabled) {
			std::fwrite(line, 1, (size_t)m, stdout);
		}
		if (m_File) {
			// TODO: Files should be plain without ANSI codes
			std::fwrite(line, 1, (size_t)m, m_File);
		}

		// NOTE: The idea here is that if a message is logged with a level of
		// SRLoggerLevel::Error it indicates that a crash might be imminent in
		// the part of the code that the log function is called from. Meaning
		// that we must flush the buffer into the OS in order for the log to
		// appear before the program crash.
		if (level >= SRLoggerLevel::Error) {
			if (m_ConsoleEnabled) {
				std::fflush(stdout);
			}
			if (m_File) {
				std::fflush(m_File);
			}
		}
	}

	template<class... A> void trace(const char* f, A&&... a) { log(SRLoggerLevel::Trace, f, std::forward<A>(a)...); }
	template<class... A> void debug(const char* f, A&&... a) { log(SRLoggerLevel::Debug, f, std::forward<A>(a)...); }
	template<class... A> void info(const char* f, A&&... a) { log(SRLoggerLevel::Info, f, std::forward<A>(a)...); }
	template<class... A> void warn(const char* f, A&&... a) { log(SRLoggerLevel::Warn, f, std::forward<A>(a)...); }
	template<class... A> void error(const char* f, A&&... a) { log(SRLoggerLevel::Error, f, std::forward<A>(a)...); }
	template<class... A> void critical(const char* f, A&&... a) { log(SRLoggerLevel::Critical, f, std::forward<A>(a)...); }

	template<class... A> void trace_cat(const char* c, const char* f, A&&... a) { log_cat(SRLoggerLevel::Trace, c, f, std::forward<A>(a)...); }
	template<class... A> void debug_cat(const char* c, const char* f, A&&... a) { log_cat(SRLoggerLevel::Debug, c, f, std::forward<A>(a)...); }
	template<class... A> void info_cat(const char* c, const char* f, A&&... a) { log_cat(SRLoggerLevel::Info, c, f, std::forward<A>(a)...); }
	template<class... A> void warn_cat(const char* c, const char* f, A&&... a) { log_cat(SRLoggerLevel::Warn, c, f, std::forward<A>(a)...); }
	template<class... A> void error_cat(const char* c, const char* f, A&&... a) { log_cat(SRLoggerLevel::Error, c, f, std::forward<A>(a)...); }
	template<class... A> void crit_cat(const char* c, const char* f, A&&... a) { log_cat(SRLoggerLevel::Critical, c, f, std::forward<A>(a)...); }

private:
	SRLogger() = default;
	~SRLogger();

	bool should_log(SRLoggerLevel level) const;
	static const char* level_to_str(SRLoggerLevel level);
	static const char* level_to_color(SRLoggerLevel level);
	static const char* category_to_color(const char* cat);

	template<class T> static auto to_cstr(T&& v) -> decltype(v) { return std::forward<T>(v); }
	static const char* to_cstr(const std::string& s) { return s.c_str(); }
	static const char* to_cstr(const char* s) { return s ? s : ""; }
	static const char* to_cstr(std::string_view) = delete;

	std::mutex m_Mutex;
	FILE* m_File = nullptr;
	bool m_ConsoleEnabled = true;
	SRLoggerLevel m_Level = SRLOG_COMPILED_LEVEL;
};

#define SRLOG_TRACE(fmt, ...)    do { if constexpr (SRLoggerLevel::Trace    >= SRLOG_COMPILED_LEVEL) SRLogger::get().trace(fmt, ##__VA_ARGS__); } while(0)
#define SRLOG_DEBUG(fmt, ...)    do { if constexpr (SRLoggerLevel::Debug    >= SRLOG_COMPILED_LEVEL) SRLogger::get().debug(fmt, ##__VA_ARGS__); } while(0)
#define SRLOG_INFO(fmt, ...)     do { if constexpr (SRLoggerLevel::Info     >= SRLOG_COMPILED_LEVEL) SRLogger::get().info (fmt, ##__VA_ARGS__); } while(0)
#define SRLOG_WARN(fmt, ...)     do { if constexpr (SRLoggerLevel::Warn     >= SRLOG_COMPILED_LEVEL) SRLogger::get().warn (fmt, ##__VA_ARGS__); } while(0)
#define SRLOG_ERROR(fmt, ...)    do { if constexpr (SRLoggerLevel::Error    >= SRLOG_COMPILED_LEVEL) SRLogger::get().error(fmt, ##__VA_ARGS__); } while(0)
#define SRLOG_CRITICAL(fmt, ...) do { if constexpr (SRLoggerLevel::Critical >= SRLOG_COMPILED_LEVEL) SRLogger::get().critical(fmt, ##__VA_ARGS__); } while(0)

// New: category variants
#define SRLOG_TRACE_CAT(cat, fmt, ...)    do { if constexpr (SRLoggerLevel::Trace    >= SRLOG_COMPILED_LEVEL) SRLogger::get().trace_cat(cat, fmt, ##__VA_ARGS__); } while(0)
#define SRLOG_DEBUG_CAT(cat, fmt, ...)    do { if constexpr (SRLoggerLevel::Debug    >= SRLOG_COMPILED_LEVEL) SRLogger::get().debug_cat(cat, fmt, ##__VA_ARGS__); } while(0)
#define SRLOG_INFO_CAT(cat, fmt, ...)     do { if constexpr (SRLoggerLevel::Info     >= SRLOG_COMPILED_LEVEL) SRLogger::get().info_cat (cat, fmt, ##__VA_ARGS__); } while(0)
#define SRLOG_WARN_CAT(cat, fmt, ...)     do { if constexpr (SRLoggerLevel::Warn     >= SRLOG_COMPILED_LEVEL) SRLogger::get().warn_cat (cat, fmt, ##__VA_ARGS__); } while(0)
#define SRLOG_ERROR_CAT(cat, fmt, ...)    do { if constexpr (SRLoggerLevel::Error    >= SRLOG_COMPILED_LEVEL) SRLogger::get().error_cat(cat, fmt, ##__VA_ARGS__); } while(0)
#define SRLOG_CRITICAL_CAT(cat, fmt, ...) do { if constexpr (SRLoggerLevel::Critical >= SRLOG_COMPILED_LEVEL) SRLogger::get().crit_cat (cat, fmt, ##__VA_ARGS__); } while(0)
