//==========================================================================================================================
// HSMNode.cpp : Base class for an HSM processing object, capable of receiving and processing HSMRequest messages
//==========================================================================================================================
#include "pch.h"
#include "HSMNode.h"
// Child class includes
#include "FuturexHSMNode.h"
using namespace FIQCPPBASE;

//==========================================================================================================================
// HSMNode::Create: Named constructor to build child class object
_Check_return_ std::shared_ptr<HSMNode> HSMNode::Create(const std::string& _name, HSMType _type) {
	switch(_type) {
	case HSMType::Futurex: return std::make_shared<FuturexHSMNode>(pass_key{}, _name);
	default: return nullptr;
	};
}
