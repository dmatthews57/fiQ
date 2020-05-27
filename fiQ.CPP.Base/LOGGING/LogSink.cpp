//==========================================================================================================================
// LogSink.cpp : Classes and functions for application logging
//==========================================================================================================================
#include "pch.h"
#include "LogSink.h"
#include "ConsoleSink.h"
using namespace FIQCPPBASE;

//==========================================================================================================================
// LogSink::GetSinkPtr: Create and return reference to static unique_ptr (populated with default ConsoleSink)
std::unique_ptr<LogSink>& LogSink::GetSinkPtr() noexcept(false) {
	static std::unique_ptr<LogSink> ls = std::make_unique<ConsoleSink>();
	return ls;
}
// LogSink::AddOrReplaceSinkPtr: Take new SinkPtr and either add it to pipeline or replace base SinkPtr
void LogSink::AddOrReplaceSinkPtr(LogLevel minlevel, std::unique_ptr<LogSink>&& sink) {
	static bool IsDefaultSink = true; // When first called, GetSinkPtr has constructed default ConsoleSink
	if(IsDefaultSink) {
		GetSinkPtr() = std::move(sink); // Replace default sink instead of adding
		GetMinLogLevel() = minlevel; // Replace default minimum log level with value provided
		IsDefaultSink = false; // Future sinks will be added instead of replacing base
	}
	else {
		GetSinkPtr()->AddNextSink(std::move(sink));
		// If provided log level is lower than current minimum, update:
		if(minlevel < GetMinLogLevel()) GetMinLogLevel() = minlevel;
	}
}
// LogSink::GetEnrichers: Create and return reference to static mask of LogEnrichers values
LogEnrichers& LogSink::GetEnrichers() noexcept {
	static LogEnrichers le = LogEnrichers::None;
	return le;
}
// LogSink::GetMinLogLevel: Create and return reference to minimum LogLevel value
LogLevel& LogSink::GetMinLogLevel() noexcept {
	static LogLevel level = LogLevel::Warn;
	return level;
}
