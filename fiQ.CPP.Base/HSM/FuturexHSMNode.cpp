//==========================================================================================================================
// FuturexHSMNode.cpp : HSMNode class implementing Futurex HSM messaging
//==========================================================================================================================
#include "pch.h"
#include "FuturexHSMNode.h"
#include "Messages/HSMRequest.h"
#include "Tools/Exceptions.h"
using namespace FIQCPPBASE;


//==========================================================================================================================
// FuturexHSMNode::Init: Connect to HSM and exchange echo message
_Check_return_ bool FuturexHSMNode::Init() noexcept(false) { // TODO: CONNECT TO HSM AND SEND ECHO
	return true;
}
// FuturexHSMNode::Cleanup: Disconnect HSM session and clean up resources
_Check_return_ bool FuturexHSMNode::Cleanup() noexcept(false) { // TODO: DISCONNECT HSM
	return true;
}

//==========================================================================================================================
// FuturexHSMNode::ProcessRequest: Send request to HSM
_Check_return_ MessageNode::RouteResult FuturexHSMNode::ProcessRequest(
	const std::shared_ptr<RoutableMessage>& rm) noexcept(false) {
	// If message is an HSM request, attempt to retrieve it by type (return failure if not retrieved)
	HSMRequest* h = (rm->GetType() == RoutableMessage::Type::HSMRequest) ? rm->GetAs<HSMRequest>() : nullptr;
	if(h == nullptr) return MessageNode::RouteResult::Unprocessed;

	// Call utility function based on requested operation:
	switch(h->GetOperation())
	{
	case HSMRequest::Operation::GenerateKey: return GenerateKey(*h);
	case HSMRequest::Operation::TranslateKey: return TranslateKey(*h);
	case HSMRequest::Operation::TranslatePIN: return TranslatePIN(*h);
	default:
		// Unrecognized command - decline, but return Unprocessed to allow rerouting if possible:
		h->SetResponse(HSMRequest::Result::NotSupported);
		return RouteResult::Unprocessed;
	};
}
// FuturexHSMNode::ProcessRequest: Receive response from another HSM node for request we initiated (future use)
_Check_return_ MessageNode::RouteResult FuturexHSMNode::ProcessResponse(
	const std::shared_ptr<RoutableMessage>&) noexcept(false) {
	return MessageNode::RouteResult::Unprocessed;
}

//==========================================================================================================================
#pragma region Encryption requests
// FuturexHSMNode::GenerateKey: Generate new outbound working key under MFK and (optionally) KEK
_Check_return_ MessageNode::RouteResult FuturexHSMNode::GenerateKey(HSMRequest& h) {
	try {
		// Retrieve request parameters from collection:
		HSMRequest::StringView mfkmod{nullptr,0}, kekmod{nullptr,0}, kek{nullptr,0};
		for(auto seek = h.GetRequestFields().cbegin(); seek != h.GetRequestFields().cend(); ++seek) {
			if(HSMRequest::IsField(*seek, HSMRequest::FieldName::MFKMod)) mfkmod = HSMRequest::GetFieldView(*seek);
			else if(HSMRequest::IsField(*seek, HSMRequest::FieldName::KEKMod)) kekmod = HSMRequest::GetFieldView(*seek);
			else if(HSMRequest::IsField(*seek, HSMRequest::FieldName::KEK)) kek = HSMRequest::GetFieldView(*seek);
		}

		// Validate request parameters - MFKMod is required and must be a single hex char:
		bool valid = (mfkmod.first != nullptr && mfkmod.second == 1);
		if(valid) valid = StringOps::Ascii::IsHexChar(mfkmod.first[0]);
		// KEK is optional, but if present must be valid length:
		if(valid && kek.first != nullptr) valid = (kek.second == 32);
		else kekmod.first = nullptr; // If KEK not present, ignore KEKMod
		// KEKMod is optional, but if present must be valid (single hex char):
		if(valid && kekmod.first != nullptr) {
			if((valid = (kekmod.second == 1 ? StringOps::Ascii::IsHexChar(kekmod.first[0]) : false)) == true) {
				if(kekmod.first[0] == mfkmod.first[0]) kekmod.first = nullptr; // Redundant, ignore
				else valid = kekmod.first[0] == '0'; // Currently only modifier zero supported
			}
		}
		if(valid == false) {
			h.SetResponse(HSMRequest::Result::InvalidArg);
			return RouteResult::Processed;
		}

		// Declare placeholders for response fields
		std::string keyoutmfk, kcvout, keyoutkek;

		// If this point is reached required parameters are present and valid; build and execute request:
		{std::string request;
		request.reserve(60); // Space for KEK plus overhead
		request.append(HSMLITERAL("[AOGWKS;AS"));
		request += mfkmod.first[0];
		request.append(HSMLITERAL(";AP"));
		static const std::string zeroes('0', 32);
		if(kek.first) request.append(kek.first, kek.second);
		else request += zeroes;
		request.append(HSMLITERAL(";FS2;AG"));
		// Lock access to echo counter, format to local buffer:
		char echoval[10] = {0};
		{auto lock = Locks::Acquire(EchoLock);
		if(++EchoCount > 9999) EchoCount = 1;
		StringOps::Decimal::ExWriteString<4>(echoval, EchoCount);}
		request.append(echoval, 4);
		request.append(HSMLITERAL(";]"));

		// Execute request, retrieve response fields:
		const Tokenizer toks = ExecRequest(request);
		if((valid = toks.TokenCount() >= 6) == true) {
			HSMRequest::StringView echo{nullptr,0};
			for(size_t i = 0; i < toks.TokenCount(); ++i) {
				if(toks.Length(i) == 34) {
					if(memcmp(toks.Value(i), "BG", 2) == 0) keyoutmfk.assign(toks.Value(i) + 2, 32);
					else if(memcmp(toks.Value(i), "BH", 2) == 0) keyoutkek.assign(toks.Value(i) + 2, 32);
				}
				else if(toks.Length(i) >= 6) {
					if(memcmp(toks.Value(i), "AE", 2) == 0) kcvout.assign(toks.Value(i) + 2, toks.Length(i) - 2);
					else if(memcmp(toks.Value(i), "AG", 2) == 0) echo = {toks.Value(i) + 2, toks.Length(i) - 2};
				}
			}
			// Ensure we extracted all expected fields, check echo value:
			valid = (
				echo.first != nullptr && echo.second == 4
				&& (keyoutmfk.empty() == false)
				&& (kcvout.empty() == false)
				&& (keyoutkek.empty() == false)
			);
			if(valid) valid = (memcmp(echo.first, echoval, 4) == 0);
		}}

		// If working key should be returned under KEK mod zero, translate now:
		if(valid && (kekmod.first ? kekmod.first[0] : 0) == '0') {
			keyoutkek.clear(); // Invalidate field, must be re-read
			std::string request;
			request.reserve(100); // Space for two keys plus overhead
			request.append(HSMLITERAL("[AOTWKA;AS0;AP"));
			request.append(kek.first, kek.second);
			request.append(HSMLITERAL(";BG"));
			request.append(keyoutmfk);
			request.append(HSMLITERAL(";AG"));
			// Lock access to echo counter, format to local buffer:
			char echoval[10] = {0};
			{auto lock = Locks::Acquire(EchoLock);
			if(++EchoCount > 9999) EchoCount = 1;
			StringOps::Decimal::ExWriteString<4>(echoval, EchoCount);}
			request.append(echoval, 4);
			request.append(HSMLITERAL(";]"));
			const Tokenizer toks = ExecRequest(request);
			if((valid = toks.TokenCount() >= 4) == true) {
				HSMRequest::StringView echo{nullptr,0};
				for(size_t i = 0; i < toks.TokenCount(); ++i) {
					if(toks.Length(i) == 34) {
						if(memcmp(toks.Value(i), "BH", 2) == 0) keyoutkek.assign(toks.Value(i) + 2, 32);
					}
					else if(toks.Length(i) >= 6) {
						if(memcmp(toks.Value(i), "AG", 2) == 0) echo = {toks.Value(i) + 2, toks.Length(i) - 2};
					}
				}
				valid = (
					echo.first != nullptr && echo.second == 4
					&& (keyoutkek.empty() == false)
				);
				if(valid) valid = (memcmp(echo.first, echoval, 4) == 0);
			}
		}

		// If processing was successful, construct response fields and apply to object:
		if(valid) {
			HSMRequest::ResponseFieldSet respfields;
			respfields.reserve(kek.first ? 3 : 2);
			respfields.emplace_back(std::piecewise_construct,
				std::forward_as_tuple(HSMRequest::FieldName::KeyOutMFK),
				std::forward_as_tuple(std::move(keyoutmfk)));
			respfields.emplace_back(std::piecewise_construct,
				std::forward_as_tuple(HSMRequest::FieldName::KCVOut),
				std::forward_as_tuple(std::move(kcvout)));
			if(kek.first) {
				respfields.emplace_back(std::piecewise_construct,
					std::forward_as_tuple(HSMRequest::FieldName::KeyOutKEK),
					std::forward_as_tuple(std::move(keyoutkek)));
			}
			// Approve request and return processed result:
			h.SetResponse(HSMRequest::Result::OK, std::move(respfields));
			return RouteResult::Processed;
		}

		// If this point is reached, processing failed - decline, but return Unprocessed to allow rerouting if possible:
		h.SetResponse(HSMRequest::Result::SystemError);
		return RouteResult::Unprocessed;
	}
	catch(const std::exception&) {std::throw_with_nested(FORMAT_RUNTIME_ERROR("Key generation failed"));}
}
// FuturexHSMNode::TranslateKey: Translate key from KEK encryption to MFK encryption
_Check_return_ MessageNode::RouteResult FuturexHSMNode::TranslateKey(HSMRequest& h) {
	try {
		// Retrieve request parameters from collection:
		HSMRequest::StringView mfkmod{nullptr,0}, kekmod{nullptr,0}, keyin{nullptr,0}, kcvin{nullptr,0}, kek{nullptr,0};
		for(auto seek = h.GetRequestFields().cbegin(); seek != h.GetRequestFields().cend(); ++seek) {
			if(HSMRequest::IsField(*seek, HSMRequest::FieldName::MFKMod)) mfkmod = HSMRequest::GetFieldView(*seek);
			else if(HSMRequest::IsField(*seek, HSMRequest::FieldName::KEKMod)) kekmod = HSMRequest::GetFieldView(*seek);
			else if(HSMRequest::IsField(*seek, HSMRequest::FieldName::KeyIn)) keyin = HSMRequest::GetFieldView(*seek);
			else if(HSMRequest::IsField(*seek, HSMRequest::FieldName::KCVIn)) kcvin = HSMRequest::GetFieldView(*seek);
			else if(HSMRequest::IsField(*seek, HSMRequest::FieldName::KEK)) kek = HSMRequest::GetFieldView(*seek);
		}

		// Validate request parameters - MFKMod is required and must be a single hex char:
		bool valid = (mfkmod.first != nullptr && mfkmod.second == 1);
		if(valid) valid = StringOps::Ascii::IsHexChar(mfkmod.first[0]);
		// KeyIn and KEK are required and must be a valid length:
		if(valid) valid = (keyin.first != nullptr && keyin.second == 32);
		if(valid) valid = (kek.first != nullptr && kek.second == 32);
		// KEKMod is optional, but must be valid if present (single hex char):
		if(valid && kekmod.first != nullptr) {
			if((valid = (kekmod.second == 1 ? StringOps::Ascii::IsHexChar(kekmod.first[0]) : false)) == true) {
				if(kekmod.first[0] == mfkmod.first[0]) kekmod.first = nullptr; // Redundant, ignore
				else valid = kekmod.first[0] == '0'; // Currently only modifier zero supported
			}
		}
		// KCVIn is optional, but must be valid if present (minimum 4 chars):
		if(valid && kcvin.first != nullptr) valid = (kcvin.second >= 4);
		if(valid == false) {
			h.SetResponse(HSMRequest::Result::InvalidArg);
			return RouteResult::Processed;
		}

		// Declare placeholders for response fields
		std::string keyoutmfk, kcvout;

		// If this point is reached required parameters are present and valid; build and execute request:
		{std::string request;
		request.reserve(100); // Space for two keys plus overhead
		if((kekmod.first ? kekmod.first[0] : 0) == '0') // Key is encrypted under KEK with no modifier
			request.append(HSMLITERAL("[AOTWKM;AS"));
		else request.append(HSMLITERAL("[AOTWKS;AS")); // Key is encrypted under desired modifier
		request += mfkmod.first[0];
		request.append(HSMLITERAL(";AP"));
		request.append(kek.first, kek.second);
		request.append(HSMLITERAL(";BH"));
		request.append(keyin.first, keyin.second);
		request.append(HSMLITERAL(";AG"));
		// Lock access to echo counter, format to local buffer:
		char echoval[10] = {0};
		{auto lock = Locks::Acquire(EchoLock);
		if(++EchoCount > 9999) EchoCount = 1;
		StringOps::Decimal::ExWriteString<4>(echoval, EchoCount);}
		request.append(echoval, 4);
		request.append(HSMLITERAL(";]"));

		// Execute request, retrieve response fields:
		const Tokenizer toks = ExecRequest(request);
		if((valid = toks.TokenCount() >= 5) == true) {
			HSMRequest::StringView echo{nullptr,0};
			for(size_t i = 0; i < toks.TokenCount(); ++i) {
				if(toks.Length(i) == 34) {
					if(memcmp(toks.Value(i), "BG", 2) == 0) keyoutmfk.assign(toks.Value(i) + 2, 32);
				}
				else if(toks.Length(i) >= 6) {
					if(memcmp(toks.Value(i), "AE", 2) == 0) kcvout.assign(toks.Value(i) + 2, toks.Length(i) - 2);
					else if(memcmp(toks.Value(i), "AG", 2) == 0) echo = {toks.Value(i) + 2, toks.Length(i) - 2};
				}
			}
			// Ensure we extracted all expected fields, check echo value:
			valid = (
				echo.first != nullptr && echo.second == 4
				&& (keyoutmfk.empty() == false)
				&& (kcvout.empty() == false)
			);
			if(valid) valid = (memcmp(echo.first, echoval, 4) == 0);

			// If incoming KCV was provided by caller, compare values:
			if(valid && kcvin.first)  {
				if(memcmp(kcvin.first, kcvout.data(), 4) != 0) {
					// Checkdigit mismatch - this is a data error, not a system error:
					h.SetResponse(HSMRequest::Result::SanityError);
					return RouteResult::Processed;
				}
			}
		}}

		// If processing was successful, construct response fields and apply to object:
		if(valid) {
			HSMRequest::ResponseFieldSet respfields;
			respfields.reserve(2);
			respfields.emplace_back(std::piecewise_construct,
				std::forward_as_tuple(HSMRequest::FieldName::KeyOutMFK),
				std::forward_as_tuple(std::move(keyoutmfk)));
			respfields.emplace_back(std::piecewise_construct,
				std::forward_as_tuple(HSMRequest::FieldName::KCVOut),
				std::forward_as_tuple(std::move(kcvout)));
			// Approve request and return processed result:
			h.SetResponse(HSMRequest::Result::OK, std::move(respfields));
			return RouteResult::Processed;
		}

		// If this point is reached, processing failed - decline, but return Unprocessed to allow rerouting if possible:
		h.SetResponse(HSMRequest::Result::SystemError);
		return RouteResult::Unprocessed;
	}
	catch(const std::exception&) {std::throw_with_nested(FORMAT_RUNTIME_ERROR("Key translation failed"));}
}
_Check_return_ MessageNode::RouteResult FuturexHSMNode::TranslatePIN(HSMRequest& h) {
	try {
		// Retrieve request parameters from collection:
		HSMRequest::StringView peksrc{nullptr,0}, pekdst{nullptr,0}, pinin{nullptr,0}, pan{nullptr,0};
		for(auto seek = h.GetRequestFields().cbegin(); seek != h.GetRequestFields().cend(); ++seek) {
			if(HSMRequest::IsField(*seek, HSMRequest::FieldName::PEKSrc)) peksrc = HSMRequest::GetFieldView(*seek);
			else if(HSMRequest::IsField(*seek, HSMRequest::FieldName::PEKDst)) pekdst = HSMRequest::GetFieldView(*seek);
			else if(HSMRequest::IsField(*seek, HSMRequest::FieldName::PINIn)) pinin = HSMRequest::GetFieldView(*seek);
			else if(HSMRequest::IsField(*seek, HSMRequest::FieldName::PAN)) pan = HSMRequest::GetFieldView(*seek);
		}

		// Validate request parameters (all parameters required):
		bool valid = (
			peksrc.first != nullptr && ValueOps::Is(peksrc.second).InSet(16ULL,32ULL)
			&& pekdst.first != nullptr && ValueOps::Is(pekdst.second).InSet(16ULL,32ULL)
			&& pinin.first != nullptr && pinin.second == 16
			&& pan.first != nullptr && pan.second >= 12
		);
		if(valid == false) {
			h.SetResponse(HSMRequest::Result::InvalidArg);
			return RouteResult::Processed;
		}

		// Declare placeholders for response fields
		std::string pinout;

		// If this point is reached required parameters are present and valid; build and execute request:
		{std::string request;
		request.reserve(128); // Space for two keys, PIN block and overhead
		request.append(HSMLITERAL("[AOTPIN;AW2;AX"));
		request.append(peksrc.first, peksrc.second);
		request.append(HSMLITERAL(";BT"));
		request.append(pekdst.first, pekdst.second);
		request.append(HSMLITERAL(";AL"));
		request.append(pinin.first, pinin.second);
		request.append(HSMLITERAL(";AK"));
		// Write in last 12 digits of PAN, excluding checkdigit (zero-pad if exactly 12 chars):
		if(pan.second == 12) {
			request += '0';
			request.append(pan.first, 11);
		}
		else request.append(pan.first + pan.second - 13, 12);
		request.append(HSMLITERAL(";ZA1;AG"));
		// Lock access to echo counter, format to local buffer:
		char echoval[10] = {0};
		{auto lock = Locks::Acquire(EchoLock);
		if(++EchoCount > 9999) EchoCount = 1;
		StringOps::Decimal::ExWriteString<4>(echoval, EchoCount);}
		request.append(echoval, 4);
		request.append(HSMLITERAL(";]"));

		// Execute request, retrieve response fields:
		const Tokenizer toks = ExecRequest(request);
		if((valid = toks.TokenCount() >= 5) == true) {
			HSMRequest::StringView echo{nullptr,0}, respcode{nullptr,0};
			for(size_t i = 0; i < toks.TokenCount(); ++i) {
				if(toks.Length(i) == 18) {
					if(memcmp(toks.Value(i), "AL", 2) == 0) pinout.assign(toks.Value(i) + 2, 16);
				}
				else if(toks.Length(i) >= 6) {
					if(memcmp(toks.Value(i), "AG", 2) == 0) echo = {toks.Value(i) + 2, toks.Length(i) - 2};
				}
				else if(toks.Length(i) == 3) {
					if(memcmp(toks.Value(i), "BB", 2) == 0) respcode = {toks.Value(i) + 2, toks.Length(i) - 2};
				}
			}
			// Ensure we extracted all expected fields, check echo value:
			valid = (
				echo.first != nullptr && echo.second == 4
				&& respcode.first != nullptr && respcode.second == 1
				&& (pinout.empty() == false)
			);
			if(valid) valid = (memcmp(echo.first, echoval, 4) == 0);
			if(valid) {
				if(ValueOps::Is(respcode.first[0]).InSet('L','N','S')) {
					// PIN block format rejected - this is a data error, not a system error:
					h.SetResponse(HSMRequest::Result::SanityError);
					return RouteResult::Processed;
				}
				else valid = (respcode.first[0] == 'Y');
			}
		}}

		// If processing was successful, construct response fields and apply to object:
		if(valid) {
			h.SetResponse(HSMRequest::Result::OK,
				HSMRequest::ResponseFieldSet{{HSMRequest::FieldName::PINOut, std::move(pinout)}});
			return RouteResult::Processed;
		}

		// If this point is reached, processing failed - decline, but return Unprocessed to allow rerouting if possible:
		h.SetResponse(HSMRequest::Result::SystemError);
		return RouteResult::Unprocessed;
	}
	catch(const std::exception&) {std::throw_with_nested(FORMAT_RUNTIME_ERROR("PIN translation failed"));}
}
#pragma endregion Encryption requests

//==========================================================================================================================
_Check_return_ Tokenizer FuturexHSMNode::ExecRequest(const std::string& Request) {

	printf("%s\n", Request.c_str()); // TODO: EXECUTE COMMAND, PARSE RESPONSE
	size_t RespLen = 0;
	char Response[256] = {0};
	if(memcmp(Request.data(), "[AOGWKS;", 8) == 0) RespLen = StringOps::ExStrCpyLiteral(Response, "[AOGWKS;BG99999999999999999999999999999999;BHAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA;AE1122;FS2;AG");
	else if(memcmp(Request.data(), "[AOTWKA;", 8) == 0) RespLen = StringOps::ExStrCpyLiteral(Response, "[AOTWKA;BHBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB;AE1122;AG");
	else if(memcmp(Request.data(), "[AOTWKM;", 8) == 0) RespLen = StringOps::ExStrCpyLiteral(Response, "[AOTWKM;BG77777777777777777777777777777777;AE8877;AG");
	else if(memcmp(Request.data(), "[AOTWKS;", 8) == 0) RespLen = StringOps::ExStrCpyLiteral(Response, "[AOTWKS;BG66666666666666666666666666666666;AE8866;AG");
	else if(memcmp(Request.data(), "[AOTPIN;", 8) == 0) RespLen = StringOps::ExStrCpyLiteral(Response, "[AOTPIN;AL5555555555555555;BBY;AG");
	else RespLen = StringOps::ExStrCpyLiteral(Response, "[AOERRO;AM1;AN2;BBLONG MESSAGE GREATER THAN HOWEVER MANY CHARS I WAS THINKING;AG");
	if(RespLen > 0) {
		const char * ag = strstr(Request.c_str(), ";AG");
		if(ag) RespLen += StringOps::ExStrCpy(Response + RespLen, ag + 3, 4);
		RespLen += StringOps::ExStrCpyLiteral(Response + RespLen, ";]");
	}
	printf("%.*s\n", gsl::narrow_cast<int>(RespLen), Response);

	// Validate incoming response (must be framed by [], and have at least enough room for command name):
	if(RespLen > 9 ? (Response[0] == '[' && Response[--RespLen] == ']') : false) {
		if(memcmp(Response, Request.data(), 8) == 0) { // Response starts with same sequence as request, parse and return
			return Tokenizer::CreateCopy<10>(Response + 1, --RespLen, ';');
		}
		else if(memcmp(Response, "[AOERRO;", 8) == 0) { // HSM error - log result for debugging purposes
			LogMessage::ContextEntries ctx{{"FuturexError", Response}};
			LOG_FROM_TEMPLATE_CONTEXT(LogLevel::Warn, &ctx, "Error response to [{:S6}] command", Request.substr(1,6));
		}
	}
	// Return empty Tokenizer object to indicate to caller that request failed
	return Tokenizer::CreateInline<1>(nullptr, 0);
}
