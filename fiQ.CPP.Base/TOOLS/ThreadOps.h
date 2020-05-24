#pragma once
//==========================================================================================================================
// ThreadOps.h : Classes and functions for managing classes with an integrated worker thread
//==========================================================================================================================

#include <process.h>
#include "Logging/LogSink.h"
#include "Tools/Exceptions.h"
#include "Tools/SteadyClock.h"

namespace FIQCPPBASE {

//==========================================================================================================================
// Locks: Class providing management tools for concurrency
class Locks {
private: struct pass_key {}; // Private function pass-key definition
public:

	//======================================================================================================================
	// Locks::Guard: Class providing C++-style scoped lock management for lock classes in this class
	template<typename T>
	class Guard {
	public:
		// Public accessor functions
		_Check_return_ bool IsLocked() const noexcept {return LockFlag;}

		// Public constructor (locked by pass_key so caller must instantiate via Locks::Acquire)
		Guard(Locks::pass_key, T& lock) noexcept(false) : Lock(lock), LockFlag(false) {Lock.Lock(LockFlag);}
		Guard(Guard&& g) noexcept : Lock(g.Lock), LockFlag(g.LockFlag) {g.LockFlag = false;}
		~Guard() noexcept(false) {Lock.Unlock(LockFlag);}

		// Deleted default and copy constructors, assignment operators
		Guard() = delete;
		Guard(const Guard&) = delete;
		Guard& operator=(const Guard&) = delete;
		Guard& operator=(Guard&&) = delete;

	private:
		T& Lock;
		bool LockFlag;
	};

	// Locks::Acquire: Helper function to provide template deduction when creating a Guard object
	template<typename T>
	_Check_return_ static Guard<T> Acquire(T& lock) {return Guard<T>(pass_key{}, lock);}

	//======================================================================================================================
	// Locks::SpinLock: Class wrapping a simple spin lock with optional spin count
	class SpinLock {
	public:

		// Lock management functions - Note these are deliberately NOT interlocked, and should only
		// be used by lock owner to initialize lock BEFORE any thread attempts to access it, and to
		// stop any thread from acquiring the lock AFTER any work should be expected to complete:
		void Init() noexcept {LockVal = 0;}
		void Invalidate() noexcept {LockVal = 2;}

		// Lock acquisition functions:
		bool Lock(bool& IsLocked);
		void Unlock(bool& IsLocked) noexcept;

		// Non-interlocked (read-only) functions:
		_Check_return_ bool IsLocked() const noexcept;
		_Check_return_ int MSecLocked() const noexcept;

		// Public "sensitive" constructor: ContinueFlag is a reference to a flag that will tell this lock whether the owning
		// object is still intending to run; if it ever becomes false, lock acquisition will be aborted immediately (if this
		// is not required, use other constructor)
		SpinLock(bool& _ContinueFlag, bool ConstructValid, unsigned short _SpinCount = 0) noexcept
			: ContinueFlag(_ContinueFlag), SpinCount(_SpinCount), LockVal(ConstructValid ? 0 : 2), LastLocked() {}
		// Public "default" constructor: ContinueFlag is not used, lock acquisition will work blindly so long as this object
		// is valid (i.e. has been initialized or was constructed valid):
		SpinLock(bool ConstructValid, unsigned short _SpinCount = 0) noexcept
			: ContinueFlag(alwaystrue), SpinCount(_SpinCount), LockVal(ConstructValid ? 0 : 2), LastLocked() {}
	private:
		const bool& ContinueFlag;
		const unsigned short SpinCount;
		long LockVal;
		SteadyClock LastLocked;
		static const bool alwaystrue = true;
	};

}; // (end class Locks)

//==========================================================================================================================
// ThreadOperator: Base class providing ability to manage an internal worker thread
template<typename T>
class ThreadOperator
{
protected:
	using ThreadWorkUnit = std::unique_ptr<T>;

	//======================================================================================================================
	// Thread execution function definition
	virtual unsigned int ThreadExecute() = 0;

	//======================================================================================================================
	// Thread management functions
	_Check_return_ bool ThreadStart(int Priority = THREAD_PRIORITY_NORMAL);
	void ThreadFlagStop();
	_Check_return_ bool ThreadWaitStop(int Timeout = INFINITE);
	_Check_return_ bool ThreadIsStopped() const noexcept;

	//======================================================================================================================
	// Thread worker queue management functions
	size_t ThreadQueueWork(ThreadWorkUnit&& work); // Move already-constructed object into queue
	template<typename...Args, std::enable_if_t<std::is_constructible_v<T, Args...>, int> = 0>
	size_t ThreadQueueWork(Args&&...args);
	_Check_return_ bool ThreadQueueEmpty() const noexcept; // Note: not thread-safe, use with care
	_Check_return_ size_t ThreadQueueSize() const noexcept; // Note: not thread-safe, use with care

	//======================================================================================================================
	// Worker thread accessor functions
	_Check_return_ bool ThreadShouldRun() const noexcept;
	bool ThreadWaitEvent(int Timeout = INFINITE) const;
	bool ThreadDequeueWork(ThreadWorkUnit& work) noexcept(false);
	bool ThreadUnsafeDequeueWork(ThreadWorkUnit& work) noexcept(false); // Note: not thread-safe, see remarks
	void ThreadRequeueWork(ThreadWorkUnit&& work);

	//======================================================================================================================
	// Manual event management functions
	void ThreadFlagEvent();
	void ThreadClearEventFlag();

	//======================================================================================================================
	// Deleted copy/move constructors and assignment operators
	ThreadOperator(const ThreadOperator&) = delete;
	ThreadOperator(ThreadOperator&&) = delete;
	ThreadOperator& operator=(const ThreadOperator&) = delete;
	ThreadOperator& operator=(ThreadOperator&&) = delete;

protected:

	//======================================================================================================================
	// Protected constructor/destructor
	ThreadOperator() noexcept : TO_QueueLock(TO_ShouldRun) {}
	~ThreadOperator() noexcept(false); // Non-virtual (don't allow deletion of objects through ThreadOperator pointer)

private:

	//======================================================================================================================
	// Thread management variables
	HANDLE TO_ThreadHandle = NULL;
	HANDLE TO_EventHandle = NULL;
	bool TO_ShouldRun = false;
	int TO_Priority = 0;
	Locks::SpinLock TO_QueueLock;
	std::deque<ThreadWorkUnit> TO_WorkQueue;

	//======================================================================================================================
	// Worker thread function definition (static class function receives pointer to runtime object)
	static unsigned int _stdcall TO_ThreadExec(void* TO_Instance) {
		try {
			// Cast void pointer to an object of my type, set thread priority and call execution function
			ThreadOperator<T>* MyObject = static_cast<ThreadOperator<T>*>(TO_Instance);
			SetThreadPriority(GetCurrentThread(), MyObject->TO_Priority);
			return MyObject->ThreadExecute();
		}
		catch(const std::exception& e) {
			const auto exceptioncontext = Exceptions::UnrollException(e);
			LOG_FROM_TEMPLATE_CONTEXT(LogLevel::Fatal, &exceptioncontext, "Thread caught unhandled exception, exiting");
			return 99;
		}
	}

}; // (end class ThreadOperator)

//==========================================================================================================================
#pragma region Locks
// SpinLock::Lock: Acquire spin lock
inline bool Locks::SpinLock::Lock(bool& IsLocked) {
	// Aggressive acquisition:
	for(unsigned short s = 0;
		IsLocked == false // We have not yet acquired lock
		&& LockVal != 2 // Lock is valid (non-interlocked access, read-only)
		&& ContinueFlag // Owning object has not flagged shutdown
		&& s <= SpinCount; // Up to SpinCount tries (infinite if SpinCount is zero)
		SpinCount == 0 ? s : ++s // Only increment loop counter if non-zero SpinCount
	) {
		if(InterlockedCompareExchange(&LockVal, 1, 0) == 0) {
			IsLocked = true;
		}
		else SwitchToThread();
	}
	// If needed, back off to slower wait:
	if(IsLocked == false) {
		while(
			IsLocked == false // We have not acquired lock
			&& LockVal != 2 // Lock is valid (non-interlocked access, read-only)
			&& ContinueFlag // Owning object has not flagged shutdown
		) {
			if(InterlockedCompareExchange(&LockVal, 1, 0) == 0) {
				IsLocked = true;
			}
			else Sleep(5);
		}
	}
	if(IsLocked) LastLocked.SetNow();
	return IsLocked;
}
// SpinLock::Unlock: Release spin lock
inline void Locks::SpinLock::Unlock(bool& IsLocked) noexcept {
	if(IsLocked) { // Do nothing unless client had acquired lock (allow lazy caller)
		IsLocked = false;
		if(LockVal == 1) LockVal = 0; // Owner may have invalidated lock, only unlock if valid
	}
}
// SpinLock::IsLocked: Read-only check of current lock status
_Check_return_ inline bool Locks::SpinLock::IsLocked() const noexcept {return (LockVal == 1);}
// SpinLock::MsecLocked: Read-only check of how long lock has been held, if locked
_Check_return_ inline int Locks::SpinLock::MSecLocked() const noexcept {
	return (LockVal == 1) ? SteadyClock().MSecSince(LastLocked) : 0;
}
#pragma endregion Locks

//==========================================================================================================================
#pragma region ThreadOperator
// ThreadOperator::~ThreadOperator: Ensure thread is stopped and release resources
template<typename T>
inline ThreadOperator<T>::~ThreadOperator() noexcept(false) {
	// In normal circumstances, thread should be shut down and all values cleaned up by child call to ThreadStop - however
	// if child class failed to do so, allowing destruction to proceed without at least stopping the worker thread could
	// potentially cause major problems, so attempt to perform shutdown now:
	if(TO_ThreadHandle > 0) {
		LogSink::StdErrLog(
			"WARNING: Thread ID %08X destructing without shutdown, attempting now", GetThreadId(TO_ThreadHandle));
		TO_ShouldRun = false;
		TO_QueueLock.Invalidate(); // Ensure any thread waiting on lock gives up
		if(TO_EventHandle != NULL) SetEvent(TO_EventHandle);
		if(WaitForSingleObject(TO_ThreadHandle, 1000) != WAIT_OBJECT_0) {
			LogSink::StdErrLog(
				"WARNING: Thread ID %08X shutdown failed, destruction will proceed", GetThreadId(TO_ThreadHandle));
		}
		CloseHandle(TO_ThreadHandle);
	}
	if(TO_EventHandle != NULL) CloseHandle(TO_EventHandle);
}
// ThreadOperator::ThreadStart: Launch worker thread
template<typename T>
GSL_SUPPRESS(type.4) // C-style cast of beginthreadex return value required (it is defined as unsigned, but may return -1)
inline _Check_return_ bool ThreadOperator<T>::ThreadStart(int Priority) {
	if(TO_ThreadHandle > 0 || TO_EventHandle != NULL) return false;
	TO_ShouldRun = true;
	TO_QueueLock.Init();
	TO_Priority = Priority;
	TO_EventHandle = CreateEvent(NULL, TRUE, FALSE, NULL);
	TO_ThreadHandle = (HANDLE)_beginthreadex(
		nullptr,	// Security (default)
		0,			// Stack size (Default)
		&(ThreadOperator<T>::TO_ThreadExec), // Function address (static member function)
		this,		// Function argument (pass in 'this' pointer of runtime object)
		0,			// Initflag (run immediately)
		nullptr		// Thread address
	);
	return (TO_ThreadHandle > 0);
}
// ThreadOperator::ThreadFlagStop: Inform worker thread it should stop and return
template<typename T>
inline void ThreadOperator<T>::ThreadFlagStop() {
	// Set thread status flag, trigger event to ensure sleeping threads wake up
	TO_ShouldRun = false;
	if(TO_EventHandle != NULL) SetEvent(TO_EventHandle);
}
// ThreadOperator::ThreadWaitStop: Inform worker thread it should stop, wait for it to do so
template<typename T>
inline _Check_return_ bool ThreadOperator<T>::ThreadWaitStop(int Timeout) {
	// Set thread status flag, trigger event to ensure sleeping threads wake up:
	TO_ShouldRun = false;
	TO_QueueLock.Invalidate(); // Ensure any thread waiting on lock gives up
	if(TO_EventHandle != NULL) SetEvent(TO_EventHandle);

	// If thread handle has not already been cleared, wait for shutdown:
	bool ShutdownClean = (TO_ThreadHandle <= 0);
	if(ShutdownClean == false) {
		ShutdownClean = (WaitForSingleObject(TO_ThreadHandle, Timeout) == WAIT_OBJECT_0);
		if(ShutdownClean) {
			CloseHandle(TO_ThreadHandle);
			TO_ThreadHandle = NULL;
			if(TO_EventHandle != NULL) {
				CloseHandle(TO_EventHandle);
				TO_EventHandle = NULL;
			}
		}
	}
	else if(TO_EventHandle != NULL) {
		CloseHandle(TO_EventHandle);
		TO_EventHandle = NULL;
	}
	return ShutdownClean;
}
// ThreadOperator::ThreadIsStopped: Check whether thread handle has been closed (indicating it has stopped)
template<typename T>
inline _Check_return_ bool ThreadOperator<T>::ThreadIsStopped() const noexcept {
	return (TO_ThreadHandle <= 0);
}
// ThreadOperator::ThreadQueueWork: Add unit of work to back of queue
template<typename T>
inline size_t ThreadOperator<T>::ThreadQueueWork(ThreadWorkUnit&& work) {
	auto lock = Locks::Acquire(TO_QueueLock);
	if(lock.IsLocked()) {
		// Transfer ownership of work unit from input pointer to back of queue:
		TO_WorkQueue.emplace_back(std::move(work));
		const size_t rc = TO_WorkQueue.size();
		if(TO_EventHandle) SetEvent(TO_EventHandle);
		return rc;
	}
	return 0;
}
// ThreadOperator::ThreadQueueWork (emplace version): Construct unit of work at back of queue
template<typename T>
template<typename...Args, std::enable_if_t<std::is_constructible_v<T, Args...>, int>>
inline size_t ThreadOperator<T>::ThreadQueueWork(Args&&...args) {
	auto lock = Locks::Acquire(TO_QueueLock);
	if(lock.IsLocked()) {
		// Construct unit of work at back of queue, using arguments provided:
		TO_WorkQueue.emplace_back(std::make_unique<T>(std::forward<Args>(args)...));
		const size_t rc = TO_WorkQueue.size();
		if(TO_EventHandle) SetEvent(TO_EventHandle);
		return rc;
	}
	return 0;
}
// ThreadOperator::ThreadQueueSize: Return current depth of work queue (note this function is not thread-safe)
template<typename T>
inline _Check_return_ size_t ThreadOperator<T>::ThreadQueueSize() const noexcept {return TO_WorkQueue.size();}
// ThreadOperator::ThreadQueueEmpty: Check if work queue is currently empty (note this function is not thread-safe)
template<typename T>
inline _Check_return_ bool ThreadOperator<T>::ThreadQueueEmpty() const noexcept {return TO_WorkQueue.empty();}
// ThreadOperator::ThreadShouldRun: Checks status of flag indicating whether thread should continue executing
template<typename T>
_Check_return_ bool ThreadOperator<T>::ThreadShouldRun() const noexcept {return TO_ShouldRun;}
// ThreadOperator::ThreadWaitEvent: Wait for thread event to become signaled
template<typename T>
inline bool ThreadOperator<T>::ThreadWaitEvent(int Timeout) const {
	// Return true if event is flagged, false if wait timed out (or event handle is invalid):
	return (TO_EventHandle == NULL) ? false : (WaitForSingleObject(TO_EventHandle, Timeout) != WAIT_TIMEOUT);
}
// ThreadOperator::ThreadDequeueWork: Retrieve work item from front of queue
template<typename T>
inline bool ThreadOperator<T>::ThreadDequeueWork(ThreadWorkUnit& work) noexcept(false) {
	bool rc = false;
	// Ensure target pointer is cleared first, to shorten potential time in lock
	work.reset(nullptr);
	auto lock = Locks::Acquire(TO_QueueLock);
	if(lock.IsLocked()) {
		if(TO_WorkQueue.empty() == false && TO_ShouldRun) {
			// Move ownership of item at front of queue to target pointer:
			work = std::move(TO_WorkQueue.front());
			rc = true;
			TO_WorkQueue.pop_front();
			// If queue is now empty, ensure status of event is cleared:
			if(TO_WorkQueue.empty() && TO_ShouldRun && TO_EventHandle != NULL) ResetEvent(TO_EventHandle);
		}
	}
	return rc;
}
// ThreadOperator::ThreadUnsafeDequeueWork: Retrieve work item from front of queue without locking
// - NOTE this function is deliberately not thread-safe; use it inside worker thread only
// - Thread-unsafe access is provided to allow clearing of queue during shutdown, when locking is impossible
template<typename T>
inline bool ThreadOperator<T>::ThreadUnsafeDequeueWork(ThreadWorkUnit& work) noexcept(false) {
	if(TO_WorkQueue.empty() == false) {
		// Move ownership of item at front of queue to target pointer:
		work = std::move(TO_WorkQueue.front());
		TO_WorkQueue.pop_front();
		return true;
	}
	else return false;
}
// ThreadOperator::ThreadRequeueWork: Returns work item to front of queue for reprocessing
template<typename T>
inline void ThreadOperator<T>::ThreadRequeueWork(ThreadWorkUnit&& work) {
	auto lock = Locks::Acquire(TO_QueueLock);
	if(lock.IsLocked()) {
		TO_WorkQueue.emplace_front(std::move(work));
		if(TO_EventHandle) SetEvent(TO_EventHandle);
	}
}
// ThreadOperator::ThreadFlagEvent: Manually set event active
template<typename T>
inline void ThreadOperator<T>::ThreadFlagEvent() {if(TO_EventHandle) SetEvent(TO_EventHandle);}
// ThreadOperator::ThreadClearEventFlag: Manually set event inactive
template<typename T>
inline void ThreadOperator<T>::ThreadClearEventFlag() {if(TO_EventHandle) ResetEvent(TO_EventHandle);}
#pragma endregion ThreadOperator

}; // (end namespace FIQCPPBASE)
