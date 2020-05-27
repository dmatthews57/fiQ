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
	Comms::ListenerTicket reg() {
		return Comms::RegisterListener(shared_from_this(), Connection{});
	}
	testrec() = default;
	~testrec() {printf("TestRec destr\n");}
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
		Comms::Initialize();

		{std::shared_ptr<CommsClient> tr = std::make_shared<testrec>();
		/*auto ticket = tr->reg();
		printf("Ticket: %llu\n", ticket);*/
		}

		Comms::Cleanup();
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
