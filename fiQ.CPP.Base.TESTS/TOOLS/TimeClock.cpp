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
		
		TEST_METHOD(SimpleTests)
		{
			const TimeClock tbase = TimeClock::Now();
			const TimeClock tlater(tbase, 5);
			Assert::IsTrue(tlater > tbase);
			Assert::IsTrue(tlater >= tbase);
			Assert::IsTrue(tbase < tlater);
			Assert::IsTrue(tbase <= tlater);
			SleepEx(10, false);
			Assert::IsTrue(tbase.Expired());
		}

	};
}
