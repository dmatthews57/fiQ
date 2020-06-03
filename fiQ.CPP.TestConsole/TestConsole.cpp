#include "pch.h"
#include "Logging/LogSink.h"
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

class testrec : public CommsClient {
public:
	void IBConnect() noexcept(false) override {printf("IBConnect\n");}
	void IBData() noexcept(false) override {printf("IBData\n");}
	void IBDisconnect() noexcept(false) override {printf("IBDisconnect\n");}
	Comms::ListenerTicket listen() {
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
		c->SetLocal(8000).ReadConfig(Tokenizer::CreateCopy<10>("EXTHEADER|RAW|TEST1=VALUE1..................|TEST2.......................=VALUE2|BLAH||TLSCERT=MY(notthere)", "|"));
		std::string lasterr;
		auto t = Comms::RegisterListener(shared_from_this(), c, &lasterr);
		if(t == 0) printf("Registration failed [%s]\n", lasterr.c_str());
		return t;
	}
	Comms::SessionTicket call() {
		auto c = std::make_shared<Connection>();
		c->SetRemote("127.0.0.1:8000").SetFlagOn(CommFlags::ExtendedHeader);
		return Comms::RequestConnect(shared_from_this(), c);
	}
	testrec() : CommsClient(name) {}
	~testrec() {printf("TestRec destr\n");}
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
		SocketOps::InitializeSockets(true);
		Comms::Initialize();

		{std::shared_ptr<testrec> tr = std::make_shared<testrec>();
		const auto lticket = tr->listen();
		printf("Listener ticket: %llu\n", lticket);
		const auto lticket2 = tr->listen();
		printf("Listener ticket: %llu\n", lticket);
		const auto sticket = tr->call();
		printf("Session ticket: %llu\n", sticket);
		}

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
