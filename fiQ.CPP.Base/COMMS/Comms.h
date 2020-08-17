#pragma once
//==========================================================================================================================
// Comms.h : Central class for managing underlying communications layer and event management
//==========================================================================================================================

#include "Comms/CommsClient.h"
#include "Comms/Connection.h"
#include "Tools/SocketOps.h"
#include "Tools/ThreadOps.h"

namespace FIQCPPBASE {

//==========================================================================================================================
// Comms: Class wrapping communications subsystem and providing static access to comms functions
class Comms
{
public:

	//======================================================================================================================
	// Public definitions and helper functions
	using ListenerTicket = unsigned int;	// Handle to registered listener
	using SessionTicket = unsigned int;		// Handle to open session
	template<typename T, std::enable_if_t<std::is_same_v<T, unsigned int>, int> = 0>
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
	_Check_return_ static ListenerTicket RegisterListener(
		const std::shared_ptr<CommsClient>& client, const std::shared_ptr<Connection>& connection,
		_Inout_opt_ std::string* LastErrString = nullptr);
	static Result DeregisterListener(ListenerTicket listener, int timeout);
	// RequestConnect: Attempt connection out to remote; returns SessionTicket
	_Check_return_ static SessionTicket RequestConnect(
		const std::shared_ptr<CommsClient>& client, const std::shared_ptr<Connection>& connection,
		_Inout_opt_ std::string* LastErrString = nullptr);
	// Send: Deliver data to specified session
	_Check_return_ static Result Send(SessionTicket session, _In_reads_(len) const char* buf, size_t len);
	// SendAndReceive: Deliver data to specified session and wait for response synchronously
	_Check_return_ static Result SendAndReceive(SessionTicket session,
		_In_reads_(len) const char* buf, size_t len, // Outbound data to be delivered
		_Out_writes_(MaxBytes) char* Tgt, size_t MaxBytes, size_t& BytesRead, // Destination for inbound response
		int Timeout);
	// Disconnect: Drop specified session
	static Result Disconnect(SessionTicket session);

private:

	//======================================================================================================================
	// Private value definitions
	static constexpr ListenerTicket LISTENER_TICKETS_MAX = 0x00FFFFFF;
	static constexpr SessionTicket SESSION_TICKET_MAX = 0x00FFFFFF;
	static constexpr SessionTicket SESSION_TICKET_REMFLAGS = ~0xFF000000;
	static constexpr SessionTicket SESSION_TICKET_SYNCDATA = 0x10000000;

	//======================================================================================================================
	// CommLink: Singleton communications management class, driving all communications functionality
	class CommLink {
	public:
		//==================================================================================================================
		// Initialization functions (forwarded from Comms static functions)
		void Initialize(size_t CommThreads);
		void Cleanup();

		//==================================================================================================================
		// Communications functions (forwarded from Comms static functions)
		_Check_return_ ListenerTicket RegisterListener(
			const std::shared_ptr<CommsClient>& client, const std::shared_ptr<Connection>& connection,
			_Inout_opt_ std::string* LastErrString);
		Result DeregisterListener(ListenerTicket listener, int timeout);
		_Check_return_ SessionTicket RequestConnect(
			const std::shared_ptr<CommsClient>& client, const std::shared_ptr<Connection>& connection,
			_Inout_opt_ std::string* LastErrString);
		_Check_return_ Result Send(SessionTicket session, _In_reads_(len) const char* buf, size_t len);
		_Check_return_ Result SendAndReceive(SessionTicket session,
			_In_reads_(len) const char* buf, size_t len, // Outbound data to be delivered
			_Out_writes_(MaxBytes) char* Tgt, size_t MaxBytes, size_t& BytesRead, // Destination for inbound response
			int Timeout);
		Result Disconnect(SessionTicket session);

		CommLink() noexcept : listenerticketmax(0), listenerticketlock(true), listenerlock(true),
			sessionticketmax(0), sessionticketlock(true) {}

	private:

		//==================================================================================================================
		// Listener management
		ListenerTicket listenerticketmax;			// Current max listener ticket value
		std::deque<ListenerTicket> listenertickets;	// Collection of available listener tickets
		Locks::SpinLock listenerticketlock;			// Lock for access to listener ticket collection

		struct ListenerControlBlock {

			// Default constructor/destructor
			ListenerControlBlock(
				const std::shared_ptr<CommsClient> _client, const std::shared_ptr<Connection>& _connection)
				: client(_client), connection(_connection), serversocket(nullptr),
				shutdownflag(false), shutdownevent(nullptr) {}
			~ListenerControlBlock() = default;

			// Deleted copy/move constructors/assignment operators
			ListenerControlBlock(const ListenerControlBlock&) = delete;
			ListenerControlBlock(ListenerControlBlock&&) = delete;
			ListenerControlBlock& operator=(const ListenerControlBlock&) = delete;
			ListenerControlBlock& operator=(ListenerControlBlock&&) = delete;

			// Const members, set at construction
			const std::weak_ptr<CommsClient> client;		// Callback handle to registered client
			const std::shared_ptr<Connection> connection;	// Configuration information for this connection

			// Processing members
			SocketOps::ServerSocketPtr serversocket;		// Handle to listener socket
			bool shutdownflag;								// Flag to indicate when this listener should close
			std::shared_ptr<ThreadOps::Event> shutdownevent;	// Event to flag when shutdown is complete, if required
		};
		std::map<ListenerTicket, std::unique_ptr<ListenerControlBlock>> listeners;
		Locks::SpinLock listenerlock;

		//==================================================================================================================
		// Session management
		SessionTicket sessionticketmax = 0;			// Current max session ticket value
		std::deque<SessionTicket> sessiontickets;	// Collection of available session tickets
		Locks::SpinLock sessionticketlock;			// Lock for access to session ticket collection
		_Check_return_ SessionTicket GetSessionTicket(); // Utility function to retrieve next free session ticket

		struct SessionControlBlock {

			// Type/value definitions
			enum class State : int {
				Connecting = 0,		// Outbound connection in progress
				Connected = 1,		// Session is connected, awaiting callback
				Open = 2,			// Session is open for data exchange
				Disconnecting = 3,	// Session is disconnected, awaiting callback
				Disconnected = 4	// Session has disconnected
			};

			// Utility functions
			_Check_return_ bool CheckFlag(CommFlags f) const noexcept {
				return (connection.get() ? connection->CheckFlag(f) : false);
			}

			// Default constructor and destructor
			SessionControlBlock(
				const std::shared_ptr<CommsClient> _client,
				const std::shared_ptr<Connection>& _connection,
				ListenerTicket _listener = 0) noexcept(false)
				: client(_client), connection(_connection), listener(_listener),
				sessionsocket(nullptr), state(State::Connecting) {}
			~SessionControlBlock() = default;

			// Deleted copy/move constructors/assignment operators
			SessionControlBlock(const SessionControlBlock&) = delete;
			SessionControlBlock(SessionControlBlock&&) = delete;
			SessionControlBlock& operator=(const SessionControlBlock&) = delete;
			SessionControlBlock& operator=(SessionControlBlock&&) = delete;

			// Const members, set at construction
			const std::weak_ptr<CommsClient> client;
			const std::shared_ptr<Connection> connection;
			const ListenerTicket listener; // Ticket of listener accepting this connection, if inbound

			// Processing members
			SocketOps::SessionSocketPtr sessionsocket;	// Handler for session socket
			SteadyClock conntimeoutat;	// Time at which connection polling should abort, if async connect
			State state;				// Current state of session
			std::timed_mutex synclock;	// Lock for sychronous send/receive operations
		};
		std::map<SessionTicket, std::unique_ptr<SessionControlBlock>> sessions;
		std::timed_mutex sessionslock;
		std::map<SessionTicket, std::unique_ptr<SessionControlBlock>> syncsessions;
		std::timed_mutex syncsessionslock;
	};

	// Static CommLink accessor function (creates precisely one CommLink during process lifetime)
	_Check_return_ static CommLink& GetCommLink() noexcept;
};

//==========================================================================================================================
inline _Check_return_ Comms::ListenerTicket Comms::RegisterListener(
	const std::shared_ptr<CommsClient>& client, const std::shared_ptr<Connection>& connection,
	_Inout_opt_ std::string* LastErrString) {
	return GetCommLink().RegisterListener(client, connection, LastErrString);
}
inline Comms::Result Comms::DeregisterListener(ListenerTicket listener, int timeout) {
	return GetCommLink().DeregisterListener(listener, timeout);
}
inline _Check_return_ Comms::SessionTicket Comms::RequestConnect(
	const std::shared_ptr<CommsClient>& client, const std::shared_ptr<Connection>& connection,
	_Inout_opt_ std::string* LastErrString) {
	return GetCommLink().RequestConnect(client, connection, LastErrString);
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
inline Comms::Result Comms::Disconnect(SessionTicket session) {
	return GetCommLink().Disconnect(session);
}

inline _Check_return_ Comms::SessionTicket Comms::CommLink::GetSessionTicket() {
	// Lock access to available SessionTicket container and acquire ticket:
	auto lock = Locks::Acquire(sessionticketlock);
	if(lock.IsLocked()) {
		if(sessiontickets.empty()) {
			// No tickets available - add up to 1000 new ticket values, stopping at max
			for(short s = 0; s < 1000 && sessionticketmax < SESSION_TICKET_MAX; ++s)
				sessiontickets.push_back(++sessionticketmax);
		}
		if(sessiontickets.empty() == false) {
			const SessionTicket ticket = sessiontickets.front();
			sessiontickets.pop_front();
			return ticket;
		}
	}
	throw FORMAT_RUNTIME_ERROR("Failed to acquire ticket");
}

}; // (end namespace FIQCPPBASE)
