#include "pch.h"
#include "Tools/Exceptions.h"
using namespace FIQCPPBASE;

#include "Tools/ThreadOps.h"

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

class ttest : private ThreadOperator<std::string> {
public:
	bool StartThread() {
		return ThreadStart();
	}
	void Queue(int i) {
		static const char buf[] = "abcdefghijklmop";
		if(i < 10) ThreadQueueWork(buf, i);
		else if(i < 20) ThreadQueueWork(std::make_unique<std::string>(std::to_string(i)));
		else {
			auto a = std::make_unique<std::string>(std::to_string(i));
			ThreadQueueWork(std::move(a));
		}
	}
	bool WaitStopThread() {
		return ThreadWaitStop(5000);
	}
	ttest() {printf("%08X ttest constructed\n", GetCurrentThreadId());}
	~ttest() {printf("%08X ttest destructed\n", GetCurrentThreadId());}

private:
	unsigned int ThreadExecute() override {
		printf("%08X Starting thread...\n", GetCurrentThreadId());
		int i = 2, j = 0;
		while(ThreadShouldRun()) {
			if(ThreadWaitEvent()) {
				std::unique_ptr<std::string> work;
				while(ThreadDequeueWork(work)) {
					printf("%08X Dequeued work: %s\n", GetCurrentThreadId(), work->c_str());
					if(++j == i) {
						printf("%08X Re-queueing value: %s\n", GetCurrentThreadId(), work->c_str());
						ThreadRequeueWork(std::move(work));
						i *= 2;
					}
				}
			}
		}
		//throw std::runtime_error("OOPS");
		printf("%08X Exiting thread...\n", GetCurrentThreadId());
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
		printf("%08X Starting program...\n", GetCurrentThreadId());

		{ttest t;
		if(t.StartThread()) {
			for(int i = 0; i < 50; ++i) {
				t.Queue(i);
				Sleep(250);
			}
			if(t.WaitStopThread()) printf("%08X Thread stopped OK\n", GetCurrentThreadId());
			else printf("%08X Thread stop failed\n", GetCurrentThreadId());
		}
		else printf("%08X Thread start failed\n", GetCurrentThreadId());}
		printf("%08X Exiting program...\n", GetCurrentThreadId());
	}
	catch(const std::runtime_error& re) {
		printf("Caught runtime error:%s\n", Exceptions::UnrollExceptionString(re).c_str());
	}
	catch(const std::invalid_argument& ia) {
		printf("Caught invalid argument:%s\n", Exceptions::UnrollExceptionString(ia).c_str());
	}
	catch(const std::exception& e) {
		printf("Caught exception:%s\n", Exceptions::UnrollExceptionString(e).c_str());
	}
}
