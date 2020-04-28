#include "pch.h"
#include "CppUnitTest.h"
#include "Tools/SteadyClock.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace FIQCPPBASE;

namespace fiQCPPBaseTESTS
{
	TEST_CLASS(SteadyClock_TEST)
	{
	public:
		
		TEST_METHOD(SteadyTests)
		{
			const SteadyClock tbase;
			const SteadyClock tlater = tbase + std::chrono::milliseconds(5);
			const SteadyClock treverse = tlater - std::chrono::milliseconds(5);
			Assert::IsTrue(treverse == tbase, L"Arithmetic checks failed");
			Assert::IsTrue(tlater.Since<std::chrono::milliseconds>(tbase) == 5, L"Wrong number of milliseconds elapsed (start to end)");
			Assert::IsTrue(tlater.Till<std::chrono::milliseconds>(tbase) == -5, L"Wrong number of milliseconds elapsed (end to start)");
			Assert::IsTrue(tlater > tbase);
			Assert::IsTrue(tlater >= tbase);
			Assert::IsTrue(tbase < tlater);
			Assert::IsTrue(tbase <= tlater);
			Sleep(10);
			Assert::IsTrue(tbase.IsPast());
		}
	};
}
