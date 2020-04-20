#include "pch.h"
#include "CppUnitTest.h"
#include "ToStrings.h"
#include "Tools/ConfigFile.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace FIQCPPBASE;

namespace fiQCPPBaseTESTS
{
	TEST_CLASS(ConfigFile_TEST)
	{
	public:
		
		TEST_METHOD(OpenAndRead)
		{
			ConfigFile cfg;
			Assert::AreEqual(true, cfg.Initialize(PROJECTDIR R"(TestFiles\TestConfig.txt)"), L"Failed to initialize config file");

			ConfigFile::SectionPtr sec = cfg.GetSection("SECTION1");
			Assert::IsNotNull(sec.get(), L"Failed to open SECTION1");
			Assert::AreEqual(4ULL, sec->GetEntryCount(), L"Invalid number of config entries (SECTION1)");
			Assert::AreEqual("Value", sec->GetNamedString("NAME").c_str());
			Assert::AreEqual("RightSpacedValue ", sec->GetNamedString(std::string("LeftSpacedName")).c_str());
			Assert::AreEqual(" LeftSpacedValue", sec->GetNamedString("RightSpacedName").c_str());
			Assert::AreEqual(" BothSpacedValue ", sec->GetNamedString("BothSpacedName").c_str());

			sec = cfg.GetSection("SECTION 2");
			Assert::IsNotNull(sec.get(), L"Failed to open SECTION 2");
			Assert::AreEqual(2ULL, sec->GetEntryCount(), L"Invalid number of config entries (SECTION 2)");
			Assert::AreEqual("Value1", sec->GetNamedString(std::string("NAME1")).c_str());
			Assert::AreEqual("Value2", sec->GetNamedString("name2").c_str());

			sec = cfg.GetSection("section 3");
			Assert::IsNull(sec.get(), L"Successfully opened SECTION 3 (should be nullptr)");

			sec = cfg.GetSection("SECTION 4");
			Assert::IsNotNull(sec.get(), L"Failed to open SECTION 4");
			Assert::AreEqual(5ULL, sec->GetEntryCount(), L"Invalid number of config entries (SECTION 4)");
			{auto b = sec->Begin(), e = sec->End();
			Assert::IsFalse(b == e, L"Invalid start/end iterators (SECTION 4)");
			Assert::AreEqual("Value1", b->GetEntry().c_str());
			Assert::IsFalse(++b == e, L"Unexpected end of SECTION 4");
			Assert::AreEqual("Value2", b->GetEntry().c_str());
			Assert::IsFalse(++b == e, L"Unexpected end of SECTION 4");
			Assert::AreEqual("Value3", b->GetEntry().c_str());
			Assert::IsFalse(++b == e, L"Unexpected end of SECTION 4");
			Assert::AreEqual("Value4", b->GetEntry().c_str());
			Assert::IsFalse(++b == e, L"Unexpected end of SECTION 4");
			Assert::AreEqual(R"("Value 5")", b->GetEntry().c_str());
			Assert::IsTrue(++b == e, L"Expected end of SECTION 4");}

			sec = cfg.GetSection("SECTION5");
			Assert::IsNotNull(sec.get(), L"Failed to open SECTION5");
			Tokenizer toks = sec->GetNamedTokenizer<10>("Tokenizer");
			Assert::AreEqual(3ULL, toks.TokenCount(), L"Invalid number of tokens");
			Assert::AreEqual("Field0", toks.Value(0));
			Assert::AreEqual("Field1", toks.Value(1));
			Assert::AreEqual("Field2", toks.Value(2));
			Assert::AreEqual(12345, sec->GetNamedInt("Int"));
			Assert::AreEqual(gsl::narrow_cast<unsigned short>(123), sec->GetNamedUShort("UShort"));
			Assert::AreEqual(0xFFAB1122ULL, sec->GetNamedHex("Hex"));
			Assert::AreEqual(true, sec->GetNamedBool("Bool"));
		}

	};
}