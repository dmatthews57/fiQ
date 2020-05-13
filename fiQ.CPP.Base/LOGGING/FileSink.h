#pragma once
//==========================================================================================================================
// FileSink.h : Class for logging event messages to file
//==========================================================================================================================

#include "Logging/LogSink.h"
#include "Tools/FileOps.h"
#include "Tools/ThreadOps.h"
#include <direct.h>

namespace FIQCPPBASE {

class FileSink : public LogSink, private ThreadOperator<LogMessage>
{
public:

	//======================================================================================================================
	// Public definitions and helper converter functions
	enum class Format : int { Flat = 0, JSON = 1 };
	template<typename T, std::enable_if_t<std::is_same_v<T, std::underlying_type_t<Format>>, int> = 0>
	LogLevel ToFormat(T t) {
		switch(static_cast<LogLevel>(t)) {
		case Format::JSON: return Format::JSON;
		default: return LogLevel::Flat;
		};
	}
	enum class Rollover : int { Daily = 0, Hourly = 1 };
	template<typename T, std::enable_if_t<std::is_same_v<T, std::underlying_type_t<Rollover>>, int> = 0>
	LogLevel ToRollover(T t) {
		switch(static_cast<LogLevel>(t)) {
		case Rollover::Hourly: return Rollover::Hourly;
		default: return Rollover::Daily;
		};
	}

	//======================================================================================================================
	// Configuration container definition
	struct Config {
		Format format = Format::JSON;
		Rollover rollover = Rollover::Daily;
		const std::string RootDir = "LOGS";
	};

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
	// Private utility functions
	void Write(_In_z_ char* FilenameRoot, _Out_writes_(17) char* Filename, const LogMessage& lm) {
		// FilenameRoot points at base of full file path, Filename points at first character of filename
		// portion; set up filename (based on timestamp of this object and rollover config):
		const tm& lt = lm.GetTimestamp().GetLocalTime();
		Filename += StringOps::Decimal::ExWriteString<4>(Filename, lt.tm_year + 1900);
		*Filename++ = '-';
		Filename += StringOps::Decimal::ExWriteString<2>(Filename, lt.tm_mon + 1);
		*Filename++ = '-';
		Filename += StringOps::Decimal::ExWriteString<2>(Filename, lt.tm_mday);
		if(config.rollover == Rollover::Hourly) {
			*Filename++ = '-';
			Filename += StringOps::Decimal::ExWriteString<2>(Filename, lt.tm_hour);
		}
		Filename += StringOps::ExStrCpyLiteral(Filename, ".txt");

		// Open file and write contents of message
		try {
			FileOps::FilePtr fp = FileOps::OpenFile(FilenameRoot, "a+", _SH_DENYWR);
			if(fp.get() == nullptr) {
				LogSink::StdErrLog("Failed to open log file [%s]: %s",
					FilenameRoot, Exceptions::ConvertCOMError(GetLastError()).c_str());
			}
			else if(config.format == Format::JSON) { // Write log data in JSON format
				// Open JSON object and write in standard fields, start of message element:
				fprintf(fp.get(),
					"{\"level\":%d"
					",\"lt\":\"%04d-%02d-%02d %02d:%02d:%02d.%03d\""
					",\"msg\":\"",
					lm.GetLevel(),
					lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday,
					lt.tm_hour, lt.tm_min, lt.tm_sec, lm.GetTimestamp().GetMilliseconds() % 1000
				);
				// Write in contents of full formatted message:
				if(lm.EscapeFormats() & FormatEscape::JSON) {
					const std::string escaped = StringOps::JSON::Escape(lm.GetString());
					fwrite(escaped.data(), sizeof(char), escaped.length(), fp.get());
				}
				else fwrite(lm.GetString().data(), sizeof(char), lm.GetString().length(), fp.get());
				// Add context values:
				for(auto seek = lm.GetContext().cbegin(); seek != lm.GetContext().cend(); ++seek) {
					{static constexpr char jsoncont[] = "\",\"";
					fwrite(jsoncont, sizeof(char), _countof(jsoncont) - 1, fp.get());} // Close previous item value, start item name
					if(lm.EscapeFormats() & FormatEscape::JSON) {
						const std::string escaped = StringOps::JSON::Escape(seek->first);
						fwrite(escaped.data(), sizeof(char), escaped.length(), fp.get());
					}
					else fwrite(seek->first.data(), sizeof(char), seek->first.length(), fp.get());
					{static constexpr char jsonmid[] = "\":\"";
					fwrite(jsonmid, sizeof(char), _countof(jsonmid) - 1, fp.get());} // Close item name, open item value
					if(lm.EscapeFormats() & FormatEscape::JSON) {
						const std::string escaped = StringOps::JSON::Escape(seek->second);
						fwrite(escaped.data(), sizeof(char), escaped.length(), fp.get());
					}
					else fwrite(seek->second.data(), sizeof(char), seek->second.length(), fp.get());
				}
				// Close last item value and add end of line:
				{static constexpr char jsonend[] = "\"}\n";
				fwrite(jsonend, sizeof(char), _countof(jsonend) - 1, fp.get());}
			}
			else { // Default to flat file - logs message string only, without context:
				fprintf_s(fp.get(),
					"[%03d][%02d:%02d:%02d.%03d] %s\n",
					lm.GetLevel(),
					lt.tm_hour, lt.tm_min, lt.tm_sec, lm.GetTimestamp().GetMilliseconds() % 1000,
					lm.GetString().c_str());
			}
		}
		catch(const std::exception& e) {
			LogSink::StdErrLog("Exception caught writing log data to FileSink:%s\n",
				Exceptions::UnrollExceptionString(e).c_str());
		}
	}

	//======================================================================================================================
	// ThreadOperator function implementations
	unsigned int ThreadExecute() override {
		// Set up vector to hold root directory with room to append filename, copy root directory:
		std::vector<char> Filename(config.RootDir.length() + 20, 0);
		char *FilenameStart = Filename.data();
		FilenameStart += StringOps::ExStrCpy(Filename.data(), config.RootDir.data(), config.RootDir.length());
		*FilenameStart++ = '/';

		// Loop for lifetime of this object:
		while(ThreadShouldRun()) {
			ThreadWaitEvent();
			ThreadWorkUnit work(nullptr);
			while(ThreadDequeueWork(work)) {
				if(work->GetLevel() >= minlevel) Write(Filename.data(), FilenameStart, *work);
				// Pass log message on to next sink in pipeline, if any (note that if this object is shutting down,
				// any downstream sinks have already shut down, so we should not bother forwarding)
				if(ThreadShouldRun()) ForwardLog(std::move(work));
			}
		}

		// After shutdown, if there is still data in queue we need to write it before exiting (note that there is a
		// risk that if this takes a long time, whoever is shutting down this object will stop waiting and destruct
		// this object, causing undefined behavior - but don't want to lose data if possible):
		if(ThreadQueueEmpty() == false) {
			ThreadWorkUnit work(nullptr);
			while(ThreadUnsafeDequeueWork(work)) {
				if(work->GetLevel() >= minlevel) Write(Filename.data(), FilenameStart, *work);
			}
		}
		return 0;
	}

	//======================================================================================================================
	// LogSink function implementations
	void Initialize() override {
		// Create log directory (must successfully create or already exist):
		const int md = _mkdir(config.RootDir.c_str());
		if(md == -1 ? (errno == EEXIST) : false) {}
		else if(md != 0) throw FORMAT_RUNTIME_ERROR("Failed to create FileSink target folder");
		// Start up worker thread:
		if(ThreadStart() == false) throw FORMAT_RUNTIME_ERROR("Failed to start FileSink logger thread");
	}
	void Cleanup() override {
		if(ThreadWaitStop(3000) == false) LogSink::StdErrLog("WARNING: FinkSink logger thread not stopped cleanly");
	}
	void ReceiveLog(std::unique_ptr<const LogMessage>&& lm) override {
		// Queue LogMessage for processing by thread; check worker queue size, and log error if needed:
		const size_t qsize = ThreadQueueWork(std::move(lm));
		if(qsize >= 100 && (qsize % 20) == 0) LogSink::StdErrLog("WARNING: %zu objects in FileSink logger queue", qsize);
	}

	// Private members
	LogLevel minlevel = LogLevel::Debug;
	Config config;
};

}; // (end namespace FIQCPPBASE)
