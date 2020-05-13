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

#include "Messages/HSMRequest.h"

int main()
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF|_CRTDBG_LEAK_CHECK_DF);
#endif
	_set_invalid_parameter_handler(Exceptions::InvalidParameterHandler);
	_set_se_translator(Exceptions::StructuredExceptionTranslator);
	SetUnhandledExceptionFilter(&Exceptions::UnhandledExceptionFilter);

	try {
		const char temp[] = "ABCD";
		std::shared_ptr<RoutableMessage> rm = HSMRequest::Create(
			HSMRequest::Operation::GenerateKey,
			HSMRequest::FieldSet {
				{HSMRequest::FieldName::KEK, temp, strlen(temp)},
				{HSMRequest::FieldName::KEK, HSMLITERAL("HELLO")}
			}
		);
		printf("%d %d\n", rm->GetType(), rm->GetSubtype());
		HSMRequest* h = rm->GetAs<HSMRequest>();
		if(h) {
			printf("CHR %zu\n", h->GetRequestFields().size());

			{auto f = HSMRequest::GetField(HSMRequest::FieldName::KEK, h->GetRequestFields());
			printf("KEK: %d %zu %.*s\n", HSMRequest::GetFieldName(f), HSMRequest::GetFieldLength(f), (int)HSMRequest::GetFieldLength(f), HSMRequest::GetFieldValue(f));}
			{auto f = HSMRequest::GetField(HSMRequest::FieldName::KCVIn, h->GetRequestFields());
			printf("KCVIn: %d %zu %.*s\n", HSMRequest::GetFieldName(f), HSMRequest::GetFieldLength(f), (int)HSMRequest::GetFieldLength(f), HSMRequest::GetFieldValue(f));}

			for(auto seek = h->GetRequestFields().cbegin(); seek != h->GetRequestFields().cend(); ++seek) {
				printf("REQLOOP: %d %zu %.*s\n", HSMRequest::GetFieldName(*seek), HSMRequest::GetFieldLength(*seek), (int)HSMRequest::GetFieldLength(*seek), HSMRequest::GetFieldValue(*seek));
			}

			h->SetResponseFields(HSMRequest::FieldCopySet {
				{HSMRequest::FieldName::KeyOutKEK, "KEYOUTKEK"},
				{HSMRequest::FieldName::KeyOutMFK, "KEYOUTMFK"}
			});

			for(auto seek = h->GetResponseFields().cbegin(); seek != h->GetResponseFields().cend(); ++seek) {
				printf("RESPLOOP: %d %zu %.*s\n", HSMRequest::GetFieldName(*seek), HSMRequest::GetFieldLength(*seek), (int)HSMRequest::GetFieldLength(*seek), HSMRequest::GetFieldValue(*seek));
			}
		}
		return 0;
	}
	catch(const std::runtime_error& e) {
		const auto exceptioncontext = Exceptions::UnrollException(e);
		LOG_FROM_TEMPLATE_CONTEXT(LogLevel::Error, &exceptioncontext, "Caught runtime error");
		printf("%s\n", Exceptions::UnrollExceptionString(e).c_str());
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
