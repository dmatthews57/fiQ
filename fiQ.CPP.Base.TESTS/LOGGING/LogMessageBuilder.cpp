#include "pch.h"
#include "CppUnitTest.h"
#include "Logging/LogMessageBuilder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace FIQCPPBASE;

namespace Microsoft {
	namespace VisualStudio {
		namespace CppUnitTestFramework {
			template<>
			std::wstring ToString<LogLevel>(const enum LogLevel& ll) { return std::to_wstring(static_cast<std::underlying_type_t<LogLevel>>(ll)); }
			template<>
			std::wstring ToString<FormatEscape>(const enum FormatEscape& fe) { return std::to_wstring(static_cast<std::underlying_type_t<FormatEscape>>(fe)); }
		}
	}
}

namespace fiQCPPBaseTESTS
{
	TEST_CLASS(LogMessageBuilder_TESTS)
	{
	public:
		
		TEST_METHOD(Placeholders)
		{
			static constexpr LogMessageTemplate lmt("{first:F9} SECOND {second:X9} THIRD {:D} {fourth:S3} FIFTH {fifth:S10} SIXTH {sixth:F22}");
			static_assert(lmt.IsValid(), "Invalid logging template string");
			static_assert(lmt.PlaceholderCount() == LogMessageTemplate::MAX_PLACEHOLDERS, "Invalid number of placeholders");
			std::string temp = "nonliteral";
			const std::string expected = "7654321.234567891 SECOND EDCBA0987 THIRD 321 lit FIFTH nonliteral SIXTH {sixth:F22}";
			auto lmb = CreateLogMessageBuilder<lmt.PlaceholderCount()>(LogLevel::Debug, lmt, 7654321.23456789123, 0xFEDCBA0987, 321UL, "literal", temp);
			auto lm = lmb.Build(LogMessage::ContextEntries {{"SAMPLE1","Whatever1"},{"SAMPLE2",std::string("wHATEVER2")}});
			Assert::AreEqual(LogLevel::Debug, lm->GetLevel(), L"Unexpected log level");
			Assert::AreEqual(6ULL, lm->GetContext().size(), L"Unexpected number of context entries");
			Assert::AreEqual(FormatEscape::None, lm->EscapeFormats(), L"Unexpected escape formats");
			Assert::AreEqual(expected, lm->GetString(), L"Unexpected final string value");
		}

		TEST_METHOD(NoPlaceholders)
		{
			static constexpr LogMessageTemplate lmt("Message with no\n placeholders");
			static_assert(lmt.IsValid(), "Invalid logging template string");
			static_assert(lmt.PlaceholderCount() == 0, "Invalid number of placeholders");
			const std::string expected = "Message with no\n placeholders";
			auto lmb = CreateLogMessageBuilder<lmt.PlaceholderCount()>(LogLevel::Info, lmt);
			auto lm = lmb.Build(LogMessage::ContextEntries {{"SAMPLE1","Whatever1"}});
			Assert::AreEqual(LogLevel::Info, lm->GetLevel(), L"Unexpected log level");
			Assert::AreEqual(1ULL, lm->GetContext().size(), L"Unexpected number of context entries");
			Assert::AreEqual(FormatEscape::JSON, lm->EscapeFormats(), L"Unexpected escape formats");
			Assert::AreEqual(expected, lm->GetString(), L"Unexpected final string value");
		}

	};
}