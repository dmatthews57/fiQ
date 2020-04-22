#pragma once
//==========================================================================================================================
// StringOps.h : Classes and functions for performing string operations
//==========================================================================================================================

#include "Tools/ValueOps.h"

namespace FIQCPPBASE {

class StringOps
{
public:

	//======================================================================================================================
	// BytesAvail: Calculate number of bytes available between "CurrPtr" and "EndPtr"
	_Check_return_ static constexpr size_t BytesAvail(_In_opt_ const char* CurrPtr, _In_ const char* EndPtr) noexcept {
		return (CurrPtr && EndPtr ? (CurrPtr <= EndPtr ? (EndPtr - CurrPtr) : 0) : 0);
	}

	//======================================================================================================================
	// ExMemSet: Initializes exactly "len" bytes of "Tgt" with "value"
	// - Does not null-terminate Tgt
	// - Returns number of bytes written to Tgt (always "len")
	// - If return value not required, just use memset
	_Check_return_ static size_t ExMemSet(_Out_writes_all_(len) char* Tgt, char value, size_t len) {
		return memset(Tgt, value, len), len;
	}
	// ExStrCpy: Copies exactly "len" bytes of "buf" into "Tgt"
	// - Does not null-terminate Tgt
	// - Returns number of bytes written to Tgt (always "len")
	// - If return value not required, use StrCpy instead
	// - If copying string literal, use ExStrCpyLiteral instead
	// - If exact length of input string not known, use FlexStrCpy instead
	_Check_return_ static size_t ExStrCpy(_Out_writes_all_(len) char* Tgt, _In_reads_(len) const char * buf, size_t len) {
		return memcpy(Tgt, buf, len), len;
	}
	// ExStrCpyLiteral: Version of ExStrCpy accepting a string literal (calculates length automatically)
	// - Does not null-terminate Tgt
	// - Returns number of bytes written to Tgt (always compile-time input array size, minus null terminator)
	// - If return value not required, use StrCpyLiteral instead
	// - Can be tricked into accepting const char array, but will copy bytes based on compile-time array size and NOT string length
	template<typename T, size_t len, std::enable_if_t<std::is_same_v<T, const char>, int> = 0>
	_Check_return_ static constexpr size_t ExStrCpyLiteral(_Out_writes_all_(len - 1) char* Tgt, _In_reads_z_(len - 1) T (&buf)[len]) {
		return memcpy(Tgt, buf, len - 1), len - 1;
	}
	// FlexStrCpy: Copies bytes from "buf" into "Tgt" up to lesser of: MaxLen or string length of "buf"
	// - Null-terminates Tgt (meaning Tgt must allow MaxLen + 1 bytes, to allow for terminator)
	// - Returns number of bytes written NOT including null terminator (returns zero if "buf" is null)
	// - If return value not required, use StrCpy instead
	// - If exact input length known, use ExStrCpy instead
	_Check_return_ static size_t FlexStrCpy(
		_Out_writes_z_(MaxLen + 1) char* Tgt, _In_reads_opt_z_(strlen(buf)) const char* buf, size_t MaxLen) {
		if(MaxLen > 0 && buf) {
			const size_t buflen = strlen(buf);
			if(buflen > 0) memcpy(Tgt, buf, MaxLen > buflen ? buflen : MaxLen);
			Tgt[MaxLen > buflen ? buflen : MaxLen] = 0;
			return MaxLen > buflen ? buflen : MaxLen;
		}
		else return (Tgt[0] = 0);
	}
	// StrCpy: Dumb copy of "len" bytes from "buf" into "Tgt"
	// - Null-terminates Tgt (meaning Tgt must allow len + 1 bytes, to allow for terminator)
	// - If copying from string literal, use StrCpyLiteral instead
	static void StrCpy(_Out_writes_z_(len + 1) char* Tgt, _In_reads_opt_(len) const char* buf, size_t len) {
		if(len > 0 && buf) {
			memcpy(Tgt, buf, len);
			Tgt[len] = 0;
		}
		else Tgt[0] = 0;
	}
	// StrCpyLiteral: Version of StrCpy accepting a string literal (calculates length automatically)
	// - Includes source null terminator in copy (meaning Tgt must be at least as large as compile-time array size, including null)
	// - Can be tricked into accepting const char array, but will blindly copy compile-time array size bytes - just use memcpy instead
	template<typename T, size_t len, std::enable_if_t<std::is_same_v<T, const char>, int> = 0>
	static constexpr void StrCpyLiteral(_Out_writes_all_(len) char* Tgt, _In_reads_z_(len) T (&buf)[len]) {
		memcpy(Tgt, buf, len);
	}

	//======================================================================================================================
	// Decimal: Helper class to perform operations related to base-10 number formatting
	class Decimal {
	public:
		// Char: Safely provide ASCII character for lowest digit of any UNSIGNED integral value
		template<typename T, std::enable_if_t<std::is_integral_v<T> && std::is_unsigned_v<T>, int> = 0>
		_Check_return_ static constexpr char Char(T t) noexcept {return DECTAB[t % 10];}
		// Char: Safely provide ASCII character for lowest digit of any SIGNED integral value
		template<typename T, std::enable_if_t<std::is_integral_v<T> && std::is_signed_v<T>, int> = 0>
		_Check_return_ static constexpr char Char(T t) noexcept {
			return DECTAB[static_cast<std::make_unsigned_t<T> >(t < 0 ? -t : t) % 10];
		}
		// IsDecChar: Check if character is an ASCII representation of a decimal character (0-9)
		_Check_return_ static constexpr bool IsDecChar(char c) noexcept {
			return ValueOps::Is(c).InRange('0','9');
		}
		// Digits: Returns the total number of characters required to represent an UNSIGNED integral value at compile time
		template<typename T, T t, std::enable_if_t<std::is_unsigned_v<T>, int> = 0>
		_Check_return_ static constexpr size_t Digits() noexcept {
			return t > 9 ? (1 + Digits<T, t / 10>()) : 1;
		}
		// Digits: Returns the total number of characters required to represent a SIGNED integral value at compile time
		template<typename T, T t, std::enable_if_t<std::is_signed_v<T>, int> = 0>
		_Check_return_ static constexpr size_t Digits() noexcept {
			// If value is negative, start with one character for negative sign; convert value to t's unsigned complement
			// plus one [required to safely flip sign on numeric_limits<T>::min()] and call unsigned version of function
			// (for positive values, just call unsigned immediately)
			return t < 0 ? (1 + Digits<std::make_unsigned_t<T>, static_cast<std::make_unsigned_t<T> >(~t) + 1>())
				: Digits<std::make_unsigned_t<T>, static_cast<std::make_unsigned_t<T> >(t) / 10>();
		}
		// MaxDigits: Determines the maximum number of digits required to represent a given integral type at compile-time
		template<typename T>
		struct MaxDigits {
			// If type is signed, use minimum value to include negative sign - for unsigned, use maximum value
			static constexpr size_t value = Digits<T,
				std::is_signed_v<T> ? (std::numeric_limits<T>::min)() : (std::numeric_limits<T>::max)()>();
		};
		template<typename T>
		static constexpr auto MaxDigits_v = MaxDigits<T>::value;
		// FlexDigits: Returns the total number of characters required to represent an UNSIGNED integral value at runtime
		// - Performs brute-force comparison of compile-time factor-of-10 values
		template<typename T, std::enable_if_t<std::is_unsigned_v<T>, int> = 0>
		_Check_return_ static constexpr size_t FlexDigits(T t) noexcept {
			return t >= ValueOps::PowerOf10(19) ? 20
				: t >= ValueOps::PowerOf10(18) ? 19
				: t >= ValueOps::PowerOf10(17) ? 18
				: t >= ValueOps::PowerOf10(16) ? 17
				: t >= ValueOps::PowerOf10(15) ? 16
				: t >= ValueOps::PowerOf10(14) ? 15
				: t >= ValueOps::PowerOf10(13) ? 14
				: t >= ValueOps::PowerOf10(12) ? 13
				: t >= ValueOps::PowerOf10(11) ? 12
				: t >= ValueOps::PowerOf10(10) ? 11
				: t >= ValueOps::PowerOf10(9) ? 10
				: t >= ValueOps::PowerOf10(8) ? 9
				: t >= ValueOps::PowerOf10(7) ? 8
				: t >= ValueOps::PowerOf10(6) ? 7
				: t >= ValueOps::PowerOf10(5) ? 6
				: t >= ValueOps::PowerOf10(4) ? 5
				: t >= ValueOps::PowerOf10(3) ? 4
				: t >= ValueOps::PowerOf10(2) ? 3
				: t >= ValueOps::PowerOf10(1) ? 2
				: 1;
		}
		// FlexDigits: Returns the total number of characters required to represent a SIGNED integral value at runtime
		template<typename T, std::enable_if_t<std::is_signed_v<T>, int> = 0>
		_Check_return_ static constexpr size_t FlexDigits(T t) noexcept {
			// If value is negative, start with one character for negative sign; convert value to t's unsigned complement
			// plus one [required to safely flip sign on numeric_limits<T>::min()] and call unsigned version of function
			// (for positive values, just call unsigned immediately)
			return t < 0 ? (1 + FlexDigits(static_cast<std::make_unsigned_t<T> >(~t) + 1))
				: FlexDigits(static_cast<std::make_unsigned_t<T> >(t));
		}

		//==================================================================================================================
		// FlexReadString: Function to read a null-terminated decimal string (as atoi) into a SIGNED integer value
		// - Maximum number of characters to read is dynamically determined based on return type (excess characters ignored)
		// - Signed version, checks for negative sign
		// - Default return type is signed int
		template<typename T = int, std::enable_if_t<std::is_integral_v<T> && std::is_signed_v<T>, int> = 0>
		_Check_return_ static T FlexReadString(_In_z_ const char* buf, size_t len = 0) noexcept(false) {
			// Maximum length to read is maximum number of digits for this type, minus one (to account for possible negative
			// sign; note that if present, negative sign will not be counted against maximum length below):
			constexpr size_t MaxChars = MaxDigits_v<T> - 1;
			const bool NegVal = (buf[0] == '-');
			if(NegVal) {
				++buf; // Skip over negative sign
				if(len > 0) { // If user provided length, decrement and cap result at maximum
					if(--len > MaxChars) len = MaxChars;
				}
				else len = MaxChars; // Default length to maximum digits
			}
			else if(len == 0 || len > MaxChars) len = MaxChars; // Default/cap length at maximum digits
			// Read all characters from string until maximum length or invalid character reached:
			long long rc = 0;
			for(size_t s = 0; s < len ? IsDecChar(buf[s]) : false; ++s) {
				rc *= 10;
				rc += static_cast<long long>(buf[s]) - '0';
			}
			// Before casting oversized "rc" value to return type, ensure it is within valid boundary:
			if(NegVal) {
				rc = -rc;
				return gsl::narrow_cast<T>(rc < (std::numeric_limits<T>::min)() ? (std::numeric_limits<T>::min)() : rc);
			}
			else return gsl::narrow_cast<T>(rc > (std::numeric_limits<T>::max)() ? (std::numeric_limits<T>::max)() : rc);
		}
		// FlexReadString: Function to read a null-terminated decimal string (as atoi) into an UNSIGNED integer value
		// - Maximum number of characters to read is dynamically determined based on return type (excess characters ignored)
		// - Unsigned version, will treat negative number strings as invalid (i.e. will return zero)
		// - No default return type (this function must be explicitly invoked)
		template<typename T, std::enable_if_t<std::is_integral_v<T> && std::is_unsigned_v<T>, int> = 0>
		_Check_return_ static T FlexReadString(_In_z_ const char* buf, size_t len = 0) {
			// Default length is maximum digits, ensure user-provided value is capped
			if(len == 0 || len > MaxDigits_v<T>) len = MaxDigits_v<T>;
			// Read all characters from string, until maximum length or invalid character reached
			unsigned long long rc = 0;
			for(size_t s = 0; s < len ? IsDecChar(buf[s]) : false; ++s) {
				rc *= 10;
				rc += static_cast<unsigned long long>(buf[s]) - '0';
			}
			return static_cast<T>(rc > (std::numeric_limits<T>::max)() ? (std::numeric_limits<T>::max)() : rc);
		}

		//==================================================================================================================
		// ExWriteString: Write unsigned integral value into string, with exact number of digits specified at compile time
		// - Does not null-terminate Tgt
		// - Returns total number of bytes written (always len)
		// - Value will be left-truncated if len is too short to represent full number
		// - This overload is selected when writing values with greater than one digit remaining, up to maximum size of
		//   input value; writes one char and makes recursive call to write next character (until last iteration)
		template<size_t len,	// Must be specified by caller
			typename T,			// Can be dynamically determined by compiler
			std::enable_if_t<std::is_integral_v<T> && std::is_unsigned_v<T> && (len > 1) && (len <= MaxDigits_v<T>), int> = 0
		>
		static size_t ExWriteString(_Out_writes_all_(len) char* Tgt, T t) {
			// Write first character, make recursive call to write next one:
			Tgt[0] = Char(t / ValueOps::PowerOf10(len - 1));
			return 1 + ExWriteString<len - 1>(Tgt + 1, t);
		}
		// ExWriteString: Write unsigned integral value into string, with exact number of digits specified at compile time
		// - This overload is terminal case - writes only the last (rightmost) digit of any value
		// - Does not null-terminate Tgt
		// - Always returns 1, since single character is always written
		template<size_t len,	// Must be specified by caller (always 1)
			typename T,			// Can be dynamically determined by compiler
			std::enable_if_t<std::is_integral_v<T> && std::is_unsigned_v<T> && (len == 1), int> = 0
		>
		static size_t ExWriteString(_Out_writes_all_(len) char* Tgt, T t) {
			return Tgt[0] = Char(t), 1;
		}
		// ExWriteString: Write unsigned integral value into string, with exact number of digits specified at compile time
		// - This overload is called when "len" is greater than maximum size of input type; performs required
		//   left-padding and calls appropriate overload for actual value
		// - Does not null-terminate Tgt
		// - Returns total number of bytes written (always len)
		template<size_t len,	// Must be specified by caller
			typename T,			// Can be dynamically determined by compiler
			std::enable_if_t<std::is_integral_v<T> && std::is_unsigned_v<T> && (len > MaxDigits_v<T>), int> = 0
		>
		static size_t ExWriteString(_Pre_writable_size_(len) char* Tgt, T t) {
			// Add number of zeroes we will be writing to return value (deduct this value from bytes to be written by
			// recursive call, so that default overload is then selected):
			return ExMemSet(Tgt, '0', len - MaxDigits_v<T>)
				+ ExWriteString<MaxDigits_v<T> >(Tgt + len - MaxDigits_v<T>, t);
		}
		// ExWriteString: Write signed integral value into string, with exact number of digits specified at compile time
		// - This overload is called for signed values - writes negative sign (if required), and makes recursive call
		//   to UNsigned version of function, casting input (after flipping sign, if required)
		// - Value will be left-truncated if len is too short (note negative sign can cause this)
		// - Does not null-terminate Tgt
		// - Returns total number of bytes written (always len)
		template<size_t len,	// Must be specified by caller
			typename T,			// Can be dynamically determined by compiler
			std::enable_if_t<std::is_integral_v<T> && std::is_signed_v<T>, int> = 0
		>
		static size_t ExWriteString(_Out_writes_all_(len) char* Tgt, T t) {
			return t < 0 ? (Tgt[0] = '-', 1 + ExWriteString<len - 1>(Tgt + 1, static_cast<std::make_unsigned_t<T> >(-t)))
				: ExWriteString<len>(Tgt, static_cast<std::make_unsigned_t<T> >(t));
		}

		//==================================================================================================================
		// FlexWriteString: Write integral value into string, with minimum required digits (determined at runtime)
		// - Does not null-terminate Tgt
		// - Returns total number of bytes written
		// - This overload is called for all unsigned values
		// - Brute-force determining of digits and hardcoding of template call performs better than explicit value testing;
		//   switch statement with all possible values is required since dynamic digit count not available at compile-time
		template<typename T, // Can be dynamically determined by compiler
			size_t MaxSize = MaxDigits_v<T>, // Allow compiler to determine this - used in annotation check below
			std::enable_if_t<std::is_integral_v<T> && std::is_unsigned_v<T>, int> = 0
		>
		static size_t FlexWriteString(_Pre_writable_size_(MaxSize) char* Tgt, T t) {
			static_assert(MaxSize == MaxDigits_v<T>, "Don't override MaxSize");
			const size_t ActualDigits = FlexDigits(t);	// Temporary var is here to satisfy code analyzer; should
			_Analysis_assume_(ActualDigits <= MaxSize);	// be optimized out of existence by compiler
			switch(ActualDigits)
			{
				case 20: return ExWriteString<20>(Tgt, t);
				case 19: return ExWriteString<19>(Tgt, t);
				case 18: return ExWriteString<18>(Tgt, t);
				case 17: return ExWriteString<17>(Tgt, t);
				case 16: return ExWriteString<16>(Tgt, t);
				case 15: return ExWriteString<15>(Tgt, t);
				case 14: return ExWriteString<14>(Tgt, t);
				case 13: return ExWriteString<13>(Tgt, t);
				case 12: return ExWriteString<12>(Tgt, t);
				case 11: return ExWriteString<11>(Tgt, t);
				case 10: return ExWriteString<10>(Tgt, t);
				case 9: return ExWriteString<9>(Tgt, t);
				case 8: return ExWriteString<8>(Tgt, t);
				case 7: return ExWriteString<7>(Tgt, t);
				case 6: return ExWriteString<6>(Tgt, t);
				case 5: return ExWriteString<5>(Tgt, t);
				case 4: return ExWriteString<4>(Tgt, t);
				case 3: return ExWriteString<3>(Tgt, t);
				case 2: return ExWriteString<2>(Tgt, t);
				default: return ExWriteString<1>(Tgt, t);
			};
		}
		// FlexWriteString: Write integral value into string, with minimum required digits (determined at runtime)
		// - This overload is called for all signed values: adds "-" if required, and calls unsigned version
		// - Does not null-terminate Tgt
		// - Returns total number of bytes written
		template<typename T, // Can be dynamically determined by compiler
			size_t MaxSize = MaxDigits_v<T>, // Allow compiler to determine this - used in annotation check below
			std::enable_if_t<std::is_integral_v<T> && std::is_signed_v<T>, int> = 0
		>
		static size_t FlexWriteString(_Pre_writable_size_(MaxSize) char* Tgt, T t) {
			static_assert(MaxSize == MaxDigits_v<T>, "Don't override MaxSize");
			return t < 0 ?
				(Tgt[0] = '-', 1 + FlexWriteString<std::make_unsigned_t<T> >(Tgt + 1, static_cast<std::make_unsigned_t<T> >(-t)))
				: FlexWriteString(Tgt, static_cast<std::make_unsigned_t<T> >(t));
		}
		// FlexWriteString: Write integral value into string, with a specific number of digits (determined at runtime)
		// - This overload is called for all unsigned values
		// - Does not null-terminate Tgt
		// - Will left-truncate any values too large to be expressed in ExactDigits
		// - Returns pair with string and total number of bytes written
		// - Brute-force method here performs better than switch'd ExWriteString calls or loop (unrolled or otherwise)
		template<typename T, // Can be dynamically determined by compiler
			std::enable_if_t<std::is_integral_v<T> && std::is_unsigned_v<T>, int> = 0
		>
		static std::pair<char*,size_t> FlexWriteString(_Out_writes_all_(ExactDigits) char* Tgt, T t, size_t ExactDigits) {
			char *ptr = Tgt;
			if(ExactDigits > 20) ptr += ExMemSet(ptr, '0', ExactDigits - 20);
			if(ExactDigits >= 20) *ptr++ = ((t / ValueOps::PowerOf10(19)) % 10) | 0x30;
			if(ExactDigits >= 19) *ptr++ = ((t / ValueOps::PowerOf10(18)) % 10) | 0x30;
			if(ExactDigits >= 18) *ptr++ = ((t / ValueOps::PowerOf10(17)) % 10) | 0x30;
			if(ExactDigits >= 17) *ptr++ = ((t / ValueOps::PowerOf10(16)) % 10) | 0x30;
			if(ExactDigits >= 16) *ptr++ = ((t / ValueOps::PowerOf10(15)) % 10) | 0x30;
			if(ExactDigits >= 15) *ptr++ = ((t / ValueOps::PowerOf10(14)) % 10) | 0x30;
			if(ExactDigits >= 14) *ptr++ = ((t / ValueOps::PowerOf10(13)) % 10) | 0x30;
			if(ExactDigits >= 13) *ptr++ = ((t / ValueOps::PowerOf10(12)) % 10) | 0x30;
			if(ExactDigits >= 12) *ptr++ = ((t / ValueOps::PowerOf10(11)) % 10) | 0x30;
			if(ExactDigits >= 11) *ptr++ = ((t / ValueOps::PowerOf10(10)) % 10) | 0x30;
			if(ExactDigits >= 10) *ptr++ = ((t / ValueOps::PowerOf10(9)) % 10) | 0x30;
			if(ExactDigits >= 9) *ptr++ = ((t / ValueOps::PowerOf10(8)) % 10) | 0x30;
			if(ExactDigits >= 8) *ptr++ = ((t / ValueOps::PowerOf10(7)) % 10) | 0x30;
			if(ExactDigits >= 7) *ptr++ = ((t / ValueOps::PowerOf10(6)) % 10) | 0x30;
			if(ExactDigits >= 6) *ptr++ = ((t / ValueOps::PowerOf10(5)) % 10) | 0x30;
			if(ExactDigits >= 5) *ptr++ = ((t / ValueOps::PowerOf10(4)) % 10) | 0x30;
			if(ExactDigits >= 4) *ptr++ = ((t / ValueOps::PowerOf10(3)) % 10) | 0x30;
			if(ExactDigits >= 3) *ptr++ = ((t / ValueOps::PowerOf10(2)) % 10) | 0x30;
			if(ExactDigits >= 2) *ptr++ = ((t / ValueOps::PowerOf10(1)) % 10) | 0x30;
			if(ExactDigits >= 1) *ptr++ = (t % 10) | 0x30;
			return {Tgt, (ptr - Tgt)};
		}
		// FlexWriteString: Write integral value into string, with a specific number of digits (determined at runtime)
		// - This overload is called for all signed values: adds "-" if required, and calls unsigned version with t's
		//   unsigned complement plus one [required to safely flip sign on numeric_limits<T>::min()]
		// - Does not null-terminate Tgt
		// - Will left-truncate any values too large to be expressed in ExactDigits (including negative sign)
		// - Returns pair with string and total number of bytes written
		template<typename T, // Can be dynamically determined by compiler
			std::enable_if_t<std::is_integral_v<T> && std::is_signed_v<T>, int> = 0
		>
		static std::pair<char*,size_t> FlexWriteString(_Out_writes_all_(ExactDigits) char* Tgt, T t, size_t ExactDigits) {
			if(t < 0 && ExactDigits > 0) {
				Tgt[0] = '-';
				return {Tgt, 1 + FlexWriteString<std::make_unsigned_t<T> >(
					Tgt + 1, static_cast<std::make_unsigned_t<T> >(~t) + 1, ExactDigits - 1).second};
			}
			else if(ExactDigits > 0) return {Tgt, FlexWriteString(Tgt, gsl::narrow_cast<std::make_unsigned_t<T> >(t), ExactDigits).second};
			else return {Tgt, 0};
		}

		//==================================================================================================================
		// ISOWriteFXRate: Write a double value into ISO-style string (one exponent character plus shifted whole number)
		// - Does not null terminate Tgt
		// - Returns pair with string and total number of bytes written (always FieldSize)
		// - Will throw exception if FieldSize is too small to contain value, or a negative rate is provided
		static std::pair<const char*,size_t> ISOWriteFXRate(_Out_writes_all_(FieldSize) char* Tgt, size_t FieldSize, double FXRate) {
			if(FieldSize < 2 || FXRate < 0) throw std::invalid_argument("Invalid field size or negative rate");
			// The maximum number that can be represented is FieldSize - 1 digits long (first character is exponent), values
			// requiring more than this cannot be represented in a field of this size:
			if(FXRate >= ValueOps::PowerOf10(FieldSize - 1)) {
				Tgt[0] = '0';
				memset(Tgt + 1, '9', FieldSize - 1);
				return {Tgt, FieldSize};
			}
			// Calculate the number of digits required to represent input value left of decimal place, then show as many
			// decimal places as possible given that the maximum number of decimal places that can be expressed in any
			// field is 9 (since this is highest possible value of exponent digit):
			const char DecimalPlaces = gsl::narrow_cast<char>(
				ValueOps::Bounded<size_t>(
					0,
					FieldSize - 1 - StringOps::Decimal::FlexDigits(static_cast<unsigned long long>(FXRate)),
					9
				)
			);
			Tgt[0] = DecimalPlaces | 0x30;
			StringOps::Decimal::FlexWriteString(Tgt + 1,
				static_cast<unsigned long long>(FXRate * ValueOps::PowerOf10(DecimalPlaces)), FieldSize - 1);
			return {Tgt, FieldSize};
		}
	private:
		// Internal constant expressions:
		static constexpr const char DECTAB[11] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 0};
	};

	//======================================================================================================================
	// Ascii: Helper class to perform operations related to ASCII/HEX characters/values/strings
	class Ascii {
	public:
		// Char: Safely provide ASCII-equivalent character for lowest nibble of any integral type
		template<typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
		_Check_return_ static constexpr char Char(T idx) noexcept {return ABTAB[idx & 0x0F];}
		// IsHexChar: Check if character is an ASCII representation of a hex character (0-9, A-F)
		_Check_return_ static constexpr bool IsHexChar(char c) noexcept {
			return ValueOps::Is(c).InRangeSet(std::make_pair('0','9'), std::make_pair('a','f'), std::make_pair('A','F'));
		}
		// CharToHex: Convert single ASCII char to hex value (e.g. 'A' to 0x0A)
		_Check_return_ static constexpr unsigned char CharToHex(char c) noexcept {
			return ValueOps::Is(c).InRange('0','9') ? (c & 0x0F)
				: ValueOps::Is(c).InRange('A','F') ? (c - 0x37)
				: ValueOps::Is(c).InRange('a','f') ? (c - 0x57)
				: 0;
		}
		// ByteToHex: Convert two-byte ASCII string to hex value (e.g. "A2" to 0xA2)
		// - Returns 0 if "s" does not point to two valid ASCII hex characters
		_Check_return_ static constexpr unsigned char ByteToHex(_In_reads_(2) const char* s) noexcept {
			return (s ? (IsHexChar(s[0]) ? IsHexChar(s[1]) : false) : false) ?
				((CharToHex(s[0]) << 4) | CharToHex(s[1])) : 0;
		}

		//==================================================================================================================
		// ReadString: Function to read ASCII-hex string into an integral value (e.g. "12AB" becomes 0x12AB)
		// - This overload is selected when reading two-character string (returns 8-bit unsigned char)
		template<size_t len, std::enable_if_t<len == 2, int> = 0>
		_Check_return_ static constexpr unsigned char ReadString(_In_reads_(len) const char* buf) {
			return buf ? ByteToHex(buf) : 0;
		}
		// ReadString: Function to read ASCII-hex string into an integral value (e.g. "12AB" becomes 0x12AB)
		// - This overload is selected when reading four-character string (returns 16-bit unsigned short)
		template<size_t len, std::enable_if_t<len == 4, int> = 0>
		_Check_return_ static constexpr unsigned short ReadString(_In_reads_(len) const char* buf) {
			return buf ?
				// Read first two characters, shift left and OR with result of recursive call for remaining bytes:
				(static_cast<unsigned short>(ByteToHex(buf)) << 8) | ReadString<len - 2>(buf + 2)
				: 0;
		}
		// ReadString: Function to read ASCII-hex string into an integral value (e.g. "12AB" becomes 0x12AB)
		// - This overload is selected when reading six- or eight-character string (returns 32-bit unsigned long)
		template<size_t len, std::enable_if_t<len == 6 || len == 8, int> = 0>
		_Check_return_ static constexpr unsigned long ReadString(_In_reads_(len) const char* buf) {
			return buf ?
				// Read first two characters, shift left and OR with result of recursive call for remaining bytes:
				(static_cast<unsigned long>(ByteToHex(buf)) << (8 * ((len / 2) - 1))) | ReadString<len - 2>(buf + 2)
				: 0;
		}
		// ReadString: Function to read ASCII-hex string into an integral value (e.g. "12AB" becomes 0x12AB)
		// - This overload is selected when reading ten- to sixteen-character strings (returns 64-bit unsigned long long)
		template<size_t len, std::enable_if_t<len == 10 || len == 12 || len == 14 || len == 16, int> = 0>
		_Check_return_ static constexpr unsigned long long ReadString(_In_reads_(len) const char* buf) {
			return buf ?
				// Read first two characters, shift left and OR with result of recursive call for remaining bytes:
				(static_cast<unsigned long long>(ByteToHex(buf)) << (8 * ((len / 2) - 1))) | ReadString<len - 2>(buf + 2)
				: 0;
		}
		// FlexReadString: Function to read a null-terminated ASCII-hex string into an unsigned value (e.g. "12AB" becomes 0x12AB)
		// - If exact number of bytes to be read is known at compile time, use ReadString functions instead as they will unroll loop
		// - Return type defaults to unsigned 64-bit integer
		// - Maximum number of characters to read is dynamically determined based on return type (excess characters are ignored)
		template<typename T = unsigned long long, std::enable_if_t<std::is_unsigned_v<T> && std::is_integral_v<T>, int> = 0>
		_Check_return_ static T FlexReadString(_In_z_ const char* buf, size_t len = 0) noexcept(false) {
			constexpr size_t MaxChars = (sizeof(T) * 2);
			// If string starts with "0x", skip ahead so long as length is dynamic (zero) or is >= 2
			if((buf[0] == '0' && (len == 0 || len >= 2)) ? (buf[1] == 'x' || buf[1] == 'X') : false) {
				buf += 2;
				if(len >= 2) { // If user provided length, deduct and cap result at maximum
					if((len -= 2) > MaxChars) len = MaxChars; // Default length to maximum digits
				}
				else len = MaxChars;
			}
			else if(len == 0 || len > MaxChars) len = MaxChars; // Default/cap length at maximum digits
			// Read all characters from string, until maximum length or invalid character reached
			T rc = 0;
			for(size_t s = 0; s < len ? IsHexChar(buf[s]) : false; ++s) {
				rc <<= 4;
				rc |= CharToHex(buf[s]);
			}
			return rc;
		}

		//==================================================================================================================
		// ExWriteString: Function to format an integral value into an ASCII hex string (e.g. 0x12AB becomes "12AB")
		// - Does not null-terminate Tgt
		// - Returns total number of bytes written (always "len")
		// - This overload is selected when writing unsigned values with greater than one nibble/char remaining, up to max
		//   size of input value (e.g. max is 4 bytes/8 chars for unsigned int, 2 bytes/4 chars for unsigned short, etc);
		//   writes one char and makes recursive call to write next character (calling terminal case on last iteration)
		template<size_t len,	// Must be specified by caller
			typename T,			// Can be dynamically determined by compiler
			std::enable_if_t<std::is_integral_v<T> && std::is_unsigned_v<T> && (len > 1) && (len <= (sizeof(T) * 2)), int> = 0
		>
		static size_t ExWriteString(_Out_writes_all_(len) char* Tgt, T Src) {
			// Write first character, make recursive call to write next one:
			Tgt[0] = Char(Src >> (4 * (len - 1)));
			return 1 + ExWriteString<len - 1, T>(Tgt + 1, Src);
		}
		// ExWriteString: Function to format an integral value into an ASCII hex string (e.g. 0x12AB becomes "12AB")
		// - This overload is terminal case - writes last nibble of unsigned value of any size
		// - Does not null-terminate Tgt
		// - Always returns 1, since single byte is always written
		template<size_t len,	// Must be specified by caller (always 1)
			typename T,			// Can be dynamically determined by compiler
			std::enable_if_t<std::is_integral_v<T> && std::is_unsigned_v<T> && (len == 1), int> = 0
		>
		static size_t ExWriteString(_Out_writes_all_(len) char* Tgt, T Src) noexcept(false) {
			return Tgt[0] = Char(Src), 1;
		}
		// WriteString: Function to format an integral value into an ASCII hex string (e.g. 0x12AB becomes "12AB")
		// - This overload is called when "len" is greater than maximum size of input type (e.g. > 8 for unsigned int, > 4
		//   for unsigned short, etc); performs required left-padding and calls appropriate overload for actual value
		// - Does not null-terminate Tgt
		// - Returns total number of bytes written (always "len")
		template<size_t len,	// Must be specified by caller
			typename T,			// Can be dynamically determined by compiler
			std::enable_if_t<std::is_integral_v<T> && std::is_unsigned_v<T> && (len > (sizeof(T) * 2)), int> = 0
		>
		static size_t ExWriteString(_Pre_writable_size_(len) char* Tgt, T Src) {
			// Add number of zeroes we will be writing to return value (deduct this value from bytes to be written by
			// recursive call, so that default overload is then selected):
			return ExMemSet(Tgt, '0', len - (sizeof(T) * 2)) + ExWriteString<sizeof(T) * 2, T>(Tgt + (len - (sizeof(T) * 2)), Src);
		}
		// ExWriteString: Function to format an integral value into an ASCII hex string (e.g. 0x12AB becomes "12AB")
		// - This overload is called when any signed type is passed to function; just casts to unsigned equivalent
		// - Does not null-terminate Tgt
		// - Returns total number of bytes written (always "len")
		template<size_t len,	// Must be specified by caller
			typename T,			// Can be dynamically determined by compiler
			std::enable_if_t<std::is_integral_v<T> && std::is_signed_v<T>, int> = 0
		>
		static size_t ExWriteString(_Out_writes_all_(len) char* Tgt, T Src) {
			return ExWriteString<len>(Tgt, static_cast<std::make_unsigned_t<T> >(Src));
		}

		//==================================================================================================================
		// PackTo: Function to pack "len * 2" bytes of ASCII hex into "len" bytes of hex data
		// - For example, PackTo<3>("12ABCD") writes values {0x12, 0xAB, 0xCD} into first three bytes of target
		// - Returns total number of bytes written (always "len")
		template<size_t len>
		static size_t PackTo(_In_reads_(len * 2) const char* Source, _Out_writes_(len) char* Dest) {
			Dest[0] = ByteToHex(Source);
			return 1 + PackTo<len - 1>(Source + 2, Dest + 1);
		}
		template<> static size_t PackTo<0>(const char*, char*) noexcept {return 0;} // Terminal template case
		//==================================================================================================================
		// UnpackFrom: Function to unpack "len" bytes of hex into "len * 2" bytes of ASCII hex
		// - For example, UnpackFrom<3>({0x12, 0xAB, 0xCD}) writes string "12ABCD" into first six bytes of target
		// - Returns total number of bytes written (always "len * 2")
		template<size_t len>
		static size_t UnpackFrom(_In_reads_(len) const char* Source, _Out_writes_(len * 2) char* Dest) {
			Dest[0] = Char((Source[0] >> 4) & 0x0F);
			Dest[1] = Char(Source[0] & 0x0F);
			return 2 + UnpackFrom<len - 1>(Source + 1, Dest + 2);
		}
		template<> static size_t UnpackFrom<0>(const char*, char*) noexcept {return 0;} // Terminal template case
	private:
		// Internal constant expressions:
		static constexpr const char ABTAB[17] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F', 0 };
	};

	//======================================================================================================================
	// TrimLeft: Trim whitespace from left-hand side of std::string
	// - Returns a std::string by value (copy should be optimized out by compiler)
	_Check_return_ static std::string TrimLeft(const std::string& input) {
		return std::string(
			// Create string from first non-space character to end of input (note find_if_not will return
			// input.cend() if no non-space characters found, resulting in zero-length return string):
			std::find_if_not(input.cbegin(), input.cend(), [](int ch){return std::isspace(ch);}), input.cend()
		);
	}
	// TrimLeft: Construct a std::string by stripping whitespace from left-hand side of character string
	// - Returns a std::string by value (copy should be optimized out by compiler)
	_Check_return_ static std::string TrimLeft(_In_reads_z_(len) const char* buf, size_t len) {
		if(buf && len) return std::string(std::find_if_not(buf, buf + len, [](int ch){return std::isspace(ch);}), buf + len);
		else return std::string();
	}
	// TrimRight: Trim whitespace from right-hand side of std::string
	// - Returns a std::string by value (copy should be optimized out by compiler)
	_Check_return_ static std::string TrimRight(const std::string& input) {
		return std::string(
			// Create string from first character to last non-space character (note find_if_not will return
			// input.crend() if no non-space characters found, resulting in zero-length return string):
			input.cbegin(), std::find_if_not(input.crbegin(), input.crend(), [](int ch) {return std::isspace(ch);}).base()
		);
	}
	// TrimRight: Construct a std::string by stripping whitespace from right-hand side of character string
	// - Returns a std::string by value (copy should be optimized out by compiler)
	_Check_return_ static std::string TrimRight(_In_reads_z_(len) const char* buf, size_t len) {
		const char *ptr = buf ? buf + len : nullptr;
		if(ptr ? (ptr > buf) : false) { // No action required for NULL or zero-length string
			--ptr; // Move behind NULL terminator
			while(ptr >= buf ? (*ptr == 0 || std::isspace(gsl::narrow_cast<unsigned char>(*ptr))) : false) --ptr;
			++ptr; // Move ahead of last character
		}
		return std::string(buf, ptr - buf);
	}
	// Trim: Trim whitespace from both sides of std::string
	// - More efficient than calling TrimRight(TrimLeft(input)) or vice-versa
	// - Returns a std::string by value (copy should be optimized out by compiler)
	_Check_return_ static std::string Trim(const std::string& input) {
		// Locate first non-blank character in string [if there are none, iterator will be input.cend()]:
		const std::string::const_iterator FirstNonBlank = std::find_if_not(
			input.cbegin(), input.cend(), [](int ch) {return std::isspace(ch);}
		);
		if(ValueOps::Is(FirstNonBlank).InRangeLeft(input.cbegin(), input.cend())) {
			// There is at least one non-blank character in this string - locate last non-space character
			// (i.e. first non-space character of reverse string):
			const std::string::const_iterator LastNonBlank = std::find_if_not(
				input.crbegin(), std::string::const_reverse_iterator(FirstNonBlank), [](int ch) {return std::isspace(ch);}
			).base();
			// If value found (or last character is at end of range), create string from range:
			if(ValueOps::Is(LastNonBlank).InRange(FirstNonBlank, input.cend())) return std::string(FirstNonBlank, LastNonBlank);
		}
		// If this point is reached, create string from first non-blank character [cend() if none] to cend():
		return std::string(FirstNonBlank, input.cend());
	}
	// Trim: Construct a std::string by stripping whitespace from both sides of character string
	// - More efficient than calling TrimRight(TrimLeft(buf,len)) or vice-versa
	// - Returns a std::string by value (copy should be optimized out by compiler)
	_Check_return_ static std::string Trim(_In_reads_z_(len) const char* buf, size_t len) {
		if(buf && len) {
			// Set pointers to end of string, first non-whitespace character within range:
			const char *EndPtr = buf + len, *StartPtr = std::find_if_not(buf, EndPtr, [](int ch){return std::isspace(ch);});
			if(StartPtr < EndPtr) { // No action required if we are already at end of string:
				--EndPtr; // Move behind NULL terminator
				while(EndPtr >= buf ? (*EndPtr == 0 || std::isspace(gsl::narrow_cast<unsigned char>(*EndPtr))) : false) --EndPtr;
				++EndPtr; // Move ahead of last character
			}
			return std::string(StartPtr, EndPtr - StartPtr);
		}
		else return std::string();
	}

	//======================================================================================================================
	// Wide character string conversion functions
	_Check_return_ static std::string ConvertFromWideString(const std::wstring& utf16) {
		return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().to_bytes(utf16);
	}
	_Check_return_ static std::wstring ConvertToWideString(const std::string& utf8) {
		return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(utf8);
	}

};

} // (end namespace FIQCPPBASE)
