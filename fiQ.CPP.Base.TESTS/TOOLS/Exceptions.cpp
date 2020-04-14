#include "pch.h"
#include "CppUnitTest.h"
#include "Tools/Exceptions.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace FIQCPPBASE;

namespace fiQCPPBaseTESTS
{
	TEST_CLASS(Exceptions_TEST)
	{
	public:
		
		TEST_METHOD(ThrowSEH)
		{
			// Set SEH translator to throw runtime error, then force a divide-by-zero exception:
			_set_se_translator(Exceptions::StructuredExceptionTranslator);
			Assert::ExpectException<std::runtime_error>([] {
				int x = 5;
				int y = 0;
				int *p = &y;
				*p = x / *p;
			}, L"Expected runtime_error");
		}

		TEST_METHOD(InvalidParameter)
		{
			// Disable debug assertion dialog, set invalid parameter handler, throw a CRT exception:
			_CrtSetReportMode(_CRT_ASSERT, 0);
			_set_invalid_parameter_handler(Exceptions::InvalidParameterHandler);
#pragma warning (suppress : 6387)
			Assert::ExpectException<std::invalid_argument>([] {printf(NULL);}, L"Expected invalid_argument");
		}

		TEST_METHOD(UnhandledExceptionFilter)
		{
			// Nothing to do here - just make sure this compiles:
			SetUnhandledExceptionFilter(&Exceptions::UnhandledExceptionFilter);
		}
	};
}
