#pragma once
//==========================================================================================================================
// SocketOps.h : Classes and functions for managing TCP/IP socket communications
//==========================================================================================================================

#include <ws2tcpip.h>
#define SECURITY_WIN32
#include <security.h>
#include <schnlsp.h>
#include "Tools/Exceptions.h"
#include "Tools/TimeClock.h"
#include "Tools/ValueOps.h"

namespace FIQCPPBASE {

//==========================================================================================================================
// Public definitions - Behaviour flags with overloaded operators for binary flag operations
enum class SocketFlags : unsigned short {
	None = 0x0000,			// Default/empty flag value
	ExtendedHeader = 0x0001	// Packets using extended 4-byte header (2 length bytes, 2 control bytes)
};
inline SocketFlags operator|(SocketFlags a, SocketFlags b) noexcept {
	return static_cast<SocketFlags>(
		static_cast<std::underlying_type_t<SocketFlags> >(a) | static_cast<std::underlying_type_t<SocketFlags> >(b));
}
inline SocketFlags operator|=(SocketFlags& a, SocketFlags b) noexcept {
	return (a = (a | b));
}
inline bool operator&(SocketFlags a, SocketFlags b) noexcept {
	return ((static_cast<std::underlying_type_t<SocketFlags> >(a) & static_cast<std::underlying_type_t<SocketFlags> >(b)) != 0);
}

//==========================================================================================================================
// SocketOps: Main class containing all TCP/IP socket functionality, with TLS support
class SocketOps {
private: struct pass_key {}; // Private function pass-key definition
public:

	//======================================================================================================================
	// Public definitions - Operation result code and associated helper functions
	enum class Result {	OK = 0, Timeout = 10, InvalidSocket = 20, Failed = 21, InvalidArg = 22 };
	_Check_return_ static constexpr bool ResultOK(Result r) noexcept {return (r == Result::OK);}
	_Check_return_ static constexpr bool ResultTimeout(Result r) noexcept {return (r == Result::Timeout);}
	_Check_return_ static constexpr bool ResultFailed(Result r) noexcept {return (r >= Result::InvalidSocket);}
	// Public definitions - TLS buffer sizes
	static const size_t TLS_BUFFER_SIZE_MIN		= 0x0080;
	static const size_t TLS_BUFFER_SIZE_DEFAULT	= 0x2000;
	static const size_t TLS_BUFFER_SIZE_MAX		= 0x8000;

	//======================================================================================================================
	// Static library initialization functions: Each should be called exactly once in program lifetime
	// - Note these functions are NOT thread-safe - call them from main() only
	static void InitializeSockets(bool TLSRequired);
	static void CleanupSockets();

	//======================================================================================================================
	// Static socket management functions: Used by callers who manage their own SOCKET handles
	// - Most provide optional LastErrString parameter, used if caller requires further information on any failures
	//======================================================================================================================
	// InitServer: Create and initialize a SOCKET, bind it to the specified port and (optional) interface and listen
	// - Returns initialized socket or INVALID_SOCKET on error
	_Check_return_ static SOCKET InitServer(unsigned short ListenPort, _In_opt_z_ const char* Interface = nullptr, int Backlog = 10,
		_Inout_opt_ std::string* LastErrString = nullptr);
	// Accept: Accept incoming connection request (will block until client request avaialble)
	// - "server" input should be a valid, listening socket (as returned by InitServer)
	// - Optional "saddr" parameter can be provided to receive details on client
	// - Returns connected SOCKET value or INVALID_SOCKET on error
	_Check_return_ static SOCKET Accept(SOCKET server, _Inout_opt_ sockaddr_in* saddr = nullptr,
		_Inout_opt_ std::string* LastErrString = nullptr);
	// DNSLookup: Retrieve IP address for specified name
	_Check_return_ static bool DNSLookup(_In_z_ const char* URL, _Out_writes_z_(20) char* TgtIP,
		_Inout_opt_ std::string* LastErrString = nullptr);
	// Connect: Attempt outbound connection to the specified IP and port
	// - Optional Timeout value (milliseconds) can override Winsock default timeout period (0 = use default)
	// - Returns connected SOCKET value or INVALID_SOCKET on error
	_Check_return_ static SOCKET Connect(_In_z_ const char* RemoteIP, unsigned short RemotePort, int Timeout = 0,
		_Inout_opt_ std::string* LastErrString = nullptr);
	// StartConnect: As Connect, but will attempt nonblocking connection and return immediately
	// - Use PollConnect to check back to determine whether it has successfully connected
	// - Returns connect-in-progress SOCKET value or INVALID_SOCKET on error
	_Check_return_ static SOCKET StartConnect(_In_z_ const char* RemoteIP, unsigned short RemotePort,
		_Inout_opt_ std::string* LastErrString = nullptr);
	_Check_return_ static Result PollConnect(SOCKET s,
		_Inout_opt_ std::string* LastErrString = nullptr);
	// WaitEvent: Wait up to "Timeout" milliseconds for activity on input session socket
	// - Socket will be shutdown (but not closed) on error
	_Check_return_ static Result WaitEvent(SOCKET s, int Timeout,
		_Inout_opt_ std::string* LastErrString = nullptr);
	// Send: Deliver data to session socket
	// - Socket will be shutdown (but not closed) on error
	_Check_return_ static Result Send(SOCKET s, _In_reads_(len) const char* buf, size_t len,
		_Inout_opt_ std::string* LastErrString = nullptr);
	// ReadExact: Read a specific number of bytes from session socket
	// - Socket will be shutdown (but not closed) on error, also on timeout IF only some bytes were read (prevent fragmentation)
	_Check_return_ static Result ReadExact(SOCKET s, _Out_writes_(BytesToRead) char* Tgt, size_t BytesToRead, int Timeout,
		_Inout_opt_ std::string* LastErrString = nullptr);
	// ReadAvailable: Read bytes currently available on session socket, up to MaxBytes
	// - Socket will be shutdown (but not closed) on error
	// - Function will block if no data pending (assumes caller has already checked for available data)
	_Check_return_ static Result ReadAvailable(SOCKET s, _Out_writes_(MaxBytes) char* Tgt, size_t MaxBytes, size_t& BytesRead,
		_Inout_opt_ std::string* LastErrString = nullptr);
	// ReadPacket: Read a two-byte length header followed by the indicated number of bytes (up to MaxBytes)
	// - Socket will be shutdown (but not closed on error), also on timeout IF partial packet is read (prevent fragmentation)
	_Check_return_ static Result ReadPacket(SOCKET s, _Out_writes_(MaxBytes) char* Tgt, size_t MaxBytes, size_t& BytesRead,
		int Timeout, SocketFlags Flags = SocketFlags::None,
		_Inout_opt_ std::string* LastErrString = nullptr);
	// ShutdownSocket: Send graceful shutdown on session socket (still requires CloseSocket call)
	static void Shutdown(SOCKET s) {if(s != INVALID_SOCKET) shutdown(s, SD_BOTH);}
	// CloseSocket: Close socket and invalidate socket handle
	static void Close(SOCKET& s) {if(s != INVALID_SOCKET) closesocket(s); s = INVALID_SOCKET;}

	// Forward declarations for ServerSocket, SessionSocket class and smart pointers:
	class ServerSocket;
	using ServerSocketPtr = std::unique_ptr<ServerSocket>;
	class SessionSocket;
	using SessionSocketPtr = std::unique_ptr<SessionSocket>;

	//======================================================================================================================
	// ServerSocket: Container class for managing a socket listening for inbound connections
	// - Note this class is NOT internally thread-safe; use object within single thread only (or protect with locks)
	class ServerSocket {
	public:
		//==================================================================================================================
		// Public named constructor
		_Check_return_ static ServerSocketPtr Create() {return std::make_unique<ServerSocket>(SocketOps::pass_key());}
		//==================================================================================================================
		// External accessor functions
		_Check_return_ const std::string& GetLastErrString() const noexcept {return LastErrString;}
		_Check_return_ bool Valid() const noexcept {
			return (SocketHandle != INVALID_SOCKET && (UsingTLS ? CredentialsValid() : true));
		}
		_Check_return_ bool SocketValid() const noexcept {return (SocketHandle != INVALID_SOCKET);}
		_Check_return_ bool IsSocket(SOCKET s) const noexcept {return (s == SocketHandle);}
		bool AddToFD(fd_set& fd) const noexcept(false) {
			return (fd.fd_count < FD_SETSIZE && SocketHandle != INVALID_SOCKET) ?
				(fd.fd_array[fd.fd_count++] = SocketHandle, true) : false;
		}
		//==================================================================================================================
		// Socket management functions
		_Check_return_ bool Open(unsigned short ListenPort, _In_opt_z_ const char* Interface = nullptr, int Backlog = 10);
		_Check_return_ Result WaitEvent(int Timeout) const;
		void Close() {SocketOps::Close(SocketHandle);}
		//==================================================================================================================
		// Accept function: Accepts new incoming session
		// - Note that this function will block waiting for a new connection, caller is assumed to have used WaitEvent
		// - Note that this function will also block to perform TLS negotiation (may add async TLS option in the future)
		_Check_return_ SessionSocketPtr Accept(_Inout_opt_ sockaddr_in* saddr = nullptr,
			int TLSTimeout = 0, size_t TLSBufferSize = SocketOps::TLS_BUFFER_SIZE_DEFAULT);
		//==================================================================================================================
		// Credential management functions
		_Check_return_ bool CredentialsValid() const noexcept {
			return ((hCreds.dwLower || hCreds.dwUpper) && pCertContext && hMyCertStore && SchannelCred.dwVersion);
		}
		_Check_return_ bool InitCredentialsFromStore( // Reads certificate from store (Local Machine or Current User)
			const std::string& CertName, const std::string& TLSMethod = "", bool LocalMachineStore = true);
		_Check_return_ bool InitCredentialsFromFile( // Reads certificate from PKCS12 store file
			const std::string& FileName, const std::string& FilePassword, const std::string& CertName,
			const std::string& TLSMethod = "");
		void CleanupCredentials();

		//==================================================================================================================
		// Public constructor (locked by private pass key), destructor
		ServerSocket(SocketOps::pass_key) noexcept {};
		~ServerSocket() noexcept(false) {
			Close();
			CleanupCredentials();
		}
		// Deleted default/copy/move constructors and assignment operators
		ServerSocket() = delete;
		ServerSocket(const ServerSocket&) = delete;
		ServerSocket(ServerSocket&&) = delete;
		ServerSocket& operator=(const ServerSocket&) = delete;
		ServerSocket& operator=(ServerSocket&&) = delete;

	private:

		// Private utility functions
		_Check_return_ bool CompleteInitCredentials(const std::string& TLSMethod);
		_Check_return_ Result TLSNegotiate(SessionSocketPtr& sp, int Timeout);

		// Private member variables
		SOCKET SocketHandle = INVALID_SOCKET;	// Handle to listener socket
		mutable std::string LastErrString;		// Storage for last error on this socket
		// Private member variables - TLS credentials
		bool UsingTLS = false;					// By default, TLS not in use (credentials not required)
		SCHANNEL_CRED SchannelCred = {0};		// Holds configuration parameters
		HCERTSTORE hMyCertStore = nullptr;		// Handle held from init to cleanup
		PCCERT_CONTEXT pCertContext = nullptr;	// Handle held from init to cleanup
		CredHandle hCreds = {0};				// Handle actually used for negotiations
	};

	//======================================================================================================================
	// SessionSocket: Container class for managing an active socket session
	// - Note this class is NOT internally thread-safe; use object within single thread only (or protect with locks)
	class SessionSocket {
	public:
		//==================================================================================================================
		// Public named constructors (client mode only, performs outbound connection attempt)
		_Check_return_ static SessionSocketPtr Connect(_In_z_ const char* RemoteIP, unsigned short RemotePort, int Timeout = 0,
			bool UsingTLS = false, const std::string& TLSMethod = "", size_t TLSBufferSize = SocketOps::TLS_BUFFER_SIZE_DEFAULT);
		_Check_return_ static SessionSocketPtr StartConnect(_In_z_ const char* RemoteIP, unsigned short RemotePort,
			bool UsingTLS = false, size_t TLSBufferSize = SocketOps::TLS_BUFFER_SIZE_DEFAULT);

		//==================================================================================================================
		// External accessor functions
		_Check_return_ const std::string& GetLastErrString() const noexcept {return LastErrString;}
		_Check_return_ bool Valid() const noexcept {
			return (SocketHandle != INVALID_SOCKET && (UsingTLS ? (hContext.dwLower || hContext.dwUpper) : true));
		}
		_Check_return_ bool SocketValid() const noexcept {return (SocketHandle != INVALID_SOCKET);}
		_Check_return_ bool IsSocket(SOCKET s) const noexcept {return (s == SocketHandle);}
		_Check_return_ bool TLSReady() const noexcept {
			return (
				ValueOps::Is(TLSBufSize).InRange(SocketOps::TLS_BUFFER_SIZE_MIN, SocketOps::TLS_BUFFER_SIZE_MAX)
				&& ReadBuf.get()
				&& ClearBuf.get()
			);
		}
		bool AddToFD(fd_set& fd) const noexcept(false) {
			return (fd.fd_count < FD_SETSIZE && SocketHandle != INVALID_SOCKET) ?
				(fd.fd_array[fd.fd_count++] = SocketHandle, true) : false;
		}
		void SetSessionFlags(SocketFlags sf) noexcept {SessionFlags |= sf;}
		std::string GetTLSCipherSuite() const {return StringOps::ConvertFromWideString(CipherInfo.szCipherSuite);}
		std::string GetTLSCipher() const {return StringOps::ConvertFromWideString(CipherInfo.szCipher);}
		std::string GetTLSHash() const {return StringOps::ConvertFromWideString(CipherInfo.szHash);}
		std::string GetTLSExchange() const {return StringOps::ConvertFromWideString(CipherInfo.szExchange);}
		//==================================================================================================================
		// Socket management functions
		_Check_return_ Result PollConnect(int TLSTimeout = 0, const std::string& TLSMethod = "");
		_Check_return_ Result WaitEvent(int Timeout) const;
		_Check_return_ Result Send(_In_reads_(len) const char* buf, size_t len);
		_Check_return_ Result ReadExact(_Out_writes_(BytesToRead) char* Tgt, size_t BytesToRead, int Timeout);
		_Check_return_ Result ReadAvailable(_Out_writes_(MaxBytes) char* Tgt, size_t MaxBytes, size_t& BytesRead);
		_Check_return_ Result ReadPacket(_Out_writes_(MaxBytes) char* Tgt, size_t MaxBytes, size_t& BytesRead, int Timeout);
		void Shutdown() {SocketOps::Shutdown(SocketHandle);}
		void Close() {
			SocketOps::Close(SocketHandle);
			// Reset state of TLS member variables (if any)
			ReadBufBytes = ClearBufBytes = 0;
			memset(&StreamSizes, 0, sizeof(StreamSizes));
			memset(&CipherInfo, 0, sizeof(CipherInfo));
			if(UsingTLS ? (hContext.dwLower || hContext.dwUpper) : false) {
				SocketOps::GetFunctionTable()->DeleteSecurityContext(&hContext);
				hContext.dwLower = hContext.dwUpper = 0;
			}
			ClientCred.reset(nullptr);
		}

		//==================================================================================================================
		// Public constructor (locked by private pass-key), public destructor
		SessionSocket(SocketOps::pass_key, bool _UsingTLS, size_t _TLSBufSize) : UsingTLS(_UsingTLS),
			TLSBufSize(ValueOps::Bounded(SocketOps::TLS_BUFFER_SIZE_MIN, _TLSBufSize, SocketOps::TLS_BUFFER_SIZE_MAX)),
			SocketHandle(INVALID_SOCKET), SessionFlags(SocketFlags::None),
			ReadBuf(nullptr), ReadBufBytes(0), ClearBuf(nullptr), ClearBufBytes(0) {
			static_assert(SocketOps::TLS_BUFFER_SIZE_MAX <= ULONG_MAX, "Invalid maximum TLS buffer size");
			if(UsingTLS) {
				ReadBuf = std::make_unique<char[]>(TLSBufSize + 10);
				ClearBuf = std::make_unique<char[]>(TLSBufSize + 10);
			}
		}
		~SessionSocket() noexcept(false) {Close();}
		// Deleted copy/move constructors and assignment operators
		SessionSocket(const SessionSocket&) = delete;
		SessionSocket(SessionSocket&&) = delete;
		SessionSocket& operator=(const SessionSocket&) = delete;
		SessionSocket& operator=(SessionSocket&&) = delete;

	private:

		// Private utility functions
		_Check_return_ Result TLSNegotiate(int Timeout, const std::string& Method);
		_Check_return_ Result SendTLS(_In_reads_(len) const char* buf, size_t len);
		_Check_return_ Result ReadExactTLS(_Out_writes_(BytesToRead) char* Tgt, size_t BytesToRead, int Timeout);
		_Check_return_ Result ReadAvailableTLS(_Out_writes_(MaxBytes) char* Tgt, size_t MaxBytes, size_t& BytesRead);
		_Check_return_ Result ReadPacketTLS(_Out_writes_(MaxBytes) char* Tgt, size_t MaxBytes, size_t& BytesRead, int Timeout);
		_Check_return_ Result PrivateReadTLS(int Timeout);

		// ClientCredentials: Container class for TLS context variables
		class ClientCredentials {
		public:
			_Check_return_ bool Valid() const noexcept {return (hCreds.dwLower || hCreds.dwUpper );}
			_Check_return_ CredHandle* GetCreds() noexcept {return &hCreds;}
			_Check_return_ bool Init(const std::string& TLSMethod, std::string& LastErrString);
			void Cleanup();
			// Public constructor/destructor
			ClientCredentials() noexcept = default;
			~ClientCredentials() noexcept(false) {Cleanup();}
			// Deleted copy/move constructors and assignment operators
			ClientCredentials(const ClientCredentials&) = delete;
			ClientCredentials(ClientCredentials&&) = delete;
			ClientCredentials& operator=(const ClientCredentials&) = delete;
			ClientCredentials& operator=(ClientCredentials&&) = delete;
		private:
			CredHandle hCreds = {0};
			SCHANNEL_CRED SchannelCred = {0};
		};

		// Private member variables
		const bool UsingTLS;				// Flag for whether TLS is in use for this session
		const size_t TLSBufSize;			// Number of bytes to be allocated to read buffers below (TLS-only)
		SOCKET SocketHandle;				// Handle to listener socket
		SocketFlags SessionFlags;			// SocketFlags values in use for this session
		mutable std::string LastErrString;	// Storage for last error on this socket

		// Private member variables - TLS-only
		std::unique_ptr<ClientCredentials> ClientCred = nullptr; // Pointer to client credentials (client mode only)
		CtxtHandle hContext = {0};						// Security context handle
		SecPkgContext_StreamSizes StreamSizes = {0};	// Cached stream buffer sizes (based on negotiated protocol)
		SecPkgContext_CipherInfo CipherInfo = {0};		// Negotiated cipher data
		std::unique_ptr<char[]>	ReadBuf;	// Standard TLS socket read buffer (holds raw incoming data)
		size_t ReadBufBytes;				// Number of bytes currently available in ReadBuf
		std::unique_ptr<char[]>	ClearBuf;	// Buffered TLS data buffer (holds decrypted cleartext data)
		size_t ClearBufBytes;				// Number of bytes currently available in ClearBuf

		// Access declarations
		friend ServerSocket; // Allow ServerSocket to access internal members
	};

private:

	// Private static object accessor functions
	_Check_return_ static HMODULE& GetSChannel() noexcept;
	_Check_return_ static PSecurityFunctionTable& GetFunctionTable() noexcept;
};

//==========================================================================================================================
#pragma region SocketOps
// SocketOps::InitServer: Create and initialize a SOCKET, bind it to the specified port and (optional) interface and listen
GSL_SUPPRESS(type.1) // reinterpret_cast is preferable to C-style cast (required for call to bind())
_Check_return_ inline SOCKET SocketOps::InitServer(
	unsigned short ListenPort, _In_opt_z_ const char* Interface, int Backlog, _Inout_opt_ std::string* LastErrString) {
	if(ListenPort == 0) {
		if(LastErrString) *LastErrString = "Invalid listening port";
		return INVALID_SOCKET;
	}
	else if(LastErrString) LastErrString->clear();
	try {
		const SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
		if(s != INVALID_SOCKET) {
			// Create binding object and attempt socket bind:
			sockaddr_in saddr = {0};
			saddr.sin_family = AF_INET;
			saddr.sin_port = htons(ListenPort);
			if((Interface ? Interface[0] : 0) != 0) inet_pton(saddr.sin_family, Interface, &saddr.sin_addr.s_addr);
			else saddr.sin_addr.s_addr = htonl(INADDR_ANY);
			if(bind(s, reinterpret_cast<const sockaddr*>(&saddr), sizeof(saddr)) != SOCKET_ERROR) {
				// Binding successful, attempt start listening and return socket if successful:
				if(listen(s, Backlog) != SOCKET_ERROR) return s;
				else if(LastErrString) *LastErrString = Exceptions::ConvertCOMError(WSAGetLastError());
			}
			else if(LastErrString) *LastErrString = Exceptions::ConvertCOMError(WSAGetLastError());
			closesocket(s);
		}
		else if(LastErrString) *LastErrString = Exceptions::ConvertCOMError(WSAGetLastError());
		return INVALID_SOCKET;
	}
	catch(const std::exception&) {std::throw_with_nested(FORMAT_RUNTIME_ERROR("Listener socket initialization failed"));}
}
// SocketOps::Accept: Accept incoming connection request (will block until client request avaialble)
GSL_SUPPRESS(type.1) // reinterpret_cast is preferable to C-style cast (required for call to accept())
_Check_return_ inline SOCKET SocketOps::Accept(
	SOCKET server, _Inout_opt_ sockaddr_in* saddr, _Inout_opt_ std::string* LastErrString) {
	// Validate inputs, default outputs:
	if(saddr != nullptr) memset(saddr, 0, sizeof(sockaddr_in));
	if(server == INVALID_SOCKET) {
		if(LastErrString) *LastErrString = "Invalid listening socket";
		return INVALID_SOCKET;
	}
	else if(LastErrString) LastErrString->clear();
	try {
		// Attempt to accept new socket connection:
		int saddr_len = sizeof(sockaddr_in);
		const SOCKET c = accept(server, reinterpret_cast<sockaddr*>(saddr), saddr ? &saddr_len : nullptr);
		if(c != INVALID_SOCKET) {
			// Connection accepted, set socket options before returning (enable keepalive, disable Nagle algorithm):
			constexpr char keepaliveopt = 1, nodelayopt = 1;
			setsockopt(c, SOL_SOCKET, SO_KEEPALIVE, &keepaliveopt, sizeof(keepaliveopt));
			setsockopt(c, IPPROTO_TCP, TCP_NODELAY, &nodelayopt, sizeof(nodelayopt));
		}
		else if(LastErrString) *LastErrString = Exceptions::ConvertCOMError(WSAGetLastError());
		return c;
	}
	catch(const std::exception&) {std::throw_with_nested(FORMAT_RUNTIME_ERROR("Session accept failed"));}
}
// SocketOps::DNSLookup: Retrieve IP address for specified name
GSL_SUPPRESS(type.1) // reinterpret_cast is preferable to C-style cast (required for call to inet_ntop())
_Check_return_ inline bool SocketOps::DNSLookup(
	_In_z_ const char* URL, _Out_writes_z_(20) char* TgtIP, _Inout_opt_ std::string* LastErrString) {
	if(LastErrString) LastErrString->clear();
	TgtIP[0] = 0;
	try {
		// Attempt DNS lookup call, if successful format IP address into destination variable:
		struct addrinfo hints = {0}, *ai = nullptr;
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		const int wrc = getaddrinfo(URL, nullptr, &hints, &ai);
		if((wrc == 0 ? ai : nullptr) != nullptr) {
			if(ai->ai_family == AF_INET) { // Only IPv4 currently supported
				const sockaddr_in* const si = reinterpret_cast<sockaddr_in*>(ai->ai_addr);
				if(inet_ntop(si->sin_family, &(si->sin_addr), TgtIP, 19) == nullptr) {
					if(LastErrString) *LastErrString = Exceptions::ConvertCOMError(wrc);
					TgtIP[0] = 0;
				}
			}
			else if(LastErrString) *LastErrString = "Invalid address family type";
			freeaddrinfo(ai);
		}
		else if(LastErrString) *LastErrString = Exceptions::ConvertCOMError(wrc);
		return (TgtIP[0] != 0);
	}
	catch(const std::exception&) {std::throw_with_nested(FORMAT_RUNTIME_ERROR("DNS name lookup failed"));}
}
// SocketOps::Connect: Attempt outbound connection to the specified IP and port
GSL_SUPPRESS(type.1) // reinterpret_cast is preferable to C-style cast (required for calls to bind() and connect())
_Check_return_ inline SOCKET SocketOps::Connect(
	_In_z_ const char* RemoteIP, unsigned short RemotePort, int Timeout, _Inout_opt_ std::string* LastErrString) {
	// Validate inputs, default outputs:
	if((RemoteIP ? RemoteIP[0] : 0) == 0 || RemotePort == 0) {
		if(LastErrString) *LastErrString = "Invalid destination IP/port";
		return INVALID_SOCKET;
	}
	else if(LastErrString) LastErrString->clear();

	SOCKET s = INVALID_SOCKET;
	try {
		// Attempt to create socket handle:
		s = socket(AF_INET, SOCK_STREAM, 0);
		if(s == INVALID_SOCKET && LastErrString) *LastErrString = Exceptions::ConvertCOMError(WSAGetLastError());
		if(s != INVALID_SOCKET) {
			// Set up destination object:
			sockaddr_in remaddr = {0};
			remaddr.sin_family = AF_INET;
			remaddr.sin_port = htons(RemotePort);
			inet_pton(remaddr.sin_family, RemoteIP, &remaddr.sin_addr.s_addr);
			// If timeout value provided, set socket to nonblocking mode:
			if(Timeout > 0) {
				unsigned long nbarg = 1;
				ioctlsocket(s, FIONBIO, &nbarg);
			}
			// Attempt connection to remote:
			if(connect(s, reinterpret_cast<const sockaddr*>(&remaddr), sizeof(remaddr)) == SOCKET_ERROR) {
				const int wsaerr = WSAGetLastError();
				if(wsaerr == WSAEWOULDBLOCK && Timeout > 0) {
					// Socket in nonblocking mode with timeout will return WOULDBLOCK; wait up to timeout for writability:
					fd_set fd = {1,s};
					timeval tv = {Timeout / 1000, (Timeout % 1000) * 1000};
					if(select(1, nullptr, &fd, nullptr, &tv) != 1) {
						if(LastErrString) *LastErrString = Exceptions::ConvertCOMError(wsaerr);
						SocketOps::Close(s);
					}
					// (else socket is connected)
				}
				else {
					if(LastErrString) *LastErrString = Exceptions::ConvertCOMError(wsaerr);
					SocketOps::Close(s);
				}
			}
			// If socket is still valid, it is successfully connected:
			if(s != INVALID_SOCKET) {
				// Set socket options before returning (enable keepalive, disable Nagle algorithm):
				constexpr char keepaliveopt = 1, nodelayopt = 1;
				setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, &keepaliveopt, sizeof(keepaliveopt));
				setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &nodelayopt, sizeof(nodelayopt));
				// Switch back to blocking mode, if timeout is in use:
				if(Timeout > 0) {
					unsigned long nbarg = 0;
					ioctlsocket(s, FIONBIO, &nbarg);
				}
				return s;
			}
		}
		return INVALID_SOCKET;
	}
	catch(const std::exception&) {
		SocketOps::Close(s); // Close socket handle, if valid
		std::throw_with_nested(FORMAT_RUNTIME_ERROR("Client socket connection failed"));
	}
}
// SocketOps::StartConnect: Initiate outbound connection in nonblocking mode
GSL_SUPPRESS(type.1) // reinterpret_cast is preferable to C-style cast (required for calls to bind() and connect())
_Check_return_ inline SOCKET SocketOps::StartConnect(
	_In_z_ const char* RemoteIP, unsigned short RemotePort, _Inout_opt_ std::string* LastErrString) {
	// Validate inputs, default outputs:
	if((RemoteIP ? RemoteIP[0] : 0) == 0 || RemotePort == 0) {
		if(LastErrString) *LastErrString = "Invalid destination IP/port";
		return INVALID_SOCKET;
	}
	else if(LastErrString) LastErrString->clear();

	SOCKET s = INVALID_SOCKET;
	try {
		// Attempt to create socket handle:
		s = socket(AF_INET, SOCK_STREAM, 0);
		if(s == INVALID_SOCKET && LastErrString) *LastErrString = Exceptions::ConvertCOMError(WSAGetLastError());
		if(s != INVALID_SOCKET) {
			// Set up destination object:
			sockaddr_in remaddr = {0};
			remaddr.sin_family = AF_INET;
			remaddr.sin_port = htons(RemotePort);
			inet_pton(remaddr.sin_family, RemoteIP, &remaddr.sin_addr.s_addr);
			// Set socket to nonblocking mode:
			{unsigned long nbarg = 1;
			ioctlsocket(s, FIONBIO, &nbarg);}
			// Attempt connection to remote:
			if(connect(s, reinterpret_cast<const sockaddr*>(&remaddr), sizeof(remaddr)) == SOCKET_ERROR) {
				const int wsaerr = WSAGetLastError();
				if(wsaerr != WSAEWOULDBLOCK) {
					if(LastErrString) *LastErrString = Exceptions::ConvertCOMError(wsaerr);
					SocketOps::Close(s);
				}
			}
		}
		return s;
	}
	catch(const std::exception&) {
		SocketOps::Close(s); // Close socket handle, if valid
		std::throw_with_nested(FORMAT_RUNTIME_ERROR("Client socket connection failed"));
	}
}
// SocketOps::PollConnect: Check if asynchronous connection has completed
_Check_return_ inline SocketOps::Result SocketOps::PollConnect(SOCKET s, _Inout_opt_ std::string* LastErrString) {
	if(s == INVALID_SOCKET) return Result::InvalidSocket;
	else if(LastErrString) LastErrString->clear();
	try {
		// Test socket for writability:
		fd_set fd = {1, s};
		timeval tv = {0, 0};
		const int sel = select(1, nullptr, &fd, nullptr, &tv);
		if(sel == 1) { // Socket is connected
			// Set socket options before returning (enable keepalive, disable Nagle algorithm):
			constexpr char keepaliveopt = 1, nodelayopt = 1;
			setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, &keepaliveopt, sizeof(keepaliveopt));
			setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &nodelayopt, sizeof(nodelayopt));
			// Switch back to blocking mode:
			unsigned long nbarg = 0;
			ioctlsocket(s, FIONBIO, &nbarg);
			return Result::OK;
		}
		else if(sel == 0) return Result::Timeout;
		else { // Socket error occurred
			if(LastErrString) *LastErrString = Exceptions::ConvertCOMError(WSAGetLastError());
			SocketOps::Shutdown(s);
			return Result::Failed;
		}
	}
	catch(const std::exception&) {std::throw_with_nested(FORMAT_RUNTIME_ERROR("Client socket connection polling failed"));}
}
// SocketOps::WaitEvent: Wait up to "Timeout" milliseconds for activity on input session socket
_Check_return_ inline SocketOps::Result SocketOps::WaitEvent(
	SOCKET s, int Timeout, _Inout_opt_ std::string* LastErrString) {
	if(s == INVALID_SOCKET) return Result::InvalidSocket;
	else if(LastErrString) LastErrString->clear();
	try {
		// Set up structures and perform select:
		fd_set fd = {1, s};
		timeval tv = {Timeout <= 0 ? 0 : (Timeout / 1000), Timeout <= 0 ? 0 : ((Timeout % 1000) * 1000)};
		const int sel = select(1, &fd, nullptr, nullptr, &tv);
		if(sel < 0) { // Socket error occurred
			if(LastErrString) *LastErrString = Exceptions::ConvertCOMError(WSAGetLastError());
			SocketOps::Shutdown(s);
			return Result::Failed;
		}
		else return sel > 0 ? Result::OK : Result::Timeout;
	}
	catch(const std::exception&) {std::throw_with_nested(FORMAT_RUNTIME_ERROR("Socket activity wait failed"));}
}
// SocketOps::Send: Deliver data to session socket
_Check_return_ inline SocketOps::Result SocketOps::Send(
	SOCKET s, _In_reads_(len) const char* buf, size_t len, _Inout_opt_ std::string* LastErrString) {
	// Validate inputs, default outputs:
	if(s == INVALID_SOCKET) return Result::InvalidSocket;
	else if(buf == nullptr || ValueOps::Is(len).InRangeLeft(1, INT_MAX) == false) return Result::InvalidArg;
	else if(LastErrString) LastErrString->clear();

	try {
		// Set up structures, perform select to ensure socket is writeable:
		Result rc = Result::Timeout;
		fd_set fd = {1, s};
		timeval tv = {0, 250000};
		const int slrc = select(1, nullptr, &fd, nullptr, &tv);
		if(slrc > 0) {
			// Socket is ready for writing; convert length to int (guaranteed safe by validation above) and attempt send;
			// if successful return immediately, otherwise fall through to logic below:
			const int ilen = gsl::narrow_cast<int>(len);
			const int src = send(s, buf, ilen, 0);
			if(src == ilen) return Result::OK; // Send successful
			else if(LastErrString && src <= 0) *LastErrString = Exceptions::ConvertCOMError(WSAGetLastError());
			rc = Result::Failed;
		}
		else if(slrc == 0) {
			if(LastErrString) *LastErrString = "Socket not ready for writing";
			rc = Result::Timeout;
		}
		else if(LastErrString) *LastErrString = Exceptions::ConvertCOMError(WSAGetLastError());
		// If this point is reached socket cannot be written to; shut down before returning:
		SocketOps::Shutdown(s);
		return rc;
	}
	catch(const std::exception&) {std::throw_with_nested(FORMAT_RUNTIME_ERROR("Data send failed"));}
}
// SocketOps::ReadExact: Read a specific number of bytes from session socket
_Check_return_ inline SocketOps::Result SocketOps::ReadExact(
	SOCKET s, _Out_writes_(BytesToRead) char* Tgt, size_t BytesToRead, int Timeout, _Inout_opt_ std::string* LastErrString) {
	// Validate inputs, default outputs:
	if(s == INVALID_SOCKET) return Result::InvalidSocket;
	else if(Tgt == nullptr || ValueOps::Is(BytesToRead).InRangeLeft(1, INT_MAX) == false) return Result::InvalidArg;
	else if(LastErrString) LastErrString->clear();

	// Set end time, convert bytes to read to int (guaranteed safe by above check):
	const TimeClock EndTime(Timeout);
	const int iBytesToRead = gsl::narrow_cast<int>(BytesToRead);
	int iBytesRead = 0;
	try {
		// Ensure data is available on socket (wait up to Timeout), if wait fails return now:
		const Result wrc = WaitEvent(s, ValueOps::MinZero(TimeClock::Now().MSecTill(EndTime)), LastErrString);
		if(wrc != Result::OK) return wrc;

		// Attempt to read specified number of bytes, return on failure OR if requested number of bytes read:
		iBytesRead = recv(s, Tgt, iBytesToRead, 0);
		if(iBytesRead <= 0) {
			if(iBytesRead < 0 && LastErrString) *LastErrString = Exceptions::ConvertCOMError(WSAGetLastError());
			SocketOps::Shutdown(s);
			return Result::Failed;
		}
		else if(iBytesRead >= iBytesToRead) return Result::OK;
		// (else continue below...)
	}
	catch(const std::exception&) {std::throw_with_nested(FORMAT_RUNTIME_ERROR("Inbound data read failed"));}

	// If this point is reached, no error occurred but not all bytes received; re-call this function, adjusted for
	// bytes that were read and time that has elapsed (performed outside try/catch to avoid recursive exceptions)
	return SocketOps::ReadExact(s, Tgt + iBytesRead,
		static_cast<size_t>(iBytesToRead) - iBytesRead, // iBytesRead guaranteed to be a positive value less than iBytesToRead
		TimeClock::Now().MSecTill(EndTime)
	);
}
// SocketOps::ReadAvailable: Read bytes currently available on session socket, up to MaxBytes
_Check_return_ inline SocketOps::Result SocketOps::ReadAvailable(
	SOCKET s, _Out_writes_(MaxBytes) char* Tgt, size_t MaxBytes, size_t& BytesRead, _Inout_opt_ std::string* LastErrString) {
	BytesRead = 0;
	// Validate inputs, default outputs:
	if(s == INVALID_SOCKET) return Result::InvalidSocket;
	else if(Tgt == nullptr || ValueOps::Is(MaxBytes).InRangeLeft(1, INT_MAX) == false) return Result::InvalidArg;
	else if(LastErrString) LastErrString->clear();
	try {
		// Attempt to read up to specified number of bytes (conversion to int guaranteed safe by above check):
		const int br = recv(s, Tgt, gsl::narrow_cast<int>(MaxBytes), 0);
		if(br <= 0) {
			if(br < 0 && LastErrString) *LastErrString = Exceptions::ConvertCOMError(WSAGetLastError());
			SocketOps::Shutdown(s);
			return Result::Failed;
		}
		BytesRead = br; // br is guaranteed to be positive value
		return Result::OK;
	}
	catch(const std::exception&) {std::throw_with_nested(FORMAT_RUNTIME_ERROR("Available data read failed"));}
}
// SocketOps::ReadPacket: Read a two-byte length header followed by the indicated number of bytes (up to MaxBytes)
_Check_return_ inline SocketOps::Result SocketOps::ReadPacket(
	SOCKET s, _Out_writes_(MaxBytes) char* Tgt, size_t MaxBytes, size_t& BytesRead, int Timeout, SocketFlags Flags,
	_Inout_opt_ std::string* LastErrString) {
	BytesRead = 0;
	// Validate inputs, default outputs:
	if(s == INVALID_SOCKET) return Result::InvalidSocket;
	else if(Tgt == nullptr || ValueOps::Is(MaxBytes).InRangeLeft(4, INT_MAX) == false) return Result::InvalidArg;
	else if(LastErrString) LastErrString->clear();
	try {
		// Read incoming packet header (2 bytes, or 4 if extended header enabled); return if read fails:
		const TimeClock EndTime(Timeout);
		Result rc = ReadExact(s, Tgt, (Flags & SocketFlags::ExtendedHeader) ? 4 : 2, Timeout, LastErrString);
		if(rc != Result::OK) return rc;

		// Calculate total packet length and validate against max read length (zero-length packet return success):
		const size_t PacketSize = ((Tgt[0] & 0xFF) << 8) | (Tgt[1] & 0xFF);
		if(PacketSize == 0) return Result::OK;
		else if(ValueOps::Is(PacketSize).InRange(1, MaxBytes) == false) {
			if(LastErrString) *LastErrString = "Length of incoming packet exceeds maximum";
			SocketOps::Shutdown(s); // Shut down socket to prevent fragmented packet being left on wire
			return Result::Failed;
		}

		// Otherwise, read specified number of bytes; if successful, assign output length variable:
		if((rc = ReadExact(s, Tgt, PacketSize, TimeClock::Now().MSecTill(EndTime), LastErrString)) == Result::OK) BytesRead = PacketSize;
		return rc;
	}
	catch(const std::exception&) {std::throw_with_nested(FORMAT_RUNTIME_ERROR("Packet read failed"));}
}
#pragma endregion SocketOps

//==========================================================================================================================
#pragma region SocketOps::ServerSocket
// ServerSocket::Open: Use SocketOps static function to initialize socket member
_Check_return_ inline bool SocketOps::ServerSocket::Open(unsigned short ListenPort, _In_opt_z_ const char* Interface, int Backlog) {
	return (SocketHandle != INVALID_SOCKET) ? true
		: ((SocketHandle = SocketOps::InitServer(ListenPort, Interface, Backlog, &LastErrString)) != INVALID_SOCKET);
}
// ServerSocket::WaitEvent: Use SocketOps static function to perform wait against socket member
_Check_return_ inline SocketOps::Result SocketOps::ServerSocket::WaitEvent(int Timeout) const {
	return SocketOps::WaitEvent(SocketHandle, Timeout, &LastErrString);
}
// ServerSocket::Accept: Use SocketOps static function to accept connection on socket member
// - If server has TLS credentials, performs TLS session negotiation on new connection before returning
_Check_return_ inline SocketOps::SessionSocketPtr SocketOps::ServerSocket::Accept(
	_Inout_opt_ sockaddr_in* saddr, int TLSTimeout, size_t TLSBufferSize) {
	// Create new SessionSocket object, passing along TLS values (if provided)
	SessionSocketPtr sp = std::make_unique<SessionSocket>(SocketOps::pass_key(), UsingTLS, TLSBufferSize);
	// Attempt to accept incoming connection, storing handle in object; perform TLS negotiation if required:
	sp->SocketHandle = SocketOps::Accept(SocketHandle, saddr, &(sp->LastErrString));
	if(sp->SocketValid() && UsingTLS) {
		if(ResultOK(TLSNegotiate(sp, TLSTimeout)) == false) sp->Close();
	}
	return sp;
}
#pragma endregion SocketOps::ServerSocket

//==========================================================================================================================
#pragma region SocketOps::SessionSocket
// SessionSocket::Connect: Static function to construct SessionSocket object and attempt outbound client connection
// - If TLS is required and connection succeeds, performs TLS session negotiation on new connection before returning
_Check_return_ inline SocketOps::SessionSocketPtr SocketOps::SessionSocket::Connect(
	_In_z_ const char* RemoteIP, unsigned short RemotePort, int Timeout,
	bool UsingTLS, const std::string& TLSMethod, size_t TLSBufferSize) {
	const TimeClock EndTime(Timeout);
	// Create new SessionSocket object, passing along TLS values (if provided)
	SessionSocketPtr sp = std::make_unique<SessionSocket>(SocketOps::pass_key(), UsingTLS, TLSBufferSize);
	// Attempt connection to remote, storing handle in object; perform TLS negotiation if required:
	sp->SocketHandle = SocketOps::Connect(RemoteIP, RemotePort, Timeout, &(sp->LastErrString));
	if(sp->SocketValid() && UsingTLS) {
		if(ResultOK(sp->TLSNegotiate(ValueOps::MinZero(TimeClock::Now().MSecTill(EndTime)), TLSMethod)) == false)
			sp->Close();
	}
	return sp;
}
_Check_return_ inline SocketOps::SessionSocketPtr SocketOps::SessionSocket::StartConnect(
	_In_z_ const char* RemoteIP, unsigned short RemotePort,	bool UsingTLS, size_t TLSBufferSize) {
	// Create new SessionSocket object, passing along TLS values (if provided)
	SessionSocketPtr sp = std::make_unique<SessionSocket>(SocketOps::pass_key(), UsingTLS, TLSBufferSize);
	// Initiate connection request to remote, storing handle in object:
	sp->SocketHandle = SocketOps::StartConnect(RemoteIP, RemotePort, &(sp->LastErrString));
	return sp;
}
// SessionSocket::PollConnect: Check if asynchronous connection has completed, perform TLS negotiation if required
_Check_return_ inline SocketOps::Result SocketOps::SessionSocket::PollConnect(int TLSTimeout, const std::string& TLSMethod) {
	Result rc = SocketOps::PollConnect(SocketHandle, &LastErrString);
	if(ResultOK(rc) && UsingTLS) {
		if(ResultOK((rc = TLSNegotiate(TLSTimeout, TLSMethod))) == false) Close();
	}
	return rc;
}
// SessionSocket::WaitEvent: Use SocketOps static function to perform wait against socket member
// - Checks read buffers (if any) prior to waiting on socket - returns OK immediately if buffered data available
_Check_return_ inline SocketOps::Result SocketOps::SessionSocket::WaitEvent(int Timeout) const {
	if(ClearBuf.get() ? (ClearBufBytes > 0) : false) return Result::OK;
	else if(ReadBuf.get() ? (ReadBufBytes > 0) : false) return Result::OK;
	else return SocketOps::WaitEvent(SocketHandle, Timeout, &LastErrString);
}
// SessionSocket::Send: Deliver data to open session
_Check_return_ inline SocketOps::Result SocketOps::SessionSocket::Send(_In_reads_(len) const char* buf, size_t len) {
	return UsingTLS ? SendTLS(buf, len)
		: SocketOps::Send(SocketHandle, buf, len, &LastErrString);
}
// SessionSocket::ReadExact: Read the specified number of bytes from open session
_Check_return_ inline SocketOps::Result SocketOps::SessionSocket::ReadExact(
	_Out_writes_(BytesToRead) char* Tgt, size_t BytesToRead, int Timeout) {
	return UsingTLS ? ReadExactTLS(Tgt, BytesToRead, Timeout)
		: SocketOps::ReadExact(SocketHandle, Tgt, BytesToRead, Timeout, &LastErrString);
}
// SessionSocket::ReadAvailable: Read up to specified number of bytes from data currently available on open session
_Check_return_ inline SocketOps::Result SocketOps::SessionSocket::ReadAvailable(
	_Out_writes_(MaxBytes) char* Tgt, size_t MaxBytes, size_t& BytesRead) {
	return UsingTLS ? ReadAvailableTLS(Tgt, MaxBytes, BytesRead)
		: SocketOps::ReadAvailable(SocketHandle, Tgt, MaxBytes, BytesRead, &LastErrString);
}
// SessionSocket::ReadPacket: Read two-byte length header followed by data from open session
_Check_return_ inline SocketOps::Result SocketOps::SessionSocket::ReadPacket(
	_Out_writes_(MaxBytes) char* Tgt, size_t MaxBytes, size_t& BytesRead, int Timeout) {
	return UsingTLS ? ReadPacketTLS(Tgt, MaxBytes, BytesRead, Timeout)
		: SocketOps::ReadPacket(SocketHandle, Tgt, MaxBytes, BytesRead, Timeout, SessionFlags, &LastErrString);
}
#pragma endregion SocketOps::SessionSocket

} // (end namespace FIQCPPBASE)
