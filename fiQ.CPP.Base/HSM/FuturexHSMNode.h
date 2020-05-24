#pragma once
//==========================================================================================================================
// FuturexHSMNode.h : HSMNode class implementing Futurex HSM messaging
//==========================================================================================================================

#include "HSM/HSMNode.h"
#include "Messages/HSMRequest.h"
#include "Tools/ThreadOps.h"
#include "Tools/Tokenizer.h"

namespace FIQCPPBASE {

class FuturexHSMNode : public HSMNode {
public:

	//======================================================================================================================
	// HSMNode function implementations
	_Check_return_ bool Init() noexcept(false) override;
	_Check_return_ bool Cleanup() noexcept(false) override;
	// MessageNode function implementations
	_Check_return_ RouteResult ProcessRequest(const std::shared_ptr<RoutableMessage>& rm) noexcept(false) override;
	_Check_return_ RouteResult ProcessResponse(const std::shared_ptr<RoutableMessage>&) noexcept(false) override;

	//======================================================================================================================
	// Public constructor (locked by base class pass_key), destructor
	FuturexHSMNode(pass_key) noexcept : HSMNode(HSMType::Futurex), EchoLock(true), EchoCount(0) {}
	~FuturexHSMNode() = default;

	// Deleted copy/move constructors and assignment operators
	FuturexHSMNode(const FuturexHSMNode&) = delete;
	FuturexHSMNode(FuturexHSMNode&&) = delete;
	FuturexHSMNode& operator=(const FuturexHSMNode&) = delete;
	FuturexHSMNode& operator=(FuturexHSMNode&&) = delete;

private:

	//======================================================================================================================
	// Encryption operation utility functions
	_Check_return_ RouteResult GenerateKey(HSMRequest& h);
	_Check_return_ RouteResult TranslateKey(HSMRequest& h);
	_Check_return_ RouteResult TranslatePIN(HSMRequest& h);

	//======================================================================================================================
	// Internal utility functions
	_Check_return_ Tokenizer ExecRequest(const std::string& Request);

	//======================================================================================================================
	// Private members
	Locks::SpinLock EchoLock;	// Spin lock for access to EchoCount member
	short EchoCount;			// Counter to use when constructing HSM requests
};

}; // (end namespace FIQCPPBASE)
