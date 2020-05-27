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
_Check_return_ Comms::CommLink& Comms::GetCommLink() {
	static Comms::CommLink clink; // Will be created only once, on first call to this function
	return clink;
}

//==========================================================================================================================
void Comms::CommLink::Initialize(size_t CommThreads) {
	UNREFERENCED_PARAMETER(CommThreads);
}
void Comms::CommLink::Cleanup() {
}

_Check_return_ Comms::ListenerTicket Comms::CommLink::RegisterListener(
	const std::shared_ptr<CommsClient>& client, Connection&& connection) {
	if(client.get() == nullptr) return 0;
	UNREFERENCED_PARAMETER(connection);
	return 0;
}
_Check_return_ Comms::SessionTicket Comms::CommLink::RequestConnect(
	const std::shared_ptr<CommsClient>& client, Connection&& connection) {
	if(client.get() == nullptr) return 0;
	UNREFERENCED_PARAMETER(connection);
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
