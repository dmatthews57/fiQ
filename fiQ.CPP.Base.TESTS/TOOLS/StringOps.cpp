#include "pch.h"
#include "CppUnitTest.h"
#include "ToStrings.h"
#include "Tools/StringOps.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace FIQCPPBASE;

namespace fiQCPPBaseTESTS
{
	TEST_CLASS(StringOps_TEST)
	{
	public:
		
		TEST_METHOD(BytesRemain)
		{
			char temp[10] = {0}, *start = temp, *mid = temp + 2, *end = temp + 10;
#pragma warning (suppress : 6387)
			Assert::AreEqual(0ULL, StringOps::BytesAvail(start, nullptr), L"Start-to-nullptr length invalid");
			Assert::AreEqual(0ULL, StringOps::BytesAvail(nullptr, start), L"Nullptr-to-start length invalid");
			Assert::AreEqual(10ULL, StringOps::BytesAvail(start, end), L"Start-to-end length invalid");
			Assert::AreEqual(0ULL, StringOps::BytesAvail(start, start), L"Start self-test invalid");
			Assert::AreEqual(2ULL, StringOps::BytesAvail(start, mid), L"Start-to-mid length invalid");
			Assert::AreEqual(10ULL, StringOps::BytesAvail(start, end), L"Start-to-end length invalid");
			Assert::AreEqual(0ULL, StringOps::BytesAvail(mid, start), L"Mid-to-start invalid");
			Assert::AreEqual(0ULL, StringOps::BytesAvail(mid, mid), L"Mid self-test length invalid");
			Assert::AreEqual(8ULL, StringOps::BytesAvail(mid, end), L"Mid-to-end length invalid");
			Assert::AreEqual(0ULL, StringOps::BytesAvail(end, start), L"End-to-start invalid");
			Assert::AreEqual(0ULL, StringOps::BytesAvail(end, mid), L"End-to-mid length invalid");
			Assert::AreEqual(0ULL, StringOps::BytesAvail(end, end), L"End self-test length invalid");
		}

		TEST_METHOD(ExMemSet)
		{
			char temp1[10] = {0}, temp2[10] = {0};
			memset(temp2, 'F', sizeof(temp2)); 
			Assert::AreEqual(10ULL, StringOps::ExMemSet(temp2, 0, _countof(temp2)), L"Invalid length returned");
			Assert::AreEqual(0, memcmp(temp1, temp2, _countof(temp2)), L"Cleared buffer invalid");
		}

		TEST_METHOD(ExStrCpy)
		{
			char temp1[20] = {0}, temp2[] = "HELLO", temp3[20] = {0};
			Assert::AreEqual(strlen(temp2), StringOps::ExStrCpy(temp1, temp2, strlen(temp2)), L"Invalid length returned (string)");
			Assert::AreEqual(0, strcmp(temp1, temp2), L"Copied string not equivalent");
			Assert::AreEqual(5ULL, StringOps::ExStrCpyLiteral(temp3, "HELLO"), L"Invalid length returned (literal)");
			Assert::AreEqual(0, strcmp(temp2, temp3), L"Copied literal not equivalent");
		}

		TEST_METHOD(FlexStrCpy)
		{
			char temp1[20] = {0}, temp2[] = "HELLO", temp3[11] = {0};
			memset(temp1, 'F', sizeof(temp1));
			memset(temp3, 'E', sizeof(temp3));
			Assert::AreEqual(0ULL, StringOps::FlexStrCpy(temp3, nullptr, 10), L"Invalid length returned (null copy)");
			Assert::AreEqual(0ULL, strlen(temp3), L"Target not properly terminated by null copy");
			Assert::AreEqual(5ULL, StringOps::FlexStrCpy(temp3, temp2, 10), L"Invalid length returned (string copy)");
			Assert::AreEqual(0, strcmp(temp2, temp3), L"Copied string not equivalent");
			Assert::AreEqual(10ULL, StringOps::FlexStrCpy(temp3, temp1, 10), L"Invalid length returned (long string copy)");
			Assert::AreEqual(0, strcmp(temp3, "FFFFFFFFFF"), L"Copied long string not equivalent");
		}

		TEST_METHOD(StrCpy)
		{
			char temp1[20] = {0}, temp2[20] = {0};
			StringOps::StrCpyLiteral(temp1, "WHATEVER");
			Assert::AreEqual(0, strcmp(temp1, "WHATEVER"), L"Copied literal not equivalent");
			StringOps::StrCpy(temp2, temp1, 4);
			Assert::AreEqual(0, strcmp(temp2, "WHAT"), L"Copied string not equivalent");
		}

		TEST_METHOD(Decimal)
		{
			Assert::AreEqual('4', StringOps::Decimal::Char(1234UL), L"Invalid unsigned value char");
			Assert::AreEqual('4', StringOps::Decimal::Char(-1234L), L"Invalid signed value char");
			Assert::IsTrue(StringOps::Decimal::IsDecChar('9'), L"Didn't recognize decimal character");
			Assert::IsFalse(StringOps::Decimal::IsDecChar('F'), L"Didn't recognize non-decimal character");
			Assert::AreEqual(7ULL, StringOps::Decimal::Digits<unsigned long long, 1234567ULL>(), L"Incorrect number of digits in unsigned value");
			Assert::AreEqual(8ULL, StringOps::Decimal::Digits<signed long long, -1234567LL>(), L"Incorrect number of digits in signed value");
			Assert::AreEqual(6ULL, StringOps::Decimal::MaxDigits_v<short>, L"Incorrect number of digits for this type");
			Assert::AreEqual(5ULL, StringOps::Decimal::MaxDigits_v<unsigned short>, L"Incorrect number of digits for this type");
			Assert::AreEqual(11ULL, StringOps::Decimal::MaxDigits_v<long>, L"Incorrect number of digits for this type");
			Assert::AreEqual(10ULL, StringOps::Decimal::MaxDigits_v<unsigned long>, L"Incorrect number of digits for this type");
			Assert::AreEqual(20ULL, StringOps::Decimal::MaxDigits_v<long long>, L"Incorrect number of digits for this type");
			Assert::AreEqual(20ULL, StringOps::Decimal::MaxDigits_v<unsigned long long>, L"Incorrect number of digits for this type");
			Assert::AreEqual(4ULL, StringOps::Decimal::FlexDigits(1234U), L"Incorrect number of digits for this value");
			Assert::AreEqual(5ULL, StringOps::Decimal::FlexDigits(-1234L), L"Incorrect number of digits for this value");
			Assert::AreEqual(1ULL, StringOps::Decimal::FlexDigits(0ULL), L"Incorrect number of digits for this value");
			Assert::AreEqual(20ULL, StringOps::Decimal::FlexDigits(ULLONG_MAX), L"Incorrect number of digits for this value");
			Assert::AreEqual(20ULL, StringOps::Decimal::FlexDigits(LLONG_MIN), L"Incorrect number of digits for this value");
			Assert::AreEqual(19ULL, StringOps::Decimal::FlexDigits(LLONG_MAX), L"Incorrect number of digits for this value");
		}

		TEST_METHOD(DecimalFlexReadString)
		{
			Assert::AreEqual(1234UL, StringOps::Decimal::FlexReadString<unsigned long>("1234"));
			Assert::AreEqual(1234UL, StringOps::Decimal::FlexReadString<unsigned long>("1234A"));
			Assert::AreEqual(1234UL, StringOps::Decimal::FlexReadString<unsigned long>("123456", 4));
			Assert::AreEqual(1234, StringOps::Decimal::FlexReadString("1234"));
			Assert::AreEqual(1234, StringOps::Decimal::FlexReadString("1234A"));
			Assert::AreEqual(1234, StringOps::Decimal::FlexReadString("123456", 4));
			Assert::AreEqual(-1234, StringOps::Decimal::FlexReadString("-1234"));
			Assert::AreEqual(-1234, StringOps::Decimal::FlexReadString("-1234A"));
			Assert::AreEqual(-1234, StringOps::Decimal::FlexReadString("-123456", 5));
			Assert::AreEqual(LLONG_MIN, StringOps::Decimal::FlexReadString<long long>("-9223372036854775808"));
			Assert::AreEqual(LLONG_MAX, StringOps::Decimal::FlexReadString<long long>("9223372036854775807"));
			Assert::AreEqual(ULLONG_MAX, StringOps::Decimal::FlexReadString<unsigned long long>("18446744073709551615"));
		}

		TEST_METHOD(DecimalExWriteString)
		{
			char temp[30] = {0};

			// Basic signed tests:
			Assert::AreEqual(3ULL, StringOps::Decimal::ExWriteString<3>(temp, 1234), L"Wrong number of digits written (signed truncation)");
			Assert::AreEqual(0, memcmp(temp, "234", 3), L"Incorrect string written (signed truncation)");
			Assert::AreEqual(4ULL, StringOps::Decimal::ExWriteString<4>(temp, 5678), L"Wrong number of digits written (signed exact)");
			Assert::AreEqual(0, memcmp(temp, "5678", 4), L"Incorrect string written (signed exact)");
			Assert::AreEqual(5ULL, StringOps::Decimal::ExWriteString<5>(temp, 9012), L"Wrong number of digits written (signed padded)");
			Assert::AreEqual(0, memcmp(temp, "09012", 5), L"Incorrect string written (padded)");

			// Extreme signed tests:
			Assert::AreEqual(StringOps::Decimal::MaxDigits_v<long long>, StringOps::Decimal::ExWriteString<StringOps::Decimal::MaxDigits_v<long long>>(temp, LLONG_MIN));
			Assert::AreEqual(0, memcmp(temp, "-9223372036854775808", 20), L"Incorrect string written (min signed)");
			Assert::AreEqual(StringOps::Decimal::MaxDigits_v<long long>, StringOps::Decimal::ExWriteString<StringOps::Decimal::MaxDigits_v<long long>>(temp, LLONG_MAX));
			Assert::AreEqual(0, memcmp(temp, "09223372036854775807", 20), L"Incorrect string written (max signed)");

			// Unsigned tests:
			Assert::AreEqual(3ULL, StringOps::Decimal::ExWriteString<3>(temp, 1234U), L"Wrong number of digits written (unsigned truncation)");
			Assert::AreEqual(0, memcmp(temp, "234", 3), L"Incorrect string written (unsigned truncation)");
			Assert::AreEqual(4ULL, StringOps::Decimal::ExWriteString<4>(temp, 5678U), L"Wrong number of digits written (unsigned exact)");
			Assert::AreEqual(0, memcmp(temp, "5678", 4), L"Incorrect string written (unsigned exact)");
			Assert::AreEqual(5ULL, StringOps::Decimal::ExWriteString<5>(temp, 9012U), L"Wrong number of digits written (unsigned padded)");
			Assert::AreEqual(0, memcmp(temp, "09012", 5), L"Incorrect string written (unsigned padded)");
			Assert::AreEqual(StringOps::Decimal::MaxDigits_v<unsigned long long>, StringOps::Decimal::ExWriteString<StringOps::Decimal::MaxDigits_v<unsigned long long>>(temp, ULLONG_MAX));
			Assert::AreEqual(0, memcmp(temp, "18446744073709551615", 20), L"Incorrect string written (max unsigned)");
		}

		TEST_METHOD(DecimalFlexWriteStringImplicit)
		{
			char temp[30] = {0};

			// Signed tests:
			Assert::AreEqual(4ULL, StringOps::Decimal::FlexWriteString(temp, 1234), L"Wrong number of digits written (signed)");
			Assert::AreEqual(0, memcmp(temp, "1234", 4), L"Incorrect string written (signed)");
			Assert::AreEqual(20ULL, StringOps::Decimal::FlexWriteString(temp, LLONG_MIN));
			Assert::AreEqual(0, memcmp(temp, "-9223372036854775808", 20), L"Incorrect string written (min signed)");
			Assert::AreEqual(19ULL, StringOps::Decimal::FlexWriteString(temp, LLONG_MAX));
			Assert::AreEqual(0, memcmp(temp, "9223372036854775807", 19), L"Incorrect string written (max signed)");

			// Unsigned tests:
			Assert::AreEqual(4ULL, StringOps::Decimal::FlexWriteString(temp, 5678U), L"Wrong number of digits written (unsigned)");
			Assert::AreEqual(0, memcmp(temp, "5678", 4), L"Incorrect string written (unsigned)");
			Assert::AreEqual(20ULL, StringOps::Decimal::FlexWriteString(temp, ULLONG_MAX));
			Assert::AreEqual(0, memcmp(temp, "18446744073709551615", 20), L"Incorrect string written (max unsigned)");
		}

		TEST_METHOD(DecimalFlexWriteStringExplicit)
		{
			char temp[30] = {0};

			// Basic signed tests:
			Assert::AreEqual(3ULL, StringOps::Decimal::FlexWriteString(temp, 1234, 3).second, L"Wrong number of digits written (signed truncation)");
			Assert::AreEqual(0, memcmp(temp, "234", 3), L"Incorrect string written (signed truncation)");
			Assert::AreEqual(4ULL, StringOps::Decimal::FlexWriteString(temp, 1234, 4).second, L"Wrong number of digits written (signed exact)");
			Assert::AreEqual(0, memcmp(temp, "1234", 4), L"Incorrect string written (signed exact)");
			Assert::AreEqual(6ULL, StringOps::Decimal::FlexWriteString(temp, 1234, 6).second, L"Wrong number of digits written (signed)");
			Assert::AreEqual(0, memcmp(temp, "001234", 6), L"Incorrect string written (signed padded)");

			// Extreme signed tests:
			Assert::AreEqual(20ULL, StringOps::Decimal::FlexWriteString(temp, LLONG_MIN, StringOps::Decimal::MaxDigits_v<long long>).second);
			Assert::AreEqual(0, memcmp(temp, "-9223372036854775808", 20), L"Incorrect string written (min signed)");
			Assert::AreEqual(20ULL, StringOps::Decimal::FlexWriteString(temp, LLONG_MAX, StringOps::Decimal::MaxDigits_v<long long>).second);
			Assert::AreEqual(0, memcmp(temp, "09223372036854775807", 20), L"Incorrect string written (max signed)");

			// Unsigned tests:
			Assert::AreEqual(3ULL, StringOps::Decimal::FlexWriteString(temp, 1234U, 3).second, L"Wrong number of digits written (unsigned truncation)");
			Assert::AreEqual(0, memcmp(temp, "234", 3), L"Incorrect string written (unsigned truncation)");
			Assert::AreEqual(4ULL, StringOps::Decimal::FlexWriteString(temp, 5678U, 4).second, L"Wrong number of digits written (unsigned exact)");
			Assert::AreEqual(0, memcmp(temp, "5678", 4), L"Incorrect string written (unsigned exact)");
			Assert::AreEqual(5ULL, StringOps::Decimal::FlexWriteString(temp, 9012U, 5).second, L"Wrong number of digits written (unsigned padded)");
			Assert::AreEqual(0, memcmp(temp, "09012", 5), L"Incorrect string written (unsigned padded)");
			Assert::AreEqual(20ULL, StringOps::Decimal::FlexWriteString(temp, ULLONG_MAX, StringOps::Decimal::MaxDigits_v<unsigned long long>).second);
			Assert::AreEqual(0, memcmp(temp, "18446744073709551615", 20), L"Incorrect string written (max unsigned)");
		}

		TEST_METHOD(Float)
		{
			Assert::ExpectException<std::invalid_argument>([] {char temp[10] = {0}; StringOps::Float::ISOWriteFXRate(temp, 0, 1.23);}, L"Expected invalid_argument");
			Assert::ExpectException<std::invalid_argument>([] {char temp[10] = {0}; StringOps::Float::ISOWriteFXRate(temp, 10, -1.23);}, L"Expected invalid_argument");

			char temp[30] = {0};
			Assert::AreEqual(5ULL, StringOps::Float::ISOWriteFXRate(temp, 5, 12345678.0).second, L"Invalid length written (out of range)");
			Assert::AreEqual(0, memcmp(temp, "09999", 5), L"Invalid value written (out of range)");
			Assert::AreEqual(5ULL, StringOps::Float::ISOWriteFXRate(temp, 5, 12.34).second, L"Invalid length written (normal)");
			Assert::AreEqual(0, memcmp(temp, "21234", 5), L"Invalid value written (normal)");
			Assert::AreEqual(5ULL, StringOps::Float::ISOWriteFXRate(temp, 5, 0.0001234).second, L"Invalid length written (below range)");
			Assert::AreEqual(0, memcmp(temp, "30000", 5), L"Invalid value written (below range)");

			Assert::AreEqual(9ULL, StringOps::Float::FlexWriteString(temp, 1234.0123, 4).second, L"Wrong number of digits written (float exact)");
			Assert::AreEqual(0, memcmp(temp, "1234.0123", 9), L"Incorrect string written (float exact)");
			Assert::AreEqual(10ULL, StringOps::Float::FlexWriteString(temp, -1234.0123, 4).second, L"Wrong number of digits written (float exact negative)");
			Assert::AreEqual(0, memcmp(temp, "-1234.0123", 10), L"Incorrect string written (float exact negative)");
			Assert::AreEqual(11ULL, StringOps::Float::FlexWriteString(temp, 1234.0123, 6).second, L"Wrong number of digits written (float pad)");
			Assert::AreEqual(0, memcmp(temp, "1234.012300", 11), L"Incorrect string written (float pad)");
		}

		TEST_METHOD(Ascii)
		{
			Assert::AreEqual('F', StringOps::Ascii::Char(0x1234CDEF), L"Invalid value char");
			Assert::IsTrue(StringOps::Ascii::IsHexChar('9'), L"Didn't recognize decimal character");
			Assert::IsTrue(StringOps::Ascii::IsHexChar('F'), L"Didn't recognize hex character");
			Assert::IsFalse(StringOps::Ascii::IsHexChar('Y'), L"Didn't recognize non-hex character");
			Assert::AreEqual(0x00ui8, StringOps::Ascii::CharToHex('0'), L"Invalid char value");
			Assert::AreEqual(0x09ui8, StringOps::Ascii::CharToHex('9'), L"Invalid char value");
			Assert::AreEqual(0x0Aui8, StringOps::Ascii::CharToHex('A'), L"Invalid char value");
			Assert::AreEqual(0x0Aui8, StringOps::Ascii::CharToHex('a'), L"Invalid char value");
			Assert::AreEqual(0x0Fui8, StringOps::Ascii::CharToHex('F'), L"Invalid char value");
			Assert::AreEqual(0x0Fui8, StringOps::Ascii::CharToHex('f'), L"Invalid char value");
			Assert::AreEqual(0x00ui8, StringOps::Ascii::CharToHex('Y'), L"Invalid char value (non-hex)");
			Assert::AreEqual(0x00ui8, StringOps::Ascii::ByteToHex("00"), L"Invalid char value");
			Assert::AreEqual(0x09ui8, StringOps::Ascii::ByteToHex("09"), L"Invalid char value");
			Assert::AreEqual(0x5Aui8, StringOps::Ascii::ByteToHex("5A"), L"Invalid char value");
			Assert::AreEqual(0x5Aui8, StringOps::Ascii::ByteToHex("5a"), L"Invalid char value");
			Assert::AreEqual(0xF0ui8, StringOps::Ascii::ByteToHex("F0"), L"Invalid char value");
			Assert::AreEqual(0xF4ui8, StringOps::Ascii::ByteToHex("f4"), L"Invalid char value");
			Assert::AreEqual(0x00ui8, StringOps::Ascii::ByteToHex("Y0"), L"Invalid char value (non-hex)");
		}

		TEST_METHOD(AsciiReadString)
		{
			Assert::AreEqual(static_cast<unsigned char>(0x12), StringOps::Ascii::ReadString<2>("123456ABCDEFEDCB"), L"Invalid value (unsigned char)");
			Assert::AreEqual(static_cast<unsigned short>(0x1234), StringOps::Ascii::ReadString<4>("123456ABCDEFEDCB"), L"Invalid value (unsigned short)");
			Assert::AreEqual(0x123456UL, StringOps::Ascii::ReadString<6>("123456ABCDEFEDCB"), L"Invalid value (unsigned long 6)");
			Assert::AreEqual(0x123456ABUL, StringOps::Ascii::ReadString<8>("123456ABCDEFEDCB"), L"Invalid value (unsigned long 8)");
			Assert::AreEqual(0x123456ABCDULL, StringOps::Ascii::ReadString<10>("123456ABCDEFEDCB"), L"Invalid value (unsigned long long 10)");
			Assert::AreEqual(0x123456ABCDEFULL, StringOps::Ascii::ReadString<12>("123456ABCDEFEDCB"), L"Invalid value (unsigned long long 12)");
			Assert::AreEqual(0x123456ABCDEFEDULL, StringOps::Ascii::ReadString<14>("123456ABCDEFEDCB"), L"Invalid value (unsigned long long 14)");
			Assert::AreEqual(0x123456ABCDEFEDCBULL, StringOps::Ascii::ReadString<16>("123456ABCDEFEDCB"), L"Invalid value (unsigned long long 16)");
		}

		TEST_METHOD(AsciiFlexReadString)
		{
			Assert::AreEqual(0x00ULL, StringOps::Ascii::FlexReadString(""), L"Invalid value (empty string)");
			Assert::AreEqual(0x01ULL, StringOps::Ascii::FlexReadString("1"));
			Assert::AreEqual(0x01ULL, StringOps::Ascii::FlexReadString("1!"));
			Assert::AreEqual(0x01ULL, StringOps::Ascii::FlexReadString("01!"));
			Assert::AreEqual(0x01ULL, StringOps::Ascii::FlexReadString("0x1"));
			Assert::AreEqual(0xffffffffffffffffULL, StringOps::Ascii::FlexReadString("0xffffffffffffffff"));
			Assert::AreEqual(0xffffffffULL, StringOps::Ascii::FlexReadString("0xffffffffffffffff", 10));
		}

		TEST_METHOD(AsciiExWriteString)
		{
			char temp[30] = {0};

			// Signed tests:
			Assert::AreEqual(6ULL, StringOps::Ascii::ExWriteString<6>(temp, 0x1234ABCD), L"Wrong number of digits written (signed truncation)");
			Assert::AreEqual(0, memcmp(temp, "34ABCD", 6), L"Incorrect string written (signed truncation )");
			Assert::AreEqual(8ULL, StringOps::Ascii::ExWriteString<8>(temp, 0x1234ABCD), L"Wrong number of digits written (signed)");
			Assert::AreEqual(0, memcmp(temp, "1234ABCD", 8), L"Incorrect string written (signed)");
			Assert::AreEqual(10ULL, StringOps::Ascii::ExWriteString<10>(temp, 0x1234ABCD), L"Wrong number of digits written (signed padded)");
			Assert::AreEqual(0, memcmp(temp, "001234ABCD", 10), L"Incorrect string written (signed padded))");
			Assert::AreEqual(16ULL, StringOps::Ascii::ExWriteString<16>(temp, LLONG_MIN), L"Wrong number of digits written (min signed)");
			Assert::AreEqual(0, memcmp(temp, "8000000000000000", 16), L"Incorrect string written (min signed)");
			Assert::AreEqual(16ULL, StringOps::Ascii::ExWriteString<16>(temp, LLONG_MAX), L"Wrong number of digits written (max signed)");
			Assert::AreEqual(0, memcmp(temp, "7FFFFFFFFFFFFFFF", 16), L"Incorrect string written (max signed)");

			// Unsigned tests:
			Assert::AreEqual(6ULL, StringOps::Ascii::ExWriteString<6>(temp, 0x1234ABCDUL), L"Wrong number of digits written (unsigned truncation)");
			Assert::AreEqual(0, memcmp(temp, "34ABCD", 6), L"Incorrect string written (unsigned truncation )");
			Assert::AreEqual(8ULL, StringOps::Ascii::ExWriteString<8>(temp, 0x1234ABCDUL), L"Wrong number of digits written (unsigned)");
			Assert::AreEqual(0, memcmp(temp, "1234ABCD", 8), L"Incorrect string written (unsigned)");
			Assert::AreEqual(10ULL, StringOps::Ascii::ExWriteString<10>(temp, 0x1234ABCDUL), L"Wrong number of digits written (unsigned padded)");
			Assert::AreEqual(0, memcmp(temp, "001234ABCD", 10), L"Incorrect string written (unsigned padded))");
			Assert::AreEqual(28ULL, StringOps::Ascii::ExWriteString<28>(temp, 0x1234ABCDUL), L"Wrong number of digits written (unsigned long padded)");
			Assert::AreEqual(0, memcmp(temp, "000000000000000000001234ABCD", 28), L"Incorrect string written (unsigned long padded)");
			Assert::AreEqual(16ULL, StringOps::Ascii::ExWriteString<16>(temp, ULLONG_MAX), L"Wrong number of digits written (max unsigned)");
			Assert::AreEqual(0, memcmp(temp, "FFFFFFFFFFFFFFFF", 16), L"Incorrect string written (max unsigned)");
		}

		TEST_METHOD(AsciiFlexWriteString)
		{
			char temp[20] = {0};
			
			const unsigned long long testval = 0xFEDCBA9876543210ULL;
			static const char compval[] = "00FEDCBA9876543210";
			for(size_t s = 0; s <= 18; ++s) {
				Assert::AreEqual(s, StringOps::Ascii::FlexWriteString(temp, testval, s).second, L"Wrong number of hex chars written");
				Assert::AreEqual(0, memcmp(temp, compval + (18 - s), 18 - s), L"Incorrect string written");
			}
		}

		TEST_METHOD(AsciiPackUnpack)
		{
			char temp[30] = {0};
			Assert::AreEqual(4ULL, StringOps::Ascii::PackTo<4>("1234ABCD", temp), L"Invalid number of bytes packed");
			Assert::AreEqual(0, memcmp(temp, "\x12\x34\xAB\xCD", 4), L"Incorrect values written");
			Assert::AreEqual(8ULL, StringOps::Ascii::UnpackFrom<4>("\x12\x34\xAB\xCD", temp), L"Invalid number of bytes packed");
			Assert::AreEqual(0, memcmp(temp, "1234ABCD", 8), L"Incorrect values written");
		}

		TEST_METHOD(Trimming)
		{
			const std::string ToTrim = "  hello there  ";
			Assert::AreEqual(0, StringOps::TrimLeft(ToTrim).compare("hello there  "), L"String left trim mismatch");
			Assert::AreEqual(0, StringOps::TrimLeft(ToTrim.data(), ToTrim.length()).compare("hello there  "), L"Buffer left trim mismatch");
			Assert::AreEqual(0, StringOps::TrimRight(ToTrim).compare("  hello there"), L"String right trim mismatch");
			Assert::AreEqual(0, StringOps::TrimRight(ToTrim.data(), ToTrim.length()).compare("  hello there"), L"Buffer right trim mismatch");
			Assert::AreEqual(0, StringOps::Trim(ToTrim).compare("hello there"), L"String trim mismatch");
			Assert::AreEqual(0, StringOps::Trim(ToTrim.data(), ToTrim.length()).compare("hello there"), L"Buffer trim mismatch");
		}

		TEST_METHOD(WideStrings)
		{
			const std::wstring ws = L"testvalue";
			const std::string s = "testvalue";
			Assert::AreEqual(ws, StringOps::ConvertToWideString(s));
			Assert::AreEqual(s, StringOps::ConvertFromWideString(ws));
		}
	};
}
