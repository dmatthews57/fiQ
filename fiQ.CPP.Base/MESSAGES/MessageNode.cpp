//==========================================================================================================================
// MessageNode.cpp : Base class for an object capable of generating or receiving and processing RoutableMessage objects
//==========================================================================================================================
#include "pch.h"
#include "MessageNode.h"
#include "HSM/HSMNode.h"
using namespace FIQCPPBASE;

//==========================================================================================================================
// MessageNode::Create: Named constructor to build MessageNode of specified type and subtype
_Check_return_ std::shared_ptr<MessageNode> MessageNode::Create(const std::string& _name, Type _type, Subtype _subtype) {
	switch(_type) {
	case Type::HSM:
		// Ensure underlying type of HSMType is Subtype to ensure safe cast:
		static_assert(std::is_same_v<std::underlying_type_t<HSMNode::HSMType>, Subtype>,
			"Underlying type of HSMNode::HSMType must be MessageNode::Subtype");
		// Pass request to HSMNode intermediate class:
		return HSMNode::Create(_name, static_cast<HSMNode::HSMType>(_subtype));
	default: // Unrecognized type, ignore
		return nullptr;
	};
}
