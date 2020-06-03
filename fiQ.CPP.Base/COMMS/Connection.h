#pragma once
//==========================================================================================================================
// Connection.h : Classes and values for configuration of a specific communications link
//==========================================================================================================================

#include "Tools/Tokenizer.h"

namespace FIQCPPBASE {

//==========================================================================================================================
// Public definitions - Behaviour flags with overloaded operators for binary flag operations
enum class CommFlags : unsigned short {
	None			= 0x0000,	// Default/empty flag value
	AppKeepAlive	= 0x0001,	// Send zero-length (header only) packet periodically to ensure session is live
	ExtendedHeader	= 0x0002,	// Packets using extended 4-byte header (2 length bytes, 2 control bytes)
	Raw				= 0x0004,	// Do not use packet headers (allow caller to directly read/write raw data)
	TraceOn			= 0x0010,	// Enable tracing on this connection
	SyncConnect		= 0x0100,	// Outbound connection requests should be performed synchronously
	SyncData		= 0x0200	// Data read/write will be performed as a synchronous operation
};
inline constexpr CommFlags operator|(CommFlags a, CommFlags b) noexcept {
	using CommFlagsType = std::underlying_type_t<CommFlags>;
	return static_cast<CommFlags>(static_cast<CommFlagsType>(a) | static_cast<CommFlagsType>(b));
}
inline constexpr CommFlags operator|=(CommFlags& a, CommFlags b) noexcept {
	return (a = (a | b));
}
inline constexpr bool operator&(CommFlags a, CommFlags b) noexcept {
	using CommFlagsType = std::underlying_type_t<CommFlags>;
	return ((static_cast<CommFlagsType>(a) & static_cast<CommFlagsType>(b)) != 0);
}

//==========================================================================================================================
// Connection: Class containing configuration details of a specified link
class Connection
{
public:
	//======================================================================================================================
	// Type definitions
	using ConfigParm = std::pair<std::string,std::string>;
	using ConfigParms = std::vector<ConfigParm>;

	//======================================================================================================================
	// External accessor functions
	_Check_return_ bool IsValidClient() const noexcept {return (!address.empty() && ValueOps::Is(port).InRange(1, 0x7FFF));}
	_Check_return_ bool IsValidServer() const noexcept {return (address.empty() && ValueOps::Is(port).InRange(1, 0x7FFF));}
	_Check_return_ const std::string& GetRemoteAddress() const noexcept {return address;}
	_Check_return_ unsigned short GetRemotePort() const noexcept {return (address.empty() ? 0 : port);}
	_Check_return_ unsigned short GetLocalPort() const noexcept {return (address.empty() ? port : 0);}
	_Check_return_ CommFlags GetFlags() const noexcept {return cflags;}
	_Check_return_ bool CheckFlag(CommFlags f) const noexcept {return (cflags & f);}
	template<typename T, size_t len, std::enable_if_t<std::is_same_v<T, const char>, int> = 0>
	_Check_return_ const std::string& GetConfigParm(_In_reads_z_(len - 1) T (&name)[len]) const; // String literal version
	_Check_return_ const std::string& GetConfigParm(const std::string& name) const; // std::string version

	//======================================================================================================================
	// Configuration builder functions
	template<typename S, typename I,
		std::enable_if_t<std::is_same_v<std::decay_t<S>, std::string> && std::is_integral_v<I>, int> = 0>
	Connection& SetRemote(S&& _address, I _port); // Specify remote address and port
	Connection& SetRemote(const std::string& _address); // Specify remote address and port in "ADDR:PORT" format
	Connection& SetLocal(unsigned short _port) noexcept; // Specify local listening port number
	Connection& SetFlags(CommFlags _cflags) noexcept; // Set all flags
	Connection& SetFlagOn(CommFlags _cflags) noexcept; // Enable specific flag(s)
	template<typename T, std::enable_if_t<std::is_same_v<std::decay_t<T>, ConfigParms>, int> = 0>
	Connection& SetConfigParms(T&& t); // Set all config parameter values
	Connection& AddConfigParms(const ConfigParms& _parms); // Add set of configuration parameters to existing collection
	template<typename S, std::enable_if_t<std::is_same_v<std::decay_t<S>, std::string>, int> = 0>
	Connection& AddConfigParm(S&& name, S&& value); // Add single configuration parameter
	Connection& ReadConfig(const Tokenizer& toks);

	//======================================================================================================================
	// Defaulted constructors, assignment operators and destructor
	Connection() = default;
	Connection(const Connection&) = default;
	Connection(Connection&&) = default;
	Connection& operator=(const Connection&) = default;
	Connection& operator=(Connection&&) = default;
	~Connection() = default;

private:

	// Static accessor to produce persistent invalid field value:
	_Check_return_ static const std::string& EMPTYSTR() noexcept;

	// Private configuration variables
	std::string address;		// Remote address if connecting outbound, blank if listening
	unsigned short port = 0;	// Remote port if connecting out, local if listening
	CommFlags cflags = CommFlags::None;		// Mask of flag options defined above
	std::vector<std::pair<std::string,std::string>> parms;	// Collection of named config parameters
};

//==========================================================================================================================
// Connection::GetParm: Retrieve configuration parameter (literal version)
template<typename T, size_t len, std::enable_if_t<std::is_same_v<T, const char>, int>>
GSL_SUPPRESS(lifetime) // Vector member will live at least as long as this object
inline _Check_return_ const std::string& Connection::GetConfigParm(_In_reads_z_(len - 1) T (&name)[len]) const {
	auto seek = std::find_if(
		parms.cbegin(),
		parms.cend(),
		[name](const ConfigParm& c) -> bool { return (_stricmp(c.first.c_str(), name) == 0); });
	return (seek == parms.cend() ? EMPTYSTR() : seek->second);
}
// Connection::GetParm: Retrieve configuration parameter (string version)
GSL_SUPPRESS(lifetime) // Vector member will live at least as long as this object
inline _Check_return_ const std::string& Connection::GetConfigParm(const std::string& name) const {
	auto seek = std::find_if(
		parms.cbegin(),
		parms.cend(),
		[name](const ConfigParm& c) -> bool { return (_stricmp(c.first.c_str(), name.c_str()) == 0); });
	return (seek == parms.cend() ? EMPTYSTR() : seek->second);
}

//==========================================================================================================================
// Connection::SetRemote (string and integral value): For outgoing connections, specify remote address and port
template<typename S, typename I,
	std::enable_if_t<std::is_same_v<std::decay_t<S>, std::string> && std::is_integral_v<I>, int>>
inline Connection& Connection::SetRemote(S&& _address, I _port) {
	address.clear();
	port = 0;
	if(_address.empty() == false && ValueOps::Is(_port).InRange(1, 0x7FFF)) {
		address = std::forward<S>(_address); // Move memory, if input is rvalue
		port =  gsl::narrow_cast<unsigned short>(_port);
	}
	return *this;
}
// Connection::SetRemote (string only): For outgoing connections, specify remote address and port (in "ADDR:PORT" format)
inline Connection& Connection::SetRemote(const std::string& _address) {
	address.clear();
	port = 0;
	const std::string::size_type st = _address.find(':');
	if((st == std::string::npos) ? false : ValueOps::Is(st).InRange(1, _address.size() - 1)) {
		const int iport = atoi(_address.c_str() + st + 1);
		if(ValueOps::Is(iport).InRange(1, 0x7FFF)) {
			address.assign(_address.data(), st);
			port = gsl::narrow_cast<unsigned short>(iport);
		}
	}
	return *this;
}
// Connection::SetLocal: For incoming connections, specify listening port number
inline Connection& Connection::SetLocal(unsigned short _port) noexcept {
	address.clear();
	port = _port;
	return *this;
}
inline Connection& Connection::SetFlags(CommFlags _cflags) noexcept {
	cflags = _cflags;
	return *this;
}
inline Connection& Connection::SetFlagOn(CommFlags _cflags) noexcept {
	cflags |= _cflags;
	return *this;
}
// Connection::SetConfigParms: Set all configuration parameter values from collection
template<typename T, std::enable_if_t<std::is_same_v<std::decay_t<T>, Connection::ConfigParms>, int>>
inline Connection& Connection::SetConfigParms(T&& t) {
	parms = std::forward<T>(t); // Move memory from source, if rvalue reference
	return *this;
}
// Connection::AddConfigParms: Add set of configuration values to collection
inline Connection& Connection::AddConfigParms(const ConfigParms& _parms) {
	for(auto seek = _parms.cbegin(); seek != _parms.cend(); ++seek) AddConfigParm(seek->first, seek->second);
	return *this;
}
// Connection::AddConfigParm: Add single configuration value to collection
template<typename S, std::enable_if_t<std::is_same_v<std::decay_t<S>, std::string>, int>>
inline Connection& Connection::AddConfigParm(S&& name, S&& value) {
	bool found = false;
	for(auto seek = parms.begin(); seek != parms.end(); ++seek) {
		if(_stricmp(seek->first.c_str(), name.c_str()) == 0) {
			seek->second.assign(std::forward<S>(value));
			found = true;
			break;
		}
	}
	if(found == false) {
		// Parameter needs to be added to collection; reserve an additional slot for this entry (all
		// existing entries will need to be copied, but at least this one new entry can be constructed
		// in-place, moving memory from source if rvalue references):
		parms.reserve(parms.size() + 1);
		parms.emplace_back(std::piecewise_construct,
			std::forward_as_tuple(std::forward<S>(name)),
			std::forward_as_tuple(std::forward<S>(value)));
	}
	return *this;
}
// Connection::BuildConfigParms: Construct configuration from tokenized string values
inline Connection& Connection::ReadConfig(const Tokenizer& toks) {
	parms.clear();
	cflags = CommFlags::None;
	// Build vector of separator positions for later use:
	{std::vector<size_t> separators(toks.TokenCount(), 0);
	size_t parmcount = 0;
	// Iterate through all tokens, searching for named parameters or recognized flags:
	for(size_t s = 0; s < toks.TokenCount(); ++s) {
		if(toks.Length(s) > 0) {
			if(const char *separator = strchr(toks.Value(s), '=')) {
				separators[s] = (separator - toks.Value(s));
				++parmcount;
			}
			else if(_stricmp(toks.Value(s), "APPKEEPALIVE") == 0) cflags |= CommFlags::AppKeepAlive;
			else if(_stricmp(toks.Value(s), "EXTHEADER") == 0) cflags |= CommFlags::ExtendedHeader;
			else if(_stricmp(toks.Value(s), "RAW") == 0) cflags |= CommFlags::Raw;
			else if(_stricmp(toks.Value(s), "TRACEON") == 0) cflags |= CommFlags::TraceOn;
			else if(_stricmp(toks.Value(s), "SYNCCONN") == 0) cflags |= CommFlags::SyncConnect;
			else if(_stricmp(toks.Value(s), "SYNCDATA") == 0) cflags |= CommFlags::SyncData;
			// ...ignore any other (unrecognized) flag values
		}
	}
	// If named parameters are present, construct collection:
	if(parmcount > 0) {
		parms.reserve(parmcount);
		for(size_t s = 0; s < toks.TokenCount(); ++s) {
			if(separators[s] > 0) {
				parms.emplace_back(std::piecewise_construct,
					std::forward_as_tuple(toks.Value(s), separators[s]),
					std::forward_as_tuple(toks.Value(s) + separators[s] + 1, toks.Length(s) - separators[s] - 1));
			}
		}
	}}
	return *this;
}

}; // (end namespace FIQCPPBASE)
