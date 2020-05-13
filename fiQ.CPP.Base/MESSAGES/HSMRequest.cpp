//==========================================================================================================================
// HSMRequest.h : Class for run-time management of an encryption request to be sent to an HSM
//==========================================================================================================================
#include "pch.h"
#include "HSMRequest.h"
using namespace FIQCPPBASE;

//==========================================================================================================================
const HSMRequest::Field& HSMRequest::GetInvalidField() {
	static const Field f { FieldName::Invalid, nullptr, 0ULL };
	return f;
}