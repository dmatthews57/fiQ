//==========================================================================================================================
// Comms.cpp : Central class for managing underlying communications layer and event management
//==========================================================================================================================
#include "pch.h"
#include "Comms.h"
using namespace FIQCPPBASE;

//==========================================================================================================================
void Comms::Initialize(size_t CommThreads) {GetCommLink().Initialize(CommThreads);}
void Comms::Cleanup() {GetCommLink().Cleanup();}

//==========================================================================================================================
// Comms::GetCommLink: Create static object and return by reference
_Check_return_ Comms::CommLink& Comms::GetCommLink() noexcept {
	static Comms::CommLink clink; // Will be created only once, on first call to this function
	return clink;
}

//==========================================================================================================================
void Comms::CommLink::Initialize(size_t CommThreads) {
	UNREFERENCED_PARAMETER(CommThreads);
	for(ListenerTicket lt = 1; lt <= listenerticketcount; ++lt) listenertickets.push_back(lt);
	for(SessionTicket st = 1; st <= listenerticketcount; ++st) sessiontickets.push_back(st);
}
void Comms::CommLink::Cleanup() {
	listenertickets.clear();
	sessiontickets.clear();
	listeners.clear();
	sessions.clear();
}

_Check_return_ Comms::ListenerTicket Comms::CommLink::RegisterListener(
	const std::shared_ptr<CommsClient>& client, const std::shared_ptr<Connection>& connection,
	_Inout_opt_ std::string* LastErrString) {
	if(client.get() == nullptr || (connection.get() ? (connection->IsValidServer() == false) : true)) {
		if(LastErrString) *LastErrString = "Invalid listener or connection object";
		return 0;
	}
	else if(LastErrString) LastErrString->clear();

	SocketOps::ServerSocketPtr serversocket(nullptr);
	try {
		// Create socket listener and attempt to initialize:
		serversocket = SocketOps::ServerSocket::Create();
		if(serversocket->Open(connection->GetLocalPort()) == false) {
			if(LastErrString) LastErrString->assign(std::move(serversocket->GetLastErrString()));
			return 0;
		}

		// Check for TLS configuration, initialize if found:
		const std::string& tlscert = connection->GetConfigParm("TLSCERT"),
			tlsmethod = connection->GetConfigParm("TLSMETHOD");
		const size_t tlscertlen = tlscert.length();
		if(tlscertlen > 4 ? (tlscert.compare(0, 3, "MY(") == 0) : false) {
			// Certificate references machine store, attempt to initialize:
			if(serversocket->InitCredentialsFromStore(tlscert.substr(3, tlscertlen - 4), tlsmethod) == false) {
				if(LastErrString) LastErrString->assign(std::move(serversocket->GetLastErrString()));
				return 0;
			}
		}
		// (Note: may support file-based certificates in the future)
	}
	catch(const std::exception&) {std::throw_with_nested(FORMAT_RUNTIME_ERROR("Failed to initialize socket listener"));}

	// Lock access to available ListenerTicket container and acquire ticket
	ListenerTicket ticket = 0;
	{auto lock = Locks::Acquire(listenerticketlock);
	if(listenertickets.empty()) {
		if(listenerticketcount < LISTENER_TICKETS_MAX) {
			const ListenerTicket ltstart = listenerticketcount + 1;
			listenerticketcount += 100;
			for(ListenerTicket lt = ltstart; lt <= listenerticketcount; ++lt) listenertickets.push_back(lt);
		}
		else throw FORMAT_RUNTIME_ERROR("Maximum listener tickets reached");
	}
	ticket = listenertickets.front();
	listenertickets.pop_front();}

	try {
		// TODO: LOCK ACCESS TO MAP (LOCK TYPE TBD)
		// Ensure ticket does not already exist in map (should not be possible):
		if(listeners.count(ticket)) throw FORMAT_RUNTIME_ERROR("Listener ticket already exists in map");
		// Add listener to map and return ticket:
		listeners.emplace(std::piecewise_construct,
			std::forward_as_tuple(ticket),
			std::forward_as_tuple(client, connection, std::move(serversocket), nullptr)
		);
		printf("Registered listener for [%s] on port %u, ticket %llu\n", client->GetName().c_str(), connection->GetLocalPort(), ticket);
		return ticket;
	}
	catch(const std::exception&) {
		// Return ticket to collection prior to re-throwing exception:
		auto lock = Locks::Acquire(listenerticketlock);
		listenertickets.push_back(ticket);
		std::throw_with_nested(FORMAT_RUNTIME_ERROR("Failed to register listener"));
	}
}
_Check_return_ Comms::SessionTicket Comms::CommLink::RequestConnect(
	const std::shared_ptr<CommsClient>& client, const std::shared_ptr<Connection>& connection,
	_Inout_opt_ std::string* LastErrString) {
	if(client.get() == nullptr || connection.get() == nullptr) {
		if(LastErrString) *LastErrString = "Invalid client or connection object";
		return 0;
	}
	else if(LastErrString) LastErrString->clear();

	printf("Requesting connection for [%s] to %s:%u\n", client->GetName().c_str(), connection->GetRemoteAddress().c_str(), connection->GetRemotePort());

	return 0;
}
_Check_return_ Comms::Result Comms::CommLink::PollConnect(SessionTicket session) {
	UNREFERENCED_PARAMETER(session);
	return Result::InvalidArg;
}
_Check_return_ Comms::Result Comms::CommLink::Send(SessionTicket session, _In_reads_(len) const char* buf, size_t len) {
	UNREFERENCED_PARAMETER(session);
	UNREFERENCED_PARAMETER(buf);
	UNREFERENCED_PARAMETER(len);
	return Result::InvalidArg;
}
_Check_return_ Comms::Result Comms::CommLink::SendAndReceive(SessionTicket session,
	_In_reads_(len) const char* buf, size_t len, // Outbound data to be delivered
	_Out_writes_(MaxBytes) char* Tgt, size_t MaxBytes, size_t& BytesRead, // Destination for inbound response
	int Timeout) {
	UNREFERENCED_PARAMETER(session);
	UNREFERENCED_PARAMETER(buf);
	UNREFERENCED_PARAMETER(len);
	UNREFERENCED_PARAMETER(Tgt);
	UNREFERENCED_PARAMETER(MaxBytes);
	UNREFERENCED_PARAMETER(BytesRead);
	UNREFERENCED_PARAMETER(Timeout);
	return Result::InvalidArg;
}
