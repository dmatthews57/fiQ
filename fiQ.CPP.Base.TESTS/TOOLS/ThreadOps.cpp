#include "pch.h"
#include "CppUnitTest.h"
#include "Tools/ThreadOps.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace FIQCPPBASE;

namespace fiQCPPBaseTESTS
{
	class ThreadTest : private ThreadOperator<int> {
	public:
		bool StartThread() {return ThreadStart();}
		bool ThreadStarted() const {return (ThreadIsStopped() == false);}
		void Queue(int i) {
			if(i < 10) ThreadQueueWork(i); // Test emplace construction
			else if(i < 20) ThreadQueueWork(std::make_unique<int>(i)); // Test rvalue unique_ptr
			else { // Test lvalue unique_ptr
				auto a = std::make_unique<int>(i);
				ThreadQueueWork(std::move(a));
			}
		}
		bool WaitStopThread(int Timeout) {return ThreadWaitStop(Timeout);}
		int GetTotal() const {return MyTotal;}

		ThreadTest() = default;
		ThreadTest(const ThreadTest&) = delete;
		ThreadTest(ThreadTest&&) = delete;
		ThreadTest& operator=(const ThreadTest&) = delete;
		ThreadTest& operator=(ThreadTest&&) = delete;
		virtual ~ThreadTest() = default;

	private:
		unsigned int ThreadExecute() override {
			while(ThreadShouldRun()) {
				if(ThreadWaitEvent()) {
					ThreadWorkUnit work;
					while(ThreadDequeueWork(work)) {
						MyTotal += *work;
					}
				}
			}
			return 0;
		}
		int MyTotal = 0;
	};

	TEST_CLASS(ThreadOps_TEST)
	{
	public:
		
		TEST_METHOD(SpinLocks)
		{
			Locks::SpinLock sl(true, true);

			// Lock/unlock testing:
			{bool IsLocked = false;
			Assert::IsTrue(sl.Lock(IsLocked), L"Failed to acquire lock");
			Assert::IsTrue(IsLocked, L"Invalid flag value after locking");
			Assert::IsTrue(sl.IsLocked(), L"Lock member does not show as locked");
			sl.Unlock(IsLocked);
			Assert::IsFalse(IsLocked, L"Invalid flag value after unlocking");
			Assert::IsFalse(sl.IsLocked(), L"Lock member does not show as unlocked");}

			// Invalidate lock and re-test:
			sl.Invalidate();
			{bool IsLocked = false;
			Assert::IsFalse(sl.Lock(IsLocked), L"Acquire lock after invalidating");
			Assert::IsFalse(IsLocked, L"Invalid flag value after failed lock");
			Assert::IsFalse(sl.IsLocked(), L"Lock member shows as locked after invalidating");}

			// Reinitialize lock and test with Guard:
			sl.Init();
			{auto lock = Locks::Acquire(sl);
			Assert::IsTrue(lock.IsLocked(), L"Failed to acquire lock");
			Assert::IsTrue(sl.IsLocked(), L"Lock member does not show as locked");}
			Assert::IsFalse(sl.IsLocked(), L"Lock member does not show as unlocked after lock scope");

			// Test with Guard in exception case:
			try {
				auto lock = Locks::Acquire(sl);
				Assert::IsTrue(lock.IsLocked(), L"Failed to acquire lock");
				Assert::IsTrue(sl.IsLocked(), L"Lock member does not show as locked");
				throw std::runtime_error("Testing error");
			}
			catch(const std::exception&) {}
			Assert::IsFalse(sl.IsLocked(), L"Lock member does not show as unlocked after exception");
		}
		TEST_METHOD(ThreadOperator)
		{
			ThreadTest tt;
			Assert::IsTrue(tt.StartThread(), L"Failed to start thread");
			Assert::IsTrue(tt.ThreadStarted(), L"Thread not showing as started");
			for(int i = 0; i < 50; ++i) tt.Queue(i);
			Sleep(200); // Allow time for thread to process
			Assert::IsTrue(tt.WaitStopThread(500), L"Failed to start thread");
			Assert::IsFalse(tt.ThreadStarted(), L"Thread still showing as running");
			Assert::AreEqual(1225, tt.GetTotal(), L"Invalid total value computed by thread");
		}

	};
}