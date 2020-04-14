#include "pch.h"
#include "CppUnitTest.h"
#include "Tools/ValueOps.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace FIQCPPBASE;

namespace fiQCPPBaseTESTS
{
	TEST_CLASS(ValueOps_TEST)
	{
	public:
		
		TEST_METHOD(InSet)
		{
			Assert::IsTrue(ValueOps::Is(5).InSet(1,2,3,4,5,99));
			Assert::IsFalse(ValueOps::Is(5).InSet(1,2,3,4,99));
			Assert::IsTrue(ValueOps::Is(123.456).InSet(123,123.123,123.456,987.654,555.222,666.333));
			Assert::IsFalse(ValueOps::Is(123.456).InSet(123,123.123,987.654,555.222,666.333));
		}

		TEST_METHOD(InRange)
		{
			Assert::IsTrue(ValueOps::Is(1).InRange(1,10));
			Assert::IsTrue(ValueOps::Is(5).InRange(1,10));
			Assert::IsTrue(ValueOps::Is(10).InRange(1,10));
			Assert::IsFalse(ValueOps::Is(11).InRange(1,10));

			Assert::IsTrue(ValueOps::Is(1).InRangeLeft(1,10));
			Assert::IsTrue(ValueOps::Is(5).InRangeLeft(1,10));
			Assert::IsFalse(ValueOps::Is(10).InRangeLeft(1,10));
			Assert::IsFalse(ValueOps::Is(11).InRangeLeft(1,10));

			Assert::IsFalse(ValueOps::Is(1).InRangeRight(1,10));
			Assert::IsTrue(ValueOps::Is(5).InRangeRight(1,10));
			Assert::IsTrue(ValueOps::Is(10).InRangeRight(1,10));
			Assert::IsFalse(ValueOps::Is(11).InRangeRight(1,10));

			Assert::IsFalse(ValueOps::Is(1).InRangeEx(1,10));
			Assert::IsTrue(ValueOps::Is(5).InRangeEx(1,10));
			Assert::IsFalse(ValueOps::Is(10).InRangeEx(1,10));
			Assert::IsFalse(ValueOps::Is(11).InRangeEx(1,10));

			Assert::IsTrue(ValueOps::Is(1).InRangeSet(std::make_pair(1,2), std::make_pair(4,6)));
			Assert::IsTrue(ValueOps::Is(5).InRangeSet(std::make_pair(1,2), std::make_pair(4,6)));
			Assert::IsTrue(ValueOps::Is(10).InRangeSet(std::make_pair(1,2), std::make_pair(4,6), std::make_pair(8,10)));
			Assert::IsFalse(ValueOps::Is(11).InRangeSet(std::make_pair(1,2), std::make_pair(4,6), std::make_pair(8,10)));
		}

		TEST_METHOD(CalcExponent)
		{
			Assert::AreEqual(1, ValueOps::CalcExponent<int, 10, 0>::value);
			Assert::AreEqual(1, ValueOps::CalcExponent<int, 50, 0>::value);
			Assert::AreEqual(1024, ValueOps::CalcExponent<int, 2, 10>::value);
			Assert::AreEqual(9223372036854775808, ValueOps::CalcExponent<unsigned long long, 2, 63>::value);
			Assert::AreNotEqual(1ULL, ValueOps::CalcExponent<unsigned long long, 2, 63>::value);
		}

		TEST_METHOD(MinZero)
		{
			Assert::AreEqual(0LL, ValueOps::MinZero(LLONG_MIN));
			Assert::AreEqual(ValueOps::MinZero(-1), 0);
			Assert::AreEqual(ValueOps::MinZero(0), 0);
			Assert::AreEqual(ValueOps::MinZero(1), 1);
			Assert::AreEqual(ValueOps::MinZero(LLONG_MAX), LLONG_MAX);
		}

		TEST_METHOD(Bounded)
		{
			Assert::AreEqual(ValueOps::Bounded(0LL, LLONG_MIN, 50LL), 0LL);
			Assert::AreEqual(ValueOps::Bounded(5, 0, 15), 5);
			Assert::AreEqual(ValueOps::Bounded(5, 7, 15), 7);
			Assert::AreEqual(ValueOps::Bounded(5, 17, 15), 15);
			Assert::AreEqual(ValueOps::Bounded(0LL, LLONG_MAX, 50LL), 50LL);
		}

	};
}
