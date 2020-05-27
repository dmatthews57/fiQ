#pragma once
//==========================================================================================================================
// Connection.h : Classes and values for configuration of a specific communications link
//==========================================================================================================================

namespace FIQCPPBASE {

//==========================================================================================================================
// Public definitions - Behaviour flags with overloaded operators for binary flag operations
enum class CommFlags : unsigned short {
	None			= 0x0000,	// Default/empty flag value
	ExtendedHeader	= 0x0001,	// Packets using extended 4-byte header (2 length bytes, 2 control bytes)
	Raw				= 0x0002,	// Do not use packet headers (allow caller to directly read/write raw data)
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
	// GetParm accessors: Retrieve configuration parameter with the specified name
	// - Provides version taking string literal name (faster) or std::string-constructible name
	template<typename T, size_t len, std::enable_if_t<std::is_same_v<T, const char>, int> = 0>
	_Check_return_ const std::string& GetParm(_In_reads_z_(len - 1) T (&name)[len]) const;
	_Check_return_ const std::string& GetParm(const std::string& name) const;
	// CheckFlag accessor: Check for defined CommFlag value in mask:
	_Check_return_ bool CheckFlag(CommFlags f) const noexcept {return (cflags & f);}

	// Defaulted constructors, assignment operators and destructor
	Connection() = default;
	Connection(const Connection&) = default;
	Connection(Connection&&) = default;
	Connection& operator=(const Connection&) = default;
	Connection& operator=(Connection&&) = default;
	~Connection() = default;

private:

	// Static accessor to produce persistent invalid field value:
	_Check_return_ static const std::string& EMPTYSTR();

	// Private configuration variables
	std::string address;		// Remote address if connecting outbound, blank if listening
	unsigned short port = 0;	// Remote port if connecting out, local if listening
	CommFlags cflags = CommFlags::None;		// Mask of flag options defined above
	std::vector<std::pair<std::string,std::string>> parms;	// Collection of named config parameters
};

//==========================================================================================================================
// Connection::GetParm: Retrieve configuration parameter (literal version)
template<typename T, size_t len, std::enable_if_t<std::is_same_v<T, const char>, int>>
inline _Check_return_ const std::string& Connection::GetParm(_In_reads_z_(len - 1) T (&name)[len]) const {
	for(auto seek = parms.cbegin(); seek != parms.end(); ++seek) {
#pragma warning (suppress : 26487) // Iterator points to member variable, reference will last as long as this object
		if(_stricmp(seek->first.c_str(), name) == 0) return seek->second;
	}
	return EMPTYSTR();
}
// Connection::GetParm: Retrieve configuration parameter (string version)
inline _Check_return_ const std::string& Connection::GetParm(const std::string& name) const {
	for(auto seek = parms.cbegin(); seek != parms.end(); ++seek) {
#pragma warning (suppress : 26487) // Iterator points to member variable, reference will last as long as this object
		if(_stricmp(seek->first.c_str(), name.c_str()) == 0) return seek->second;
	}
	return EMPTYSTR();
}

}; // (end namespace FIQCPPBASE)
