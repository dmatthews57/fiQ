#include "pch.h"
#include "CppUnitTest.h"
#include "Tools/TimeClock.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace FIQCPPBASE;

namespace fiQCPPBaseTESTS
{
	TEST_CLASS(TimeClock_TEST)
	{
	public:
		
		TEST_METHOD(TimeTests)
		{
			const TimeClock tbase = TimeClock::Now();
			Sleep(5);
			const TimeClock tlater;
			Assert::IsTrue(tlater > tbase);
			Assert::IsTrue(tlater >= tbase);
			Assert::IsTrue(tbase < tlater);
			Assert::IsTrue(tbase <= tlater);
			Sleep(5);
			Assert::IsTrue(tbase < TimeClock());
		}
	};
}
