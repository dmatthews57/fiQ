#pragma once
//==========================================================================================================================
// MessageNode.h : Base class for an object capable of generating or receiving and processing RoutableMessage objects
//==========================================================================================================================

namespace FIQCPPBASE {

class RoutableMessage;

class MessageNode {
protected: struct pass_key {}; // Private function pass-key definition
public:

	//======================================================================================================================
	// Type definitions
	enum class Type : int { HSM = 1 };
	using Subtype = int; // Provide alias for child classes to define their own subtypes
	enum class RouteResult : int {
		Pending = 0,		// Request has been routed, processing now pending (originator ProcessResponse will be called)
		Unprocessed = 1,	// Request was not processed - can be rerouted to another node
		Processed = 2		// Request was processed inline (originator ProcessResponse will not be called)
	};

	//======================================================================================================================
	// Named constructor - Create MessageNode of the specified type/subtype
	_Check_return_ static std::shared_ptr<MessageNode> Create(const std::string& _name, Type _type, Subtype _subtype);
	// Named constructor - Overload receiving underlying type of Type (provided for caller convenience)
	template<typename T, std::enable_if_t<std::is_same_v<T, std::underlying_type_t<Type>>, int> = 0>
	_Check_return_ static std::shared_ptr<MessageNode> Create(const std::string& _name, T _type, Subtype _subtype);

	//======================================================================================================================
	// Public accessor functions
	const std::string& GetName() const noexcept {return name;}

	//======================================================================================================================
	// Pure virtual function definitions for child classes:
	_Check_return_ virtual bool Init() noexcept(false) = 0;
	_Check_return_ virtual bool Cleanup() noexcept(false) = 0;
	_Check_return_ virtual RouteResult ProcessRequest(const std::shared_ptr<RoutableMessage>& rm) noexcept(false) = 0;
	_Check_return_ virtual RouteResult ProcessResponse(const std::shared_ptr<RoutableMessage>& rm) noexcept(false) = 0;

	//======================================================================================================================
	// Virtual default destructor (public to allow destruction via base class pointer)
	virtual ~MessageNode() = default;

	//======================================================================================================================
	// Deleted copy/move constructors and assignment operators
	MessageNode(const MessageNode&) = delete;
	MessageNode(MessageNode&&) = delete;
	MessageNode& operator=(const MessageNode&) = delete;
	MessageNode& operator=(MessageNode&&) = delete;

protected:

	//======================================================================================================================
	// Protected default constructor (MessageNodes can be created via child class only)
	MessageNode(const std::string& _name, Type _type, Subtype _subtype) : name(_name), type(_type), subtype(_subtype) {}

private:

	//======================================================================================================================
	// Private const members (set at construction)
	const std::string name;
	const Type type;
	const Subtype subtype;
};

//==========================================================================================================================
// MessageNode::Create: Named constructor overload receiving underlying_type<Type> (performs cast on behalf of caller)
template<typename T, std::enable_if_t<std::is_same_v<T, std::underlying_type_t<MessageNode::Type>>, int>>
_Check_return_ inline std::shared_ptr<MessageNode> MessageNode::Create(
	const std::string& _name, T _type, Subtype _subtype) {
	return MessageNode::Create(_name, static_cast<Type>(_type), _subtype);
}

}; // (end namespace FIQCPPBASE)
