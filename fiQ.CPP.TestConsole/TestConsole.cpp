#include "pch.h"
#include "Tools/Exceptions.h"
#include "Tools/LoggingOps.h"
using namespace FIQCPPBASE;

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

class LogMessage {
private: struct pass_key {}; // Private function pass-key definition
public:
	using ContextEntry = std::pair<std::string,std::string>;
	using ContextEntries = std::vector<ContextEntry>;

	// Named constructor
	template<typename...Args>
	static std::unique_ptr<LogMessage> Create(Args&&...args) {
		return std::make_unique<LogMessage>(pass_key(), std::forward<Args>(args)...);
	}

	// Public accessors
	const std::string& GetMessage() const noexcept {return message;}
	const ContextEntries& GetContext() const noexcept {return context;}

	// Public constructors
	// - Public to allow make_unique access, locked by pass_key to enforce use of named constructor
	template<typename...Args,
		std::enable_if_t<std::is_constructible_v<std::string, Args...>, int> = 0>
	LogMessage(pass_key, Args&&...args) noexcept(false) : message(std::forward<Args>(args)...) {}
	LogMessage(pass_key, std::string&& _message) noexcept(false)
		: message(std::forward<std::string>(_message)) {}
	LogMessage(pass_key, std::string&& _message, ContextEntries&& _context) noexcept(false)
		: message(std::forward<std::string>(_message)), context(std::forward<ContextEntries>(_context)) {}

private:
	std::string message;
	ContextEntries context;
};

template<size_t N, // Placeholder count dictated by LogMessageTemplate (provided separately for compile-time validation)
	typename T // Tuple of N logging arguments (wrapped and forwarded by helper function)
>
class LogMessageBuilder {
public:

	// Build function: Construct and return a LogMessage object
	std::unique_ptr<LogMessage> Build() const;

	constexpr LogMessageBuilder(const LogMessageTemplate& _lt, T&& t) noexcept : lt(_lt), runtimeargs(t) {
		// Ensure number of arguments received by constructor matches expected number of placeholders in
		// message template (done with static_assert instead of SFINAE to provide friendlier error):
		static_assert(std::tuple_size_v<T> == N, "Mismatch between number of arguments and template placeholders");
	}

private:

	// Private members - set at construction
	const LogMessageTemplate& lt; // Private reference to static format template for this log message
	T runtimeargs; // Tuple of runtime arguments to be applied to template placeholder

	// Element size calculation functions - used by ElementSize below to determine approximate length of element:
	template<typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
	static constexpr size_t CalcElementSize(const T&, size_t precision) {
		// Use value explicitly set in template, if any, otherwise max decimal digits for type:
		return (precision ? precision : StringOps::Decimal::MaxDigits_v<T>);
	}
	template<typename T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
	static constexpr size_t CalcElementSize(const T&, size_t precision) {
		// Floating point values will ultimately be written as one long integer, decimal point
		// plus number of decimal digits equal to precision (minimum 1):
		return StringOps::Decimal::MaxDigits_v<long long> + 1 + precision;
	}
	template<typename T, std::enable_if_t<std::is_same_v<std::string,T>, int> = 0>
	static constexpr size_t CalcElementSize(const T& t, size_t precision) {
		// Use exact length of string, up to precision:
		return (t.length() > precision ? precision : t.length());
	}
	template<typename T, std::enable_if_t<std::is_same_v<std::remove_const_t<T>, char>, int> = 0>
	static constexpr size_t CalcElementSize(T* t, size_t precision) {
		// Use length of string, up to precision:
		const size_t len = strlen(t);
		return (len > precision ? precision : len);
	}

	// Element size calculation functions - used during Build determine approximate total length of message:
	template<size_t s, std::enable_if_t<(s > (N + N)), int> = 0> // Terminal case
	size_t ElementSize() const noexcept(false) {return 0;}
	template<size_t s, std::enable_if_t<(s <= (N + N) && (s % 2) != 0), int> = 0> // Placeholder case
	size_t ElementSize() const noexcept(false) {
		return CalcElementSize(std::get<s / 2>(runtimeargs), lt.PlaceholderPrecision(s / 2))
			+ ElementSize<s + 1>(); // Continue to next argument
	}
	template<size_t s, std::enable_if_t<(s <= (N + N) && (s % 2) == 0), int> = 0> // Fixed string case
	size_t ElementSize() const noexcept(false) {
		return lt.TokenLength(s) // Exact length of non-placeholder text
			+ ElementSize<s + 1>(); // Continue to next argument
	}
	size_t ElementSizes() const noexcept(false) {return ElementSize<0>();}

	// Element formatting functions - used during Build to format a single element:
	template<typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
	static constexpr std::pair<const char*,size_t> FormatElement(
		_Out_writes_(LogMessageTemplate::MAX_PLACEHOLDER_LEN) char* buf, T t,
		LogMessageTemplate::Format format, size_t precision) {
		// Write integral value to preallocated buffer, returning buffer and bytes written:
		if(format == LogMessageTemplate::Format::Hex) return StringOps::Ascii::FlexWriteString(buf, t, precision);
		else if(precision > 0) return StringOps::Decimal::FlexWriteString(buf, t, precision);
		else return { buf, StringOps::Decimal::FlexWriteString(buf, t) };
	}
	template<typename T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
	static constexpr std::pair<const char*,size_t> FormatElement(
		_Out_writes_(LogMessageTemplate::MAX_PLACEHOLDER_LEN) char* buf, T t,
		LogMessageTemplate::Format, size_t precision) {
		// Write float value into preallocated buffer, returning buffer and bytes written:
		return StringOps::Float::FlexWriteString(buf, t, precision);
	}
	template<typename T, std::enable_if_t<std::is_same_v<std::string,T>, int> = 0>
	static constexpr std::pair<const char*,size_t> FormatElement(char*, // Preallocated buffer not required
		const T& t, LogMessageTemplate::Format, size_t precision) {
		return { t.data(), ValueOps::Bounded<size_t>(0, t.length(), precision) };
	}
	template<typename T, std::enable_if_t<std::is_same_v<std::remove_const_t<T>, char>, int> = 0>
	static constexpr std::pair<const char*,size_t> FormatElement(char*, // Preallocated buffer not required
		const T* t, LogMessageTemplate::Format, size_t precision) {
		return { t, ValueOps::Bounded<size_t>(0, strlen(t), precision) };
	}

	// Element construction functions - called during Build to apply template and parameters to
	// output string and context entry collection
	template<size_t s, std::enable_if_t<(s > (N + N)), int> = 0> // Terminal case
	void BuildElement(char*, std::string&, LogMessage::ContextEntries&) const noexcept(false) {}
	template<size_t s, std::enable_if_t<(s <= (N + N) && (s % 2) != 0), int> = 0> // Placeholder case
	GSL_SUPPRESS(con.4) // "arg" value below is sometimes pointer, sometimes not - can't always be const const
	void BuildElement(
		_Out_writes_(LogMessageTemplate::MAX_PLACEHOLDER_LEN) char* buf,
		std::string& message, LogMessage::ContextEntries& context) const noexcept(false) {
		// Retrieve argument from tuple, format and precision from template:
		const auto arg = std::get<s / 2>(runtimeargs);
		const auto phformat = lt.PlaceholderFormat(s / 2);
		const size_t phprecision = lt.PlaceholderPrecision(s / 2);
		// Format argument (using preallocated buffer, if required) and capture data/length:
		const std::pair<const char*,size_t> phvalue = FormatElement(buf, arg, phformat, phprecision);
		if(phvalue.second) { // Ignore zero-length values
			message.append(phvalue.first, phvalue.second); // Append to member string
			// If this is a named placeholder, add it to context collection:
			const size_t phnamelen = lt.PlaceholderLen(s / 2);
			if(phnamelen > 0) {
				context.emplace_back(std::piecewise_construct,
					std::forward_as_tuple(lt.Placeholder(s / 2), phnamelen),
					std::forward_as_tuple(phvalue.first, phvalue.second)
				);
			}
		}
		BuildElement<s + 1>(buf, message, context);
	}
	template<size_t s, std::enable_if_t<(s <= (N + N) && (s % 2) == 0), int> = 0> // Fixed string case
	void BuildElement(
		_Out_writes_(LogMessageTemplate::MAX_PLACEHOLDER_LEN) char* buf,
		std::string& message, LogMessage::ContextEntries& context) const noexcept(false) {
		message.append(lt.Token(s), lt.TokenLength(s));
		BuildElement<s + 1>(buf, message, context);
	}
	template<size_t bufsize>
	void BuildElements(char (&buf)[bufsize], std::string& message, LogMessage::ContextEntries& context) const noexcept(false) {
		static_assert(bufsize > LogMessageTemplate::MAX_PLACEHOLDER_LEN, "Write buffer too small");
		BuildElement<0>(buf, message, context);
	}

};

// Build LogMessage by applying runtime arguments to template:
template<size_t N, typename T>
inline std::unique_ptr<LogMessage> LogMessageBuilder<N,T>::Build() const {
	// Declare local variables to hold data, reserve expected sizes:
	std::string message;
	message.reserve(ElementSizes());
	LogMessage::ContextEntries context;
	context.reserve(lt.PlaceholderCount());
	// Apply arguments to local variables, then move to named constructor of LogMessage:
	char temp[LogMessageTemplate::MAX_PLACEHOLDER_LEN + 5] = {0};
	BuildElements(temp, message, context);
	return LogMessage::Create(std::move(message), std::move(context));
}

// Partial specialization of builder class for messages with zero placeholders (static log message)
template<typename T> // Tuple of logging arguments (unused in this specialization)
class LogMessageBuilder<0,T> {
public:
	constexpr std::unique_ptr<LogMessage> Build() const;
	constexpr LogMessageBuilder(const LogMessageTemplate& _lt) : lt(_lt) {}
private:
	const LogMessageTemplate& lt;
};
template<typename T>
inline constexpr std::unique_ptr<LogMessage> LogMessageBuilder<0,T>::Build() const {
	return LogMessage::Create(lt.Token(0), lt.TokenLength(0));
}

// LogMessage argument validator: Ensures only valid types are passed to LogMessage as placeholder values
template<typename T>
struct ValidLogArgument : std::integral_constant<bool,
	std::is_integral_v<std::decay_t<T>>
	|| std::is_floating_point_v<std::decay_t<T>>
	|| std::is_same_v<std::decay_t<T>, char const*>
	|| std::is_same_v<std::decay_t<T>, char*>
	|| std::is_same_v<std::decay_t<T>, std::string>
> {};
template<typename...Args>
struct ValidLogArguments : std::conjunction<ValidLogArgument<Args>...> {};

// Helper function to create a LogMessage object combining a compile-time formatting template and run-time arguments
// (receives token count of LogMessageTemplate via explicit template parameter in order to allow compile-time
// validation of arguments received against placeholders in template):
template<size_t PlaceholderCount, typename...Args>
const auto CreateLogMessageBuilder(const LogMessageTemplate& _lt, Args&&... args) noexcept {
	const auto a = std::forward_as_tuple(args...);
	static_assert(ValidLogArguments<Args...>::value, "Invalid arguments found in list");
	return LogMessageBuilder<PlaceholderCount, std::tuple<Args...>>(_lt, a);
}
template<size_t N>
constexpr auto CreateLogMessageBuilder(const LogMessageTemplate& _lt) noexcept {
	return LogMessageBuilder<0,void>(_lt);
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
		if(StringOps::ExStrCpyLiteral(temp, "nonliteral")) {}
		//std::string temp = "nonliteral";
		//{static constexpr LogMessageTemplate lt1("HELLO {blah:D8} {bleh:D135}");
		{static constexpr LogMessageTemplate lt1("{first:F9} SECOND {second:X9} THIRD {:D} {fourth:S3} FIFTH {fifth:S20} SIXTH {sixth:F22}");
		//{static constexpr LogMessageTemplate lt1("Blah");
		static_assert(lt1.IsValid(), "Invalid string (1)");

		const auto lm1 = CreateLogMessageBuilder<lt1.PlaceholderCount()>(lt1, 7654321.23456789123, 0xFEDCBA0987, 321UL, std::string("literal"), temp);
		//temp = "changed";
		StringOps::StrCpyLiteral(temp, "changed");
		//const auto lm1 = CreateLogMessage<lt1.PlaceholderCount()>(lt1, 1, 2);
		auto lm = lm1.Build();
		printf("LOGMESSAGE WITH PH [%s][%zu:%zu][%zu:%zu]\n",
			lm->GetMessage().c_str(),
			lm->GetMessage().length(),
			lm->GetMessage().capacity(),
			lm->GetContext().size(),
			lm->GetContext().capacity()
		);}

		{static constexpr LogMessageTemplate lt2("Goodbye, nothing to say");
		static_assert(lt2.IsValid(), "Invalid string (2)");
		constexpr auto lm2 = CreateLogMessageBuilder<lt2.PlaceholderCount()>(lt2);
		auto lm = lm2.Build();
		printf("LOGMESSAGE NO PH [%s][%zu:%zu][%zu:%zu]\n",
			lm->GetMessage().c_str(),
			lm->GetMessage().length(),
			lm->GetMessage().capacity(),
			lm->GetContext().size(),
			lm->GetContext().capacity()
		);}

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

