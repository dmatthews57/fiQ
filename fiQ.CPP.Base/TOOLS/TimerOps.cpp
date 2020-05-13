//==========================================================================================================================
// TimerOps.cpp : Classes and functions for managing execution of timed functions
//==========================================================================================================================
#include "pch.h"
#include "TimerOps.h"
using namespace FIQCPPBASE;

//==========================================================================================================================
// TimerExecutor::Initialize: Start up worker threads for timer execution
void TimerHandle::TimerExecutor::Initialize(size_t TimerThreads) {
	if(ThreadsShouldRun == false) {

		// Flag thread startup and initialize lock:
		ThreadsShouldRun = true;
		OpenTimersLock.Init();

		// Start up all requested threads (if a given thread handle already exists OR if a thread
		// fails to start, throw exception - neither situation should occur):
		for(size_t i = 0; i < TimerThreads && i < TIMER_THREADS_MAX; ++i) {
			if(ThreadHandles[i] > 0) throw FORMAT_RUNTIME_ERROR("Thread handle not closed");
			ThreadHandles[i] = (HANDLE)_beginthreadex(
				nullptr,	// Security (default)
				0,			// Stack size (Default)
				&(TimerExecutor::TimerThread), // Function address (static member function)
				nullptr,	// Function argument (unused)
				0,			// Initflag (run immediately)
				nullptr		// Thread address
			);
			if(ThreadHandles[i] <= 0) throw FORMAT_RUNTIME_ERROR("Error initializing thread");
			// Put slight delay between thread initializations, to keep them from all running
			// on the exact same schedule (probably this is just superstition as execution will
			// wind up desynchronizing them, but whatever):
			Sleep(10);
		}
	}
}
// TimerExecutor::Cleanup: Stop worker threads and clean up object
bool TimerHandle::TimerExecutor::Cleanup() {
	// Flag shutdown and invalidate lock to ensure any waiting threads give up:
	ThreadsShouldRun = false;
	OpenTimersLock.Invalidate();

	// Create a local array of thread handles, and move any valid handles to it:
	HANDLE RunningThreads[TIMER_THREADS_MAX] = { NULL };
	DWORD RunningThreadCount = 0;
	for(size_t i = 0; i < TIMER_THREADS_MAX; ++i) {
		if(ThreadHandles[i] > 0) RunningThreads[RunningThreadCount++] = ThreadHandles[i];
		ThreadHandles[i] = NULL;
	}

	// If any thread handles were still valid, wait for all threads to exit:
	bool ShutdownClean = (RunningThreadCount == 0);
	if(ShutdownClean == false) {
		const DWORD rc = WaitForMultipleObjects(RunningThreadCount, RunningThreads, TRUE, 2500);
		if((ShutdownClean = (rc == WAIT_OBJECT_0)) == false)
			LogSink::StdErrLog("WARNING: Timer manager threads not stopped cleanly [%d]", rc);
		for(DWORD i = 0; i < RunningThreadCount; ++i) CloseHandle(RunningThreads[i]);
	}
	return ShutdownClean;
}
// TimerExecutor::~TimerExecutor: Ensure object was shut down cleanly
TimerHandle::TimerExecutor::~TimerExecutor() noexcept(false) {
	// In normal circumstances, all worker threads should be shut down and all values cleaned up by call to Cleanup;
	// if main() failed to do so, just log warning (if this is being destructed, program is terminating anyway):
	if(ThreadsShouldRun) LogSink::StdErrLog("WARNING: Timer manager destructing without shutdown");
}
// TimerExecutor::TimerThreadExec: For lifetime of timer system, pick up and execute functions
unsigned int TimerHandle::TimerExecutor::TimerThreadExec() {
	LOG_FROM_TEMPLATE(LogLevel::Debug, "Timer thread started");
	while(ThreadsShouldRun) {

		// Look for expired timers ready to execute:
		std::shared_ptr<const TimerControlBlock> ToExec(nullptr);
		bool NoTimersScheduled = false;
		try {
			auto lock = Locks::Acquire(OpenTimersLock);
			if(lock.IsLocked()) {
				if((NoTimersScheduled = OpenTimers.empty()) == false) {
					const SteadyClock now;
					for(bool CheckingExpired = true; CheckingExpired == true;) {
						auto seek = OpenTimers.cbegin(), end = OpenTimers.cend();
						for(seek; seek != end && ThreadsShouldRun; ++seek) {

							// Attempt to promote weak_ptr to shared_ptr; if successful, check whether
							// timer has expired and if so move into outer scope pointer:
							std::shared_ptr<const TimerControlBlock> seekptr = seek->lock();
							if(seekptr ? (seekptr->ExecAt < now) : false) ToExec = std::move(seekptr);

							// If seekptr is null, either the scheduler has abandoned this timer or we
							// just moved from it to the outer pointer; either way, remove from vector,
							// reset iterators for safety and break inner loop:
							if(seekptr == nullptr) {
								OpenTimers.erase(seek);
								seek = OpenTimers.cbegin(), end = OpenTimers.cend();
								NoTimersScheduled = OpenTimers.empty();
								break;
							}

						}
						// Continue outer loop only if we did not find a function to execute, iterators
						// are valid (i.e. map is not empty) and shutdown has not been flagged:
						CheckingExpired = (ToExec == nullptr && seek != end && ThreadsShouldRun);
					}
				}
			}
		}
		catch(const std::exception& e) {
			const auto exceptioncontext = Exceptions::UnrollException(e);
			LOG_FROM_TEMPLATE_CONTEXT(LogLevel::Error, &exceptioncontext, "Exception polling timers");
		}

		// If we located a function to execute (and shutdown has not been flagged), do so now:
		if(ToExec != nullptr && ThreadsShouldRun) {
			try {
				ToExec->Exec();
			}
			catch(const std::exception& e) {
				const auto exceptioncontext = Exceptions::UnrollException(e);
				LOG_FROM_TEMPLATE_CONTEXT(LogLevel::Error, &exceptioncontext, "Exception caught from function");
			}
			ToExec = nullptr;
		}

		// If shutdown has not been flagged, sleep before next iteration (extending time if
		// there are no functions waiting to execute)
		if(ThreadsShouldRun) Sleep(5);
	}
	LOG_FROM_TEMPLATE(LogLevel::Debug, "Timer thread stopped");
	return 0;
}

//==========================================================================================================================
// TimerHandle::GetTimerExecutor: Create static object and return by reference
_Check_return_ TimerHandle::TimerExecutor& TimerHandle::GetTimerExecutor() {
	static TimerHandle::TimerExecutor texec; // Will be created only once, on first call to this function
	return texec;
}
