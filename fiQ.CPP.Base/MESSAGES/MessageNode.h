#pragma once
//==========================================================================================================================
// MessageNode.h : Base class for an object capable of generating or receiving and processing RoutableMessage objects
//==========================================================================================================================

namespace FIQCPPBASE {

class RoutableMessage;

class MessageNode {
public:

	//======================================================================================================================
	// Type definitions
	enum class Type : int { HSM = 1 };
	using Subtype = int; // Provide alias for child classes to define their own subtypes
	enum class RouteResult : int {
		Pending = 0,		// Request has been routed, processing now pending
		Unprocessed = 1,	// Request was not processed, and can be rerouted
		Processed = 2		// Request was processed
	};

	//======================================================================================================================
	// Pure virtual function definitions for child classes:
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
	MessageNode(Type _type, Subtype _subtype) noexcept : type(_type), subtype(_subtype) {}

private:

	//======================================================================================================================
	// Private const members (set at construction)
	const Type type;
	const Subtype subtype;
};

}; // (end namespace FIQCPPBASE)
