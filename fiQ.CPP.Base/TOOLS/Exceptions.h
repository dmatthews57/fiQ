#pragma once
//==========================================================================================================================
// Exceptions.h : Classes and functions for exception handling
//==========================================================================================================================

#include <comdef.h>
#include <errhandlingapi.h>
#include <stdexcept>
#include "StringOps.h"

// FORMAT_RUNTIME_ERROR: Helper macro for producing runtime_error object with information on throw site
// - Requires use of macro rather than inline function so that FILE and FUNCTION macros work as expected
// - Arguments to macro are passed direct to std::string::append (so incoming message can be any set of arguments that could
//   be used to construct a std::string object)
// - Usage: throw FORMAT_RUNTIME_ERROR("<error description text>");
#define FORMAT_RUNTIME_ERROR(...) \
	std::runtime_error(std::string(__FILE__).append("::").append(__FUNCTION__).append(": ").append(__VA_ARGS__))

namespace FIQCPPBASE {

	//======================================================================================================================
	// Exceptions: Namespace to contain processor functions which cannot be bound class members
	namespace Exceptions {

		// TranslateExceptionCode: Return string literal corresponding with input value
		template<typename T> // Templated input type as this could be DWORD or UINT, depending on source
		constexpr const char* TranslateExceptionCode(T t) noexcept;

		// ConvertCOMError: Convert value from GetLastError or GetWSALastError to string value
		inline std::string ConvertCOMError(int errcode) {
			return StringOps::ConvertFromWideString(_com_error(errcode).ErrorMessage());
		}

		// InvalidParameterHandler: Convert invalid parameter condition to an exception
		// - Usage: _set_invalid_parameter_handler(Exceptions::InvalidParameterHandler);
		void InvalidParameterHandler(
			const wchar_t * expression, const wchar_t * function, const wchar_t * file, unsigned int line, uintptr_t);

		// StructuredExceptionTranslator: Convert SEH code to C++ exception
		// - Usage: _set_se_translator(Exceptions::StructuredExceptionTranslator);
		// - Note this must be called once per thread; it does not apply globally
		void StructuredExceptionTranslator(unsigned int u, _EXCEPTION_POINTERS*);

		// UnhandledExceptionFilter: Capture any exceptions not handled by application
		// - Usage: SetUnhandledExceptionFilter(&Exceptions::UnhandledExceptionFilter);
		LONG UnhandledExceptionFilter(_In_ struct _EXCEPTION_POINTERS *ep);

		// UnrollExceptions: Unroll nexted exceptions into a map of strings indexed by depth
		_Check_return_ std::map<int,std::string> UnrollExceptions(const std::exception& e);

	} // (end namespace Exceptions)

} // (end namespace FIQCPPBASE)
