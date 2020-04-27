#pragma once
//==========================================================================================================================
// Tokenizer.h : Class for performing string tokenization
//==========================================================================================================================

#include "Tools/ValueOps.h"

namespace FIQCPPBASE {

class Tokenizer {
public:

	//======================================================================================================================
	// Named constructor: Create copy of read-only source string and tokenize
	// - MaxToks specified via template parameter so it can be validated at compile time
	// - "delimiter" parameter can be a string literal collection of delimiters (e.g. "|,"), a single delimiter character
	//   (e.g. ',' or 0x1c), or can be left out of function call (default is comma delimiter only), required at compile-time
	template<size_t MaxToks, typename...Args>
	static Tokenizer CreateCopy(_In_reads_opt_z_(len) const char* buf, size_t len, Args&&...delimiter) noexcept(false) {
		static_assert(MaxToks > 0, "Invalid MaxToks value");
		return Tokenizer(std::true_type{}, buf, len, MaxToks, std::forward<Args>(delimiter)...);
	}
	// Named constructor: Create copy of read-only std::string and tokenize
	// - See notes above re: MaxToks and "delimiter" parameter
	template<size_t MaxToks, typename...Args>
	static Tokenizer CreateCopy(const std::string& Src, Args&&...delimiter) noexcept(false) {
		static_assert(MaxToks > 0, "Invalid MaxToks value");
		return Tokenizer(std::true_type{}, Src.data(), Src.length(), MaxToks, std::forward<Args>(delimiter)...);
	}
	// Named constructor: Tokenize writable source string in-place
	// - See notes above re: MaxToks and "delimiter" parameter
	template<size_t MaxToks, typename...Args>
	static Tokenizer CreateInline(_Inout_updates_opt_z_(len + 1) char* buf, size_t len, Args&&...delimiter) noexcept(false) {
		static_assert(MaxToks > 0, "Invalid MaxToks value");
		return Tokenizer(std::false_type{}, buf, len, MaxToks, std::forward<Args>(delimiter)...);
	}

	//======================================================================================================================
	// External accessors
	size_t TokenCount() const noexcept {return toks.size();}
	_Check_return_ char* operator[](size_t Idx) const noexcept(false) {
		return (ValueOps::Is(Idx).InRangeLeft(0, toks.size()) ? toks[Idx].first : nullptr);
	}
	_Check_return_ char* Value(size_t Idx) const noexcept(false) {
		return (ValueOps::Is(Idx).InRangeLeft(0, toks.size()) ? toks[Idx].first : nullptr);
	}
	_Check_return_ size_t Length(size_t Idx) const noexcept(false) {
		return (ValueOps::Is(Idx).InRangeLeft(0, toks.size()) ? toks[Idx].second : 0);
	}
	_Check_return_ std::string GetString(size_t Idx) const noexcept(false) {
		return (ValueOps::Is(Idx).InRangeLeft(0, toks.size()) ? std::string(toks[Idx].first, toks[Idx].second) : std::string());
	}
	using Token = std::pair<char*,size_t>;
	_Check_return_ Token GetToken(size_t Idx) noexcept(false) {
		if(ValueOps::Is(Idx).InRangeLeft(0, toks.size())) return toks[Idx];
		else return { nullptr, 0 };
	}

	//======================================================================================================================
	// Reassignment function: Reset this object, create copy of new read-only source string and tokenize
	// - See "Create" function notes above re: MaxToks and "delimiter" parameter
	template<size_t MaxToks, typename...Args>
	Tokenizer& AssignCopy(_In_reads_opt_z_(len) const char* buf, size_t len, Args&&...delimiter);
	template<size_t MaxToks, typename...Args>
	Tokenizer& AssignCopy(const std::string& Src, Args&&...delimiter);
	// Reassignment function: Reset this object, tokenize new writable source string in place
	// - See "Create" function notes above re: MaxToks and "delimiter" parameter
	template<size_t MaxToks, typename...Args>
	Tokenizer& AssignInline(_Inout_updates_opt_z_(len + 1) char* buf, size_t len, Args&&...delimiter);

	//======================================================================================================================
	// Move constructor: Required due to named constructor, but should never be called (RVO should optimize out)
	Tokenizer(Tokenizer&&) noexcept(false) {throw std::runtime_error("Tokenizer move construction is not valid");}
	// Defaulted destructor (no special logic required)
	~Tokenizer() = default;
	// Deleted constructors, assignment operators
	Tokenizer() = delete;
	Tokenizer(const Tokenizer&) = delete;
	Tokenizer& operator=(const Tokenizer&) = delete;
	Tokenizer& operator=(Tokenizer&&) = delete;

private:

	//======================================================================================================================
	// Private constructor: Read-only source string
	template<typename...Args>
	constexpr Tokenizer(std::true_type, _In_reads_opt_z_(len) const char* buf, size_t len, size_t MaxToks, Args&&...delimiter);
	// Private constructor: Writable source string
	template<typename...Args>
	constexpr Tokenizer(std::false_type, _Inout_updates_opt_z_(len) char* buf, size_t len, size_t MaxToks, Args&&...delimiter);

	//======================================================================================================================
	// CompChar: Helper class to detect specific delimiter character
	class CompChar {
	public:
		constexpr bool Match(char c) const noexcept {return (c == d);}
		constexpr CompChar(char c) noexcept : d(c) {}
	private:
		const char d;
	};
	// CompStr: Helper class to detect delimiter character belonging to a set
	template<size_t len>
	class CompStr {
	public:
		constexpr bool Match(char c, size_t start = 0) const {
			// Check "current" character, make recursive call to next character if required (note this loop
			// should theoretically be unrolled by compiler, as string must be a compile-time-constant):
			return (start < len ? (buf[start] == c ? true : Match(c, start + 1)) : false);
		}
		constexpr CompStr(_In_ const char* _buf) noexcept : buf(_buf) {}
	private:
		const char * const buf;
	};
	//======================================================================================================================
	// Comparison retrieval function: Build CompStr object from string literal:
	template<typename T, size_t len, std::enable_if_t<(std::is_same_v<T, const char> && len > 2), int> = 0>
	static constexpr auto GetComp(T (&buf)[len]) {return CompStr<len - 1>(buf);}
	// Comparison retrieval function: Build CompChar object from one-character string literal:
	template<typename T, size_t len, std::enable_if_t<(std::is_same_v<T, const char> && len == 2), int> = 0>
	static constexpr auto GetComp(T (&buf)[len]) {return CompChar(buf[0]);}
	// Comparison retrieval function: Build CompChar object from single character (allows any integral input type):
	template<typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
	static constexpr auto GetComp(T c) {return CompChar(gsl::narrow_cast<char>(c));}
	// Comparison retrieval function: Default implementation builds CompChar object with comma:
	static constexpr auto GetComp() {return CompChar(',');}
	// Comparison retrieval function: Catch-all (error) implementation
	template<typename...Args>
	static constexpr auto GetComp(Args&&...args) {static_assert(false, "Invalid delimiter argument");}

	//======================================================================================================================
	// ParseString: Read source string, terminating tokens and storing pointers
	template<typename T>
	constexpr void ParseString(_Inout_updates_z_(len) char* buf, size_t len, size_t MaxToks, const T& delimiter);

	//======================================================================================================================
	// Private variables
	std::vector<Token> toks;		// Collection of pointers stored with length of pointed-at string
	std::vector<char> StringCopy;	// Copy of read-only source data, if required
};

//==========================================================================================================================
// Private constructor: Create copy of read-only source string and tokenize
template<typename...Args>
inline constexpr Tokenizer::Tokenizer(
	std::true_type, _In_reads_opt_z_(len) const char* buf, size_t len, size_t MaxToks, Args&&...delimiter) {
	if(len > 0 && buf) {
		StringCopy.assign(len + 1, 0);
		memcpy(StringCopy.data(), buf, len);
		ParseString(StringCopy.data(), len, MaxToks, GetComp(std::forward<Args>(delimiter)...));
	}
}
// Private constructor: Tokenize writable source string in-place
template<typename...Args>
inline constexpr Tokenizer::Tokenizer(
	std::false_type, _Inout_updates_opt_z_(len + 1) char* buf, size_t len, size_t MaxToks, Args&&...delimiter) {
	if(len > 0 && buf) {
		buf[len] = 0; // Ensure source string is null-terminated
		ParseString(buf, len, MaxToks, GetComp(std::forward<Args>(delimiter)...));
	}
}

//==========================================================================================================================
// AssignCopy: Reset object, create copy of read-only source string and tokenize
template<size_t MaxToks, typename...Args>
inline Tokenizer& Tokenizer::AssignCopy(_In_reads_opt_z_(len) const char* buf, size_t len, Args&&...delimiter) {
	static_assert(MaxToks > 0, "Invalid MaxToks value");
	toks.clear();
	if(len > 0 && buf) { // Allocate new bytes for copy, destroying any existing data
		StringCopy.assign(len + 1, 0);
		memcpy(StringCopy.data(), buf, len);
		ParseString(StringCopy.data(), len, MaxToks, GetComp(std::forward<Args>(delimiter)...));
	}
	else StringCopy.clear();
	return *this;
}
// AssignCopy: Pass read-only source string to buffer version of function:
template<size_t MaxToks, typename...Args>
inline Tokenizer& Tokenizer::AssignCopy(const std::string& Src, Args&&...delimiter) {
	static_assert(MaxToks > 0, "Invalid MaxToks value");
	return AssignCopy<MaxToks>(Src.data(), Src.length(), std::forward<Args>(delimiter)...);
}
// AssignCopy: Reset object, create copy of read-only source string and tokenize
template<size_t MaxToks, typename...Args>
inline Tokenizer& Tokenizer::AssignInline(_Inout_updates_opt_z_(len + 1) char* buf, size_t len, Args&&...delimiter) {
	static_assert(MaxToks > 0, "Invalid MaxToks value");
	toks.clear();
	StringCopy.clear();
	if(len > 0 && buf) {
		buf[len] = 0; // Ensure source string is null-terminated
		ParseString(buf, len, MaxToks, GetComp(std::forward<Args>(delimiter)...));
	}
	return *this;
}

//==========================================================================================================================
// ParseString: Iterate through source string, terminating tokens and storing pointer locations
template<typename T>
inline constexpr void Tokenizer::ParseString(_Inout_updates_z_(len + 1) char* buf, size_t len, size_t MaxToks, const T& delimiter) {
	toks.reserve(MaxToks);
	MaxToks--; // Convert to zero-based
	char *BasePtr = buf, *EndPtr = buf + len + 1; // Set pointers at start of string and at NULL terminator

	// Step through all characters in string, looking for delimiter values
	size_t ntoks = 0;
	for(char *ptr = BasePtr; ptr < EndPtr && ntoks < MaxToks; ++ptr) {
		if(*ptr == 0) {
			// Early NULL terminator encountered - reposition EndPtr and break loop:
			EndPtr = ptr;
			break;
		}
		else if(delimiter.Match(*ptr)) {
			// Delimiter found - NULL terminate string and add to vector, reposition BasePtr to start of next token
			*ptr = 0;
			toks.emplace_back(std::make_pair(BasePtr, ptr - BasePtr));
			BasePtr = ptr + 1;
			++ntoks;
		}
	}
	// Add final token from last position of BasePtr to end of data:
	toks.emplace_back(std::make_pair(BasePtr, EndPtr - BasePtr));
}

} // (end namespace FIQCPPBASE)
