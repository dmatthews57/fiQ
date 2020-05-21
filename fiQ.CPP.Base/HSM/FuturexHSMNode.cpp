//==========================================================================================================================
// FuturexHSMNode.cpp : HSMNode class implementing Futurex HSM messaging
//==========================================================================================================================
#include "pch.h"
#include "FuturexHSMNode.h"
#include "MESSAGES/HSMRequest.h"
using namespace FIQCPPBASE;


//==========================================================================================================================
_Check_return_ bool FuturexHSMNode::Init() noexcept(false) {
	return true;
}
_Check_return_ bool FuturexHSMNode::Cleanup() noexcept(false) {
	return true;
}

//==========================================================================================================================
_Check_return_ bool FuturexHSMNode::ProcessRequest(const std::shared_ptr<RoutableMessage>& rm) noexcept(false) {
	// If message is an HSM request, attempt to retrieve it by type (return failure if not retrieved)
	HSMRequest* h = (rm->GetType() == RoutableMessage::Type::HSMRequest) ? rm->GetAs<HSMRequest>() : nullptr;
	if(h == nullptr) return false;

	// Call utility function based on requested operation:
	switch(h->GetOperation()) {
	case HSMRequest::Operation::GenerateKey:
		GenerateKey(*h);
		break;
	case HSMRequest::Operation::TranslateKey:
		TranslateKey(*h);
		break;
	case HSMRequest::Operation::TranslatePIN:
		TranslatePIN(*h);
		break;
	default:
		h->SetResponse(HSMRequest::Result::NotSupported);
		break;
	};
	return true;
}
_Check_return_ bool FuturexHSMNode::ProcessResponse(const std::shared_ptr<RoutableMessage>&) noexcept(false) {
	return false;
}


//==========================================================================================================================
void FuturexHSMNode::GenerateKey(HSMRequest& h) {
	printf("GENERATE KEY - %zu FIELDS:\n", h.GetRequestFields().size());
	for(auto seek = h.GetRequestFields().cbegin(); seek != h.GetRequestFields().cend(); ++seek) {
		printf(" %d %zu %.*s\n",
			HSMRequest::GetFieldName(*seek),
			HSMRequest::GetFieldLength(*seek),
			gsl::narrow_cast<int>(HSMRequest::GetFieldLength(*seek)),
			HSMRequest::GetFieldValue(*seek)
		);
	}
	h.SetResponse(
		HSMRequest::Result::OK,
		HSMRequest::ResponseFieldSet {
			{HSMRequest::FieldName::KeyOutKEK, "KEYOUTKEK"},
			{HSMRequest::FieldName::KeyOutMFK, "KEYOUTMFK"}
		}
	);
}
void FuturexHSMNode::TranslateKey(HSMRequest& h) {
	printf("TRANSLATE KEY - %zu FIELDS:\n", h.GetRequestFields().size());
	for(auto seek = h.GetRequestFields().cbegin(); seek != h.GetRequestFields().cend(); ++seek) {
		printf(" %d %zu %.*s\n",
			HSMRequest::GetFieldName(*seek),
			HSMRequest::GetFieldLength(*seek),
			gsl::narrow_cast<int>(HSMRequest::GetFieldLength(*seek)),
			HSMRequest::GetFieldValue(*seek)
		);
	}
	h.SetResponse(
		HSMRequest::Result::OK,
		HSMRequest::ResponseFieldSet {
			{HSMRequest::FieldName::KeyOutKEK, "KEYOUTKEK"},
			{HSMRequest::FieldName::KeyOutMFK, "KEYOUTMFK"}
		}
	);
}
void FuturexHSMNode::TranslatePIN(HSMRequest& h) {
	printf("TRANSLATE PIN - %zu FIELDS:\n", h.GetRequestFields().size());
	for(auto seek = h.GetRequestFields().cbegin(); seek != h.GetRequestFields().cend(); ++seek) {
		printf(" %d %zu %.*s\n",
			HSMRequest::GetFieldName(*seek),
			HSMRequest::GetFieldLength(*seek),
			gsl::narrow_cast<int>(HSMRequest::GetFieldLength(*seek)),
			HSMRequest::GetFieldValue(*seek)
		);
	}
	h.SetResponse(
		HSMRequest::Result::OK,
		HSMRequest::ResponseFieldSet {
			{HSMRequest::FieldName::KeyOutKEK, "KEYOUTKEK"},
			{HSMRequest::FieldName::KeyOutMFK, "KEYOUTMFK"}
		}
	);
}
