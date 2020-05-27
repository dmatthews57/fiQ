//==========================================================================================================================
// Connection.cpp : Classes and values for configuration of a specific communications link
//==========================================================================================================================
#include "pch.h"
#include "Connection.h"
using namespace FIQCPPBASE;

//==========================================================================================================================
_Check_return_ const std::string& Connection::EMPTYSTR() {
	static const std::string emptystr;
	return emptystr;
}
