//==========================================================================================================================
// Exceptions.cpp : Classes and functions for exception handling
//==========================================================================================================================
#include "pch.h"
#include "Exceptions.h"
#include "Logging/LogSink.h"
using namespace FIQCPPBASE;

//==========================================================================================================================
// Exceptions::TranslateExceptionCode: Return string literal corresponding with input value
template<typename T>
constexpr const char* Exceptions::TranslateExceptionCode(T t) noexcept {
	switch(t)
	{
	case EXCEPTION_ACCESS_VIOLATION: return "Access Violation";
	case EXCEPTION_DATATYPE_MISALIGNMENT: return "Datatype Misalign";
	case EXCEPTION_BREAKPOINT: return "Breakpoint";
	case EXCEPTION_SINGLE_STEP: return "Single Step";
	case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "Array Bounds Exceeded";
	case EXCEPTION_FLT_DENORMAL_OPERAND: return "Denormal Operand";
	case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "Divide By Zero";
	case EXCEPTION_FLT_INEXACT_RESULT: return "Inexact Result";
	case EXCEPTION_FLT_INVALID_OPERATION: return "Invalid Operation";
	case EXCEPTION_FLT_OVERFLOW: return "Float Overflow";
	case EXCEPTION_FLT_STACK_CHECK: return "Stack Check";
	case EXCEPTION_FLT_UNDERFLOW: return "Underflow";
	case EXCEPTION_INT_DIVIDE_BY_ZERO: return "Int Divide by Zero";
	case EXCEPTION_INT_OVERFLOW: return "Int Overflow";
	case EXCEPTION_PRIV_INSTRUCTION: return "Priv Instruction";
	case EXCEPTION_IN_PAGE_ERROR: return "In Page Error";
	case EXCEPTION_ILLEGAL_INSTRUCTION: return "Illegal Instruction";
	case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "Noncontinuable Exception";
	case EXCEPTION_STACK_OVERFLOW: return "Stack Overflow";
	case EXCEPTION_INVALID_DISPOSITION: return "Invalid Disposition";
	case EXCEPTION_GUARD_PAGE: return "Guard Page";
	case EXCEPTION_INVALID_HANDLE: return "Invalid Handle";
	case 0xE06D7363: return "C++ Exception";
	default: return "Unrecognized";
	};
}

// Exceptions::InvalidParameterHandler: Convert invalid parameter condition to an exception
void Exceptions::InvalidParameterHandler(
	const wchar_t * expression, const wchar_t * function, const wchar_t * file, unsigned int line, uintptr_t) {
#ifdef _DEBUG
	// In Debug mode, function and failed expression are available; build helpful error message:
	throw std::invalid_argument(
		StringOps::ConvertFromWideString(
			std::wstring(file)
				.append(L"(")
				.append(std::to_wstring(line))
				.append(L")::")
				.append(function)
				.append(L"(): ")
				.append(expression)
		)
	);
#else
	// In Release mode, all arguments are NULL; use simple message:
	UNREFERENCED_PARAMETER(expression);
	UNREFERENCED_PARAMETER(function);
	UNREFERENCED_PARAMETER(file);
	UNREFERENCED_PARAMETER(line);
	throw std::invalid_argument("Invalid parameter");
#endif
}

// Exceptions::StructuredExceptionTranslator: Convert SEH code to C++ exception
void Exceptions::StructuredExceptionTranslator(unsigned int u, _EXCEPTION_POINTERS*) {
	std::throw_with_nested(std::runtime_error(TranslateExceptionCode(u)));
}

// UnhandledExceptionFilter: Capture any exceptions not handled by application
GSL_SUPPRESS(con.3) // Parameter cannot be const - would not match expected signature
LONG Exceptions::UnhandledExceptionFilter(_In_ struct _EXCEPTION_POINTERS *ep) {
	// Log exception details to stderr for debugging purposes:
	LogSink::StdErrLog("UNHANDLED EXCEPTION 0x%X in Thread ID %08X: %s",
		ep ? (ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionCode : 0) : 0,
		GetCurrentThreadId(),
		ep ? (ep->ExceptionRecord ? TranslateExceptionCode(ep->ExceptionRecord->ExceptionCode) : "(NULL)") : "(NULL)");
	// Return to inform program that it should execute its default handler (which will typically terminate the process):
	return EXCEPTION_EXECUTE_HANDLER;
}
// AddExceptionToContext: Local function used while unrolling exceptions to add the current exception to context
// collection, then recursively throw until no longer nested
namespace FIQCPPBASE {
	namespace Exceptions {
		namespace {
			void AddExceptionToVector(std::vector<std::string>& Tgt, const std::exception& e) {
				Tgt.emplace_back(e.what());
				try {
					std::rethrow_if_nested(e);
				}
				catch(const std::exception& ee) {AddExceptionToVector(Tgt, ee);}
			}
		};
	};
};
// UnrollException: Unroll nested exceptions into context collection
_Check_return_ LogMessage::ContextEntries Exceptions::UnrollException(const std::exception& e) noexcept(false) {
	// Create a vector of exception details (ordered from outermost to innermost):
	std::vector<std::string> exceptions;
	Exceptions::AddExceptionToVector(exceptions, e);

	// Now create vector of context entries, and move strings into it (from innermost to outermost):
	LogMessage::ContextEntries retval;
	retval.reserve(exceptions.size());
	for(auto seek = exceptions.rbegin(); seek != exceptions.rend(); ++seek) {
		retval.emplace_back(seek == exceptions.rbegin() ? "Caught" : "From", std::move(*seek));
	}
	return retval;
}
// UnrollExceptionString: Unroll nested exceptions into an easy-to-display string
_Check_return_ std::string Exceptions::UnrollExceptionString(const std::exception& e) noexcept(false) {

	// Create a vector of exception details (ordered from outermost to innermost):
	std::vector<std::string> exceptions;
	Exceptions::AddExceptionToVector(exceptions, e);

	// Now concatenate collection entries into string (from innermost to outermost):
	std::string retval;
	char temp[20] = { '\n', '\t', 0 };
	size_t templen = 2;
	int i = 0;
	for(auto seek = exceptions.crbegin(); seek != exceptions.crend(); ++seek, ++i, templen = 2) {
		// Add newline, tab, depth and space (format local temp buffer first before appending
		// to string to cut down on string reallocations, though we will wind up reallocating
		// at least once per iteration):
		templen += StringOps::Decimal::FlexWriteString(temp + 2, i);
		temp[templen++] = ' ';
		retval.reserve(retval.size() + templen + seek->length());
		retval.append(temp, templen);
		retval.append(*seek);
	}
	return retval;
}
