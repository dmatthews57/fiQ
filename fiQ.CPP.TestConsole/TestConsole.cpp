#include "pch.h"
#include "Logging/ConsoleSink.h"
#include "Tools/Exceptions.h"
using namespace FIQCPPBASE;

void Innermost() noexcept(false) {
	try {
		throw std::runtime_error("first domino");
	}
	catch(const std::exception&) {std::throw_with_nested(FORMAT_RUNTIME_ERROR("INNERMOSTMSG"));}
}
void Outermost() {
	try {
		Innermost();
	}
	catch(const std::exception&) {std::throw_with_nested(FORMAT_RUNTIME_ERROR("OUTERMOSTMSG"));}
}

#include "Comms/Comms.h"

std::timed_mutex tmut;

class testrec : public CommsClient, ThreadOperator<int> {
public:
	void IBConnect() noexcept(false) override {printf("IBConnect\n");}
	void IBData() noexcept(false) override {printf("IBData\n");}
	void IBDisconnect() noexcept(false) override {printf("IBDisconnect\n");}
	Comms::ListenerTicket listen(unsigned short port) {
		//printf("[%s]\n", c.GetConfigParm("test1").c_str());
		//printf("[%s]\n", c.GetConfigParm("Test2.......................").c_str());
		//printf("[%s]\n", c.GetConfigParm("NOTHERE").c_str());
		//c.SetRemote("192.168.0.1", 8000);
		//c.SetRemote("172.16.0.1:1234");
		//Connection::ConfigParms parms{{"TEST1","VAL1"},{"TEST2","VAL2"}};
		//c.SetConfigParms(std::move(parms));
		//c.SetConfigParms(Connection::ConfigParms{{"TEST1","VAL1"},{"TEST2","VAL2"}});
		//c.AddConfigParm(std::string("TEST1....................."), std::string("TEST2....................."));
		auto c = std::make_shared<Connection>();
		c->SetLocal(port).ReadConfig(Tokenizer::CreateCopy<10>("EXTHEADER|RAW|TEST1=VALUE1..................|TEST2.......................=VALUE2|BLAH||TLSCERT=MY(localhost)", "|"));
		std::string lasterr;
		auto t = Comms::RegisterListener(shared_from_this(), c, &lasterr);
		if(t == 0) printf("Registration failed [%s]\n", lasterr.c_str());
		return t;
	}
	Comms::SessionTicket call() {
		auto c = std::make_shared<Connection>();
		c->SetRemote("127.0.0.1:8000").SetFlagOn(CommFlags::ExtendedHeader);
		std::string lasterr;
		auto t = Comms::RequestConnect(shared_from_this(), c, &lasterr);
		if(t == 0) printf("Registration failed [%s]\n", lasterr.c_str());
		return t;
	}
	testrec() : CommsClient(name) {}
	~testrec() noexcept(false) {printf("TestRec destr\n");}

	void StartThread() {ThreadStart();}
	void StopThread() {ThreadWaitStop(INFINITE);}

	unsigned int ThreadExecute() override {
		printf("Thread running\n");
		const SteadyClock EndTime(std::chrono::milliseconds{500});
		std::unique_lock<std::timed_mutex> lock(tmut, EndTime.GetTimePoint());
		printf("Thread lock: %d\n", lock.owns_lock());
		lock.lock();
		printf("Thread lock: %d\n", lock.owns_lock());
		Sleep(2000);
		printf("Thread stopping\n");
		return 0;
	}

private:
	const std::string name = "TESTREC";
};

int main()
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF|_CRTDBG_LEAK_CHECK_DF);
#endif
	_set_invalid_parameter_handler(Exceptions::InvalidParameterHandler);
	_set_se_translator(Exceptions::StructuredExceptionTranslator);
	SetUnhandledExceptionFilter(&Exceptions::UnhandledExceptionFilter);

	try {
		LogSink::AddSink<ConsoleSink>(LogLevel::Debug, ConsoleSink::Config { });
		LogSink::InitializeSinks();
		SocketOps::InitializeSockets(true);
		Comms::Initialize();

		std::unique_lock<std::timed_mutex> lock(tmut);
		printf("Main lock: %d\n", lock.owns_lock());

		{std::shared_ptr<testrec> tr = std::make_shared<testrec>();
		tr->StartThread();

		const auto lticket = tr->listen(8000);
		const auto lticket2 = tr->listen(8001);
		const auto sticket = tr->call();

		Comms::DeregisterListener(lticket, 500);
		Comms::DeregisterListener(lticket2, 0);
		Comms::Disconnect(sticket);
		lock.unlock();
		tr->StopThread();}
		printf("Main lock: %d\n", lock.owns_lock());
		lock.lock();
		printf("Main lock: %d\n", lock.owns_lock());

		Comms::Cleanup();
		SocketOps::CleanupSockets();
		return 0;
	}
	catch(const std::runtime_error& e) {
		const auto exceptioncontext = Exceptions::UnrollException(e);
		LOG_FROM_TEMPLATE_CONTEXT(LogLevel::Error, &exceptioncontext, "Caught runtime error");
	}
	catch(const std::invalid_argument& e) {
		const auto exceptioncontext = Exceptions::UnrollException(e);
		LOG_FROM_TEMPLATE_CONTEXT(LogLevel::Error, &exceptioncontext, "Caught invalid argument");
	}
	catch(const std::exception& e) {
		const auto exceptioncontext = Exceptions::UnrollException(e);
		LOG_FROM_TEMPLATE_CONTEXT(LogLevel::Error, &exceptioncontext, "Caught exception");
	}
}
