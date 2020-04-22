#pragma once
//==========================================================================================================================
// LoggingOps.h : Classes and functions for application logging
//==========================================================================================================================

#include "Tools/TimeClock.h"

namespace FIQCPPBASE {

class LoggingOps
{
public:

	//======================================================================================================================
	// Static logging functions - Log to console (stdout)
	// - Expects string literal format followed by variable arguments; will add timestamp and output in standard format
	template<typename T, size_t len, std::enable_if_t<std::is_same_v<T, const char>, int> = 0, typename ... Args >
	static void StdOutLog(_In_z_ _Printf_format_string_ T (&format)[len], Args const & ... args) {
		static_assert(len > 0, "Invalid format string length");
		const TimeClock CurrTime = TimeClock::Now(); tm TempTM = {0}; localtime_s(&TempTM, &CurrTime.GetSeconds());
		// Construct format string by concatenating console timestamp format with incoming format and newline;
		// string construction (and two append operations) more efficient than multiple print calls:
		fprintf_s(stdout, std::string("[00][%02d:%02d:%02d.%03d] ").append(format, len - 1).append("\n").c_str(),
			TempTM.tm_hour, TempTM.tm_min, TempTM.tm_sec, CurrTime.GetMilliseconds() % 1000, args ...);
		fflush(stdout);
	}
	// Static logging functions - Log to console (stderr)
	// - Expects string literal format followed by variable arguments; will add timestamp and output in standard format
	template<typename T, size_t len, std::enable_if_t<std::is_same_v<T, const char>, int> = 0, typename ... Args >
	static void StdErrLog(_In_z_ _Printf_format_string_ T (&format)[len], Args const & ... args) {
		static_assert(len > 0, "Invalid format string length");
		const TimeClock CurrTime = TimeClock::Now(); tm TempTM = {0}; localtime_s(&TempTM, &CurrTime.GetSeconds());
		// Construct format string by concatenating console timestamp format with incoming format and newline;
		// string construction (and two append operations) more efficient than multiple print calls:
		fprintf_s(stderr, std::string("%04d%02d%02d|%02d:%02d:%02d.%03d|").append(format, len - 1).append("\n").c_str(),
			TempTM.tm_year + 1900, TempTM.tm_mon + 1, TempTM.tm_mday, TempTM.tm_hour, TempTM.tm_min, TempTM.tm_sec,
			CurrTime.GetMilliseconds() % 1000, args ...);
		fflush(stderr);
	}

};

} // (end namespace FIQCPPBASE)
