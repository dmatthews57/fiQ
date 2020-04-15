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

//==========================================================================================================================
// SocketOps::Initialize: Start up Winsock library, initialize SChannel interface if required
void SocketOps::InitializeSockets(bool TLSRequired) {
	int wsarc = -1;
	HMODULE hSChannel = NULL;
	try {
		// Initialize Winsock library, throw exception on failure
		{WSADATA wsa;
		if((wsarc = WSAStartup(0x0202, &wsa)) != 0) {
			throw std::runtime_error(std::string("WSAStartup: ").append(Exceptions::ConvertCOMError(wsarc)));
		}}
		// If TLS requested, initialize SChannel:
		if(TLSRequired) {
			// Load DLL handle into local variable:
			if((hSChannel = LoadLibrary(L"SCHANNEL.DLL")) == NULL)
				throw std::runtime_error(std::string("LoadLibrary: ").append(Exceptions::ConvertCOMError(GetLastError())));
			// Get address of InitSecurityInterface function from DLL handle:
			INIT_SECURITY_INTERFACE pInitSecurityInterface =
				reinterpret_cast<INIT_SECURITY_INTERFACE>(GetProcAddress(hSChannel, SECURITY_ENTRYPOINT_ANSI));
			if(pInitSecurityInterface == NULL)
				throw std::runtime_error(std::string("GetProcAddress: ").append(Exceptions::ConvertCOMError(GetLastError())));
			// Use InitSecurityInterface to retrieve function table pointer:
			PSecurityFunctionTable pFunctionTable = pInitSecurityInterface();
			if(pFunctionTable == nullptr)
				throw std::runtime_error(std::string("InitSecurityInterface: ").append(Exceptions::ConvertCOMError(GetLastError())));

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

//==========================================================================================================================
// ServerSocket::InitCredentialsFromStore: Open system certificate store and locate specified certificate
_Check_return_ bool SocketOps::ServerSocket::InitCredentialsFromStore(
	const std::string& CertName, const std::string& Method, bool LocalMachineStore) {
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
			LastErrString = LocalMachineStore ? "CertOpenStore: " : "CertOpenSystemStore: ";
			LastErrString += Exceptions::ConvertCOMError(GetLastError());
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
				LastErrString = "CertFindCertificateInStore: ";
				LastErrString += Exceptions::ConvertCOMError(GetLastError());
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
			LastErrString = CertName;
			LastErrString += ": Certificate not found";
			return false;
		}
		// Initialize common members, and return result:
		return CompleteInitCredentials(Method);
	}
	catch(const std::exception&) {std::throw_with_nested(FORMAT_RUNTIME_ERROR("Server credential initialization failed"));}
}
// ServerSocket::InitCredentialsFromFile: Open PKCS12 file and import certificate from store
_Check_return_ bool SocketOps::ServerSocket::InitCredentialsFromFile(
	const std::string& FileName, const std::string& FilePassword, const std::string& CertName, const std::string& Method) {
	UsingTLS = true; // Set flag to ensure credentials are validated on all future checks
	// Ensure this object has not yet been initialized, but that static members have:
	if(hCreds.dwLower || hCreds.dwUpper || pCertContext || hMyCertStore || SchannelCred.dwVersion) {
		LastErrString.assign("Credentials have already been initialized");
		return false;
	}
	else if(SocketOps::GetFunctionTable() == nullptr) {
		LastErrString.assign("SChannel library has not been initialized");
		return false;
	}
	try {
		{FileOps::FilePtr CertFile = FileOps::OpenFile(FileName.c_str(), "rb", _SH_DENYWR);
		if(CertFile.get() == nullptr) {
			LastErrString.assign(Exceptions::ConvertCOMError(GetLastError()));
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
				CRYPT_DATA_BLOB fdata = {static_cast<DWORD>(filesize), FileBuf.get()};
				hMyCertStore = PFXImportCertStore(&fdata, StringOps::ConvertToWideString(FilePassword).c_str(), 0);
				if(hMyCertStore == nullptr) {
					LastErrString = "PFXImportCertStore: ";
					LastErrString += Exceptions::ConvertCOMError(GetLastError());
					return false;
				}
				// Locate individual certificate by subject name (note this assumes there are no overlapping entries):
				pCertContext = CertFindCertificateInStore(
					hMyCertStore, X509_ASN_ENCODING, 0, CERT_FIND_SUBJECT_STR,
					StringOps::ConvertToWideString(CertName).c_str(), pCertContext);
				if(pCertContext == nullptr) {
					LastErrString = "CertFindCertificateInStore: ";
					LastErrString += Exceptions::ConvertCOMError(GetLastError());
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
		return CompleteInitCredentials(Method);
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
_Check_return_ bool SocketOps::ServerSocket::CompleteInitCredentials(const std::string& Method) {
	// Set up SChannel structure values:
	SchannelCred.dwVersion = SCHANNEL_CRED_VERSION;
	SchannelCred.cCreds = 1;
	SchannelCred.paCred = &pCertContext; // Already initialized by caller
	// Default protocols to TLSv1.2 only; allow override to downgrade based on Method:
	SchannelCred.grbitEnabledProtocols = SP_PROT_TLS1_2_SERVER;
	if(Method.length() >= 3) {
		if(_stricmp(Method.c_str(), "TLSV1.1") == 0) // TLS1.1+ protocols only
			SchannelCred.grbitEnabledProtocols = SP_PROT_TLS1_1PLUS_SERVER;
		else if(_stricmp(Method.c_str(), "TLSV1") == 0) // TLS1.0+ protocols only
			SchannelCred.grbitEnabledProtocols = SP_PROT_TLS1_SERVER | SP_PROT_TLS1_1PLUS_SERVER;
	}
	SchannelCred.dwFlags = (SCH_USE_STRONG_CRYPTO | SCH_CRED_NO_SYSTEM_MAPPER | SCH_SEND_ROOT_CERT);
	const SECURITY_STATUS s = SocketOps::GetFunctionTable()->AcquireCredentialsHandle(
		NULL, const_cast<wchar_t*>(UNISP_NAME), SECPKG_CRED_INBOUND, NULL, &SchannelCred, NULL, NULL, &hCreds, NULL);
	if(s != SEC_E_OK) {
		LastErrString = "AcquireCredentialsHandle: ";
		LastErrString += Exceptions::ConvertCOMError(s);
		return false;
	}
	return CredentialsValid();
}
// ServerSocket::TLSNegotiate: Perform server-side TLS negotiation on new socket connection
_Check_return_ SocketOps::Result SocketOps::ServerSocket::TLSNegotiate(SocketOps::SessionSocketPtr& sp, int Timeout) {
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
		const TimeClock EndTime(Timeout);
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
							if(br != 0) LastErrString = "TLS negotiation failed: Invalid number of bytes read";
						}
					}
					else if(LastErrString.empty()) sp->LastErrString = "TLS negotiation failed: Socket read error";
					else sp->LastErrString = "TLS negotiation failed: " + LastErrString;
				}
				// If any non-timeout error occurred above, break now (LastErrString value will already be set):
				if(ResultFailed(src)) {
					rc = src;
					break;
				}
			}
			// Update timeout value for next iteration, if any:
			Timeout = TimeClock::Now().MSecTill(EndTime);

			// Set up SSL objects - assign incoming data as token (with NULL array terminator), attempt accept:
			InBuffers[0].pvBuffer = sp->ReadBuf.get();
			InBuffers[0].cbBuffer = static_cast<unsigned long>(sp->ReadBufBytes);
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
			if(scRet >= 0 || (dwSSPIOutFlags & ASC_RET_EXTENDED_ERROR) != 0) {
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

			// If continuation needed, continue loop (note this is the only way for this loop to execute more than once):
			if(scRet == SEC_E_INCOMPLETE_MESSAGE || scRet > 0) continue;
			else if(scRet != SEC_E_OK) {
				// Non-wait error occurred; set error data:
				sp->Shutdown();
				rc = Result::Failed;
				LastErrString = "TLS negotiation failed: ";
				LastErrString += Exceptions::ConvertCOMError(scRet);
				break;
			}
			// Negotiation complete; retrieve cipher info:
			if((scRet = SocketOps::GetFunctionTable()->QueryContextAttributes(
				&(sp->hContext), SECPKG_ATTR_CIPHER_INFO, &(sp->CipherInfo))) != SEC_E_OK) {
				sp->Shutdown();
				rc = Result::Failed;
				LastErrString = "QueryContextAttributes (CIPHER_INFO): ";
				LastErrString += Exceptions::ConvertCOMError(scRet);
				break;
			}
			// Retrieve stream sizes for negotiated protocol:
			if((scRet = SocketOps::GetFunctionTable()->QueryContextAttributes(
				&sp->hContext, SECPKG_ATTR_STREAM_SIZES, &sp->StreamSizes)) != SEC_E_OK) {
				sp->Shutdown();
				rc = Result::Failed;
				LastErrString = "QueryContextAttributes (STREAM_SIZES): ";
				LastErrString += Exceptions::ConvertCOMError(scRet);
				break;
			}
			// Validate stream sizes (we must be able to process a packet with header and trailer):
			if((static_cast<size_t>(sp->StreamSizes.cbHeader) + sp->StreamSizes.cbTrailer) >= sp->TLSBufSize) {
				sp->Shutdown();
				rc = Result::Failed;
				LastErrString = "TLS negotiation failed: Protocol message size exceeds maximum";
				break;
			}
			// If this point is reached, negotiation successful - break loop now:
			rc = Result::OK;
			break;
		}
		return rc;
	}
	catch(const std::exception&) {std::throw_with_nested(FORMAT_RUNTIME_ERROR("TLS negotiation (server) failed"));}
}

//==========================================================================================================================
// SessionSocket::TLSNegotiate: Perform client-side negotiation on socket connection
void SocketOps::SessionSocket::TLSNegotiate(int Timeout) {
	UNREFERENCED_PARAMETER(Timeout);
}
// SessionSocket::SendTLS:
_Check_return_ SocketOps::Result SocketOps::SessionSocket::SendTLS(_In_reads_(len) const char* buf, size_t len) {
	UNREFERENCED_PARAMETER(buf);
	UNREFERENCED_PARAMETER(len);
	return SocketOps::Result::Failed;
}
// SessionSocket::ReadExactTLS:
_Check_return_ SocketOps::Result SocketOps::SessionSocket::ReadExactTLS(
	_Out_writes_(BytesToRead) char* Tgt, size_t BytesToRead, int Timeout) {
	UNREFERENCED_PARAMETER(Tgt);
	UNREFERENCED_PARAMETER(BytesToRead);
	UNREFERENCED_PARAMETER(Timeout);
	return SocketOps::Result::Failed;
}
// SessionSocket::ReadAvailableTLS:
_Check_return_ SocketOps::Result SocketOps::SessionSocket::ReadAvailableTLS(_Out_writes_(MaxBytes) char* Tgt, size_t MaxBytes, size_t& BytesRead) {
	UNREFERENCED_PARAMETER(Tgt);
	UNREFERENCED_PARAMETER(MaxBytes);
	UNREFERENCED_PARAMETER(BytesRead);
	return SocketOps::Result::Failed;
}
// SessionSocket::ReadPacketTLS:
_Check_return_ SocketOps::Result SocketOps::SessionSocket::ReadPacketTLS(_Out_writes_(MaxBytes) char* Tgt, size_t MaxBytes, size_t& BytesRead, int Timeout) {
	UNREFERENCED_PARAMETER(Tgt);
	UNREFERENCED_PARAMETER(MaxBytes);
	UNREFERENCED_PARAMETER(BytesRead);
	UNREFERENCED_PARAMETER(Timeout);
	return SocketOps::Result::Failed;
}
