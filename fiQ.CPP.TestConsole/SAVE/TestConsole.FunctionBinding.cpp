#include "pch.h"
#include "Tools/Exceptions.h"
#include "Tools/LoggingOps.h"
using namespace FIQCPPBASE;

#include "Tools/TimerOps.h"

class test {
public:

	//template<typename... T>
	//using bind_type = decltype(std::bind(std::declval<std::function<void(T...)>>(), std::declval<T>()...));

	void DoSomethingII(int x, int y) {printf("Member %d\n", x + y);}
	void DoSomethingV() {printf("Member void\n");}
	void DoSomethingP(const std::shared_ptr<int>& i) {printf("Member ptr %d\n", *i);}
	void DoSomethingBad() {
		a *= 2;
		b *= 2;
		const int* xa = &a;
		const int* xb = &b;
		printf("Member bad at %p %d:%d\n", this, *xa, *xb);
	}
	auto DoBind1() {return std::bind(&test::DoSomethingII, this, 1, 2);}
	auto DoBind2() {return std::bind(&test::DoSomethingV, this);}
	auto DoBind3() {return std::bind(&test::DoSomethingP, this, std::make_shared<int>(7));}
	auto DoBindBad() {return std::bind(&test::DoSomethingBad, this);}

	test() {printf("Test constructed at %p\n", this);}
	//test() : add(Action<int,int>([this](int x, int y) { printf("Class lambda %d\n", x + y); }, this, a, b)) {printf("Test constructor\n");}
	//test(const test& t) : add(t.add) {printf("Test copy constructor\n");}
	//test(test&& t) : add(std::move(t.add)) {printf("Test move constructor\n");}
	//test& operator=(const test& t) {add = t.add; printf("Test copy assignment operator");}
	//test& operator=(test&& t) {add = std::move(t.add); printf("Test move assignment operator");}
	~test() {printf("Test destructed at %p\n", this);}

private:
	int a = 1;
	int b = 2;
};



int main()
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF|_CRTDBG_LEAK_CHECK_DF);
#endif
	_set_invalid_parameter_handler(Exceptions::InvalidParameterHandler);
	_set_se_translator(Exceptions::StructuredExceptionTranslator);
	SetUnhandledExceptionFilter(&Exceptions::UnhandledExceptionFilter);

	try
	{
		TimerHandle::InitializeTimers();

		using fvec = std::vector<std::function<void()>>;
		fvec fv;
		{
			auto t = std::make_unique<test>();
			fv.emplace_back(t->DoBind1());
			fv.emplace_back(t->DoBind2());
			fv.emplace_back(t->DoBind3());
			//fv.emplace_back(t->DoBindBad());
			fv.emplace_back(std::function<void()>());
		}
		for(fvec::const_iterator seek = fv.begin(); seek != fv.end(); ++seek) {
			if(*seek) {
				printf("%s\n", typeid(*seek).name());
				seek->operator()();
			}
			else printf("Bad function\n");
		}

		TimerHandle::CleanupTimers();
		return 0;
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
