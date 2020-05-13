#pragma once
//==========================================================================================================================
// HSMRequest.h : Class for run-time management of an encryption request to be sent to an HSM
//==========================================================================================================================

#include "RoutableMessage.h"

// Helper macro to pass string literals to FieldSet initialization list:
#define HSMLITERAL(x) x, _countof(x) - 1

namespace FIQCPPBASE {

class HSMRequest : public RoutableMessage {
private: struct pass_key {};
public:

	// Type definitions - HSM operation (used as RoutableMessage::subtype value):
	enum class Operation : Subtype {
		Unspecified = 0,
		GenerateKey = 1,	// INPUTS: MODIFIER (R), KEK (O?) DEST MODIFIER(?) / OUTPUTS: KEY(MFK), KEY(KEK), KCV
		TranslateKey = 2,	// INPUTS: KEY (R), KEK (R), CHK (O), SRC MODIFIER (R), DST MODIFIER (?) / OUTPUTS: KEY(MFK)
		TranslatePIN = 3	// INPUTS: SRC KEY (R), PIN(SRC) (R), DST KEY (R), PAN (R) / OUTPUTS: PIN(DST)
	};

	// Type definitions - Request/response fields:
	enum class FieldName : int {
		Invalid = 0,
		// Request fields:
		MFKMod = 1,		// Modifier of MFK to apply to operation
		KEKMod = 2,		// Modifier of KEK (if different from MFKMod)
		KeyIn = 3,		// Incoming key to be operated on (for keychange/translation requests)
		KCVIn = 4,		// KCV of KeyIn
		KEK = 5,		// KEK to use in operation
		PAN = 6,		// PAN associated with this operation
		PINIn = 7,		// Incoming encrypted PIN block
		PEKSrc = 8,		// PEK under which incoming PIN is currently encrypted
		PEKDst = 9,		// PEK under which outgoing PIN should be encrypted (for PIN translation)
		// Response fields:
		KeyOutMFK = 101,	// KeyIn encrypted under requested modifier of MFK
		KeyOutKEK = 102,	// KeyIn encrypted under requested modifier of KEK
		KCVOut = 103,		// KCV of KeyOut
		PINOut = 104		// PINIn encrypted under PEKDst
	};

	// Type definitions - Request/response field and collection (string-view version, for inline requests):
	using Field = std::tuple<FieldName, const char * const, size_t>;
	using FieldSet = std::vector<Field>;

	// Type definitions - Request/response field and collection (copy/string-owning version, for responses):
	using FieldCopy = std::tuple<FieldName, std::string>;
	using FieldCopySet = std::vector<FieldCopy>;

	// Public named constructor
	_Check_return_ static std::shared_ptr<HSMRequest> Create(Operation op, FieldSet&& _requestfields) {
		return std::make_shared<HSMRequest>(pass_key{}, op, std::move(_requestfields));
	}

	// Public static utility functions
	static const Field& GetField(FieldName fname, const FieldSet& fields);
	static FieldName GetFieldName(const Field& f) noexcept {return std::get<0>(f);}
	static const char* GetFieldValue(const Field& f) noexcept {return std::get<1>(f);}
	static size_t GetFieldLength(const Field& f) noexcept {return std::get<2>(f);}

	// Public accessors
	const FieldSet& GetRequestFields() const noexcept {return requestfields;}
	const FieldSet& GetResponseFields() const;
	void SetResponseFields(FieldCopySet&& _responsefields) {responsefields = std::move(_responsefields);}

	// Public constructors (accessible to make_shared, but locked with pass_key), destructor
	HSMRequest(pass_key, Operation op, FieldSet&& _requestfields)
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

	// Private static utility functions:
	static FieldSet CopyToView(const FieldCopySet& fields);

	// Static accessor to produce invalid field:
	static const Field& GetInvalidField();

	// Private members
	const FieldSet requestfields;	// Outgoing request values, must be provided to constructor
	FieldCopySet responsefields;	// Incoming response values, provided by processor
	mutable FieldSet responseview;	// View of 
};

inline const HSMRequest::Field& HSMRequest::GetField(FieldName fname, const FieldSet& fields) {
	for(auto seek = fields.cbegin(); seek != fields.cend(); ++seek) {
		if(std::get<0>(*seek) == fname) return (*seek);
	}
	return GetInvalidField();
}
inline HSMRequest::FieldSet HSMRequest::CopyToView(const FieldCopySet& fields) {
	FieldSet retval;
	retval.reserve(fields.size());
	for(auto seek = fields.cbegin(); seek != fields.cend(); ++seek) {
		retval.push_back({std::get<0>(*seek), std::get<1>(*seek).data(), std::get<1>(*seek).length()});
	}
	return retval;
}
inline const HSMRequest::FieldSet& HSMRequest::GetResponseFields() const {
	// If responseview is empty and responsefields is not (the reverse should never be true), populate
	// responseview vector with pointer/length values from responsefields:
	if(responseview.empty() != responsefields.empty()) responseview = CopyToView(responsefields);
	return responseview;
}



}; // (end namespace FIQCPPBASE)
