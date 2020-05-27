//==========================================================================================================================
// SocketOps.cpp : Classes and functions for managing TCP/IP socket communications
//==========================================================================================================================
#include "pch.h"
#include "FileOps.h"
#include "SocketOps.h"
#include "StringOps.h"
using namespace FIQCPPBASE;

// Link libraries required for socket and TLS operations
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "secur32.lib")

//==========================================================================================================================
#pragma region SocketOps
// SocketOps::GetSChannel: Declares static HMODULE and returns by reference
_Check_return_ HMODULE& SocketOps::GetSChannel() noexcept {
	static HMODULE hSChannel = NULL;
	return hSChannel;
}
// SocketOps::GetFunctionTable: Declares static pointer to security function table and returns by reference
_Check_return_ PSecurityFunctionTable& SocketOps::GetFunctionTable() noexcept {
	static PSecurityFunctionTable pFunctionTable = nullptr;
	return pFunctionTable;
}
// SocketOps::Initialize: Start up Winsock library, initialize SChannel interface if required
GSL_SUPPRESS(type.1) // reinterpret_cast is preferable to C-style cast (required to set INIT_SECURITY_INTERFACE)
void SocketOps::InitializeSockets(bool TLSRequired) {
	int wsarc = -1;
	HMODULE hSChannel = NULL;
	try {
		// Initialize Winsock library, throw exception on failure
		{WSADATA wsa;
		if((wsarc = WSAStartup(0x0202, &wsa)) != 0) {
			throw std::runtime_error("WSAStartup: " + Exceptions::ConvertCOMError(wsarc));
		}}
		// If TLS requested, initialize SChannel:
		if(TLSRequired) {
			// Load DLL handle into local variable:
			if((hSChannel = LoadLibrary(L"SCHANNEL.DLL")) == NULL)
				throw std::runtime_error("LoadLibrary: " + Exceptions::ConvertCOMError(GetLastError()));
			// Get address of InitSecurityInterface function from DLL handle:
			INIT_SECURITY_INTERFACE pInitSecurityInterface =
				reinterpret_cast<INIT_SECURITY_INTERFACE>(GetProcAddress(hSChannel, SECURITY_ENTRYPOINT_ANSI));
			if(pInitSecurityInterface == NULL)
				throw std::runtime_error("GetProcAddress: " + Exceptions::ConvertCOMError(GetLastError()));
			// Use InitSecurityInterface to retrieve function table pointer:
			PSecurityFunctionTable pFunctionTable = pInitSecurityInterface();
			if(pFunctionTable == nullptr)
				throw std::runtime_error("InitSecurityInterface: " + Exceptions::ConvertCOMError(GetLastError()));

			// If this point is reached, initialization was successful; assign local handles to static members:
			SocketOps::GetSChannel() = hSChannel;
			SocketOps::GetFunctionTable() = pFunctionTable;
		}
	}
	catch(const std::exception&) {
		// If DLL handle was initialized, free now:
		if(hSChannel != NULL) FreeLibrary(hSChannel);
		// If WSAStartup was succesful before exception occurred, clean up now:
		if(wsarc == 0) WSACleanup();
		// Re-throw exception to caller (this function does not return a value):
		std::throw_with_nested(FORMAT_RUNTIME_ERROR("Socket library initialization failed"));
	}
}
// SocketOps::Cleanup: Close SChannel interface if required, clean up Winsock library
void SocketOps::CleanupSockets() {
	try {
		// Clear function table pointer (cleanup not required)
		SocketOps::GetFunctionTable() = nullptr;
		// Retrieve DLL handle from static member and clear; if it was initialized, free now:
		HMODULE hSChannel = SocketOps::GetSChannel();
		SocketOps::GetSChannel() = NULL;
		if(hSChannel != NULL) FreeLibrary(hSChannel);
		// Clean up Winsock library:
		WSACleanup();
	}
	catch(const std::exception&) {std::throw_with_nested(FORMAT_RUNTIME_ERROR("Socket library cleanup failed"));}
}
#pragma endregion SocketOps

//==========================================================================================================================
#pragma region SocketOps::ServerSocket
// ServerSocket::InitCredentialsFromStore: Open system certificate store and locate specified certificate
_Check_return_ bool SocketOps::ServerSocket::InitCredentialsFromStore(
	const std::string& CertName, const std::string& TLSMethod, bool LocalMachineStore) {
	UsingTLS = true; // Set flag to ensure credentials are validated on all future checks
	// Ensure this object has not yet been initialized, but that static members have:
	if(hCreds.dwLower || hCreds.dwUpper || pCertContext || hMyCertStore || SchannelCred.dwVersion) {
		LastErrString = "Credentials have already been initialized";
		return false;
	}
	else if(SocketOps::GetFunctionTable() == nullptr) {
		LastErrString = "SChannel library has not been initialized";
		return false;
	}
	try {
		// Open "Personal" key in either local machine store or current user store, based in input parameter:
		hMyCertStore = LocalMachineStore ?
			CertOpenStore(CERT_STORE_PROV_SYSTEM, 0, NULL, CERT_SYSTEM_STORE_LOCAL_MACHINE, L"MY")
			: CertOpenSystemStore(NULL, L"MY");
		if(hMyCertStore == nullptr) {
			LastErrString = (LocalMachineStore ? "CertOpenStore: " : "CertOpenSystemStore: ") + Exceptions::ConvertCOMError(GetLastError());
			return false;
		}
		// To locate requested certificate in store, search for certificate name; as some certificate names
		// may be a substring of longer names, do search in loop with full "CN=" checks on all matches:
		{const std::wstring wCertName = StringOps::ConvertToWideString(CertName);
		const std::wstring wCertSearchName(L"CN=" + wCertName);
		PCCERT_CONTEXT tempCertContext = nullptr;
		while(1) {
			// Perform search - note that providing tempCertContext as an input argument here will free any
			// resources acquired on previous iteration of this loop but discarded due to mismatched CN:
			if((tempCertContext = CertFindCertificateInStore(
				hMyCertStore, X509_ASN_ENCODING, 0, CERT_FIND_SUBJECT_STR, wCertName.c_str(), tempCertContext)) == nullptr) {
				LastErrString = "CertFindCertificateInStore: " + Exceptions::ConvertCOMError(GetLastError());
				return false;
			}
			// Retrieve certificate subject/name string into local buffer, and perform CN check:
			wchar_t CertNameTemp[260] = {0};
			const size_t s = CertNameToStr(
				X509_ASN_ENCODING, &(tempCertContext->pCertInfo->Subject), CERT_X500_NAME_STR, CertNameTemp, 255);
			if(ValueOps::Is(s).InRange(3, 255)) {
				if(std::wstring(CertNameTemp, s).find(wCertSearchName) != std::string::npos) {
					// Confirmed this certificate matches - assign to member pointer and break loop
					pCertContext = tempCertContext;
					break;
				}
			}
		}}
		// Double-check that CertContext was initialized (this should never happen, as
		// failure of FindCertificateInStore function should have returned above):
		if(pCertContext == nullptr) {
			LastErrString = CertName + ": Certificate not found";
			return false;
		}
		// Initialize common members, and return result:
		return CompleteInitCredentials(TLSMethod);
	}
	catch(const std::exception&) {std::throw_with_nested(FORMAT_RUNTIME_ERROR("Server credential initialization failed"));}
}
// ServerSocket::InitCredentialsFromFile: Open PKCS12 file and import certificate from store
_Check_return_ bool SocketOps::ServerSocket::InitCredentialsFromFile(
	const std::string& FileName, const std::string& FilePassword, const std::string& CertName, const std::string& TLSMethod) {
	UsingTLS = true; // Set flag to ensure credentials are validated on all future checks
	// Ensure this object has not yet been initialized, but that static members have:
	if(hCreds.dwLower || hCreds.dwUpper || pCertContext || hMyCertStore || SchannelCred.dwVersion) {
		LastErrString = "Credentials have already been initialized";
		return false;
	}
	else if(SocketOps::GetFunctionTable() == nullptr) {
		LastErrString = "SChannel library has not been initialized";
		return false;
	}
	try {
		{FileOps::FilePtr CertFile = FileOps::OpenFile(FileName.c_str(), "rb", _SH_DENYWR);
		if(CertFile.get() == nullptr) {
			LastErrString = Exceptions::ConvertCOMError(GetLastError());
			return false;
		}
		fseek(CertFile.get(), 0, SEEK_END); // Skip to end of file
		const long filesize = ftell(CertFile.get()); // Record total file length
		if(ValueOps::Is(filesize).InRange(0, 0x7FFF)) { // Validate length is within reasonable boundary
			// Allocate buffer to hold file contents, return file pointer to start of file and read contents:
			std::unique_ptr<BYTE[]> FileBuf = std::make_unique<BYTE[]>(static_cast<size_t>(filesize) + 5);
			fseek(CertFile.get(), 0, SEEK_SET);
			if(fread(FileBuf.get(), sizeof(BYTE), filesize, CertFile.get()) == filesize) {
				// Construct CRYPT_DATA_BLOB object from buffer and size, open certificate store from it:
				CRYPT_DATA_BLOB fdata = {gsl::narrow_cast<DWORD>(filesize), FileBuf.get()};
				hMyCertStore = PFXImportCertStore(&fdata, StringOps::ConvertToWideString(FilePassword).c_str(), 0);
				if(hMyCertStore == nullptr) {
					LastErrString = "PFXImportCertStore: " + Exceptions::ConvertCOMError(GetLastError());
					return false;
				}
				// Locate individual certificate by subject name (note this assumes there are no overlapping entries):
				pCertContext = CertFindCertificateInStore(
					hMyCertStore, X509_ASN_ENCODING, 0, CERT_FIND_SUBJECT_STR,
					StringOps::ConvertToWideString(CertName).c_str(), pCertContext);
				if(pCertContext == nullptr) {
					LastErrString = "CertFindCertificateInStore: " + Exceptions::ConvertCOMError(GetLastError());
					return false;
				}
			}
			else { // Read failed
				LastErrString = Exceptions::ConvertCOMError(GetLastError());
				return false;
			}
		}
		else { // Length invalid
			LastErrString = "Certificate file store length invalid";
			return false;
		}}
		// Note that at the end of this block file pointer has been closed, memory buffer and DATA_BLOB objects have all
		// been destructed, PFXImportCertStore has imported certificate store into memory (with hMyCertStore holding a
		// reference to it), and pCertContext has been set; initialize common members and return result:
		return CompleteInitCredentials(TLSMethod);
	}
	catch(const std::exception&) {std::throw_with_nested(FORMAT_RUNTIME_ERROR("Server credential initialization failed"));}
}
// ServerSocket::CleanupCredentials: Free any handles required
void SocketOps::ServerSocket::CleanupCredentials() {
	// Free all allocated objects and reset to default values:
	if(hCreds.dwLower != 0 || hCreds.dwUpper != 0) FreeCredentialsHandle(&hCreds);
	hCreds.dwLower = hCreds.dwUpper = 0;
	if(pCertContext) CertFreeCertificateContext(pCertContext);
	pCertContext = nullptr;
	if(hMyCertStore) CertCloseStore(hMyCertStore, 0);
	hMyCertStore = nullptr;
	memset(&SchannelCred, 0, sizeof(SchannelCred));
}
// ServerSocket::CompleteInitCredentials: Initialize SChannel object and acquire hCreds handles
// - Before this function executes, caller should have opened certificate store and loaded cert context
_Check_return_ bool SocketOps::ServerSocket::CompleteInitCredentials(const std::string& TLSMethod) {
	// Set up SChannel structure values:
	SchannelCred.dwVersion = SCHANNEL_CRED_VERSION;
	SchannelCred.cCreds = 1;
	SchannelCred.paCred = &pCertContext; // Already initialized by caller
	// Default protocols to TLSv1.2 only; allow override to downgrade based on Method:
	SchannelCred.grbitEnabledProtocols = SP_PROT_TLS1_2_SERVER;
	if(TLSMethod.length() >= 3) {
		if(_stricmp(TLSMethod.c_str(), "TLSV1.1") == 0) // TLS1.1+ protocols only
			SchannelCred.grbitEnabledProtocols = SP_PROT_TLS1_1PLUS_SERVER;
		else if(_stricmp(TLSMethod.c_str(), "TLSV1") == 0) // TLS1.0+ protocols only
			SchannelCred.grbitEnabledProtocols = SP_PROT_TLS1_SERVER | SP_PROT_TLS1_1PLUS_SERVER;
	}
	SchannelCred.dwFlags = (SCH_USE_STRONG_CRYPTO | SCH_CRED_NO_SYSTEM_MAPPER | SCH_SEND_ROOT_CERT);
	const SECURITY_STATUS s = SocketOps::GetFunctionTable()->AcquireCredentialsHandle(
		NULL, const_cast<wchar_t*>(UNISP_NAME), SECPKG_CRED_INBOUND, NULL, &SchannelCred, NULL, NULL, &hCreds, NULL);
	if(s != SEC_E_OK) {
		LastErrString = "AcquireCredentialsHandle: " + Exceptions::ConvertCOMError(s);
		return false;
	}
	return CredentialsValid();
}
// ServerSocket::TLSNegotiate: Perform server-side TLS negotiation on new socket connection
_Check_return_ SocketOps::Result SocketOps::ServerSocket::TLSNegotiate(
	const SocketOps::SessionSocketPtr& sp, int Timeout) {
	// Validate input socket, validate this object's TLS credentials
	if((sp.get() ? sp->TLSReady() : false) == false) {
		sp->LastErrString = "TLS negotiation failed: Invalid socket";
		return Result::InvalidSocket;
	}
	else if(CredentialsValid() == false) {
		sp->LastErrString = "TLS negotiation failed: Server credentials not initialized";
		return Result::InvalidArg;
	}
	sp->LastErrString.clear();
	LastErrString.clear();
	try {
		// Set up function constants - end time, end-of-buffer marker:
		const SteadyClock EndTime(std::chrono::milliseconds{Timeout});
		const char * const EndPtr = sp->ReadBuf.get() + sp->TLSBufSize;

		// Create security context initialization variables; OutBufferDesc members will remain
		// constant (only contents of OutBuffers[0] will change), so set up now:
		SecBufferDesc InBufferDesc = {0}, OutBufferDesc = {0};
		SecBuffer InBuffers[2] = {0}, OutBuffers[1] = {0};
		OutBufferDesc.cBuffers = 1;
		OutBufferDesc.pBuffers = OutBuffers;
		OutBufferDesc.ulVersion = SECBUFFER_VERSION;

		SECURITY_STATUS scRet = SEC_I_CONTINUE_NEEDED;
		Result rc = Result::Timeout;
		//==================================================================================================================
		// Main negotiation loop start
		for(short s = 0; s < 15 && rc == Result::Timeout && Timeout >= 0; ++s) {
			if(sp->ReadBufBytes == 0 || scRet == SEC_E_INCOMPLETE_MESSAGE) {
				// We are waiting on further data from socket; ensure there is space remaining in read buffer:
				char * const ReadPtr = sp->ReadBuf.get() + sp->ReadBufBytes;
				const size_t MaxBytes = StringOps::BytesAvail(ReadPtr, EndPtr);
				if(ValueOps::Is(MaxBytes).InRange(1, sp->TLSBufSize) == false) {
					sp->Shutdown();
					rc = Result::Failed;
					sp->LastErrString = "TLS negotiation failed: Buffer size exceeded";
					break;
				}
				// Wait for data to arrive on socket, read if available:
				Result src = sp->WaitEvent(Timeout);
				if(ResultOK(src)) {
					size_t br = 0;
					if(ResultOK(src = SocketOps::ReadAvailable(sp->SocketHandle, ReadPtr, MaxBytes, br, &LastErrString))) {
						if(ValueOps::Is(br).InRange(1, MaxBytes)) sp->ReadBufBytes += br;
						else {
							sp->Shutdown();
							src = Result::Failed;
							if(br != 0) sp->LastErrString = "TLS negotiation failed: Invalid number of bytes read";
						}
					}
					else if(LastErrString.empty()) sp->LastErrString = "TLS negotiation failed: Socket read error";
					else sp->LastErrString = "TLS negotiation failed: " + LastErrString;
				}
				// If error occurred (on wait or read) or either operation timed out,
				// set result and break now; LastErrString should already be set
				if(ResultOK(src) == false) {
					rc = src;
					break;
				}
			}

			// Set up TLS objects - assign incoming data as token (with NULL array terminator), attempt accept:
			InBuffers[0].pvBuffer = sp->ReadBuf.get();
			InBuffers[0].cbBuffer = gsl::narrow_cast<unsigned long>(sp->ReadBufBytes);
			InBuffers[0].BufferType = SECBUFFER_TOKEN;
			InBuffers[1].pvBuffer = nullptr;
			InBuffers[1].cbBuffer = 0;
			InBuffers[1].BufferType = SECBUFFER_EMPTY;
			InBufferDesc.cBuffers = 2;
			InBufferDesc.pBuffers = InBuffers;
			InBufferDesc.ulVersion = SECBUFFER_VERSION;
			OutBuffers[0].pvBuffer = nullptr;
			OutBuffers[0].cbBuffer = 0;
			OutBuffers[0].BufferType = SECBUFFER_EMPTY;
			DWORD dwSSPIOutFlags = 0;
			scRet = SocketOps::GetFunctionTable()->AcceptSecurityContext(
				&hCreds,
				// Only pass in hContext (input) if initialized:
				sp->hContext.dwLower == 0 && sp->hContext.dwUpper == 0 ? nullptr : &(sp->hContext),
				&InBufferDesc,
				ASC_REQ_SEQUENCE_DETECT | ASC_REQ_REPLAY_DETECT | ASC_REQ_CONFIDENTIALITY | ASC_RET_EXTENDED_ERROR | ASC_REQ_ALLOCATE_MEMORY | ASC_REQ_STREAM,
				0,
				// Only pass in hContext (output) if NOT initialized:
				sp->hContext.dwLower == 0 && sp->hContext.dwUpper == 0 ? &(sp->hContext) : nullptr,
				&OutBufferDesc,
				&dwSSPIOutFlags,
				nullptr);

			// If SECBUFFER_TOKEN was provided (via OutBuffer) by AcceptSecurityContext (meaning accept function succeeded,
			// continuation required or an error has occurred and client wants extended error data), send now:
			if(scRet >= 0 || ((dwSSPIOutFlags & ASC_RET_EXTENDED_ERROR) != 0)) {
				if(OutBuffers[0].cbBuffer != 0 && OutBuffers[0].pvBuffer != nullptr) {
					const Result src = SocketOps::Send(sp->SocketHandle,
						static_cast<const char*>(OutBuffers[0].pvBuffer), OutBuffers[0].cbBuffer, &LastErrString);
					SocketOps::GetFunctionTable()->FreeContextBuffer(OutBuffers[0].pvBuffer);
					OutBuffers[0].pvBuffer = nullptr;
					if(ResultOK(src) == false) {
						rc = src;
						if(LastErrString.empty()) sp->LastErrString = "TLS negotiation failed: Socket send failed";
						else sp->LastErrString = "TLS negotiation failed: " + LastErrString;
						break;
					}
				}
			}

			// If function was successful (or continuation required) but data has been read beyond end of last token,
			// save in ReadBuf for subsequent read operations (this represents start of data after negotiation):
			if(scRet >= 0 && InBuffers[1].BufferType == SECBUFFER_EXTRA) {
				if(ValueOps::Is<size_t>(InBuffers[1].cbBuffer).InRange(1, sp->ReadBufBytes)) {
					memcpy(sp->ReadBuf.get(), sp->ReadBuf.get() + sp->ReadBufBytes - InBuffers[1].cbBuffer, InBuffers[1].cbBuffer);
					sp->ReadBufBytes = InBuffers[1].cbBuffer;
				}
				else sp->ReadBufBytes = 0; // Data is invalid length - discard
			}
			else if(scRet >= 0) sp->ReadBufBytes = 0; // Clear read buffer

			// If continuation needed, continue loop (note this is the only way for this loop to execute more than
			// once as all other paths below will break):
			if(scRet == SEC_E_INCOMPLETE_MESSAGE || scRet > 0) {
				Timeout = SteadyClock().MSecTill(EndTime);
				continue;
			}
			else if(scRet != SEC_E_OK) {
				// Non-wait error occurred; set error data:
				sp->Shutdown();
				rc = Result::Failed;
				sp->LastErrString = "TLS negotiation failed: " + Exceptions::ConvertCOMError(scRet);
				break;
			}
			// Negotiation complete; retrieve cipher info:
			if((scRet = SocketOps::GetFunctionTable()->QueryContextAttributes(
				&(sp->hContext), SECPKG_ATTR_CIPHER_INFO, &(sp->CipherInfo))) != SEC_E_OK) {
				sp->Shutdown();
				rc = Result::Failed;
				sp->LastErrString = "QueryContextAttributes (CIPHER_INFO): " + Exceptions::ConvertCOMError(scRet);
				break;
			}
			// Retrieve stream sizes for negotiated protocol:
			if((scRet = SocketOps::GetFunctionTable()->QueryContextAttributes(
				&sp->hContext, SECPKG_ATTR_STREAM_SIZES, &sp->StreamSizes)) != SEC_E_OK) {
				sp->Shutdown();
				rc = Result::Failed;
				sp->LastErrString = "QueryContextAttributes (STREAM_SIZES): " + Exceptions::ConvertCOMError(scRet);
				break;
			}
			// Validate stream sizes (we must be able to process a packet with header and trailer):
			if((static_cast<size_t>(sp->StreamSizes.cbHeader) + sp->StreamSizes.cbTrailer) >= sp->TLSBufSize) {
				sp->Shutdown();
				rc = Result::Failed;
				sp->LastErrString = "TLS negotiation failed: Protocol message size exceeds maximum";
				break;
			}

			// If this point is reached, negotiation successful - break loop now:
			sp->TLSComplete = true;
			rc = Result::OK;
			break;
		}
		// Main negotiation loop end
		//==================================================================================================================
		return rc;
	}
	catch(const std::exception&) {std::throw_with_nested(FORMAT_RUNTIME_ERROR("TLS negotiation (server) failed"));}
}
#pragma endregion SocketOps::ServerSocket

//==========================================================================================================================
#pragma region SocketOps::SessionSocket
// SessionSocket::ClientCredentials::Init: Initialize client credential values
_Check_return_ bool SocketOps::SessionSocket::ClientCredentials::Init(const std::string& TLSMethod, std::string& LastErrString) {
	LastErrString.clear();
	// Ensure this object has not yet been initialized, but that static members have:
	if(SchannelCred.dwVersion != 0) {
		LastErrString = "Credentials have already been initialized";
		return false;
	}
	else if(SocketOps::GetFunctionTable() == nullptr) {
		LastErrString = "SChannel library has not been initialized";
		return false;
	}
	SchannelCred.dwVersion = SCHANNEL_CRED_VERSION;
	// Default protocols to TLSv1.2 only; allow override to downgrade based on Method:
	SchannelCred.grbitEnabledProtocols = SP_PROT_TLS1_2_CLIENT;
	if(TLSMethod.length() >= 3) {
		if(_stricmp(TLSMethod.c_str(), "TLSV1.1") == 0) // TLS 1.1+ protocols only
			SchannelCred.grbitEnabledProtocols = SP_PROT_TLS1_1PLUS_CLIENT;
		else if(_stricmp(TLSMethod.c_str(), "TLSV1") == 0) // TLS protocols only
			SchannelCred.grbitEnabledProtocols = SP_PROT_TLS1_CLIENT | SP_PROT_TLS1_1PLUS_CLIENT;
	}
	// By default do not validate server credentials (may be added in future):
	SchannelCred.dwFlags = (SCH_USE_STRONG_CRYPTO | SCH_CRED_NO_DEFAULT_CREDS
		| SCH_CRED_MANUAL_CRED_VALIDATION | SCH_CRED_NO_SERVERNAME_CHECK);
	const SECURITY_STATUS s = SocketOps::GetFunctionTable()->AcquireCredentialsHandle(
		NULL, const_cast<wchar_t*>(UNISP_NAME), SECPKG_CRED_OUTBOUND, NULL, &SchannelCred, NULL, NULL, &hCreds, NULL);
	if(s != SEC_E_OK) {
		LastErrString = "AcquireCredentialsHandle: " + Exceptions::ConvertCOMError(s);
		return false;
	}
	return Valid();
}
// SessionSocket::ClientCredentials::Cleanup: Release client credential values
void SocketOps::SessionSocket::ClientCredentials::Cleanup() {
	if(hCreds.dwLower || hCreds.dwUpper) FreeCredentialsHandle(&hCreds);
	hCreds.dwLower = hCreds.dwUpper = 0;
}
// SessionSocket::TLSNegotiate: Perform client-side negotiation on socket connection
_Check_return_ SocketOps::Result SocketOps::SessionSocket::TLSNegotiate(int Timeout, const std::string& Method) {
	if((SocketValid() ? TLSReady() : false) == false) {
		LastErrString = "TLS negotiation failed: Invalid socket";
		return Result::InvalidSocket;
	}
	LastErrString.clear();
	const SteadyClock EndTime(std::chrono::milliseconds{Timeout});

	static constexpr DWORD ContextFlags = (ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT | ISC_REQ_CONFIDENTIALITY | ISC_RET_EXTENDED_ERROR | ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM);

	if(ClientCred.get() == nullptr) {
		//==================================================================================================================
		// Client credential and handshake begin
		try {
			ClientCred = std::make_unique<ClientCredentials>();
			Result rc = Result::Failed;
			if(ClientCred->Init(Method, LastErrString)) {

				// Create and deliver TLS token to start negotiation:
				SecBufferDesc OutBufferDesc = {0};
				SecBuffer OutBuffers[1] = {0};
				OutBuffers[0].pvBuffer = nullptr;
				OutBuffers[0].cbBuffer = 0;
				OutBuffers[0].BufferType = SECBUFFER_TOKEN;
				OutBufferDesc.cBuffers = 1;
				OutBufferDesc.pBuffers = OutBuffers;
				OutBufferDesc.ulVersion = SECBUFFER_VERSION;
				DWORD dwSSPIOutFlags = 0;
				const SECURITY_STATUS scRet = SocketOps::GetFunctionTable()->InitializeSecurityContext(
					ClientCred->GetCreds(),
					nullptr,
					nullptr, 
					ContextFlags,
					0,
					SECURITY_NATIVE_DREP, 
					nullptr,
					0,
					&hContext,
					&OutBufferDesc,
					&dwSSPIOutFlags,
					nullptr);

				// Expected result is CONTINUE_NEEDED (token has been constructed, and now needs to
				// be sent [in plaintext] to remote peer to start TLS handshake process)
				if(scRet == SEC_I_CONTINUE_NEEDED && OutBuffers[0].cbBuffer != 0 && OutBuffers[0].pvBuffer != nullptr)
					rc = SocketOps::Send(SocketHandle, static_cast<const char*>(OutBuffers[0].pvBuffer), OutBuffers[0].cbBuffer, &LastErrString);
				else if(scRet != SEC_I_CONTINUE_NEEDED) LastErrString = "InitializeSecurityContext: " + Exceptions::ConvertCOMError(scRet);
				else LastErrString = "InitializeSecurityContext: Error constructing token";

				// Free any memory allocated above
				if(OutBuffers[0].cbBuffer != 0 && OutBuffers[0].pvBuffer != nullptr)
					SocketOps::GetFunctionTable()->FreeContextBuffer(OutBuffers[0].pvBuffer);
			}

			// If above process did not complete successfully, return now:
			if(ResultOK(rc) == false) {
				Shutdown();
				return rc;
			}
		}
		catch(const std::exception&) {std::throw_with_nested(FORMAT_RUNTIME_ERROR("TLS initialization failed"));}
		// Client credential and handshake end
		//==================================================================================================================
	}

	//======================================================================================================================
	// Primary TLS negotiation loop begin
	try {
		Result rc = Result::Timeout;
		const char * const EndPtr = ReadBuf.get() + TLSBufSize;

		// Create TLS objects; OutBufferDesc members will remain constant (only contents of OutBuffers[0] will change):
		SecBufferDesc InBufferDesc = {0}, OutBufferDesc = {0};
		SecBuffer InBuffers[2] = {0}, OutBuffers[1] = {0};
		OutBufferDesc.cBuffers = 1;
		OutBufferDesc.pBuffers = OutBuffers;
		OutBufferDesc.ulVersion = SECBUFFER_VERSION;

		// Update time remaining, but ensure loop executes at least once:
		Timeout = ValueOps::MinZero(SteadyClock().MSecTill(EndTime));

		SECURITY_STATUS scRet = SEC_I_CONTINUE_NEEDED;
		for(short s = 0; s < 15 && rc == Result::Timeout && Timeout >= 0; ++s) {

			// If there is no data in buffer or previous message was incomplete, we need to read data:
			if(ReadBufBytes == 0 || scRet == SEC_E_INCOMPLETE_MESSAGE) {

				// Ensure there is space remaining in buffer and all pointers are valid:
				char * const ReadPtr = ReadBuf.get() + ReadBufBytes;
				const size_t MaxBytes = StringOps::BytesAvail(ReadPtr, EndPtr);
				if(ValueOps::Is(MaxBytes).InRange(1, TLSBufSize) == false) {
					Shutdown();
					rc = Result::Failed;
					LastErrString = "Maximum data length exceeded";
					break;
				}

				// Wait up to timeout for data to arrive on socket, read if available:
				Result src = WaitEvent(Timeout);
				if(ResultOK(src)) {
					size_t br = 0;
					if(ResultOK(src = SocketOps::ReadAvailable(SocketHandle, ReadPtr, MaxBytes, br, &LastErrString))) {
						if(ValueOps::Is(br).InRange(1, MaxBytes) == false) {
							Shutdown();
							src = Result::Failed;
							if(br != 0) LastErrString = "Invalid number of bytes read";
						}
						else ReadBufBytes += br;
					}
				}
				// If error occurred (on wait or read) or either operation timed out,
				// set result and break now; LastErrString should already be set
				if(ResultOK(src) == false) {
					rc = src;
					break;
				}
			}

			// Now take data we have received, assign it as token in TLS objects and attempt initialization:
			InBuffers[0].pvBuffer = ReadBuf.get();
			InBuffers[0].cbBuffer = gsl::narrow_cast<unsigned long>(ReadBufBytes);
			InBuffers[0].BufferType = SECBUFFER_TOKEN;
			InBuffers[1].pvBuffer = nullptr;
			InBuffers[1].cbBuffer = 0;
			InBuffers[1].BufferType = SECBUFFER_EMPTY;
			InBufferDesc.cBuffers = 2;
			InBufferDesc.pBuffers = InBuffers;
			InBufferDesc.ulVersion = SECBUFFER_VERSION;
			OutBuffers[0].pvBuffer = nullptr;
			OutBuffers[0].cbBuffer = 0;
			OutBuffers[0].BufferType = SECBUFFER_TOKEN;
			DWORD dwSSPIOutFlags = 0;
			scRet = SocketOps::GetFunctionTable()->InitializeSecurityContext(
				ClientCred->GetCreds(),
				&hContext,
				nullptr,
				ContextFlags,
				0,
				SECURITY_NATIVE_DREP,
				&InBufferDesc,
				0,
				nullptr,
				&OutBufferDesc,
				&dwSSPIOutFlags,
				nullptr);

			// If security context was successfully initialized OR continuation is required OR
			// an error has occurred and client wants to receive extended error data, send outbound
			// SECBUFFER_TOKEN if InitializeSecurityContext provided it via OutBuffer:
			if(scRet >= 0 || ((dwSSPIOutFlags & ISC_RET_EXTENDED_ERROR) != 0)) {
				if(OutBuffers[0].cbBuffer != 0 && OutBuffers[0].pvBuffer != nullptr) {
					const SocketOps::Result src = SocketOps::Send(SocketHandle,
						static_cast<const char*>(OutBuffers[0].pvBuffer), OutBuffers[0].cbBuffer,
						&LastErrString);
					SocketOps::GetFunctionTable()->FreeContextBuffer(OutBuffers[0].pvBuffer);
					OutBuffers[0].pvBuffer = nullptr;
					// If delivery failed, break loop now
					if(ResultOK(src) == false) {
						rc = src; // LastErrString should already have been set
						break;
					}
				}
			}

			// If security context was successfully initiated or continuation is required, but
			// data has been read beyond end of token, we need to save for subsequent reads:
			if(scRet >= 0 && InBuffers[1].BufferType == SECBUFFER_EXTRA) {
				if(ValueOps::Is(static_cast<size_t>(InBuffers[1].cbBuffer)).InRange(1, ReadBufBytes)) {
					memcpy(ReadBuf.get(), ReadBuf.get() + ReadBufBytes - InBuffers[1].cbBuffer, InBuffers[1].cbBuffer);
					ReadBufBytes = InBuffers[1].cbBuffer;
				}
				else ReadBufBytes = 0; // Invalid data length - discard
			}
			else if(scRet >= 0) ReadBufBytes = 0; // Operation complete, clear read buffer

			// If response code indicates continue needed, continue loop (note this is the only way
			// for this loop to ever execute more than once, as all other paths below will break):
			if(scRet == SEC_E_INCOMPLETE_MESSAGE || scRet > 0) {
				Timeout = SteadyClock().MSecTill(EndTime);
				continue;
			}
			else if(scRet != SEC_E_OK) {
				// Non-wait error occurred; set error data:
				Shutdown();
				rc = Result::Failed;
				LastErrString = "TLS negotiation failed: " + Exceptions::ConvertCOMError(scRet);
				break;
			}
			// Negotiation complete; retrieve cipher info:
			if((scRet = SocketOps::GetFunctionTable()->QueryContextAttributes(
				&hContext, SECPKG_ATTR_CIPHER_INFO, &CipherInfo)) != SEC_E_OK) {
				Shutdown();
				rc = Result::Failed;
				LastErrString = "QueryContextAttributes (CIPHER_INFO): " + Exceptions::ConvertCOMError(scRet);
				break;
			}
			// Retrieve stream sizes for negotiated protocol:
			if((scRet = SocketOps::GetFunctionTable()->QueryContextAttributes(
				&hContext, SECPKG_ATTR_STREAM_SIZES, &StreamSizes)) != SEC_E_OK) {
				Shutdown();
				rc = Result::Failed;
				LastErrString = "QueryContextAttributes (STREAM_SIZES): " + Exceptions::ConvertCOMError(scRet);
				break;
			}
			// Validate stream sizes (we must be able to process a packet with header and trailer):
			if((static_cast<size_t>(StreamSizes.cbHeader) + StreamSizes.cbTrailer) >= TLSBufSize) {
				Shutdown();
				rc = Result::Failed;
				LastErrString = "TLS negotiation failed: Protocol message size exceeds maximum";
				break;
			}

			// If this point is reached, negotiation successful - break loop now:
			TLSComplete = true;
			rc = Result::OK;
			break;
		}
		return rc;
	}
	catch(const std::exception&) {std::throw_with_nested(FORMAT_RUNTIME_ERROR("TLS negotiation (client) failed"));}
	// Primary TLS negotiation loop end
	//======================================================================================================================
}
// SessionSocket::SendTLS:
_Check_return_ SocketOps::Result SocketOps::SessionSocket::SendTLS(_In_reads_(len) const char* buf, size_t len) {
	if(Valid() == false) return Result::InvalidSocket;
	else if(buf == nullptr || ValueOps::Is(len).InRange(1, TLSBufSize) == false) return Result::InvalidArg;
	else if (len > StreamSizes.cbMaximumMessage) return Result::InvalidArg;

	try {
		// Allocate buffer for full outbound message plus header/trailer, copy outbound data ahead of trailer space:
		std::unique_ptr<char[]> SendBuf = std::make_unique<char[]>(StreamSizes.cbHeader + len + StreamSizes.cbTrailer + 10);
		memcpy(SendBuf.get() + StreamSizes.cbHeader, buf, len);
		// Build TLS variables and attempt encryption:
		SecBuffer Buffers[4] = {0};
		Buffers[0].pvBuffer = SendBuf.get();
		Buffers[0].cbBuffer = StreamSizes.cbHeader;
		Buffers[0].BufferType = SECBUFFER_STREAM_HEADER;
		Buffers[1].pvBuffer = SendBuf.get() + StreamSizes.cbHeader;
		Buffers[1].cbBuffer = gsl::narrow_cast<unsigned long>(len);
		Buffers[1].BufferType = SECBUFFER_DATA;
		Buffers[2].pvBuffer = SendBuf.get() + StreamSizes.cbHeader + len;
		Buffers[2].cbBuffer = StreamSizes.cbTrailer;
		Buffers[2].BufferType = SECBUFFER_STREAM_TRAILER;
		Buffers[3].BufferType = SECBUFFER_EMPTY;
		SecBufferDesc BufferDesc = {0};
		BufferDesc.ulVersion = SECBUFFER_VERSION;
		BufferDesc.cBuffers = 4;
		BufferDesc.pBuffers = Buffers;
		const SECURITY_STATUS scRet = SocketOps::GetFunctionTable()->EncryptMessage(&hContext, 0, &BufferDesc, 0);
		if(scRet != SEC_E_OK) {
			Shutdown();
			LastErrString = "EncryptMessage: " + Exceptions::ConvertCOMError(scRet);
			return Result::Failed;
		}
		// Message encrypted, deliver length of header plus data plus footer to socket:
		return SocketOps::Send(SocketHandle,
			SendBuf.get(),
			static_cast<size_t>(Buffers[0].cbBuffer) + Buffers[1].cbBuffer + Buffers[2].cbBuffer,
			&LastErrString);
	}
	catch(const std::exception&) {std::throw_with_nested(FORMAT_RUNTIME_ERROR("TLS data send failed"));}
}
// SessionSocket::ReadExactTLS:
_Check_return_ SocketOps::Result SocketOps::SessionSocket::ReadExactTLS(
	_Out_writes_(BytesToRead) char* Tgt, size_t BytesToRead, int Timeout) {
	if(Valid() == false || TLSReady() == false) return Result::InvalidSocket;
	else if(ValueOps::Is(ReadBufBytes).InRange(0, TLSBufSize) == false
		|| ValueOps::Is(ClearBufBytes).InRange(0, TLSBufSize) == false) {
		LastErrString = "Invalid socket state";
		return Result::InvalidSocket;
	}
	else if(Tgt == nullptr || ValueOps::Is(BytesToRead).InRange(1, TLSBufSize) == false) return Result::InvalidArg;

	try {
		const SteadyClock EndTime(std::chrono::milliseconds{Timeout});
		Result rc = Result::OK;

		// Read incoming data from socket until timeout is reached or enough bytes are available in clear
		// packet buffer to fulfill request:
		for(short s = 0; s < 15 && rc == Result::OK && Timeout >= 0 && ClearBufBytes < BytesToRead; ++s) {
			// Wait to ensure data is available on socket:
			rc = SocketOps::WaitEvent(SocketHandle, Timeout, &LastErrString);
			if(ResultOK(rc)) {
				// Data available - update timeout, read data (then update timeout again,
				// in case we have only received partial data and need to continue reading):
				Timeout = SteadyClock().MSecTill(EndTime);
				rc = PrivateReadTLS(ValueOps::MinZero(Timeout));
				Timeout = SteadyClock().MSecTill(EndTime);
			}
		}

		// If no errors raised above, ensure we have enough data to service request:
		if(ResultOK(rc)) {
			if(ClearBufBytes >= BytesToRead) {
				// Requested data is available in cleartext - copy to target:
				memcpy(Tgt, ClearBuf.get(), BytesToRead);
				// Remove data from cleartext buffer, shift remaining bytes (if any) left:
				ClearBufBytes -= BytesToRead;
				if(ClearBufBytes > 0) memcpy(ClearBuf.get(), ClearBuf.get() + BytesToRead, ClearBufBytes);
			}
			else rc = Result::Timeout;
		}
		return rc;
	}
	catch(const std::exception&) {std::throw_with_nested(FORMAT_RUNTIME_ERROR("TLS data read failed"));}
}
// SessionSocket::ReadAvailableTLS:
_Check_return_ SocketOps::Result SocketOps::SessionSocket::ReadAvailableTLS(
	_Out_writes_(MaxBytes) char* Tgt, size_t MaxBytes, size_t& BytesRead) {
	BytesRead = 0;
	if(Valid() == false || TLSReady() == false) return Result::InvalidSocket;
	else if(ValueOps::Is(ReadBufBytes).InRange(0, TLSBufSize) == false
		|| ValueOps::Is(ClearBufBytes).InRange(0, TLSBufSize) == false) {
		LastErrString = "Invalid socket state";
		return Result::InvalidSocket;
	}
	else if(Tgt == nullptr || ValueOps::Is(MaxBytes).InRange(1, TLSBufSize) == false) return Result::InvalidArg;

	try {
		if(ClearBufBytes < MaxBytes) {
			// We still have room to read bytes over what we have already decrypted; check for available
			// data on socket (without waiting); if non-timeout error occurs, return immediately:
			Result rc = SocketOps::WaitEvent(SocketHandle, 0, &LastErrString);
			if(ResultFailed(rc)) return rc;
			else if(ResultOK(rc)) {
				// Data is available to be read, do so now (again without waiting):
				if(ResultFailed(rc = PrivateReadTLS(0))) return rc;
			}
		}

		// If this point is reached, either data was read successfully or no new data
		// was available - in either case, our return value depends on what is available
		// in clear data buffer:
		if(ClearBufBytes > 0) {
			// Cleartext data is available - copy to target:
			BytesRead = ValueOps::Bounded(1ULL, ClearBufBytes, MaxBytes);
			memcpy(Tgt, ClearBuf.get(), BytesRead);
			// Remove data from cleartext buffer, shift remaining bytes (if any) left:
			ClearBufBytes -= BytesRead;
			if(ClearBufBytes > 0) memcpy(ClearBuf.get(), ClearBuf.get() + BytesRead, ClearBufBytes);
			return Result::OK;
		}
		else return Result::Timeout;
	}
	catch(const std::exception&) {std::throw_with_nested(FORMAT_RUNTIME_ERROR("TLS available data read failed"));}
}
// SessionSocket::ReadPacketTLS:
_Check_return_ SocketOps::Result SocketOps::SessionSocket::ReadPacketTLS(_Out_writes_(MaxBytes) char* Tgt, size_t MaxBytes, size_t& BytesRead, int Timeout) {
	BytesRead = 0;
	if(Tgt == nullptr || ValueOps::Is(MaxBytes).InRangeLeft(4, INT_MAX) == false) return Result::InvalidArg;
	try {
		const SteadyClock EndTime(std::chrono::milliseconds{Timeout});

		// Read incoming two-byte header:
		Result rc = ReadExactTLS(Tgt, 2, Timeout);
		if(ResultOK(rc)) {

			// Calculate total packet length and validate against max read length (zero-length packet return success):
			const size_t PacketSize = ((Tgt[0] & 0xFF) << 8) | (Tgt[1] & 0xFF);
			if(PacketSize == 0) return Result::OK;
			else if(ValueOps::Is(PacketSize).InRange(1, MaxBytes) == false) {
				LastErrString = "Length of incoming packet exceeds maximum";
				Shutdown(); // Shut down socket to prevent fragmented packet being left on wire
				return Result::Failed;
			}
			// Attempt to read bytes specified by packet header; if successful, set output length:
			else if(ResultOK(rc = ReadExactTLS(Tgt, PacketSize, ValueOps::MinZero(SteadyClock().MSecTill(EndTime)))))
				BytesRead = PacketSize;
		}
		return rc;
	}
	catch(const std::exception&) {std::throw_with_nested(FORMAT_RUNTIME_ERROR("TLS packet read failed"));}
}
// SessionSocket::PrivateReadTLS:
_Check_return_ SocketOps::Result SocketOps::SessionSocket::PrivateReadTLS(int Timeout) {
	// Note this function should only be called from one of SessionSocket's ReadTLS functions,
	// which should have already validated the session socket members and checked that incoming
	// data is available
	try {
		const SteadyClock EndTime(std::chrono::milliseconds{Timeout});
		const char * const EndPtr = ReadBuf.get() + TLSBufSize;

		// Attempt receive/decrypt operation until data decrypted or error/timeout occurs:
		Result rc = Result::Timeout;
		for(short s = 0; s < 15 && rc == Result::Timeout && Timeout >= 0; ++s) {
			// Ensure there is space remaining in read buffer to pull more incoming data:
			{char * const ReadPtr = ReadBuf.get() + ReadBufBytes;
			const size_t MaxBytes = StringOps::BytesAvail(ReadPtr, EndPtr);
			if(ValueOps::Is(MaxBytes).InRange(1, TLSBufSize) == false) {
				Shutdown();
				rc = Result::Failed;
				LastErrString = "TLS read failed: Buffer size exceeded";
				break;
			}

			// Unless this is first loop iteration (and there is already data in read buffer), wait for socket events:
			Result src = (s == 0 && ReadBufBytes > 0) ? Result::Timeout : SessionSocket::WaitEvent(Timeout);
			if(ResultOK(src)) {
				size_t br = 0;
				if(ResultOK(src = SocketOps::ReadAvailable(SocketHandle, ReadPtr, MaxBytes, br, &LastErrString))) {
					if(ValueOps::Is(br).InRange(1, MaxBytes)) ReadBufBytes += br;
					else {
						Shutdown();
						src = Result::Failed;
						if(br != 0) LastErrString = "TLS read failed: Invalid number of bytes read";
					}
				}
			}
			// If error occurred (on wait or read) or either operation timed out,
			// set result and break now; LastErrString should already be set
			if(ResultFailed(src)) {
				rc = src;
				break;
			}}

			// Set up incoming message TLS objects and attempt to decrypt:
			SecBuffer Buffers[4] = {0};
			Buffers[0].pvBuffer = ReadBuf.get();
			Buffers[0].cbBuffer = gsl::narrow_cast<unsigned long>(ReadBufBytes);
			Buffers[0].BufferType = SECBUFFER_DATA;
			Buffers[1].BufferType = Buffers[2].BufferType = Buffers[3].BufferType = SECBUFFER_EMPTY;
			SecBufferDesc BufferDesc = {0};
			BufferDesc.ulVersion = SECBUFFER_VERSION;
			BufferDesc.cBuffers = 4;
			BufferDesc.pBuffers = Buffers;
			const SECURITY_STATUS scRet = SocketOps::GetFunctionTable()->DecryptMessage(&hContext, &BufferDesc, 0, nullptr);

			// If response code indicates continue needed, continue loop (note this is the only way
			// for this loop to ever execute more than once, as all other paths below will break):
			if(scRet == SEC_E_INCOMPLETE_MESSAGE) {
				Timeout = SteadyClock().MSecTill(EndTime);
				continue;
			}
			else if(scRet == SEC_I_CONTEXT_EXPIRED) {
				// Remote has gracefully closed TLS session - shut down socket and return:
				Shutdown();
				rc = Result::Failed;
				break;
			}
			else if(scRet != SEC_E_OK) {
				// Non-wait error occurred; set error data:
				Shutdown();
				rc = Result::Failed;
				LastErrString = "DecryptMessage: " + Exceptions::ConvertCOMError(scRet);
				break;
			}

			// Data decrypted; step through returned data buffers, locating decrypted and 'extra' data
			SecBuffer *DataBuf = nullptr, *ExtraBuf = nullptr;
			for(short i = 0; i < 4; ++i) {
				if(Buffers[i].BufferType == SECBUFFER_DATA) DataBuf = &Buffers[i];
				else if(Buffers[i].BufferType == SECBUFFER_EXTRA) ExtraBuf = &Buffers[i];
			}

			// If decrypted data located, validate length and write to target:
			if(DataBuf != nullptr ? (DataBuf->cbBuffer != 0) : false) {
				if(ValueOps::Is(static_cast<size_t>(DataBuf->cbBuffer)).InRange(1, TLSBufSize - ClearBufBytes) && DataBuf->pvBuffer != nullptr) {
					memcpy(ClearBuf.get() + ClearBufBytes, DataBuf->pvBuffer, DataBuf->cbBuffer);
					ClearBufBytes += DataBuf->cbBuffer;
				}
				else { // Invalid data or buffer overrun, abort:
					Shutdown();
					rc = Result::Failed;
					LastErrString = "Invalid inbound data or TLS buffer overrun";
					break;
				}
			}

			// If "extra" data was located after encrypted packet copy back to read buffer:
			if(ExtraBuf ? ValueOps::Is(static_cast<size_t>(ExtraBuf->cbBuffer)).InRange(1, ReadBufBytes) : false) {
				memcpy(ReadBuf.get(), ReadBuf.get() + ReadBufBytes - ExtraBuf->cbBuffer, ExtraBuf->cbBuffer);
				ReadBufBytes = ExtraBuf->cbBuffer;
			}
			// Otherwise all available data has been decrypted, reset read count:
			else ReadBufBytes = 0;

			// If this point is reached, data decryption successful - break loop now:
			rc = Result::OK;
			break;

		} // End for loop
		return rc;
	}
	catch(const std::exception&) {std::throw_with_nested(FORMAT_RUNTIME_ERROR("Failed to read TLS data"));}
}
#pragma endregion SocketOps::SessionSocket
