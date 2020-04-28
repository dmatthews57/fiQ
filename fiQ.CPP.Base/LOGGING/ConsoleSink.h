#pragma once
//==========================================================================================================================
// ConsoleSink.h : Class for logging event messages directly to console
//==========================================================================================================================

#include "Logging/LogSink.h"

namespace FIQCPPBASE {

class ConsoleSink : public LogSink
{
public:

	// Public constructor/destructor
	ConsoleSink() = default;
	virtual ~ConsoleSink() = default;
	// Deleted copy/move constructors and assignment operators
	ConsoleSink(const ConsoleSink&) = delete;
	ConsoleSink(ConsoleSink&&) = delete;
	ConsoleSink& operator=(const ConsoleSink&) = delete;
	ConsoleSink& operator=(ConsoleSink&&) = delete;

private:

	// Base class function implementations
	LogLevel GetMinLogLevel() const noexcept override {return LogLevel::Debug;}
	void ReceiveLog(std::unique_ptr<LogMessage>&& lm) override {
		const tm& LocalTime = lm->GetTimestamp().GetLocalTime();
		fprintf_s(stdout,
			"[%03d][%02d:%02d:%02d.%03d] %s [%zu]\n", lm->GetLevel(),
			LocalTime.tm_hour, LocalTime.tm_min, LocalTime.tm_sec, lm->GetTimestamp().GetMilliseconds() % 1000,
			lm->GetMessage().c_str(), lm->GetContext().size());
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

};

}; // (end namespace FIQCPPBASE)
