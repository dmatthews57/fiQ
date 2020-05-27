//==========================================================================================================================
// HSMRequest.cpp : Class for run-time management of an encryption request to be sent to an HSM
//==========================================================================================================================
#include "pch.h"
#include "HSMRequest.h"
using namespace FIQCPPBASE;

//==========================================================================================================================
const std::string& HSMRequest::EMPTYSTR() noexcept {
	static const std::string emptystr;
	return emptystr;
}