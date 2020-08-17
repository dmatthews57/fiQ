#pragma once
//==========================================================================================================================
// CommsClient.h : Defines interface for an object wishing to register itself with comms library
//==========================================================================================================================

namespace FIQCPPBASE {

//==========================================================================================================================
// CommsClient: Base class for object to be registered for communications event callbacks
class CommsClient : public std::enable_shared_from_this<CommsClient>
{
public:

	//======================================================================================================================
	// Pure virtual function definitions - comms event handlers
	virtual void IBConnect() noexcept(false) = 0;
	virtual void IBData() noexcept(false) = 0;
	virtual void IBDisconnect() noexcept(false) = 0;

	//======================================================================================================================
	// Public accessors
	const std::string& GetName() const noexcept {return name;}

	//======================================================================================================================
	// Defaulted public virtual destructor
	virtual ~CommsClient() noexcept(false) {} // Cannot be defaulted
	// Deleted copy/move constructors and assignment operators
	CommsClient(const CommsClient&) = delete;
	CommsClient(CommsClient&&) = delete;
	CommsClient& operator=(const CommsClient&) = delete;
	CommsClient& operator=(CommsClient&&) = delete;

protected:

	// Protected constructor (class cannot be created directly; child must provide reference to name variable)
	CommsClient(const std::string& _name) noexcept : name(_name) {}

private:
	const std::string& name;
};

//==========================================================================================================================

}; // (end namespace FIQCPPBASE)
