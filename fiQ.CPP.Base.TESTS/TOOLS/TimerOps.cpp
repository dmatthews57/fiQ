#include "pch.h"
#include "CppUnitTest.h"
#include "Tools/TimerOps.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace FIQCPPBASE;

namespace fiQCPPBaseTESTS
{
	TEST_CLASS(TimerOps_TEST)
	{
	public:

		// Definitions of expected test values:
		const int START_VAL = 1; // Default starting value
		const int SHOULD_BE = 10; // Should be only value that executes
		const int SHOULD_NOT_BE = 11; // Should never actually execute

		// Member function to be scheduled:
		void UpdateInt(int& tgt, int val) noexcept(false) {tgt = val;}

		TEST_CLASS_INITIALIZE(Class_Init) // Executes before any TEST_METHODs
		{
			TimerHandle::InitializeTimers();
			Logger::WriteMessage("Timers initialized");
		}		

		TEST_METHOD(TimerBasics)
		{
			// Start timer to update ShouldUpdate1 to value that should never execute, then reschedule shorter timer to make it value that should:
			Assert::IsTrue(t1.Start(std::chrono::milliseconds(100), std::bind(&TimerOps_TEST::UpdateInt, this, std::ref(ShouldUpdate1), SHOULD_NOT_BE)), L"Failed to start timer");
			Assert::IsTrue(t1.IsSet(), L"Timer not set");
			Assert::IsTrue(t1.Start(std::chrono::milliseconds(50), std::bind(&TimerOps_TEST::UpdateInt, this, std::ref(ShouldUpdate1), SHOULD_BE)), L"Failed to start timer");
			Assert::IsTrue(t1.IsSet(), L"Timer not set");

			// Start timer to update ShouldNotUpdate2, then cancel it:
			Assert::IsTrue(t2.Start(std::chrono::milliseconds(100), std::bind(&TimerOps_TEST::UpdateInt, this, std::ref(ShouldNotUpdate1), SHOULD_NOT_BE)), L"Failed to start timer");
			Assert::IsTrue(t2.IsSet(), L"Timer not set");
			t2.Cancel();
			Assert::IsFalse(t2.IsSet(), L"Timer still set after cancel");

			// Create IntWrapper object around ShouldUpdate2 (implicitly starts timer on this member):
			IntWrapper iw1(ShouldUpdate2, SHOULD_BE);
			Assert::IsTrue(iw1.IsTimerSet(), L"Wrapper timer not set");

			// Create and then destruct IntWrapper object around ShouldNotUpdate2 (implicitly starts timer, then should cancel at destruction):
			{IntWrapper iw2(ShouldNotUpdate2, SHOULD_NOT_BE);
			Assert::IsTrue(iw2.IsTimerSet(), L"Wrapper timer not set");}

			// Allow time for all timers to fire:
			Sleep(250);

			// Ensure all values are what they should be:
			Assert::AreEqual(SHOULD_BE, ShouldUpdate1, L"ShouldUpdate1 not updated");
			Assert::AreEqual(START_VAL, ShouldNotUpdate1, L"ShouldNotUpdate1 was updated");
			Assert::AreEqual(SHOULD_BE, ShouldUpdate2, L"ShouldUpdate2 not updated");
			Assert::AreEqual(START_VAL, ShouldNotUpdate2, L"ShouldNotUpdate2 was updated");
		}

		TEST_CLASS_CLEANUP(Class_Cleanup) // Executes after all TEST_METHODs
		{
			TimerHandle::CleanupTimers();
			Logger::WriteMessage("Timers cleaned up");
		}		

	private:

		// Member variables to be updated (or not) by timed functions:
		int ShouldUpdate1 = START_VAL;
		int ShouldUpdate2 = START_VAL;
		int ShouldNotUpdate1 = START_VAL;
		int ShouldNotUpdate2 = START_VAL;

		// Handles to member timers:
		TimerHandle t1;
		TimerHandle t2;

		// Class wrapping timer and reference to integer (to test destruction before execution):
		class IntWrapper {
		public:
			bool IsTimerSet() const noexcept {return t.IsSet();}
			void UpdateInt(int j) const noexcept(false) {i = j;}
			IntWrapper(int& _i, int _j) : i(_i) {
				t.Start(
					std::chrono::milliseconds(100),
					std::bind(&IntWrapper::UpdateInt, this, _j)
				);
			}
		private:
			int& i;
			TimerHandle t;
		};
	};
}