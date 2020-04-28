#pragma once
//==========================================================================================================================
// LogSink.h : Classes and functions for application logging
//==========================================================================================================================

#include "Tools/TimeClock.h"
#include "Logging/LogMessageBuilder.h"

//==========================================================================================================================
// Helper macros - Create static constexpr template from string and validate (once), bind arguments and log (each time)
#define LOG_FROM_TEMPLATE(lvl,tmplt,...) \
	if(lvl >= LogSink::MinLogLevel()) {static constexpr LogMessageTemplate lmt(tmplt);\
	static_assert(lmt.IsValid(), "Invalid logging template string");\
	LogSink::Log(CreateLogMessageBuilder<lmt.PlaceholderCount()>(lmt, lvl, __VA_ARGS__)\
		.Build(__FILE__ "::" __FUNCTION__ + StringOps::PathLength(__FILE__)));}
// Helper macros - Create static constexpr template from string and validate (once), bind arguments and log (each time),
// use Enrich function to add vector returned by GetLogContext (must be valid in local scope) to data
#define LOG_FROM_TEMPLATE_ENRICH(lvl,tmplt,...) \
	if(lvl >= LogSink::MinLogLevel()) {static constexpr LogMessageTemplate lmt(tmplt);\
	static_assert(lmt.IsValid(), "Invalid logging template string");\
	auto lm = CreateLogMessageBuilder<lmt.PlaceholderCount()>(lmt, lvl, __VA_ARGS__)\
		.Build(__FILE__ "::" __FUNCTION__ + StringOps::PathLength(__FILE__));\
	lm->AddContext(GetLogContext()); LogSink::Log(std::move(lm));}

namespace FIQCPPBASE {

class LogSink
{
public:

	//======================================================================================================================
	// Static logging setup/sink registration functions
	// - Note these functions are NOT thread-safe - call them from main() only
	static void InitializeLogging() {} // TODO: SET UP SINK(s)
	static void AddSink() {} // TODO: ADD SINK TO PIPELINE
	static void CleanupLogging() {} // TODO: CLEAN UP SINK(s)

	//======================================================================================================================
	// Static logging functions
	static LogLevel MinLogLevel() noexcept {return GetSink().GetMinLogLevel();}
	static void Log(std::unique_ptr<LogMessage>&& lm) { // TODO: ADD LEVEL
		GetSink().ReceiveLog(std::forward<std::unique_ptr<LogMessage>>(lm));
	}

	//======================================================================================================================
	// Static logging functions - Log to console (stderr)
	// - Bypasses sink system entirely (for critical, must-log errors)
	// - Expects string literal format followed by variable arguments; will add timestamp and output in standard format
	template<typename T, size_t len, std::enable_if_t<std::is_same_v<T, const char>, int> = 0, typename...Args>
	static void StdErrLog(_In_z_ _Printf_format_string_ T (&format)[len], Args const & ... args) {
		static_assert(len > 0, "Invalid format string length");
		const TimeClock CurrTime;
		const tm& LocalTime = CurrTime.GetLocalTime();
		// Construct format string by concatenating console timestamp format with incoming format and newline;
		// string construction (and two append operations) more efficient than multiple print calls:
		fprintf_s(stderr, std::string("%04d%02d%02d|%02d:%02d:%02d.%03d|").append(format, len - 1).append("\n").c_str(),
			LocalTime.tm_year + 1900, LocalTime.tm_mon + 1, LocalTime.tm_mday, LocalTime.tm_hour, LocalTime.tm_min,
			LocalTime.tm_sec, CurrTime.GetMilliseconds() % 1000, args ...);
		fflush(stderr);
	}

	//======================================================================================================================
	// Public virtual defaulted destructor
	virtual ~LogSink() = default;
	// Deleted copy/move constructors and assignment operators
	LogSink(const LogSink&) = delete;
	LogSink(LogSink&&) = delete;
	LogSink& operator=(const LogSink&) = delete;
	LogSink& operator=(LogSink&&) = delete;

protected:

	//======================================================================================================================
	// Pure virtual function definitions for Sink classes
	virtual LogLevel GetMinLogLevel() const noexcept = 0;
	virtual void ReceiveLog(std::unique_ptr<LogMessage>&&) = 0;

	LogSink() = default;

private:
	static LogSink& GetSink();
};

}; // (end namespace FIQCPPBASE)
