#include "pch.h"
#include "Logging/LogSink.h"
#include "Tools/Exceptions.h"
using namespace FIQCPPBASE;

void Innermost() noexcept(false) {
	try {
		throw std::runtime_error("first domino");
	}
	catch(const std::exception&) {std::throw_with_nested(FORMAT_RUNTIME_ERROR("INNERMOSTMSG"));}
}
void Outermost() {
	try {
		Innermost();
	}
	catch(const std::exception&) {std::throw_with_nested(FORMAT_RUNTIME_ERROR("OUTERMOSTMSG"));}
}

#include "Logging/ConsoleSink.h"
#include "Logging/FileSink.h"

int main()
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF|_CRTDBG_LEAK_CHECK_DF);
#endif
	_set_invalid_parameter_handler(Exceptions::InvalidParameterHandler);
	_set_se_translator(Exceptions::StructuredExceptionTranslator);
	SetUnhandledExceptionFilter(&Exceptions::UnhandledExceptionFilter);

	try {
		LogSink::EnableEnrichers(LogEnrichers::ThreadID);
		LogSink::AddSink<FileSink>(LogLevel::Debug, FileSink::Config { FileSink::Format::JSON, FileSink::Rollover::Hourly, "\\TEMP\\LOGS" });
		LogSink::AddSink<ConsoleSink>(LogLevel::Debug, ConsoleSink::Config { });
		LogSink::InitializeSinks();

		const LogMessage::ContextEntries ce = {{"SAMPLE1","Whatever1"},{"SAMPLE2",std::string("wHATEVER2")}};
		std::string temp = "nonliteral";
		LOG_FROM_TEMPLATE_CONTEXT(LogLevel::Debug, &ce,
			"{first:F9} SECOND {second:X9} THIRD {:D} {fourth:S3} FIFTH {fifth:S10} SIXTH {sixth:F22}",
			7654321.23456789123, 0xFEDCBA0987, 321UL, "literal", temp);
		LOG_FROM_TEMPLATE(LogLevel::Info, "Message with no\x1c placeholders");
		LOG_FROM_TEMPLATE(LogLevel::Warn, "Message with some \nplaceholders [${amount:F2}][{account}]", 21.50, 30);
		LOG_FROM_TEMPLATE(LogLevel::Error, "Error with no placeholders");
		LOG_FROM_TEMPLATE_CONTEXT(LogLevel::Fatal, &ce, "Goodbye, fatal error");

		//Outermost();
		Sleep(100);
		LogSink::CleanupSinks();

		return 0;
	}
	catch(const std::runtime_error& e) {
		const auto exceptioncontext = Exceptions::UnrollException(e);
		LOG_FROM_TEMPLATE_CONTEXT(LogLevel::Error, &exceptioncontext, "Caught runtime error");
		printf("%s\n", Exceptions::UnrollExceptionString(e).c_str());
	}
	catch(const std::invalid_argument& e) {
		const auto exceptioncontext = Exceptions::UnrollException(e);
		LOG_FROM_TEMPLATE_CONTEXT(LogLevel::Error, &exceptioncontext, "Caught invalid argument");
	}
	catch(const std::exception& e) {
		const auto exceptioncontext = Exceptions::UnrollException(e);
		LOG_FROM_TEMPLATE_CONTEXT(LogLevel::Error, &exceptioncontext, "Caught exception");
	}
}

