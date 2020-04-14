#include "pch.h"
#include "CppUnitTest.h"
#include "Tools/SerialOps.h"
#include "Tools/TimeClock.h"
#include <random>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace FIQCPPBASE;

namespace fiQCPPBaseTESTS
{
	TEST_CLASS(SerialOps_TEST)
	{
	public:
		
		TEST_METHOD(SerializeTimeClock)
		{
			const TimeClock tstart = TimeClock::Now();
			std::unique_ptr<char[]> TempBuf = std::make_unique<char[]>(200);
			Assert::IsTrue(tstart.SerializeTo(SerialOps::MemoryStream(TempBuf.get(), 200)), L"Serialization failed");
			TimeClock tend(500); // Ensure second TimeClock does not equal start
			Assert::IsTrue(tend.ReadFrom(SerialOps::MemoryStream(TempBuf.get(), 200)), L"Deserialization failed");
			Assert::IsTrue(tstart == tend, L"Deserialized value does not match");
		}

		TEST_METHOD(SerializePrimitive)
		{
			std::random_device rd;
			const std::uniform_int_distribution<> dis(INT_MIN, INT_MAX);
			std::unique_ptr<char[]> TempBuf = std::make_unique<char[]>(200);
			for(int i = 0; i < 20; ++i) {
				int j = dis(rd), k = 0;
				Assert::IsTrue(SerialOps::MemoryStream(TempBuf.get(), 200).Write(j), L"Serialization failed");
				Assert::IsTrue(SerialOps::MemoryStream(TempBuf.get(), 200).Read(k), L"Deserialization failed");
				Assert::AreEqual(j, k, L"Deserialized value does not match");
			}
		}

		TEST_METHOD(SerializeString)
		{
			const std::string start = "TEST STRING VALUE";
			std::unique_ptr<char[]> TempBuf = std::make_unique<char[]>(200);
			Assert::IsTrue(SerialOps::MemoryStream(TempBuf.get(), 200).Write(start), L"Serialization failed");
			std::string end;
			Assert::IsTrue(SerialOps::MemoryStream(TempBuf.get(), 200).Read(end), L"Deserialization failed");
			Assert::AreEqual(start, end, L"Deserialized value does not match");
		}

		TEST_METHOD(SerializeChar)
		{
			const char start[20] = "1234567890123456789";
			std::unique_ptr<char[]> TempBuf = std::make_unique<char[]>(200);
			Assert::IsTrue(SerialOps::MemoryStream(TempBuf.get(), 200).Write(start, sizeof(start)), L"Serialization failed");
			char end[20] = {0};
			Assert::IsTrue(SerialOps::MemoryStream(TempBuf.get(), 200).Read(end, sizeof(end)), L"Deserialization failed");
			Assert::IsTrue(memcmp(start, end, sizeof(start)) == 0, L"Deserialized value does not match");
		}
	};
}
