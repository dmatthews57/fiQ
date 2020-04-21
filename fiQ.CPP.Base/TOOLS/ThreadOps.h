#pragma once
//==========================================================================================================================
// ThreadOps.h : Classes and functions for managing classes with an integrated worker thread
//==========================================================================================================================

#include <process.h>
#include "Tools/Logger.h"

namespace FIQCPPBASE {

template<typename T>
class ThreadOperator
{
protected:

	//======================================================================================================================
	// Thread execution function definition
	virtual unsigned int ThreadExecute() = 0;

	//======================================================================================================================
	// Thread management functions
	bool ThreadStart(int Priority = THREAD_PRIORITY_NORMAL);
	void ThreadFlagStop();
	bool ThreadWaitStop(int Timeout = INFINITE);
	bool ThreadIsStopped() const noexcept;

	//======================================================================================================================
	// Thread worker queue management functions
	size_t ThreadQueueWork(std::unique_ptr<T>&& work);
	template<typename...Args, std::enable_if_t<std::is_constructible_v<T, Args...>, int> = 0>
	size_t ThreadQueueWork(Args&&...args);
	size_t ThreadQueueSize() const; // Note: not thread-safe, use with care

	//======================================================================================================================
	// Worker thread accessor functions
	_Check_return_ bool ThreadShouldRun() const noexcept;
	bool ThreadWaitEvent(int Timeout = INFINITE) const;
	bool ThreadDequeueWork(std::unique_ptr<T>& work);
	void ThreadRequeueWork(std::unique_ptr<T>&& work);

	//======================================================================================================================
	// Manual event management functions
	void ThreadFlagEvent();
	void ThreadClearEventFlag();

	//======================================================================================================================
	// Public constructor/destructor
	ThreadOperator() noexcept(false) = default;
	~ThreadOperator() noexcept(false); // Non-virtual (don't allow deletion of objects through ThreadOperator pointer)
	// Deleted copy/move constructors and assignment operators
	ThreadOperator(const ThreadOperator&) = delete;
	ThreadOperator(ThreadOperator&&) = delete;
	ThreadOperator& operator=(const ThreadOperator&) = delete;
	ThreadOperator& operator=(ThreadOperator&&) = delete;

private:

	//======================================================================================================================
	// Thread management variables
	HANDLE TO_ThreadHandle = NULL;
	HANDLE TO_EventHandle = NULL;
	bool TO_ShouldRun = false;
	int TO_Priority = 0;
	std::deque<std::unique_ptr<T>> TO_WorkQueue;

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
			Logger::StdErrLog("WARNING: Thread ID %08X caught unhandled exception, exiting:%s",
				GetCurrentThreadId(), Exceptions::UnrollExceptionString(e).c_str());
			return 99;
		}
	}
};


//==========================================================================================================================
// ThreadOperator::~ThreadOperator
template<typename T>
inline ThreadOperator<T>::~ThreadOperator() noexcept(false) {
	// In normal circumstances, thread should be shut down and all values cleaned up by child call to ThreadStop - however
	// if child class failed to do so, allowing destruction to proceed without at least stopping the worker thread could
	// potentially cause major problems, so attempt to perform shutdown now:
	if(TO_ThreadHandle > 0) {
		Logger::StdErrLog("WARNING: Thread ID %08X destructing without shutdown, attempting now", GetCurrentThreadId());
		TO_ShouldRun = false;
		if(TO_EventHandle != NULL) SetEvent(TO_EventHandle);
		if(WaitForSingleObject(TO_ThreadHandle, 1000) == WAIT_TIMEOUT)
			Logger::StdErrLog("WARNING: Thread ID %08X shutdown failed, destruction will proceed", GetCurrentThreadId());
		CloseHandle(TO_ThreadHandle);
	}
	if(TO_EventHandle != NULL) CloseHandle(TO_EventHandle);
}
//==========================================================================================================================
#pragma region ThreadOperator: Thread management functions
// ThreadOperator::ThreadStart: Launch worker thread
template<typename T>
GSL_SUPPRESS(type.4) // C-style cast of beginthreadex return value required (it is defined as unsigned but may return -1)
inline bool ThreadOperator<T>::ThreadStart(int Priority) {
	if(TO_ThreadHandle > 0 || TO_EventHandle != NULL) return false;
	TO_ShouldRun = true;
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
inline bool ThreadOperator<T>::ThreadWaitStop(int Timeout) {
	// Set thread status flag, trigger event to ensure sleeping threads wake up:
	TO_ShouldRun = false;
	if(TO_EventHandle != NULL) SetEvent(TO_EventHandle);

	// If thread handle has not already been cleared, wait for shutdown:
	bool ShutdownClean = (TO_ThreadHandle <= 0);
	if(ShutdownClean == false) {
		ShutdownClean = (WaitForSingleObject(TO_ThreadHandle, Timeout) != WAIT_TIMEOUT);
		CloseHandle(TO_ThreadHandle);
	}
	TO_ThreadHandle = NULL;

	// Close event handle:
	if(TO_EventHandle != NULL) CloseHandle(TO_EventHandle);
	TO_EventHandle = NULL;

	return ShutdownClean;
}
// ThreadOperator::ThreadIsStopped: Check whether thread handle has been closed (indicating it has stopped)
template<typename T>
inline bool ThreadOperator<T>::ThreadIsStopped() const noexcept {
	return (TO_ThreadHandle <= 0);
}
#pragma endregion ThreadOperator: Thread management functions

//==========================================================================================================================
#pragma region ThreadOperator: Thread worker queue management functions
// ThreadOperator::ThreadQueueWork: Add unit of work to back of queue
template<typename T>
inline size_t ThreadOperator<T>::ThreadQueueWork(std::unique_ptr<T>&& work) {
	// Transfer ownership of work unit from input pointer to back of queue:
	TO_WorkQueue.emplace_back(std::move(work)); // TODO: LOCK FIRST
	const size_t rc = TO_WorkQueue.size();
	if(TO_EventHandle) SetEvent(TO_EventHandle);
	return rc;
}
// ThreadOperator::ThreadQueueWork (emplace version): Construct unit of work at back of queue
template<typename T>
template<typename...Args, std::enable_if_t<std::is_constructible_v<T, Args...>, int>>
inline size_t ThreadOperator<T>::ThreadQueueWork(Args&&...args) {
	// Construct unit of work at back of queue, using arguments provided:
	TO_WorkQueue.emplace_back(std::make_unique<T>(std::forward<Args>(args)...)); // TODO: LOCK FIRST
	const size_t rc = TO_WorkQueue.size();
	if(TO_EventHandle) SetEvent(TO_EventHandle);
	return rc;
}
// ThreadOperator::ThreadQueueSize: Return current depth of work queue (note this function is not thread-safe)
template<typename T>
inline size_t ThreadOperator<T>::ThreadQueueSize() const {return TO_WorkQueue.size();}
#pragma endregion ThreadOperator: Thread worker queue management functions

//======================================================================================================================
#pragma region ThreadOperator: Worker thread accessor functions
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
inline bool ThreadOperator<T>::ThreadDequeueWork(std::unique_ptr<T>& work) {
	bool rc = false;
	// Ensure target pointer is cleared first, to shorten potential time in lock
	work.reset(nullptr);
	// TODO: LOCK FIRST
	if(TO_WorkQueue.empty() == false && TO_ShouldRun) {
		// Move ownership of item at front of queue to target pointer:
		work = std::move(TO_WorkQueue.front());
		rc = true;
		TO_WorkQueue.pop_front();
		// If queue is now empty, ensure status of event is cleared:
		if(TO_WorkQueue.empty() && TO_ShouldRun && TO_EventHandle != NULL) ResetEvent(TO_EventHandle);
	}
	return rc;
}
// ThreadOperator::ThreadRequeueWork: Places work item back at front of queue
template<typename T>
inline void ThreadOperator<T>::ThreadRequeueWork(std::unique_ptr<T>&& work) {
	// TODO: LOCK FIRST
	TO_WorkQueue.emplace_front(std::move(work));
	if(TO_EventHandle) SetEvent(TO_EventHandle);
}
#pragma endregion ThreadOperator: Worker thread accessor functions

//======================================================================================================================
#pragma region ThreadOperator: Manual event management functions
template<typename T>
inline void ThreadOperator<T>::ThreadFlagEvent() {if(TO_EventHandle) SetEvent(TO_EventHandle);}
template<typename T>
inline void ThreadOperator<T>::ThreadClearEventFlag() {if(TO_EventHandle) ResetEvent(TO_EventHandle);}
#pragma region ThreadOperator: Manual event management functions

} // (end namespace FIQCPPBASE)
