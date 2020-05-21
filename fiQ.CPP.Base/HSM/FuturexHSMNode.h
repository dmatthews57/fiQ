#pragma once
//==========================================================================================================================
// FuturexHSMNode.h : HSMNode class implementing Futurex HSM messaging
//==========================================================================================================================

#include "HSM/HSMNode.h"
#include "Messages/HSMRequest.h"

namespace FIQCPPBASE {

class FuturexHSMNode : public HSMNode {
public:

	//======================================================================================================================
	// HSMNode function implementations
	_Check_return_ bool Init() noexcept(false) override;
	_Check_return_ bool Cleanup() noexcept(false) override;
	// MessageNode function implementations
	_Check_return_ bool ProcessRequest(const std::shared_ptr<RoutableMessage>& rm) noexcept(false) override;
	_Check_return_ bool ProcessResponse(const std::shared_ptr<RoutableMessage>&) noexcept(false) override;

	//======================================================================================================================
	// Public constructor (locked by base class pass_key), destructor
	FuturexHSMNode(pass_key) noexcept : HSMNode(HSMType::Futurex) {}
	~FuturexHSMNode() = default;

	// Deleted copy/move constructors and assignment operators
	FuturexHSMNode(const FuturexHSMNode&) = delete;
	FuturexHSMNode(FuturexHSMNode&&) = delete;
	FuturexHSMNode& operator=(const FuturexHSMNode&) = delete;
	FuturexHSMNode& operator=(FuturexHSMNode&&) = delete;

private:

	//======================================================================================================================
	void GenerateKey(HSMRequest& h);
	void TranslateKey(HSMRequest& h);
	void TranslatePIN(HSMRequest& h);
};

}; // (end namespace FIQCPPBASE)
