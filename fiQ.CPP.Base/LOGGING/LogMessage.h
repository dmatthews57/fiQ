#pragma once
//==========================================================================================================================
// LogMessage.h : Runtime package for message to be logged, with optional structured context data
//==========================================================================================================================

#include "Tools/TimeClock.h"

namespace FIQCPPBASE {

enum class LogLevel : int {
	None = 0,
	Debug = 200,
	Info = 400,
	Warn = 600,
	Error = 800
};

class LogMessage {
private: struct pass_key {}; // Private function pass-key definition
public:
	using ContextEntry = std::pair<std::string,std::string>;
	using ContextEntries = std::vector<ContextEntry>;

	// Named constructor
	template<typename...Args>
	static std::unique_ptr<LogMessage> Create(LogLevel _level, Args&&...args) {
		return std::make_unique<LogMessage>(pass_key(), _level, std::forward<Args>(args)...);
	}

	// Public accessors
	const LogLevel GetLevel() const noexcept {return level;}
	const std::string& GetMessage() const noexcept {return message;}
	const TimeClock& GetTimestamp() const noexcept {return timestamp;}
	const ContextEntries& GetContext() const noexcept {return context;}

	// Public context update functions
	void AddContext(const ContextEntries& ce) {
		if(ce.empty() == false) {
			context.reserve(context.size() + ce.size());
			context.insert(context.end(), ce.cbegin(), ce.cend());
		}
	}
	void AddContext(_In_z_ const char* a, _In_z_ const char* b) {
		context.emplace_back(std::piecewise_construct,
			std::forward_as_tuple(a),
			std::forward_as_tuple(b)
		);
	}

	// Public constructors
	// - Public to allow make_unique access, locked by pass_key to enforce use of named constructor
	template<typename...Args,
		std::enable_if_t<std::is_constructible_v<std::string, Args...>, int> = 0>
	LogMessage(pass_key, LogLevel _level, Args&&...args) noexcept(false)
		: level(_level), message(std::forward<Args>(args)...) {}
	LogMessage(pass_key, LogLevel _level, std::string&& _message) noexcept(false)
		: level(_level), message(std::forward<std::string>(_message)) {}
	LogMessage(pass_key, LogLevel _level, std::string&& _message, ContextEntries&& _context) noexcept(false)
		: level(_level), message(std::forward<std::string>(_message)), context(std::forward<ContextEntries>(_context)) {}

private:
	const LogLevel level;
	const std::string message;
	const TimeClock timestamp;
	ContextEntries context;
};

}; // (end namespace FIQCPPBASE)
