#pragma once
//==========================================================================================================================
// HSMNode.h : Base class for an HSM processing object, capable of receiving and processing HSMRequest messages
//==========================================================================================================================

#include "Messages/MessageNode.h"

namespace FIQCPPBASE {

class HSMNode : public MessageNode {
protected: struct pass_key {}; // Private function pass-key definition
public:

	//======================================================================================================================
	// Type definitions - HSM types (used as MessageNode::subtype value):
	enum class HSMType : Subtype {
		Futurex = 1
	};

	//======================================================================================================================
	// Named constructor - Create HSMNode of the specified type
	_Check_return_ static std::shared_ptr<HSMNode> Create(HSMType _type);

	//======================================================================================================================
	// Pure virtual function definitions for child classes:
	_Check_return_ virtual bool Init() noexcept(false) = 0;
	_Check_return_ virtual bool Cleanup() noexcept(false) = 0;

	//======================================================================================================================
	// Default destructor (implicitly virtual; public to allow destruction via base class pointer)
	~HSMNode() = default;

	// Deleted copy/move constructors and assignment operators
	HSMNode(const HSMNode&) = delete;
	HSMNode(HSMNode&&) = delete;
	HSMNode& operator=(const HSMNode&) = delete;
	HSMNode& operator=(HSMNode&&) = delete;

protected:

	// Protected default constructor (HSMNodes can be created via child class only)
	HSMNode(HSMType _hsmtype) noexcept : MessageNode(Type::HSM, static_cast<std::underlying_type_t<HSMType>>(_hsmtype)) {}

};

}; // (end namespace FIQCPPBASE)
