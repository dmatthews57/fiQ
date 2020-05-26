#pragma once
//==========================================================================================================================
// HSMNode.h : Base class for an HSM processing object, capable of receiving and processing HSMRequest messages
//==========================================================================================================================

#include "Messages/MessageNode.h"

namespace FIQCPPBASE {

class HSMNode : public MessageNode {
public:

	//======================================================================================================================
	// Type definitions - HSM types (used as MessageNode::subtype value):
	enum class HSMType : Subtype {
		Futurex = 1
	};

	//======================================================================================================================
	// Named constructor - Create HSMNode of the specified type
	_Check_return_ static std::shared_ptr<HSMNode> Create(const std::string& _name, HSMType _type);

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
	HSMNode(const std::string& _name, HSMType _hsmtype)
		: MessageNode(_name, Type::HSM, static_cast<std::underlying_type_t<HSMType>>(_hsmtype)) {}

};

}; // (end namespace FIQCPPBASE)
