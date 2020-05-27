#pragma once
//==========================================================================================================================
// Comms.h : Central class for managing underlying communications layer and event management
//==========================================================================================================================

#include "Comms/CommsClient.h"
#include "Comms/Connection.h"

namespace FIQCPPBASE {

//==========================================================================================================================
// Comms: Class wrapping communications subsystem and providing static access to comms functions
class Comms
{
public:

	//======================================================================================================================
	// Public definitions and helper functions
	using ListenerTicket = unsigned long long;	// Handle to registered listener
	using SessionTicket = unsigned long long;	// Handle to open session
	template<typename T, std::enable_if_t<std::is_same_v<T, unsigned long long>, int> = 0>
	_Check_return_ static bool TicketValid(T ticket) noexcept {return (ticket > 0);}
	enum class Protocol : int { TCP = 0, HTTP = 1 };
	enum class Result { OK = 0, Timeout = 10, InvalidTicket = 20, Failed = 21, InvalidArg = 22 };
	_Check_return_ static constexpr bool ResultOK(Result r) noexcept {return (r == Result::OK);}
	_Check_return_ static constexpr bool ResultTimeout(Result r) noexcept {return (r == Result::Timeout);}
	_Check_return_ static constexpr bool ResultFailed(Result r) noexcept {return (r >= Result::InvalidTicket);}
	//======================================================================================================================
	// Public definitions - Worker thread pool size
	static constexpr size_t COMM_THREADS_MIN		= 1;
	static constexpr size_t COMM_THREADS_DEFAULT	= 10;
	static constexpr size_t COMM_THREADS_MAX		= 100;

	//======================================================================================================================
	// Static library initialization functions: Each should be called exactly once in program lifetime
	// - Note these functions are NOT thread-safe - call them from main() only
	static void Initialize(size_t CommThreads = COMM_THREADS_DEFAULT);
	static void Cleanup();

	//======================================================================================================================
	// RegisterListener: Start listening for inbound connections on specified connection; returns ListenerTicket
	template<typename...Args, std::enable_if_t<std::is_constructible_v<Connection, Args...>, int> = 0>
	_Check_return_ static ListenerTicket RegisterListener(const std::shared_ptr<CommsClient>& client, Args&&...args);
	template<typename...Args, std::enable_if_t<std::is_constructible_v<Connection, Args...>, int> = 0>
	_Check_return_ static SessionTicket RequestConnect(const std::shared_ptr<CommsClient>& client, Args&&...args);
	_Check_return_ static Result PollConnect(SessionTicket session);
	_Check_return_ static Result Send(SessionTicket session, _In_reads_(len) const char* buf, size_t len);
	_Check_return_ static Result SendAndReceive(SessionTicket session,
		_In_reads_(len) const char* buf, size_t len, // Outbound data to be delivered
		_Out_writes_(MaxBytes) char* Tgt, size_t MaxBytes, size_t& BytesRead, // Destination for inbound response
		int Timeout);

private:

	//======================================================================================================================
	// CommLink: Singleton communications management class, driving all communications functionality
	class CommLink {
	public:
		void Initialize(size_t CommThreads);
		void Cleanup();

		_Check_return_ ListenerTicket RegisterListener(const std::shared_ptr<CommsClient>& client, Connection&& connection);
		_Check_return_ SessionTicket RequestConnect(const std::shared_ptr<CommsClient>& client, Connection&& connection);
		_Check_return_ Result PollConnect(SessionTicket session);
		_Check_return_ Result Send(SessionTicket session, _In_reads_(len) const char* buf, size_t len);
		_Check_return_ Result SendAndReceive(SessionTicket session,
			_In_reads_(len) const char* buf, size_t len, // Outbound data to be delivered
			_Out_writes_(MaxBytes) char* Tgt, size_t MaxBytes, size_t& BytesRead, // Destination for inbound response
			int Timeout);

		CommLink() = default;
	};

	// Static CommLink accessor function (creates precisely one CommLink during process lifetime)
	_Check_return_ static CommLink& GetCommLink();
};

//==========================================================================================================================
template<typename...Args, std::enable_if_t<std::is_constructible_v<Connection, Args...>, int>>
inline _Check_return_ static Comms::ListenerTicket Comms::RegisterListener(
	const std::shared_ptr<CommsClient>& client, Args&&...args) {
	return GetCommLink().RegisterListener(client, Connection{std::forward<Args>(args)...});
}
template<typename...Args, std::enable_if_t<std::is_constructible_v<Connection, Args...>, int>>
inline _Check_return_ static Comms::SessionTicket Comms::RequestConnect(
	const std::shared_ptr<CommsClient>& client, Args&&...args) {
	return GetCommLink().RequestConnect(client, Connection{std::forward<Args>(args)...});
}
inline _Check_return_ Comms::Result Comms::PollConnect(SessionTicket session) {
	return GetCommLink().PollConnect(session);
}
inline _Check_return_ Comms::Result Comms::Send(SessionTicket session, _In_reads_(len) const char* buf, size_t len) {
	return GetCommLink().Send(session, buf, len);
}
inline _Check_return_ Comms::Result Comms::SendAndReceive(SessionTicket session,
	_In_reads_(len) const char* buf, size_t len, // Outbound data to be delivered
	_Out_writes_(MaxBytes) char* Tgt, size_t MaxBytes, size_t& BytesRead, // Destination for inbound response
	int Timeout) {
	return GetCommLink().SendAndReceive(session, buf, len, Tgt, MaxBytes, BytesRead, Timeout);
}

}; // (end namespace FIQCPPBASE)
