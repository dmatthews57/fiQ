#include "pch.h"
#include "Tools/Exceptions.h"
using namespace FIQCPPBASE;


#include "Logging/LogSink.h"

LogMessage::ContextEntries GetLogContext() {
	LogMessage::ContextEntries ce = {{"SAMPLE","Whatever"},{"ThreadID",std::to_string(GetCurrentThreadId())}};
	return ce;
}

int main()
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF|_CRTDBG_LEAK_CHECK_DF);
#endif
	_set_invalid_parameter_handler(Exceptions::InvalidParameterHandler);
	_set_se_translator(Exceptions::StructuredExceptionTranslator);
	SetUnhandledExceptionFilter(&Exceptions::UnhandledExceptionFilter);

	try {
		std::string temp = "nonliteral";
		LOG_FROM_TEMPLATE_ENRICH(LogLevel::Debug,
			"{first:F9} SECOND {second:X9} THIRD {:D} {fourth:S3} FIFTH {fifth:S10} SIXTH {sixth:F22}",
			7654321.23456789123, 0xFEDCBA0987, 321UL, "literal", temp);

		LOG_FROM_TEMPLATE_ENRICH(LogLevel::Info, "Goodbye, nothing to say");
		return 0;
	}
	catch(const std::runtime_error& re) {
		LogSink::StdErrLog("Caught runtime error:%s", Exceptions::UnrollExceptionString(re).c_str());
	}
	catch(const std::invalid_argument& ia) {
		LogSink::StdErrLog("Caught invalid argument:%s", Exceptions::UnrollExceptionString(ia).c_str());
	}
	catch(const std::exception& e) {
		LogSink::StdErrLog("Caught exception:%s", Exceptions::UnrollExceptionString(e).c_str());
	}
}

