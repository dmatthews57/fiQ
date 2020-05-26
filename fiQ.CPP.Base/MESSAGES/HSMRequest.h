#pragma once
//==========================================================================================================================
// HSMRequest.h : Class for run-time management of an encryption request to be sent to an HSM
//==========================================================================================================================

#include "Messages/RoutableMessage.h"
#include "Tools/ValueOps.h"

// Helper macro to pass string literals to FieldSet initialization list:
#define HSMLITERAL(x) x, _countof(x) - 1

namespace FIQCPPBASE {

//==========================================================================================================================
// HSMRequest: Container for a single HSM request to be processed and carry back response values (if successful)
class HSMRequest : public RoutableMessage {
private: struct pass_key {};
public:

	//======================================================================================================================
	// Type definitions - HSM operation (used as RoutableMessage::subtype value):
	enum class Operation : Subtype {
		Unspecified = 0,
		GenerateKey = 1,
		TranslateKey = 2,
		TranslatePIN = 3
	};

	// Type definitions - operation result code
	enum class Result {	NotSupported = -2, SystemError = -1, OK = 0, InvalidArg = 1, SanityError = 2 };

	// Type definitions - Request/response fields:
	enum class FieldName : int {
		Invalid = 0,
		// Request fields:
		MFKMod = 1,			// Modifier of MFK to apply to operation
		KEKMod = 2,			// Modifier of KEK (if different from MFKMod)
		KeyIn = 3,			// Incoming key to be operated on (for keychange/translation requests)
		KCVIn = 4,			// KCV of KeyIn
		KEK = 5,			// KEK to use in operation
		PAN = 6,			// PAN associated with this operation
		PINIn = 7,			// Incoming encrypted PIN block
		PEKSrc = 8,			// PEK under which incoming PIN is currently encrypted
		PEKDst = 9,			// PEK under which outgoing PIN should be encrypted (for PIN translation)
		// Response fields:
		KeyOutMFK = 101,	// KeyIn encrypted under requested modifier of MFK
		KeyOutKEK = 102,	// KeyIn encrypted under requested modifier of KEK
		KCVOut = 103,		// KCV of KeyOut
		PINOut = 104,		// PINIn encrypted under PEKDst
		// Misc fields:
		Echo = 201			// String sent to HSM and expected to be echoed back
	};

	// Type definitions - Request field and collection (string-view version, for inline requests):
	using RequestField = std::tuple<FieldName, const char * const, size_t>;
	using RequestFieldSet = std::vector<RequestField>;
	using StringView = std::pair<const char*, size_t>;

	// Type definitions - Response field and collection (copy/string-owning version, for responses):
	using ResponseField = std::pair<FieldName, std::string>;
	using ResponseFieldSet = std::vector<ResponseField>;

	//======================================================================================================================
	// Public named constructor and static utility functions
	_Check_return_ static std::shared_ptr<HSMRequest> Create(Operation op, RequestFieldSet&& _requestfields) {
		return std::make_shared<HSMRequest>(pass_key{}, op, std::move(_requestfields));
	}
	_Check_return_ static FieldName GetFieldName(const RequestField& rf) noexcept {return std::get<0>(rf);}
	_Check_return_ static bool IsField(const RequestField& rf, FieldName fn) noexcept {return (std::get<0>(rf) == fn);}
	_Check_return_ static const char* GetFieldValue(const RequestField& rf) noexcept {return std::get<1>(rf);}
	_Check_return_ static size_t GetFieldLength(const RequestField& rf) noexcept {return std::get<2>(rf);}
	_Check_return_ static StringView GetFieldView(const RequestField& rf) noexcept {
		return {std::get<1>(rf), std::get<2>(rf)};
	}

	//======================================================================================================================
	// Public non-static accessors
	_Check_return_ Operation GetOperation() const noexcept {return static_cast<Operation>(GetSubtype());}
	_Check_return_ const RequestFieldSet& GetRequestFields() const noexcept {return requestfields;}
	_Check_return_ Result GetResult() const noexcept {return result;}
	_Check_return_ bool ResultOK() noexcept {return (result == Result::OK);}
	_Check_return_ bool ResultSystemError() noexcept {return (result < Result::OK);}
	_Check_return_ bool ResultDataError() noexcept {return (result > Result::OK);}
	_Check_return_ const std::string& GetResponseField(FieldName fname) const;

	//======================================================================================================================
	// HSM processor response setting accessors
	void SetResponse(Result _result) noexcept;
	void SetResponse(Result _result, ResponseFieldSet&& _responsefields);

	//======================================================================================================================
	// Public constructors (accessible to make_shared, but locked with pass_key) and default destructor
	HSMRequest(pass_key, Operation op, RequestFieldSet&& _requestfields)
		: RoutableMessage(Type::HSMRequest, static_cast<std::underlying_type_t<Operation>>(op)),
		requestfields(_requestfields) {}
	HSMRequest(pass_key, Operation op)
		: RoutableMessage(Type::HSMRequest, static_cast<std::underlying_type_t<Operation>>(op)) {}
	~HSMRequest() noexcept = default;

	// Deleted copy/move constructors and assignment operators
	HSMRequest(const HSMRequest&) = delete;
	HSMRequest(HSMRequest&&) = delete;
	HSMRequest& operator=(const HSMRequest&) = delete;
	HSMRequest& operator=(HSMRequest&&) = delete;

private:

	// Static accessor to produce persistent invalid field value:
	static const std::string& EMPTYSTR();

	// Private members
	const RequestFieldSet requestfields;	// Outgoing request values, must be provided to constructor
	ResponseFieldSet responsefields;		// Incoming response values, provided by processor
	Result result = Result::SystemError;	// Default to system error (i.e. unprocessed)
};

//==========================================================================================================================
// HSMRequest::GetResponseField: Search through response vector and return first matching value
_Check_return_ inline const std::string& HSMRequest::GetResponseField(FieldName fname) const {
	for(auto seek = responsefields.cbegin(); seek != responsefields.cend(); ++seek) {
		if(seek->first == fname) {
#pragma warning (suppress : 26487) // Iterator points to member variable, reference will last as long as this object
			return seek->second;
		}
	}
	return EMPTYSTR(); // Response field not present
}
// HSMRequest::SetResponse: Allow HSM processor to set result code
inline void HSMRequest::SetResponse(Result _result) noexcept {
	result = _result;
}
// HSMRequest::SetResponse: Allow HSM processor to set result code and response values
inline void HSMRequest::SetResponse(Result _result, ResponseFieldSet&& _responsefields) {
	result = _result;
	responsefields = std::move(_responsefields);
}

}; // (end namespace FIQCPPBASE)
