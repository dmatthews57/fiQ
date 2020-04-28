#pragma once
//==========================================================================================================================
// LogMessageTemplate.h : Class establishing a string and placeholder template for a specific log message
//==========================================================================================================================

#include "Tools/StringOps.h"

namespace FIQCPPBASE {

class LogMessageTemplate {
public:

	// Public definitions
	static constexpr size_t MAX_PLACEHOLDERS = 5;
	static constexpr size_t MAX_PLACEHOLDER_LEN = 60;
	enum class Format { String = 0, Decimal = 1, Float = 2, Hex = 3 };

	// Public accessors
	constexpr bool IsValid() const {return valid;}
	constexpr size_t TokenCount() const {return tokencount;}
	constexpr const char* Token(size_t n) const {return (n < tokencount ? tokens[n] : nullptr);}
	constexpr size_t TokenLength(size_t n) const {return (n < tokencount ? tokenlengths[n] : 0);}
	constexpr size_t PlaceholderCount() const {return (tokencount / 2);} // Round down (always one more string than placeholder)
	constexpr const char* Placeholder(size_t n) const {return ((n * 2) + 1) < tokencount ? tokens[(n * 2) + 1] : nullptr;}
	constexpr size_t PlaceholderLen(size_t n) const {return ((n * 2) + 1) < tokencount ? tokenlengths[(n * 2) + 1] : 0;}
	constexpr Format PlaceholderFormat(size_t n) const {return (n < (tokencount / 2)) ? placeholderformats[n] : Format::String;}
	constexpr size_t PlaceholderPrecision(size_t n) const {return (n < (tokencount / 2)) ? placeholderprecision[n] : 0;}

	// Public constructor - takes a string literal with up to MAX_PLACEHOLDERS
	// - Should be evaluated at compile-time, to ensure it is in a valid format (do static assert on IsValid)
	template<typename T, size_t len, std::enable_if_t<std::is_same_v<T, const char>, int> = 0>
	constexpr LogMessageTemplate(_In_reads_z_(len - 1) T (&templatestr)[len]) {
		static_assert(len > 0, "Invalid format string length");
		if((valid = (templatestr[0] != 0)) == true) { // String must have non-zero content
			tokens[0] = templatestr; // Save location of start of string

			// Walk through string looking for placeholders:
			const char *ptr = templatestr, * const EndPtr = templatestr + len - 1;
			bool inPH = false; // Indicates whether we are currently inside placeholder
			for(ptr; ptr < EndPtr && tokencount < MAX_TOKENS && valid; ++ptr) {
				if(*ptr == 0) valid = false; // Don't allow null terminator in literal
				else if(*ptr == '{') {

					// Formatting placeholder opened - save length of current token:
					tokenlengths[tokencount] = ptr - tokens[tokencount];

					// Advance pointer and save new location (start of placeholder content) as next token:
					tokens[++tokencount] = ++ptr;

					// Now move ahead, looking for end of placeholder:
					for(inPH = true; ptr < EndPtr && inPH && valid; ++ptr) {
						if(*ptr == '}') {
							// End of placeholder; clear flag, and save length of placeholder content:
							inPH = false;
							tokenlengths[tokencount] = ptr - tokens[tokencount];
							// Save next pointer location as start of next token, but do NOT advance pointer
							// as the end of the current iteration of the outer loop will do this:
							tokens[++tokencount] = ptr + 1;
							break;
						}
						else if(*ptr == ':') {
							// Divider between placeholder name and format data; save length of placeholder
							// name (at end of this block, we will have completed placeholder and started
							// next token, or set valid to false):
							tokenlengths[tokencount] = ptr - tokens[tokencount];

							// Advance to next character and ensure that format type is a supported value:
							++ptr;
							Format format = Format::String;
							if(*ptr == 'D') format = Format::Decimal;
							else if(*ptr == 'F') format = Format::Float;
							else if(*ptr == 'X') format = Format::Hex;
							else if(*ptr != 'S') valid = false;

							// If format type is valid, check for precision and end of placeholder:
							size_t precision = 0;
							while((valid = (precision <= MAX_PLACEHOLDER_LEN)) == true && ptr < EndPtr) {
								++ptr;
								if(*ptr == '}') {
									// End of placeholder found; flag end of placeholder block, validate precision:
									inPH = false;
									if((valid = ValidatePrecision(format, precision)) == true) {
										// Save format and precision values, save next pointer location as start of next
										// token (but do NOT increment pointer, as the end of the current iteration of
										// the outermost loop will do this):
										placeholderformats[tokencount / 2] = format;
										placeholderprecision[tokencount / 2] = precision;
										tokens[++tokencount] = ptr + 1;
									}
									break;
								}
								// Only numeric decimal characters can follow format; if present, update local value:
								else if((valid = StringOps::Decimal::IsDecChar(*ptr)) == true) {
									precision *= 10;
									precision += (*ptr & 0x0F);
								}
							}
							break;
						}
						// Only alphanumeric characters valid for placeholder names:
						else valid = StringOps::IsAlphaNumChar(*ptr);
					}
				}
				// (else continue reading message text)
			}

			// If placeholder was started but not finished, this string was invalid:
			if(inPH) valid = false;
			// Otherwise if string is valid and last token is not zero-length, save length of last token (distance
			// to end of literal, before NULL terminator) and increment counter to account for last token:
			else if(valid && EndPtr > tokens[tokencount]) {
				tokenlengths[tokencount] = EndPtr - tokens[tokencount];
				++tokencount;
			}
		}
	}

private:

	// Private definitions and utilities
	static constexpr size_t MAX_TOKENS = (MAX_PLACEHOLDERS * 2);
	static constexpr bool ValidatePrecision(Format f, size_t p) {
		return ValueOps::Is(p).InRange(
			f == Format::Decimal ? 0 : 1, // Decimal does not require explicit precision
			f == Format::Float ? 9 : MAX_PLACEHOLDER_LEN // Only allow up to 9 decimal places for floating points
		);
	}

	// Arrays storing pointers to tokens (one extra, for base of string), lengths of tokens:
	const char* tokens[MAX_TOKENS + 1] = { nullptr };
	size_t tokenlengths[MAX_TOKENS + 1] = { 0 };

	// Arrays storing placeholder formatting data:
	Format placeholderformats[MAX_PLACEHOLDERS] = { Format::String };
	size_t placeholderprecision[MAX_PLACEHOLDERS] = { 0 };
	
	size_t tokencount = 0;	// Count of entries in tokens/tokenlengths
	bool valid = false;		// Flag indicating whether format string is valid
};

}; // (end namespace FIQCPPBASE)
