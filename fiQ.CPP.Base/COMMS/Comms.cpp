//==========================================================================================================================
// Comms.cpp : Central class for managing underlying communications layer and event management
//==========================================================================================================================
#include "pch.h"
#include "Comms.h"
#include "Logging/LogSink.h"
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
	for(listenerticketmax = 0; listenerticketmax < 100;) listenertickets.push_back(++listenerticketmax);
	for(sessionticketmax = 0; sessionticketmax < 100;) sessiontickets.push_back(++sessionticketmax);
}
void Comms::CommLink::Cleanup() {
	listenertickets.clear();
	sessiontickets.clear();
	listeners.clear();
	sessions.clear();
}

//==========================================================================================================================
_Check_return_ Comms::ListenerTicket Comms::CommLink::RegisterListener(
	const std::shared_ptr<CommsClient>& client, const std::shared_ptr<Connection>& connection,
	_Inout_opt_ std::string* LastErrString) {
	if(client.get() == nullptr || connection.get() == nullptr) {
		if(LastErrString) *LastErrString = "Invalid listener or connection object";
		return 0;
	}
	else if(connection->IsValidServer() == false) {
		if(LastErrString) *LastErrString = "Invalid listener configuration";
		return 0;
	}
	else if(LastErrString) LastErrString->clear();

	std::unique_ptr<ListenerControlBlock> lcb = std::make_unique<ListenerControlBlock>(client, connection);
	try {
		// Create socket listener and attempt to initialize:
		lcb->serversocket = SocketOps::ServerSocket::Create();
		if(lcb->serversocket->Open(connection->GetLocalPort()) == false) {
			if(LastErrString) LastErrString->assign(std::move(lcb->serversocket->GetLastErrString()));
			return 0;
		}

		// Check for TLS configuration, initialize if found:
		const std::string& tlscert = connection->GetConfigParm("TLSCERT"),
			tlsmethod = connection->GetConfigParm("TLSMETHOD");
		const size_t tlscertlen = tlscert.length();
		if(tlscertlen > 4 ? (tlscert.compare(0, 3, "MY(") == 0) : false) {
			// Certificate references machine store, attempt to initialize:
			if(lcb->serversocket->InitCredentialsFromStore(tlscert.substr(3, tlscertlen - 4), tlsmethod) == false) {
				if(LastErrString) LastErrString->assign(std::move(lcb->serversocket->GetLastErrString()));
				return 0;
			}
		}
		// (Note: may support file-based certificates in the future)
	}
	catch(const std::exception&) {std::throw_with_nested(FORMAT_RUNTIME_ERROR("Failed to initialize socket listener"));}

	// Lock access to available ListenerTicket container and acquire ticket
	ListenerTicket ticket = 0;
	{auto lock = Locks::Acquire(listenerticketlock);
	if(lock.IsLocked()) {
		if(listenertickets.empty()) {
			// No tickets available - add up to 100 new ticket values, stopping at max
			for(short s = 0; s < 100 && listenerticketmax < LISTENER_TICKETS_MAX; ++s)
				listenertickets.push_back(++listenerticketmax);
		}
		if(listenertickets.empty() == false) {
			ticket = listenertickets.front();
			listenertickets.pop_front();
		}
	}}
	if(ticket == 0) throw FORMAT_RUNTIME_ERROR("Failed to acquire ticket");

	try {
		auto lock = Locks::Acquire(listenerlock);
		lock.EnsureLocked();

		// Ensure ticket does not already exist in map (should not be possible):
		if(listeners.count(ticket)) throw FORMAT_RUNTIME_ERROR("Listener ticket already exists in map");

		// Add listener to map; worker thread will now be responsible for listening for connections:
		listeners.emplace(std::piecewise_construct,
			std::forward_as_tuple(ticket),
			std::forward_as_tuple(std::move(lcb))
		);
	}
	catch(const std::exception&) {
		// Return ticket to available collection prior to re-throwing exception:
		auto lock = Locks::Acquire(listenerticketlock);
		if(lock.IsLocked()) listenertickets.push_back(ticket);
		std::throw_with_nested(FORMAT_RUNTIME_ERROR("Failed to register listener"));
	}

	// Log listener registration and return ticket:
	LOG_FROM_TEMPLATE(LogLevel::Debug, "Registered listener ticket {:X8} for {:S60} on port {:D}",
		ticket, client->GetName().c_str(), connection->GetLocalPort());
	return ticket;
}
Comms::Result Comms::CommLink::DeregisterListener(ListenerTicket listener, int timeout) {
	Result rc = Result::InvalidTicket;
	std::shared_ptr<CommsClient> client(nullptr);

	// Lock access to listener map, then look up listener:
	std::shared_ptr<ThreadOps::Event> shutdownevent(nullptr);
	{auto lock = Locks::Acquire(listenerticketlock);
	lock.EnsureLocked();
	auto seek = listeners.find(listener);
	if((seek == listeners.end() ? nullptr : seek->second.get()) != nullptr) {
		if(seek->second->shutdownflag == false) {
			// Initialize event object and share with listener, if waiting:
			if(timeout != 0) {
				shutdownevent = std::make_shared<ThreadOps::Event>();
				seek->second->shutdownevent = shutdownevent;
			}
			// Save client pointer, flag that listener should be removed by worker thread:
			client = seek->second->client.lock();
			seek->second->shutdownflag = true;
			rc = Result::OK;
		}
	}}

	// Log result of operation and return:
	if(ResultOK(rc)) {
		LOG_FROM_TEMPLATE(LogLevel::Debug, "Deregistered listener ticket {:X8} for {:S60}",
			listener, client.get() ? client->GetName().c_str() : "[unknown client]");
		// If caller requested wait for thread to shut down listener, wait up to timeout now:
		if((shutdownevent.get() ? shutdownevent->Wait(timeout) : true) == false) {
			LOG_FROM_TEMPLATE(LogLevel::Debug, "Timeout waiting for shutdown of listener ticket {:X8} for {:S60}",
				listener, client.get() ? client->GetName().c_str() : "[unknown client]");
		}
	}
	else LOG_FROM_TEMPLATE(LogLevel::Warn, "Attempted to deregister invalid ticket {:X8}", listener);
	return rc; 
}

//==========================================================================================================================
_Check_return_ Comms::SessionTicket Comms::CommLink::RequestConnect(
	const std::shared_ptr<CommsClient>& client, const std::shared_ptr<Connection>& connection,
	_Inout_opt_ std::string* LastErrString) {
	if(client.get() == nullptr || connection.get() == nullptr) {
		if(LastErrString) *LastErrString = "Invalid client or connection object";
		return 0;
	}
	else if(connection->IsValidClient() == false) {
		if(LastErrString) *LastErrString = "Invalid client configuration";
		return 0;
	}
	else if(LastErrString) LastErrString->clear();

	const bool syncconnect = connection->CheckFlag(CommFlags::SyncConnect),
		syncdata = connection->CheckFlag(CommFlags::SyncData);

	SessionTicket ticket = 0;
	std::unique_ptr<SessionControlBlock> scb = std::make_unique<SessionControlBlock>(client, connection);
	try {

		// Retrieve configuration parameters, parse connection timeout:
		const std::string& conntimeout = connection->GetConfigParm("CONNTIMEOUT"),
			tlsmethod = connection->GetConfigParm("TLSMETHOD");
		const int iconntimeout = conntimeout.empty() ? 0 : atoi(conntimeout.c_str());

		LOG_FROM_TEMPLATE(LogLevel::Debug, "{:S20} connection for {:S60} to {:S20}:{:D}",
			syncconnect ? "Attempting" : "Initiating", client->GetName().c_str(),
			connection->GetRemoteAddress().c_str(), connection->GetRemotePort());

		// Attempt outbound connection (either synchronous or not, based on config flags):
		if(syncconnect) {
			scb->sessionsocket = SocketOps::SessionSocket::Connect(connection->GetRemoteAddress().c_str(),
				connection->GetRemotePort(), iconntimeout, (tlsmethod.empty() == false), tlsmethod);
			// Flag that connection has already been established (skip ahead to open if sync data mode):
			scb->state = syncdata ? SessionControlBlock::State::Open : SessionControlBlock::State::Connected;
		}
		else {
			scb->sessionsocket = SocketOps::SessionSocket::StartConnect(connection->GetRemoteAddress().c_str(),
				connection->GetRemotePort(), tlsmethod.empty() == false);
			// Set connection timeout period (default to 30 seconds if not provided):
			scb->conntimeoutat += std::chrono::milliseconds(iconntimeout > 0 ? iconntimeout : 30000);
		}

		// If socket not initialized, return now:
		if((scb->sessionsocket.get() ? scb->sessionsocket->SocketValid() : false) == false) {
			if(scb->sessionsocket.get() && LastErrString)
				LastErrString->assign(std::move(scb->sessionsocket->GetLastErrString()));
			return 0;
		}

		// Otherwise, connection has been initiated - retrieve ticket:
		ticket = GetSessionTicket();
	}
	catch(const std::exception&) {std::throw_with_nested(FORMAT_RUNTIME_ERROR("Outbound connection failed"));}

	try {
		if(syncdata) {
			// Add sync data flag to ticket, so it can be identified in subsequent calls as a sync session
			ticket |= SESSION_TICKET_SYNCDATA;
			// Ensure ticket does not already exist in sync map (should not be possible), then add session;
			// worker thread will ignore connections in sync map (client responsible for managing):
			std::unique_lock<std::timed_mutex> lock(syncsessionslock);
			if(lock.owns_lock() == false) throw FORMAT_RUNTIME_ERROR("Unable to lock access to session map");
			else if(syncsessions.count(ticket)) throw FORMAT_RUNTIME_ERROR("Session ticket already exists in map");
			syncsessions.emplace(std::piecewise_construct,
				std::forward_as_tuple(ticket),
				std::forward_as_tuple(std::move(scb))
			);
		}
		else {
			// Ensure ticket does not already exist in map (should not be possible), then add session; worker
			// thread responsible for polling connection (if not syncconnect), calling back to client object
			// with connection notification then monitoring session for events
			std::unique_lock<std::timed_mutex> lock(sessionslock);
			if(lock.owns_lock() == false) throw FORMAT_RUNTIME_ERROR("Unable to lock access to session map");
			else if(sessions.count(ticket)) throw FORMAT_RUNTIME_ERROR("Session ticket already exists in map");
			sessions.emplace(std::piecewise_construct,
				std::forward_as_tuple(ticket),
				std::forward_as_tuple(std::move(scb))
			);
		}
	}
	catch(const std::exception&) {
		// Return ticket to collection prior to re-throwing exception:
		auto lock = Locks::Acquire(listenerticketlock);
		if(lock.IsLocked()) listenertickets.push_back(ticket & SESSION_TICKET_REMFLAGS);
		std::throw_with_nested(FORMAT_RUNTIME_ERROR("Failed to register listener"));
	}

	// Log session registration and return ticket:
	LOG_FROM_TEMPLATE(LogLevel::Debug, "Registered session ticket {:X8} for {:S60} to {:S20}:{:D} ({:S10})",
		ticket, client->GetName().c_str(), connection->GetRemoteAddress().c_str(), connection->GetRemotePort(),
		syncconnect ? "connected" : "pending");
	return ticket;
}

//==========================================================================================================================
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
	BytesRead = 0;
	if(buf == nullptr || len == 0 || Tgt == nullptr || MaxBytes == 0) return Result::InvalidArg;
	else if((session & SESSION_TICKET_SYNCDATA) == 0) return Result::InvalidTicket;

	const SteadyClock EndTime(std::chrono::milliseconds{Timeout});
	try {
		// Locate session ticket in map, ensure entry is found and has a non-null SessionSocketPtr:
		std::unique_lock<std::timed_mutex> maplock(syncsessionslock, EndTime.GetTimePoint());
		if(maplock.owns_lock() == false) throw FORMAT_RUNTIME_ERROR("Unable to lock access to session map");
		auto seek = syncsessions.find(session);
		if((seek == syncsessions.end() ? nullptr : seek->second.get()) == nullptr) return Result::InvalidTicket;
		else if(seek->second->sessionsocket.get() == nullptr) return Result::InvalidTicket;
		// If session socket is not valid or control block is not currently in "open" state, reject request:
		else if(seek->second->sessionsocket->Valid() == false || seek->second->state != SessionControlBlock::State::Open)
			return Result::InvalidTicket;

		// Acquire lock for session object (so we will hold both locks temporarily):
		//std::unique_lock<std::timed_mutex> sesslock(seek->second->synclock, EndTime.GetTimePoint());
		// shit this is no good

		Tgt[0] = 0;
		UNREFERENCED_PARAMETER(Timeout);
		return Result::InvalidArg;
	}
	catch(const std::exception&) {std::throw_with_nested(FORMAT_RUNTIME_ERROR("Send and receive operation failed"));}
}

//==========================================================================================================================
Comms::Result Comms::CommLink::Disconnect(SessionTicket session) {
	Result rc = Result::InvalidTicket;
	std::shared_ptr<CommsClient> client(nullptr);

	if(session & SESSION_TICKET_SYNCDATA) {
		// Look up session in sync map, and (if found) flag that it should be disconnected:
		std::unique_lock<std::timed_mutex> lock(syncsessionslock);
		if(lock.owns_lock() == false) throw FORMAT_RUNTIME_ERROR("Unable to lock access to session map");
		auto seek = syncsessions.find(session);
		if((seek == syncsessions.end() ? nullptr : seek->second.get()) != nullptr) {
			if(seek->second->state < SessionControlBlock::State::Disconnecting) {
				client = seek->second->client.lock();
				seek->second->state = SessionControlBlock::State::Disconnecting;
				rc = Result::OK;
			}
		}
	}
	else if(session > 0) {
		// Look up session in map, and (if found) flag that it should be disconnected by worker thread:
		std::unique_lock<std::timed_mutex> lock(sessionslock);
		if(lock.owns_lock() == false) throw FORMAT_RUNTIME_ERROR("Unable to lock access to session map");
		auto seek = sessions.find(session);
		if((seek == sessions.end() ? nullptr : seek->second.get()) != nullptr) {
			if(seek->second->state < SessionControlBlock::State::Disconnecting) {
				client = seek->second->client.lock();
				seek->second->state = SessionControlBlock::State::Disconnecting;
				rc = Result::OK;
			}
		}
	}

	if(ResultOK(rc)) {
		LOG_FROM_TEMPLATE(LogLevel::Debug, "Disconnected session ticket {:X8} for {:S60}",
			session, client.get() ? client->GetName().c_str() : "[unknown client]");
	}
	else LOG_FROM_TEMPLATE(LogLevel::Warn, "Attempted to disconnect invalid session {:X8}", session);
	return rc; 
}
