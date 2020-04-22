#include "pch.h"
#include "Tools/Exceptions.h"
using namespace FIQCPPBASE;

#include "Tools/ThreadOps.h"
#include <mutex>

void Innermost() {
	try {
		throw std::runtime_error("first domino");
	}
	catch(const std::exception&) {std::throw_with_nested(FORMAT_RUNTIME_ERROR("Innermost"));}
}
void Outermost() {
	try {
		Innermost();
	}
	catch(const std::exception&) {std::throw_with_nested(FORMAT_RUNTIME_ERROR("Outermost"));}
}

class ttest : private ThreadOperator<int> {
public:
	bool StartThread() {
		return ThreadStart();
	}
	void Queue(int i) {
		/*
		static const char buf[] = "abcdefghijklmop";
		if(i < 10) ThreadQueueWork(buf, i);
		else if(i < 20) ThreadQueueWork(std::make_unique<std::string>(std::to_string(i)));
		else {
			auto a = std::make_unique<std::string>(std::to_string(i));
			ThreadQueueWork(std::move(a));
		}
		*/
		ThreadQueueWork(i);
	}
	bool WaitStopThread() {
		return ThreadWaitStop(5000);
	}
	ttest(Locks::SpinLock& sl) : Lock(sl) {LoggingOps::StdOutLog("%08X ttest constructed", GetCurrentThreadId());}
	ttest() = delete;
	ttest(const ttest&) = delete;
	ttest(ttest&&) = delete;
	ttest& operator=(const ttest&) = delete;
	ttest& operator=(ttest&&) = delete;
	virtual ~ttest() {LoggingOps::StdOutLog("%08X ttest destructed", GetCurrentThreadId());}

private:
	Locks::SpinLock& Lock;
	unsigned int ThreadExecute() override {
		LoggingOps::StdOutLog("%08X Starting thread...", GetCurrentThreadId());
		int i = 2, j = 0;
		while(ThreadShouldRun()) {
			if(ThreadWaitEvent()) {
				ThreadWorkUnit work;
				auto l = Locks::Acquire(Lock);
				if(l.IsLocked()) {
					try {
						while(ThreadDequeueWork(work)) {
							LoggingOps::StdOutLog("%08X Dequeued work: %d", GetCurrentThreadId(), *work);
							if(*work == i) {
								LoggingOps::StdOutLog("%08X Re-queueing value: %d", GetCurrentThreadId(), *work);
								ThreadRequeueWork(std::move(work));
								i *= 2;
							}
							/*
							LoggingOps::StdOutLog("%08X Dequeued work: %s", GetCurrentThreadId(), work->c_str());
							if(++j == i) {
								LoggingOps::StdOutLog("%08X Re-queueing value: %s", GetCurrentThreadId(), work->c_str());
								ThreadRequeueWork(std::move(work));
								i *= 2;
							}
							*/
						}
					}
					catch(const std::exception& e) {LoggingOps::StdOutLog("%08X Caught exception:%s", GetCurrentThreadId(), Exceptions::UnrollExceptionString(e).c_str());}
					//catch(...) {printf("\nException\n");}
				}
				else LoggingOps::StdOutLog("%08X Failed to acquire lock", GetCurrentThreadId());
			}
		}
		//throw std::runtime_error("OOPS");
		LoggingOps::StdOutLog("%08X Exiting thread...", GetCurrentThreadId());
		return 0;
	}
};

int main()
{
	_set_invalid_parameter_handler(Exceptions::InvalidParameterHandler);
	_set_se_translator(Exceptions::StructuredExceptionTranslator);
	SetUnhandledExceptionFilter(&Exceptions::UnhandledExceptionFilter);

	try
	{
		LoggingOps::StdOutLog("%08X Starting program...", GetCurrentThreadId());

		Locks::SpinLock sl(true, true, 100);
		{ttest t(sl);
		if(t.StartThread()) {
			for(int i = 0; i < 50; ++i) {
				{auto l = Locks::Acquire(sl);
				t.Queue(i);
				Sleep(2500);}
				Sleep(250);
			}
			if(t.WaitStopThread()) LoggingOps::StdOutLog("%08X Thread stopped OK", GetCurrentThreadId());
			else LoggingOps::StdOutLog("%08X Thread stop failed", GetCurrentThreadId());
		}
		else LoggingOps::StdOutLog("%08X Thread start failed", GetCurrentThreadId());}
		LoggingOps::StdOutLog("%08X Exiting program...", GetCurrentThreadId());
	}
	catch(const std::runtime_error& re) {
		LoggingOps::StdOutLog("Caught runtime error:%s", Exceptions::UnrollExceptionString(re).c_str());
	}
	catch(const std::invalid_argument& ia) {
		LoggingOps::StdOutLog("Caught invalid argument:%s", Exceptions::UnrollExceptionString(ia).c_str());
	}
	catch(const std::exception& e) {
		LoggingOps::StdOutLog("Caught exception:%s", Exceptions::UnrollExceptionString(e).c_str());
	}
}
