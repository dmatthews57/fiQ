#pragma once
//==========================================================================================================================
// LogMessage.h : Runtime package for message to be logged, with optional structured context data
//==========================================================================================================================

#include "Tools/StringOps.h"
#include "Tools/TimeClock.h"

namespace FIQCPPBASE {

//==========================================================================================================================
// LogLevel: Defines values for use throughout application
enum class LogLevel : int {
	Debug = 200,
	Info = 400,
	Warn = 600,
	Error = 800,
	Fatal = 1000
};
template<typename T, std::enable_if_t<std::is_same_v<T, std::underlying_type_t<LogLevel>>, int> = 0>
LogLevel ToLogLevel(T t) {
	switch(static_cast<LogLevel>(t)) {
	case LogLevel::Fatal: return LogLevel::Fatal;
	case LogLevel::Error: return LogLevel::Error;
	case LogLevel::Warn: return LogLevel::Warn;
	case LogLevel::Info: return LogLevel::Info;
	default: return LogLevel::Debug;
	};
}

//==========================================================================================================================
// LogMessage: Container for a single log message, with associated (optional) structured data
class LogMessage {
private: struct pass_key {}; // Private function pass-key definition
public:

	// Type definitions:
	using ContextEntry = std::pair<std::string, std::string>;
	using ContextEntries = std::vector<ContextEntry>;

	//======================================================================================================================
	// Named constructor
	template<typename...Args>
	static std::unique_ptr<const LogMessage> Create(LogLevel _level, Args&&...args) {
		return std::make_unique<const LogMessage>(pass_key{}, _level, std::forward<Args>(args)...);
	}

	//======================================================================================================================
	// Public const accessors
	const LogLevel GetLevel() const noexcept {return level;}
	const std::string& GetString() const noexcept {return message;}
	const TimeClock& GetTimestamp() const noexcept {return timestamp;}
	const ContextEntries& GetContext() const noexcept {return context;}
	const FormatEscape EscapeFormats() const noexcept {return escapeformats;}

	//======================================================================================================================
	// Public constructors
	// - Public to allow make_unique access, locked by pass_key to enforce use of named constructor
	LogMessage(pass_key, LogLevel _level,
		_In_reads_(len) const char* buf, size_t len, // Copy of compile-time string literal
		ContextEntries&& _context, FormatEscape _escapeformats) noexcept(false)
		: level(_level), message(buf, len), context(std::move(_context)), escapeformats(_escapeformats) {}
	LogMessage(pass_key, LogLevel _level,
		std::string&& _message, // String with placeholders inserted at runtime
		ContextEntries&& _context, FormatEscape _escapeformats) noexcept(false)
		: level(_level), message(std::move(_message)), context(std::move(_context)), escapeformats(_escapeformats) {}

private:
	const LogLevel level;
	const std::string message;
	const TimeClock timestamp;
	const ContextEntries context;
	const FormatEscape escapeformats;
};

}; // (end namespace FIQCPPBASE)
