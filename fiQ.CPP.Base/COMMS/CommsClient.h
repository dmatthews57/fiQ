#pragma once
//==========================================================================================================================
// CommsClient.h : Defines interface for an object wishing to register itself with comms library
//==========================================================================================================================

namespace FIQCPPBASE {

//==========================================================================================================================
// CommsClient:...
class CommsClient : public std::enable_shared_from_this<CommsClient>
{
public:

	//======================================================================================================================
	virtual void IBConnect() noexcept(false) = 0;
	virtual void IBData() noexcept(false) = 0;
	virtual void IBDisconnect() noexcept(false) = 0;

	// Defaulted public constructor and virtual destructor
	CommsClient() = default;
	virtual ~CommsClient() = default;
	// Deleted copy/move constructors and assignment operators
	CommsClient(const CommsClient&) = delete;
	CommsClient(CommsClient&&) = delete;
	CommsClient& operator=(const CommsClient&) = delete;
	CommsClient& operator=(CommsClient&&) = delete;

};

//==========================================================================================================================

}; // (end namespace FIQCPPBASE)
