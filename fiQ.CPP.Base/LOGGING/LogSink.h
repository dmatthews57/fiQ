#pragma once
//==========================================================================================================================
// LogSink.h : Classes and functions for application logging
//==========================================================================================================================

#include "Tools/TimeClock.h"
#include "Logging/LogMessageBuilder.h"

//==========================================================================================================================
// Helper macros - Create static constexpr template from string (validate at compile time), bind runtime arguments and log
#define LOG_FROM_TEMPLATE(lvl,tmplt,...) \
	if(lvl >= LogSink::MinLogLevel()) {static constexpr LogMessageTemplate lmt(tmplt);\
	static_assert(lmt.IsValid(), "Invalid logging template string");\
	LogSink::Log(CreateLogMessageBuilder<lmt.PlaceholderCount()>(lvl, lmt, __VA_ARGS__).Build(\
		LogSink::PrepareContext(lmt.PlaceholderCount(), __FILE__ "::" __FUNCTION__ + StringOps::PathLength(__FILE__))));}
// Helper macros - Create static constexpr template from string (validate at compile time), bind runtime arguments and log,
// adding a set of local context logging values to include in LogMessage
#define LOG_FROM_TEMPLATE_CONTEXT(lvl,context,tmplt,...) \
	if(lvl >= LogSink::MinLogLevel()) {static constexpr LogMessageTemplate lmt(tmplt);\
	static_assert(lmt.IsValid(), "Invalid logging template string");\
	LogSink::Log(CreateLogMessageBuilder<lmt.PlaceholderCount()>(lvl, lmt, __VA_ARGS__).Build(\
		LogSink::PrepareContext(lmt.PlaceholderCount(), __FILE__ "::" __FUNCTION__ + StringOps::PathLength(__FILE__), context)));}

namespace FIQCPPBASE {

//==========================================================================================================================
// Public definitions - Log enrichment flags with overloaded operators for binary flag operations
enum class LogEnrichers : unsigned short {
	None = 0x0000,		// Default/empty flag value
	ThreadID = 0x0001	// Add ThreadID to all LogMessages
};
using LogEnrichersType = std::underlying_type_t<LogEnrichers>;
inline constexpr LogEnrichers operator|(LogEnrichers a, LogEnrichers b) noexcept {
	return static_cast<LogEnrichers>(static_cast<LogEnrichersType>(a) | static_cast<LogEnrichersType>(b));
}
inline constexpr LogEnrichers operator|=(LogEnrichers& a, LogEnrichers b) noexcept {
	return (a = (a | b));
}
inline constexpr bool operator&(LogEnrichers a, LogEnrichers b) noexcept {
	return ((static_cast<LogEnrichersType>(a) & static_cast<LogEnrichersType>(b)) != 0);
}
inline constexpr LogEnrichers operator&=(LogEnrichers& a, LogEnrichers b) noexcept {
	return (a = static_cast<LogEnrichers>(static_cast<LogEnrichersType>(a) & static_cast<LogEnrichersType>(b)));
}
inline constexpr LogEnrichers operator~(LogEnrichers a) noexcept {
	return static_cast<LogEnrichers>(~(static_cast<LogEnrichersType>(a)));
}

//==========================================================================================================================
// LogSink: Base class for logging sinks, and interface for global static logging management functions
class LogSink
{
public:

	//======================================================================================================================
	// Static setup/initialization functions
	// - Note these functions are NOT thread-safe - call them from main() only
	//======================================================================================================================
	// AddSink: Add a new logging sink to the end of the current logging pipeline
	// - If there is currently only the default sink in the pipeline, the new sink will replace it instead
	// - Add all sinks prior to calling InitializeSinks
	template<typename T, std::enable_if_t<std::is_base_of_v<LogSink,T>, int> = 0>
	static void AddSink(LogLevel minlevel, const typename T::Config& c);
	static void InitializeSinks(); // Call after adding all sinks to pipeline, before log producers start up
	static void CleanupSinks(); // Call at end of program, after log producers have shut down
	static void EnableEnrichers(LogEnrichers le) noexcept; // Turns on the specified field(s) for all logging events
	static void DisableEnrichers(LogEnrichers le) noexcept; // Turns off the specified field(s) for all logging events

	//======================================================================================================================
	// Static logging functions
	//======================================================================================================================
	// StdErrLog: Write event text directly to stderr
	// - Bypasses sink system entirely (intended for critical, must-log errors)
	// - Expects string literal format followed by variable arguments; will add timestamp and output in standard format
	template<typename T, size_t len, std::enable_if_t<std::is_same_v<T, const char>, int> = 0, typename...Args>
	static void StdErrLog(_In_z_ _Printf_format_string_ T (&format)[len], Args const & ... args);
	// MinLogLevel: Retrieve lowest logging level of any active sink
	static LogLevel MinLogLevel() noexcept;
	// PrepareContext: Prepopulate context entry collection with logging data
	static LogMessage::ContextEntries PrepareContext(
		size_t PlaceholderCount, // Number of placeholders in message template
		_In_opt_z_ const char* fname, // Function name (typically provided by macro above)
		_In_opt_ const LogMessage::ContextEntries* localcontext = nullptr);
	// Log: Relay LogMessage object to active sink(s)
	static void Log(std::unique_ptr<LogMessage>&& lm);

	//======================================================================================================================
	// Virtual defaulted constructor/destructor (public to allow access by unique_ptr)
	LogSink() noexcept = default;
	virtual ~LogSink() noexcept(false) {} // Cannot be defaulted
	// Deleted copy/move constructors and assignment operators
	LogSink(const LogSink&) = delete;
	LogSink(LogSink&&) = delete;
	LogSink& operator=(const LogSink&) = delete;
	LogSink& operator=(LogSink&&) = delete;

protected:

	//======================================================================================================================
	// Pure virtual function definitions for Sink classes
	virtual void Initialize() = 0;
	virtual void Cleanup() = 0;
	virtual void ReceiveLog(std::unique_ptr<LogMessage>&&) = 0;

	// Protected utilities - Forward LogMessage to next sink in pipeline, if any
	void ForwardLog(std::unique_ptr<LogMessage>&& lm) {
		if(NextSink.get()) NextSink->ReceiveLog(std::move(lm));
	}

private:

	// Private variables and functions for managing sink pipeline
	std::unique_ptr<LogSink> NextSink = nullptr;
	void AddNextSink(std::unique_ptr<LogSink>&& next) {
		if(NextSink.get()) NextSink->AddNextSink(std::move(next));
		else NextSink = std::move(next);
	}
	void InitNextSinkThenSelf() {
		if(NextSink.get()) NextSink->InitNextSinkThenSelf();
		Initialize();
	}
	void CleanupNextSinkThenSelf() {
		if(NextSink.get()) NextSink->CleanupNextSinkThenSelf();
		Cleanup();
	}

	// Enrich: Private static utility function to pre-populate logging context collection
	static void Enrich(LogMessage::ContextEntries& context, size_t minsize) {
		// Reserve space requested by caller, add in additional entries based on enabled enrichers:
		context.reserve(minsize + ((GetEnrichers() & LogEnrichers::ThreadID) ? 1 : 0));
		if(GetEnrichers() & LogEnrichers::ThreadID) {
			char temp[15] = {0};
			const size_t templen = StringOps::Ascii::ExWriteString<8>(temp, GetCurrentThreadId());
			context.emplace_back(std::piecewise_construct,
				std::forward_as_tuple("ThreadID"),
				std::forward_as_tuple(temp, templen)
			);
		}
	}

	// Private static object accessors
	static std::unique_ptr<LogSink>& GetSinkPtr() noexcept;
	static void AddOrReplaceSinkPtr(LogLevel minlevel, std::unique_ptr<LogSink>&& sink);
	static LogEnrichers& GetEnrichers() noexcept;
	static LogLevel& GetMinLogLevel() noexcept;
};

//==========================================================================================================================
#pragma region Static setup/initialization functions
template<typename T, std::enable_if_t<std::is_base_of_v<LogSink,T>, int>>
inline void LogSink::AddSink(LogLevel minlevel, const typename T::Config& c) {
	AddOrReplaceSinkPtr(minlevel, std::make_unique<T>(minlevel, c));
}
inline void LogSink::InitializeSinks() {GetSinkPtr()->InitNextSinkThenSelf();}
inline void LogSink::CleanupSinks() {GetSinkPtr()->CleanupNextSinkThenSelf();}
inline void LogSink::EnableEnrichers(LogEnrichers le) noexcept {GetEnrichers() |= le;}
inline void LogSink::DisableEnrichers(LogEnrichers le) noexcept {GetEnrichers() &= ~le;}
#pragma endregion Static setup/initialization functions

//==========================================================================================================================
#pragma region Static logging functions
template<typename T, size_t len, std::enable_if_t<std::is_same_v<T, const char>, int>, typename...Args>
inline void LogSink::StdErrLog(_In_z_ _Printf_format_string_ T (&format)[len], Args const & ... args) {
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
inline LogLevel LogSink::MinLogLevel() noexcept {return GetMinLogLevel();}
inline LogMessage::ContextEntries LogSink::PrepareContext(
	size_t PlaceholderCount, _In_opt_z_ const char* fname, _In_opt_ const LogMessage::ContextEntries* localcontext) {
	LogMessage::ContextEntries context;
	Enrich(context, PlaceholderCount + (fname ? 1 : 0) + (localcontext ? localcontext->size() : 0));
	if(fname) context.emplace_back(std::piecewise_construct, std::forward_as_tuple("FUNC"), std::forward_as_tuple(fname));
	if(localcontext) context.insert(context.end(), localcontext->begin(), localcontext->end());
	// Return local variable (compiler should optimize out and pass memory straight through LogMessageBuilder::Build
	// to LogMessage constructor, avoiding reallocation or copy operations):
	return context;
}
inline void LogSink::Log(std::unique_ptr<LogMessage>&& lm) {GetSinkPtr()->ReceiveLog(std::move(lm));}
#pragma endregion Static logging functions

}; // (end namespace FIQCPPBASE)
