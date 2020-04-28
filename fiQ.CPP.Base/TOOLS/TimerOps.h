#pragma once
//==========================================================================================================================
// TimerOps.h : Classes and functions for managing execution of timed functions
//==========================================================================================================================

#include "ThreadOps.h"
#include <functional>

namespace FIQCPPBASE {

//==========================================================================================================================
// TimerHandle: Class providing access to an executable timer
// - To make an application capable of executing timers, call the static initialization function near the start of main()
//   before attempting to schedule any timers, and static cleanup function near end of main when no longer needed
// - To make a class capable of executing timers, have it contain one or more TimerHandle members (each TimerHandle will
//   only manage a single outstanding function call at a time - so classes require one TimerHandle per independent timer)
//   and use that object's public non-static functions to Start and Cancel execution
// - TimerHandle::Start expects the result of a std::bind call, e.g. "Start(std::bind(&MyClass::MyFunc, this, MyArg);"
// - Since the class scheduling the timer contains the TimerHandle object, if it is destructed before the timer goes off
//   the TimerHandle object will also be destructed, orphaning the TimerControlBlock in TimerExecutor's vector of pending
//	 timers and preventing it from executing (as TimerExecutor will ignore any TimerControlBlock with no owner)
// - std::bind will make copies of all arguments; avoid using pointers/references, but if necessary at least ensure they
//   are to values (or members) that are guaranteed to live at least as long as the TimerHandle member does
class TimerHandle {
public:

	// Public definitions - Worker thread pool size
	static constexpr size_t TIMER_THREADS_MIN		= 1;
	static constexpr size_t TIMER_THREADS_DEFAULT	= 4;
	static constexpr size_t TIMER_THREADS_MAX		= 10;

	// Static library initialization functions: Each should be called exactly once in program lifetime
	// - Note these functions are NOT thread-safe - call them from main() only
	static void InitializeTimers(size_t TimerThreads = TIMER_THREADS_DEFAULT);
	static void CleanupTimers();

	// Non-static timer management functions:
	bool Start(std::chrono::steady_clock::duration d, std::function<void()>&& f);
	void Cancel() noexcept;
	_Check_return_ bool IsSet() const noexcept;

private:

	//======================================================================================================================
	// TimerControlBlock: Container for execution time and function
	struct TimerControlBlock {
		TimerControlBlock(std::chrono::steady_clock::duration d, std::function<void()>&& f)
			: ExecAt(d), Exec(f) {}
		const SteadyClock ExecAt;
		const std::function<void()> Exec;
	};

	// Private member variables:
	std::shared_ptr<TimerControlBlock> tcb; // Pointer to control block for currently-scheduled timer

	//======================================================================================================================
	// TimerExecutor: Singleton timer execution management class, created once by TimerHandle's static accessor
	class TimerExecutor {
	public:

		// Initialization functions
		void Initialize(size_t TimerThreads);
		bool Cleanup();

		// Timer management functions
		_Check_return_ bool CreateTimer(const std::shared_ptr<TimerControlBlock>& timer) {
			auto lock = Locks::Acquire(OpenTimersLock);
			if(lock.IsLocked()) return OpenTimers.emplace_back(timer), true;
			else return false;
		}

		// Public default constructor/destructor (note OpenTimersLock is receiving reference to
		// ThreadsShouldRun member, not value - so init order doesn't matter):
		TimerExecutor() noexcept(false) : ThreadsShouldRun(false), OpenTimersLock(ThreadsShouldRun) {
			OpenTimers.reserve(100); // Reserve space for a moderate number of open timers
		}
		~TimerExecutor() noexcept(false);

		// Deleted copy/move constructors and assignment operators (should never be called,
		// as this class will only be created once and only destructed at program exit)
		TimerExecutor(const TimerExecutor&) = delete;
		TimerExecutor(TimerExecutor&&) = delete;
		TimerExecutor& operator=(const TimerExecutor&) = delete;
		TimerExecutor& operator=(TimerExecutor&&) = delete;

	private:
		// Timer thread execution function:
		unsigned int TimerThreadExec();

		// Private member variables:
		HANDLE ThreadHandles[TIMER_THREADS_MAX] = { NULL }; // Array of handles to executing threads
		bool ThreadsShouldRun;			// Flag to indicate continued running of threads
		Locks::SpinLock OpenTimersLock;	// Lock to control access to OpenTimers member
		std::vector<std::weak_ptr<TimerControlBlock>> OpenTimers;	// Collection of currently-scheduled timers

		//==================================================================================================================
		// Worker thread function definition
		static unsigned int _stdcall TimerThread(void*) {
			try {
				SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
				return GetTimerExecutor().TimerThreadExec();
			}
			catch(const std::exception& e) {
				LogSink::StdErrLog("WARNING: Timer thread ID %08X caught unhandled exception, exiting:%s",
					GetCurrentThreadId(), Exceptions::UnrollExceptionString(e).c_str());
				return 99;
			}
		}
	};

	// Static TimerExecutor accessor function (creates precisely one TimerExecutor during process lifetime)
	_Check_return_ static TimerExecutor& GetTimerExecutor();
};

//==========================================================================================================================
// TimerHandle::InitializeTimers: Pass request to static manager
inline void TimerHandle::InitializeTimers(size_t TimerThreads) {
	GetTimerExecutor().Initialize(ValueOps::Bounded(TIMER_THREADS_MIN, TimerThreads, TIMER_THREADS_MAX));
}
// TimerHandle::CleanupTimers: Pass request to static manager
inline void TimerHandle::CleanupTimers() {
	GetTimerExecutor().Cleanup();
}
// TimerHandle::Start: Create timer control block and add to static manager
inline bool TimerHandle::Start(std::chrono::steady_clock::duration d, std::function<void()>&& f) {
	tcb = nullptr; // Ensure any existing timer is orphaned immediately
	tcb = std::make_shared<TimerControlBlock>(d, std::move(f));
	return GetTimerExecutor().CreateTimer(tcb) ? true : (tcb = nullptr, false);
}
// TimerHandle::Cancel: Clear control block pointer to prevent static manager from executing current function
inline void TimerHandle::Cancel() noexcept {
	tcb = nullptr;
}
// TimerHandle::IsSet: Checks whether timer is set by checking for control block
_Check_return_ inline bool TimerHandle::IsSet() const noexcept {
	return (tcb != nullptr);
}

}; // (end namespace FIQCPPBASE)
