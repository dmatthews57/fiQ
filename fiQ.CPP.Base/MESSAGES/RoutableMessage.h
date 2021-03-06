#pragma once
//==========================================================================================================================
// RoutableMessage.h : Base class for messaging object which can be routed through system
//==========================================================================================================================

namespace FIQCPPBASE {

class RoutableMessage {
public:

	//======================================================================================================================
	// Type definitions
	enum class Type : int { Invalid = 0, Transaction = 1, BMPMessage = 2, HSMRequest = 3, DBRequest = 4 };
	using Subtype = int; // Provide alias for child classes to define their own subtypes

	//======================================================================================================================
	// Public accessors
	_Check_return_ Type GetType() const noexcept {return type;}
	_Check_return_ Subtype GetSubtype() const noexcept {return subtype;}
	template<typename T, std::enable_if_t<std::is_base_of_v<RoutableMessage,T>, int> = 0>
	_Check_return_ const T* GetAs() const noexcept(false) {return dynamic_cast<const T*>(this);}
	template<typename T, std::enable_if_t<std::is_base_of_v<RoutableMessage,T>, int> = 0>
	_Check_return_ T* GetAs() noexcept(false) {return dynamic_cast<T*>(this);}

	//======================================================================================================================
	// Defaulted public virtual destructor
	virtual ~RoutableMessage() noexcept = default;

	// Deleted copy/move constructors and assignment operators
	RoutableMessage(const RoutableMessage&) = delete;
	RoutableMessage(RoutableMessage&&) = delete;
	RoutableMessage& operator=(const RoutableMessage&) = delete;
	RoutableMessage& operator=(RoutableMessage&&) = delete;


protected:

	//======================================================================================================================
	// Protected constructor (RoutableMessage objects can be created via child classes only)
	RoutableMessage(Type _type, Subtype _subtype) noexcept : type(_type), subtype(_subtype) {}

private:

	//======================================================================================================================
	// Private const members (set at construction)
	const Type type;
	const Subtype subtype;
};

}; // (end namespace FIQCPPBASE)
