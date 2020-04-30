#pragma once
//==========================================================================================================================
// FileSink.h : Class for logging event messages to file
//==========================================================================================================================

#include "Logging/LogSink.h"
#include "Tools/ThreadOps.h"

namespace FIQCPPBASE {

class FileSink : public LogSink, private ThreadOperator<LogMessage>
{
public:

	//======================================================================================================================
	// Configuration class
	struct Config {};

	//======================================================================================================================
	// Public constructors/destructor
	FileSink() noexcept = default;
	FileSink(LogLevel _minlevel, const Config& _config) noexcept : minlevel(_minlevel), config(_config) {}
	~FileSink() = default;
	// Deleted copy/move constructors and assignment operators
	FileSink(const FileSink&) = delete;
	FileSink(FileSink&&) = delete;
	FileSink& operator=(const FileSink&) = delete;
	FileSink& operator=(FileSink&&) = delete;

private:

	//======================================================================================================================
	// ThreadOperator function implementations
	unsigned int ThreadExecute() override {
		while(ThreadShouldRun()) Sleep(10000);
		return 0;
	}

	//======================================================================================================================
	// LogSink function implementations
	void Initialize() override {
		if(ThreadStart()) printf("FileSink init\n");
	}
	void Cleanup() override {printf("FileSink cleanup\n");}
	void ReceiveLog(std::unique_ptr<LogMessage>&& lm) override {
		// TODO: QUEUE MESSAGE INSTEAD OF HANDLING


		// vvvvvv NOTE: LOGIC HERE SHOULD BE MOVED TO THREADEXECUTE....
		// If we are logging messages at this level, open and write to file:
		if(lm->GetLevel() >= minlevel) {
			printf("[%d] FileSink got message\n", lm->GetLevel()); // TODO: WRITE TO FILE INSTEAD
		}

		// Pass log message on to next sink in pipeline, if any
		ForwardLog(std::move(lm));
		// ^^^^^^ ENDNOTE
	}

	// Private members
	LogLevel minlevel = LogLevel::Debug;
	Config config;
};

}; // (end namespace FIQCPPBASE)
