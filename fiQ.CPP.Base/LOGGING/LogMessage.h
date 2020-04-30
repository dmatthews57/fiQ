#pragma once
//==========================================================================================================================
// LogMessage.h : Runtime package for message to be logged, with optional structured context data
//==========================================================================================================================

#include "Tools/TimeClock.h"

namespace FIQCPPBASE {

//==========================================================================================================================
// LogLevel: Defines values for use throughout application
enum class LogLevel : int {
	None = 0,
	Debug = 200,
	Info = 400,
	Warn = 600,
	Error = 800,
	Fatal = 1000
};

//==========================================================================================================================
// LogMessage: Container for a single log message, with associated (optional) structured data
class LogMessage {
private: struct pass_key {}; // Private function pass-key definition
public:

	// Type definitions:
	using ContextEntry = std::pair<std::string,std::string>;
	using ContextEntries = std::vector<ContextEntry>;

	//======================================================================================================================
	// Named constructor
	template<typename...Args>
	static std::unique_ptr<LogMessage> Create(LogLevel _level, Args&&...args) {
		return std::make_unique<LogMessage>(pass_key(), _level, std::forward<Args>(args)...);
	}

	//======================================================================================================================
	// Public const accessors
	const LogLevel GetLevel() const noexcept {return level;}
	const std::string& GetMessage() const noexcept {return message;}
	const TimeClock& GetTimestamp() const noexcept {return timestamp;}
	const ContextEntries& GetContext() const noexcept {return context;}

	//======================================================================================================================
	// Public context update functions: Copy all entries from a source collection
	void AddContext(const ContextEntries& ce) {
		if(ce.empty() == false) {
			context.reserve(context.size() + ce.size());
			context.insert(context.end(), ce.cbegin(), ce.cend());
		}
	}
	// Public context update functions: Add single entry to context collection
	void AddContext(_In_z_ const char* a, _In_z_ const char* b) {
		context.emplace_back(std::piecewise_construct,
			std::forward_as_tuple(a),
			std::forward_as_tuple(b)
		);
	}

	//======================================================================================================================
	// Public constructors
	// - Public to allow make_unique access, locked by pass_key to enforce use of named constructor
	LogMessage(pass_key, LogLevel _level, _In_reads_(len) const char* buf, size_t len, ContextEntries&& _context) noexcept(false)
		: level(_level), message(buf, len), context(std::move(_context)) {}
	LogMessage(pass_key, LogLevel _level, std::string&& _message, ContextEntries&& _context) noexcept(false)
		: level(_level), message(std::move(_message)), context(std::move(_context)) {}

private:
	const LogLevel level;
	const std::string message;
	const TimeClock timestamp;
	ContextEntries context;
};

}; // (end namespace FIQCPPBASE)
