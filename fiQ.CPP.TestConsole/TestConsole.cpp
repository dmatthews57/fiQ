#include "pch.h"
#include "Tools/Exceptions.h"
#include "Tools/LoggingOps.h"
using namespace FIQCPPBASE;

int main()
{
	_set_invalid_parameter_handler(Exceptions::InvalidParameterHandler);
	_set_se_translator(Exceptions::StructuredExceptionTranslator);
	SetUnhandledExceptionFilter(&Exceptions::UnhandledExceptionFilter);

	try
	{
		const SteadyClock start;
		Sleep(1234);
		SteadyClock end;
		printf("%lld since start\n", end.Since<std::chrono::milliseconds>(start));
		printf("%lld to end\n", start.Till<std::chrono::milliseconds>(end));
		printf("%d since end\n", start.MSecSince(end));
		printf("%d to start\n", end.MSecTill(start));

		end = SteadyClock::NowPlus(std::chrono::seconds(3));
		const clock_t cstart = clock();
		while(!end.IsPast()) Sleep(1);
		const clock_t cend = clock();
		printf("%d\n", cend-cstart);
	}
	catch(const std::runtime_error& re) {
		LoggingOps::StdOutLog("Caught runtime error:%s", Exceptions::UnrollExceptionString(re).c_str());
	}
	catch(const std::invalid_argument& ia) {
		LoggingOps::StdOutLog("Caught invalid argument:%s", Exceptions::UnrollExceptionString(ia).c_str());
	}
	catch(const std::exception& e) {
		LoggingOps::StdOutLog("Caught exception:%s", Exceptions::UnrollExceptionString(e).c_str());
	}
}
