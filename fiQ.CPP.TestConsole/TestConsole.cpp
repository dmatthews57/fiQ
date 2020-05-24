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
#include "HSM/FuturexHSMNode.h"

int main()
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF|_CRTDBG_LEAK_CHECK_DF);
#endif
	_set_invalid_parameter_handler(Exceptions::InvalidParameterHandler);
	_set_se_translator(Exceptions::StructuredExceptionTranslator);
	SetUnhandledExceptionFilter(&Exceptions::UnhandledExceptionFilter);

	try {
		std::shared_ptr<MessageNode> hsm = HSMNode::Create(HSMNode::HSMType::Futurex);

		{std::shared_ptr<HSMRequest> h = HSMRequest::Create(
			HSMRequest::Operation::GenerateKey,
			HSMRequest::RequestFieldSet {
				{HSMRequest::FieldName::MFKMod, HSMLITERAL("D")},
				{HSMRequest::FieldName::KEK, HSMLITERAL("11223344556677889900AABBCCDDEEFF")},
				{HSMRequest::FieldName::KEKMod, HSMLITERAL("0")}
			}
		);
		if(hsm->ProcessRequest(h) == MessageNode::RouteResult::Processed) {
			printf("\nHSM RESPONSE (%d):\n", h->GetResult());
			{auto f = h->GetResponseField(HSMRequest::FieldName::KeyOutKEK);
			printf("KEYOUTKEK: [%s]\n", f.c_str());}
			{auto f = h->GetResponseField(HSMRequest::FieldName::KeyOutMFK);
			printf("KEYOUTMFK: [%s]\n", f.c_str());}
			{auto f = h->GetResponseField(HSMRequest::FieldName::KCVOut);
			printf("KCFOut: [%s]\n", f.c_str());}

		}
		else printf("Generate key processing failed\n");}

		{std::shared_ptr<HSMRequest> h = HSMRequest::Create(
			HSMRequest::Operation::TranslateKey,
			HSMRequest::RequestFieldSet {
				{HSMRequest::FieldName::MFKMod, HSMLITERAL("D")},
				{HSMRequest::FieldName::KeyIn, HSMLITERAL("88888888888888888888888888888888")},
				{HSMRequest::FieldName::KEK, HSMLITERAL("11223344556677889900AABBCCDDEEFF")},
				{HSMRequest::FieldName::KEKMod, HSMLITERAL("D")}
			}
		);
		if(hsm->ProcessRequest(h) == MessageNode::RouteResult::Processed) {
			printf("\nHSM RESPONSE (%d):\n", h->GetResult());
			{auto f = h->GetResponseField(HSMRequest::FieldName::KeyOutMFK);
			printf("KEYOUTMFK: [%s]\n", f.c_str());}
			{auto f = h->GetResponseField(HSMRequest::FieldName::KCVOut);
			printf("KCFOut: [%s]\n", f.c_str());}

		}
		else printf("Translate key processing failed\n");}

		{std::shared_ptr<HSMRequest> h = HSMRequest::Create(
			HSMRequest::Operation::TranslatePIN,
			HSMRequest::RequestFieldSet {
				{HSMRequest::FieldName::PEKSrc, HSMLITERAL("88888888888888888888888888888888")},
				{HSMRequest::FieldName::PEKDst, HSMLITERAL("88888888888888888888888888888888")},
				{HSMRequest::FieldName::PINIn, HSMLITERAL("0123456789ABCDEF")},
				{HSMRequest::FieldName::PAN, HSMLITERAL("4219730010000001")}
			}
		);
		if(hsm->ProcessRequest(h) == MessageNode::RouteResult::Processed) {
			printf("\nHSM RESPONSE (%d):\n", h->GetResult());
			{auto f = h->GetResponseField(HSMRequest::FieldName::PINOut);
			printf("PINOut: [%s]\n", f.c_str());}
		}
		else printf("Translate PIN processing failed\n");}
		return 0;
	}
	catch(const std::runtime_error& e) {
		const auto exceptioncontext = Exceptions::UnrollException(e);
		LOG_FROM_TEMPLATE_CONTEXT(LogLevel::Error, &exceptioncontext, "Caught runtime error");
		//printf("%s\n", Exceptions::UnrollExceptionString(e).c_str());
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
