#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <mutex>
#include <deque>
#include <cstdarg>
#include <fstream>

enum class LogLevel { Debug, Info, Warn, Error };

struct LogEntry {
	LogLevel level;
	unsigned long long timestamp; // GetTickCount64()
	std::string message;
};

class GuiLogger {
public:
	static GuiLogger& instance() {
		static GuiLogger s;
		return s;
	}

	void log(LogLevel level, const char* fmt, ...) {
		va_list args;
		va_start(args, fmt);
		logV(level, fmt, args);
		va_end(args);
	}

	void logV(LogLevel level, const char* fmt, va_list args) {
		char buf[2048];
		vsnprintf(buf, sizeof(buf), fmt, args);

		LogEntry entry;
		entry.level = level;
		entry.timestamp = GetTickCount64();
		entry.message = buf;

		std::lock_guard<std::mutex> lock(m_mutex);
		m_entries.push_back(std::move(entry));
		if (m_entries.size() > m_maxEntries) {
			m_entries.pop_front();
		}

		// Also print to console (visible when AllocConsole is active)
		printf("%s\n", buf);

		// Optional file sink
		if (m_fileSink.is_open()) {
			m_fileSink << buf << "\n";
			m_fileSink.flush();
		}
	}

	void debug(const char* fmt, ...) {
		va_list args; va_start(args, fmt);
		logV(LogLevel::Debug, fmt, args);
		va_end(args);
	}
	void info(const char* fmt, ...) {
		va_list args; va_start(args, fmt);
		logV(LogLevel::Info, fmt, args);
		va_end(args);
	}
	void warn(const char* fmt, ...) {
		va_list args; va_start(args, fmt);
		logV(LogLevel::Warn, fmt, args);
		va_end(args);
	}
	void error(const char* fmt, ...) {
		va_list args; va_start(args, fmt);
		logV(LogLevel::Error, fmt, args);
		va_end(args);
	}

	// Thread-safe: returns a snapshot of all entries from startIndex
	std::vector<LogEntry> getEntries(size_t startIndex = 0) const {
		std::lock_guard<std::mutex> lock(m_mutex);
		std::vector<LogEntry> result;
		if (startIndex < m_entries.size()) {
			result.assign(m_entries.begin() + startIndex, m_entries.end());
		}
		return result;
	}

	size_t entryCount() const {
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_entries.size();
	}

	void clear() {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_entries.clear();
	}

	void setMaxEntries(size_t max) { m_maxEntries = max; }

	void setFileSink(const std::string& path) {
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_fileSink.is_open()) m_fileSink.close();
		if (!path.empty()) {
			m_fileSink.open(path, std::ios::app);
		}
	}

	void closeFileSink() {
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_fileSink.is_open()) m_fileSink.close();
	}

private:
	GuiLogger() = default;
	mutable std::mutex m_mutex;
	std::deque<LogEntry> m_entries;
	size_t m_maxEntries = 10000;
	std::ofstream m_fileSink;
};

#define LOG_DEBUG(fmt, ...) GuiLogger::instance().debug(fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  GuiLogger::instance().info(fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  GuiLogger::instance().warn(fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) GuiLogger::instance().error(fmt, ##__VA_ARGS__)
