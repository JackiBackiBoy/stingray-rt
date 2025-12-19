#include "Logger.hpp"

void SRLogger::set_level(SRLoggerLevel level) {
	m_Level = level;
}

SRLogger::~SRLogger() {
	if (m_File) {
		std::fclose(m_File);
	}
}

bool SRLogger::should_log(SRLoggerLevel level) const {
	return (int)level >= (int)SRLOG_COMPILED_LEVEL && (int)level >= (int)m_Level;
}

const char* SRLogger::level_to_str(SRLoggerLevel level) {
	switch (level) {
	case SRLoggerLevel::Trace:    return "TRACE";
	case SRLoggerLevel::Debug:    return "DEBUG";
	case SRLoggerLevel::Info:     return "INFO";
	case SRLoggerLevel::Warn:     return "WARN";
	case SRLoggerLevel::Error:    return "ERROR";
	case SRLoggerLevel::Critical: return "CRIT";
	default:                      return "OFF";
	}
}

const char* SRLogger::level_to_color(SRLoggerLevel level) {
	switch (level) {
	case SRLoggerLevel::Trace:    return SRLOG_COLOR_TRACE;
	case SRLoggerLevel::Debug:    return SRLOG_COLOR_DEBUG;
	case SRLoggerLevel::Info:     return SRLOG_COLOR_INFO;
	case SRLoggerLevel::Warn:     return SRLOG_COLOR_WARN;
	case SRLoggerLevel::Error:    return SRLOG_COLOR_ERROR;
	case SRLoggerLevel::Critical: return SRLOG_COLOR_CRITICAL;
	default:                      return SRLOG_COLOR_RESET;
	}
}

const char* SRLogger::category_to_color(const char* cat) {
	if (!cat || !*cat) {
		return SRLOG_COLOR_CAT_APP;
	}

	if (std::strcmp(cat, SRLOG_CAT_VULKAN) == 0) {
		return SRLOG_COLOR_CAT_VULKAN;
	}
	if (std::strcmp(cat, SRLOG_CAT_DX12) == 0) {
		return SRLOG_COLOR_CAT_DX12;
	}

	return SRLOG_COLOR_CAT_APP; // default for unknown categories
}
