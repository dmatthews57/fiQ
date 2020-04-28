#include "pch.h"
#include "Tools/Exceptions.h"
#include "Tools/LoggingOps.h"
using namespace FIQCPPBASE;

class LogMessageFormatTemplate {
public:

	// Public definitions
	static constexpr size_t MAX_PLACEHOLDERS = 5;
	enum class Format { Default = 0, Decimal = 1, Float = 2, Hex = 3 };

	// Public accessors
	constexpr bool IsValid() const {return valid;}
	constexpr size_t TokenCount() const {return tokencount;}
	constexpr const char* Token(size_t n) const {return (n < tokencount ? tokens[n] : nullptr);}
	constexpr size_t TokenLength(size_t n) const {return (n < tokencount ? tokenlengths[n] : 0);}
	constexpr size_t PlaceholderCount() const {return (tokencount / 2);} // Round down (always one more string than placeholder)
	constexpr const char* Placeholder(size_t n) const {return ((n * 2) + 1) < tokencount ? tokens[(n * 2) + 1] : nullptr;}
	constexpr size_t PlaceholderLen(size_t n) const {return ((n * 2) + 1) < tokencount ? tokenlengths[(n * 2) + 1] : 0;}
	constexpr Format PlaceholderFormat(size_t n) const {return (n < (tokencount / 2)) ? placeholderformats[n] : Format::Default;}
	constexpr size_t PlaceholderPrecision(size_t n) const {return (n < (tokencount / 2)) ? placeholderprecision[n] : 0;}

	// Public constructor - takes a string literal with up to MAX_PLACEHOLDERS
	// - Should be evaluated at compile-time, to ensure it is in a valid format
	template<typename T, size_t len, std::enable_if_t<std::is_same_v<T, const char>, int> = 0>
	constexpr LogMessageFormatTemplate(_In_reads_z_(len - 1) T (&templatestr)[len]) {
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
							if(*ptr == 'D') placeholderformats[tokencount / 2] = Format::Decimal;
							else if(*ptr == 'F') placeholderformats[tokencount / 2] = Format::Float;
							else if(*ptr == 'X') placeholderformats[tokencount / 2] = Format::Hex;
							else valid = false;

							// If format type is valid, check for precision and end of placeholder:
							size_t precision = 0;
							while(valid && ptr < EndPtr) {
								++ptr;
								if(*ptr == '}') {
									// End of placeholder found; flag end of placeholder block, save precision value,
									// and save next pointer location as start of next token (but do NOT increment
									// pointer, as the end of the current iteration of the outermost loop will do this):
									inPH = false;
									placeholderprecision[tokencount / 2] = precision;
									tokens[++tokencount] = ptr + 1;
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

	template<size_t s = 0, size_t N, typename ArgT, typename FormatT, std::enable_if_t<(s == N && (N > 0)), int> = 0> // Terminal case
	static constexpr bool ValidateA(const FormatT&) {return true;}
	template<size_t s = 0, size_t N, typename ArgT, typename FormatT, std::enable_if_t<(s < N && (N > 0)), int> = 0>
	static constexpr bool ValidateA(const FormatT& t1) {
		using te = std::tuple_element_t<s, FormatT>;
		constexpr auto f = std::get<s>(t1);
		return (
			(
				(f == Format::Decimal ? std::is_integral_v<std::decay<te>> : false)
				|| (f == Format::Float ? std::is_floating_point_v<std::decay<te>> : false)
				|| (f == Format::Hex ? (std::is_integral_v<std::decay<te>> && std::is_unsigned_v<std::decay<te>>) : false)
				|| std::is_same_v<std::decay_t<te>, char const*>
				|| std::is_same_v<std::decay_t<te>, char*>
			)
			&& ValidateA<s + 1, N, ArgT, FormatT>(t1)
		);
	}

	// Placeholder tuple accessor (needs to receive token count as template parameter in order to evaluate
	// at compile-time, since member tokencount is not considered constexpr in this context):
	template<size_t N>
	constexpr auto Placeholders() const {
		return FormatsToTuple(placeholderformats, std::make_index_sequence<N>{});
	}

#if 0
	template<Format f, typename T>
	struct ArgFormatMatch : std::integral_constant<bool,
		(f == Format::Decimal ? std::is_integral_v<std::decay<T>> : false)
		|| (f == Format::Float ? std::is_floating_point_v<std::decay<T>> : false)
		|| (f == Format::Hex ? (std::is_integral_v<std::decay<T>> && std::is_unsigned_v<std::decay<T>>) : false)
		|| std::is_same_v<std::decay_t<T>, char const*>
		|| std::is_same_v<std::decay_t<T>, char*>> {};
	template<Format f, typename T>
	static constexpr auto ArgFormatMatch_v = ArgFormatMatch<f,T>::value;

	//template<size_t N, typename T>
	//constexpr bool MyFmtYourArg(T&&) const {return ArgFormatMatch_v<placeholderformats[N], std::tuple_element_t<N, T>>;}
	template<size_t N, typename T>
	constexpr bool MyFmtYourArg() const {return ArgFormatMatch_v<placeholderformats[N], T>;}
#endif

private:



	// Generic placeholder-formats-to-tuple function implementation:
	template<size_t N, size_t...I>
	static constexpr auto FormatsToTuple(const Format (&arr)[N], std::index_sequence<I...>) {
		// Ensure correct number of arguments was passed to template (done as static_assert instead of SFINAE
		// to provide friendlier error message):
		static_assert(sizeof...(I) <= N, "Invalid placeholder count passed to template function");
		return std::make_tuple(arr[I]...);
	}

	// Private definitions
	static constexpr size_t MAX_TOKENS = (MAX_PLACEHOLDERS * 2);

	// Arrays storing pointers to tokens (one extra, for base of string), lengths of tokens:
	const char* tokens[MAX_TOKENS + 1] = { nullptr };
	size_t tokenlengths[MAX_TOKENS + 1] = { 0 };

	// Arrays storing placeholder formatting data:
	Format placeholderformats[MAX_PLACEHOLDERS] = { Format::Default };
	size_t placeholderprecision[MAX_PLACEHOLDERS] = { 0 };
	
	size_t tokencount = 0;	// Count of entries in tokens/tokenlengths
	bool valid = false;		// Flag indicating whether format string is valid
};

template<size_t N, // Placeholder count dictated by LogMessageFormatTemplate (provided separately for compile-time validation)
	typename T // Tuple of logging arguments (wrapped and forwarded by helper function)
>
class LogMessage {
public:

	constexpr LogMessage(const LogMessageFormatTemplate& _lt, T&& t) : lt(_lt), runtimeargs(t) {
		// Ensure number of arguments received by constructor matches expected number of placeholders in
		// message template (done with static_assert instead of SFINAE to provide friendlier error):
		static_assert(std::tuple_size_v<T> == N, "Mismatch between number of arguments and template placeholders");
	}

	void DoDumbPrint() const; // TODO: PLACEHOLDER OUTPUT FUNCTION

private:
	const LogMessageFormatTemplate& lt; // Private reference to static format template for this log message
	T runtimeargs; // Tuple of runtime arguments to be applied to template placeholder

	// TODO: PLACEHOLDER OUTPUT FUNCTIONS
	template<size_t s = 0, std::enable_if_t<(s == N), int> = 0> // Terminal case
	void PrintElement() const noexcept(false) {}
	template<size_t s = 0, std::enable_if_t<(s < N), int> = 0>
	void PrintElement() const noexcept(false) {
		const auto a = std::get<s>(runtimeargs);
		const char* phname = lt.Placeholder(s);
		const size_t phnamelen = lt.PlaceholderLen(s);
		const LogMessageFormatTemplate::Format phformat = lt.PlaceholderFormat(s);
		const size_t phprecision = lt.PlaceholderPrecision(s);
		printf("[%.*s][%d][%zu][%s][%s][%d]\n",
			gsl::narrow_cast<int>(phnamelen), phname,
			phformat, phprecision,
			typeid(a).name(),
			MTString(a).c_str(),
			0//lt.MyFmtYourArg<s,std::tuple_element<s,T>>()
		);
		PrintElement<s + 1>();
	}

	template<typename T, std::enable_if_t<std::is_integral_v<T> || std::is_same_v<double,T>, int> = 0>
	static constexpr std::string MTString(T t) {return std::to_string(t);}
	template<typename T, std::enable_if_t<std::is_same_v<std::remove_const_t<T>, char>, int> = 0>
	static constexpr std::string MTString(T* t) {return std::string(t);}
};

// Partial specialization of class for messages with zero placeholders (static log message)
template<typename T> // Tuple of logging arguments (unused in this specialization)
class LogMessage<0,T> {
public:
	constexpr LogMessage(const LogMessageFormatTemplate& _lt) : lt(_lt) {}
	void DoDumbPrint() const; // TODO: PLACEHOLDER OUTPUT FUNCTION
private:
	const LogMessageFormatTemplate& lt;
};

// TODO: DEFAULT DUMB PRINT:
template<size_t N, typename T>
inline void LogMessage<N,T>::DoDumbPrint() const {
	printf("\x1B[97mLogMessage with placeholders: [");
	for(size_t s = 0; s < lt.TokenCount(); ++s) printf("%.*s", gsl::narrow_cast<int>(lt.TokenLength(s)), lt.Token(s));
	printf("]\x1B[37m\n");
	PrintElement();
}
// TODO: SPECIALIZATION OF DUMB PRINT:
// Partial specialization for LogMessages built on templates with no placeholders:
template<typename T>
inline void LogMessage<0,T>::DoDumbPrint() const {
	printf("\x1B[97mLogMessage with no placeholders: [%.*s]\n\x1B[37m\n", gsl::narrow_cast<int>(lt.TokenLength(0)), lt.Token(0));
}

template<typename T>
struct ValidateLogArgument : std::integral_constant<bool,
	std::is_integral_v<std::decay_t<T>>
	|| std::is_floating_point_v<std::decay_t<T>>
	|| std::is_same_v<std::decay_t<T>, char const*>
	|| std::is_same_v<std::decay_t<T>, char*>
> {};
template<typename...Args>
struct ValidateLogArguments : std::conjunction<ValidateLogArgument<Args>...> {};

void f() {}
template<typename T, typename...Args>
void f(T, Args...args) {
	printf("%s:%d/%d\n",
		typeid(T).name(),
		ValidateLogArgument<T>::value,
		0
	);
	f(args...);
}

// Helper function to create a LogMessage object combining a compile-time formatting template and run-time arguments
// (receives token count of LogMessageFormatTemplate via explicit template parameter in order to allow compile-time
// validation of arguments received against placeholders in template):
template<size_t PlaceholderCount, typename T, typename...Args>
const auto CreateLogMessage(const LogMessageFormatTemplate& _lt, const T& t, Args&&... args) {
	//static_assert(std::tuple_size_v<std::tuple<Args...>> == N, "Arguments received do not match number of placeholders"); // No longer required, constructor does this
	const auto a = std::forward_as_tuple(args...);
	constexpr bool b1 = LogMessageFormatTemplate::ValidateA<0, std::tuple_size_v<T>, decltype(a)>(t);

	//f(args...);
	static_assert(ValidateLogArguments<Args...>::value, "Invalid arguments found in list");
	return LogMessage<PlaceholderCount, std::tuple<Args...>>(_lt, a);
}
template<size_t N>
constexpr auto CreateLogMessage(const LogMessageFormatTemplate& _lt) {
	return LogMessage<0,void>(_lt);
}

#include "Tools/Tokenizer.h"

int main()
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF|_CRTDBG_LEAK_CHECK_DF);
#endif
	_set_invalid_parameter_handler(Exceptions::InvalidParameterHandler);
	_set_se_translator(Exceptions::StructuredExceptionTranslator);
	SetUnhandledExceptionFilter(&Exceptions::UnhandledExceptionFilter);

	try
	{
		char temp[20] = {0};
		StringOps::ExStrCpyLiteral(temp, "nonliteral");
		{static constexpr LogMessageFormatTemplate lt1("HELLO {blah:D8} {bleh:D135}");
		//{static constexpr LogMessageFormatTemplate lt1("{first:D}SECOND{second:D1} THIRD {:D11}{fourth:X} FIFTH {fifth:F81} SIXTH {sixth:F22}");
		//{static constexpr LogMessageFormatTemplate lt1("Blah");
		static_assert(lt1.IsValid(), "Invalid string (1)");

		//static constexpr
		printf("%zu\n", lt1.PlaceholderCount());
		static constexpr auto of = lt1.Placeholders<lt1.PlaceholderCount()>();


		//int i1 = 1;
		//const auto lm1 = CreateLogMessage<lt1.PlaceholderCount()>(lt1, i1, 2ULL, 3UL, "literal", temp);
		const auto lm1 = CreateLogMessage<lt1.PlaceholderCount()>(lt1, of, 1, 2);
		lm1.DoDumbPrint();}

		//{static constexpr LogMessageFormatTemplate lt2("Goodbye, nothing to say");
		//static_assert(lt2.IsValid(), "Invalid string (2)");
		//constexpr auto lm2 = CreateLogMessage<lt2.PlaceholderCount()>(lt2, of);
		//lm2.DoDumbPrint();}

		return 0;


		//printf("%zu tokens\n", lt.TokenCount());
		//for(size_t i = 0; i < lt.tokencount; ++i) {
		//	printf("%zu [%.*s]\n", lt.tokenlengths[i], gsl::narrow_cast<int>(lt.tokenlengths[i]), lt.tokens[i]);
		//}
	}
	catch(const std::runtime_error& re) {
		LoggingOps::StdOutLog("Caught runtime error:%s", Exceptions::UnrollExceptionString(re).c_str());
	}
	catch(const std::invalid_argument& ia) {
		LoggingOps::StdOutLog("Caught invalid argument:%s", Exceptions::UnrollExceptionString(ia).c_str());
	}
	catch(const std::exception& e) {
		LoggingOps::StdOutLog("Caught exception:%s", Exceptions::UnrollExceptionString(e).c_str());
	}
}

