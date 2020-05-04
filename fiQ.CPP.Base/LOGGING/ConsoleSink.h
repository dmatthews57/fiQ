#pragma once
//==========================================================================================================================
// ConsoleSink.h : Class for logging event messages directly to console
//==========================================================================================================================

#include "Logging/LogSink.h"

namespace FIQCPPBASE {

class ConsoleSink : public LogSink
{
public:

	//======================================================================================================================
	// Configuration class: Limited configuration for console (minimum log level only)
	struct Config {};

	//======================================================================================================================
	// Public constructors/destructor
	ConsoleSink() noexcept = default;
	ConsoleSink(LogLevel _minlevel, const Config& _config) noexcept : minlevel(_minlevel), config(_config) {}
	~ConsoleSink() = default;
	// Deleted copy/move constructors and assignment operators
	ConsoleSink(const ConsoleSink&) = delete;
	ConsoleSink(ConsoleSink&&) = delete;
	ConsoleSink& operator=(const ConsoleSink&) = delete;
	ConsoleSink& operator=(ConsoleSink&&) = delete;

private:

	//======================================================================================================================
	// Private utility functions
	constexpr static int GetConsoleColor(LogLevel l) {
		if(l >= LogLevel::Error) return 91;
		else if(l >= LogLevel::Warn) return 93;
		else if(l >= LogLevel::Info) return 97;
		else return 0;
	}

	//======================================================================================================================
	// Base class function implementations
	void Initialize() noexcept override {} // No initialization required
	void Cleanup() noexcept override {} // No cleanup required
	void ReceiveLog(std::unique_ptr<LogMessage>&& lm) override {
		if(lm->GetLevel() >= minlevel) {
			const tm& LocalTime = lm->GetTimestamp().GetLocalTime();
			if(lm->GetLevel() >= LogLevel::Fatal) { // Write to stderr in same format as LogSink static function
				fprintf_s(stderr, "%04d%02d%02d|%02d:%02d:%02d.%03d|%s\n",
					LocalTime.tm_year + 1900, LocalTime.tm_mon + 1, LocalTime.tm_mday,
					LocalTime.tm_hour, LocalTime.tm_min, LocalTime.tm_sec, lm->GetTimestamp().GetMilliseconds() % 1000,
					lm->GetString().c_str()
				);
				const auto context = lm->GetContext();
				if(context.empty() == false) {
					for(auto seek = context.begin(); seek != context.end(); ++seek) {
						fprintf_s(stdout,
							"    [%s][%s]\n",
							seek->first.c_str(),
							seek->second.c_str()
						);
					}
				}
				fflush(stderr);
			}
			else { // Write to stdout in more friendly format
				fprintf_s(stdout,
					"\x1B[%dm[%03d][%02d:%02d:%02d.%03d] %s\x1B[0m\n",
					GetConsoleColor(lm->GetLevel()), lm->GetLevel(),
					LocalTime.tm_hour, LocalTime.tm_min, LocalTime.tm_sec, lm->GetTimestamp().GetMilliseconds() % 1000,
					lm->GetString().c_str());
				const auto context = lm->GetContext();
				if(context.empty() == false) {
					for(auto seek = context.begin(); seek != context.end(); ++seek) {
						fprintf_s(stdout,
							"    [%s][%s]\n",
							seek->first.c_str(),
							seek->second.c_str()
						);
					}
				}
				fflush(stdout);
			}
		}

		// Pass log message on to next sink in pipeline, if any
		ForwardLog(std::move(lm));
	}

	// Private members
	LogLevel minlevel = LogLevel::Debug;
	Config config;
};

}; // (end namespace FIQCPPBASE)
