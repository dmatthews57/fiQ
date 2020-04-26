#include "pch.h"
#include "Tools/Exceptions.h"
#include "Tools/LoggingOps.h"
using namespace FIQCPPBASE;




namespace detail
{
    template<std::size_t I>
    struct visit_impl
    {
        template<typename R, typename Tuple>
        inline static R visit(Tuple const &tuple, std::size_t idx)
        {
            return (idx == (I - 1U)) ? std::get<I - 1U>(tuple)
				: (visit_impl<I - 1U>::template visit<R>(tuple, idx));
        }
    };

    template<>
    struct visit_impl<0U>
    {
        template<typename R, typename Tuple>
        inline static constexpr R visit(Tuple const&, std::size_t)
        {
            //static_assert(std::is_default_constructible<R>::value, "Explicit return type of visit_at method must be default-constructible");
            return R{};
        }
    };
}

template<typename R, typename Tuple>
inline constexpr R visit_at(Tuple const &tuple, std::size_t idx)
{
    return detail::visit_impl<std::tuple_size_v<Tuple>>::template visit<R>(tuple, idx);
}


class LogMessageFormatTemplate {
public:

	static constexpr size_t MAX_PLACEHOLDERS = 5;

	constexpr bool IsValid() const {return valid;}
	constexpr size_t TokenCount() const {return tokencount;}
	constexpr const char* Token(size_t n) const {return (n < tokencount ? tokens[n] : nullptr);}
	constexpr size_t TokenLength(size_t n) const {return (n < tokencount ? tokenlengths[n] : 0);}
	constexpr size_t PlaceholderCount() const {return (tokencount / 2);} // Round down (always one more string than placeholder)
	constexpr const char* Placeholder(size_t n) const {return ((n * 2) + 1) < tokencount ? tokens[(n * 2) + 1] : nullptr;}
	constexpr size_t PlaceholderLen(size_t n) const {return ((n * 2) + 1) < tokencount ? tokenlengths[(n * 2) + 1] : 0;}

	// Public constructor - takes a string literal with up to MAX_PLACEHOLDERS
	// - Should be evaluated at compile-time, to ensure it is in a valid format
	template<typename T, size_t len, std::enable_if_t<std::is_same_v<T, const char>, int> = 0>
	constexpr LogMessageFormatTemplate(_In_reads_z_(len - 1) T (&templatestr)[len]) {
		static_assert(len > 0, "Invalid format string length");
		if((valid = (templatestr[0] != 0)) == true) { // String must have non-zero content
			tokens[0] = templatestr; // Save location of start of string

			// Walk through string looking for placeholders:
			const char *ptr = templatestr, * const EndPtr = templatestr + len - 1;
			char inPH = 0; // Indicates whether we are currently inside placeholder, one of: 0 (not), 1 (name), 2 (format)
			for(ptr; ptr < EndPtr && tokencount < MAX_TOKENS && valid; ++ptr) {
				if(*ptr == 0) valid = false; // Don't allow null terminator in literal
				else if(*ptr == '{') {

					// Formatting placeholder opened - save length of current token:
					tokenlengths[tokencount] = ptr - tokens[tokencount];

					// Advance pointer and save new location (start of placeholder content) as next token:
					tokens[++tokencount] = ++ptr;

					// Now move ahead, looking for end of placeholder:
					for(inPH = 1; ptr < EndPtr && inPH != 0 && valid; ++ptr) {
						if(*ptr == '}') {
							// End of placeholder; clear flag, and save length of placeholder content:
							inPH = 0;
							tokenlengths[tokencount] = ptr - tokens[tokencount];
							// Save next pointer location as start of next token, but do NOT increment pointer
							// as the end of the current iteration of the outer loop will do this:
							tokens[++tokencount] = ptr + 1;
							break;
						}
						else if(*ptr == ':') {
							// Divider between placeholder name and format data; if we were already reading format
							// data from this placeholder content, this is an invalid placeholder:
							if(inPH == 2) valid = false;
							else inPH = 2;
						}
						// Only alpha characters valid for placeholder name, only alphanumeric for format:
						else valid = (StringOps::IsAlphaChar(*ptr) || (inPH == 2 ? StringOps::Decimal::IsDecChar(*ptr) : false));
					}
				}
				// (else continue reading message text)
			}

			// If placeholder was started but not finished, this string was invalid:
			if(inPH != 0) valid = false;
			// Otherwise if string is valid and last token is not zero-length, save length of last token (distance
			// to end of literal, before NULL terminator) and increment counter to account for last token:
			else if(valid && EndPtr > tokens[tokencount]) {
				tokenlengths[tokencount] = EndPtr - tokens[tokencount];
				++tokencount;
			}
		}
	}

private:

	static constexpr size_t MAX_TOKENS = (MAX_PLACEHOLDERS * 2);

	// Array of pointers to tokens (one extra, for base of string), array of token lengths:
	const char* tokens[MAX_TOKENS + 1] = { nullptr };
	size_t tokenlengths[MAX_TOKENS + 1] = { 0 };
	
	size_t tokencount = 0;	// Count of entries in tokens/tokenlengths
	bool valid = false;		// Flag indicating whether format string is valid
};

template<size_t N, // Placeholder count (dictated by LogMessageFormatTemplate)
	typename T, // Tuple of logging arguments (wrapped and forwarded by helper function)
	std::enable_if_t<std::tuple_size_v<T> == N, int> Z = 0 // Ensure number of items in tuple matches placeholder count
>
class LogMessage {
public:

	constexpr auto GetTupleSize() const {return N;}
	constexpr const LogMessageFormatTemplate& GetLMFT() const {return lt;}

	constexpr LogMessage(const LogMessageFormatTemplate& _lt, T&& t) : lt(_lt), members(t) {}

	void DoDumbPrint() const;

private:
	const LogMessageFormatTemplate& lt;
	T members;
};

template<//size_t N, // Placeholder count (dictated by LogMessageFormatTemplate)
	typename T, // Tuple of logging arguments (wrapped and forwarded by helper function)
	std::enable_if_t<std::tuple_size_v<T> == 0, int> Z // Ensure number of items in tuple matches placeholder count
>
class LogMessage<0,T,Z> {
public:
	void DoDumbPrint() const;
	constexpr LogMessage(const LogMessageFormatTemplate& _lt, T&&) : lt(_lt) {}
private:
	const LogMessageFormatTemplate& lt;
};

template<size_t N, typename T, std::enable_if_t<std::tuple_size_v<T> == N, int> Z>
inline void LogMessage<N,T,Z>::DoDumbPrint() const {
	printf("LogMessage with placeholders: [");
	for(size_t s = 0; s < lt.TokenCount(); ++s) printf("%.*s", gsl::narrow_cast<int>(lt.TokenLength(s)), lt.Token(s));
	printf("]\n");
	for(size_t s = 0; s < N; ++s) {
		printf("[%.*s][%.d]\n",
			gsl::narrow_cast<int>(lt.PlaceholderLen(s)), lt.Placeholder(s),
			visit_at<int, T>(members, s)
		);
	}
}
// Partial specialization for LogMessages built on templates with no placeholders:
template<typename T, std::enable_if_t<std::tuple_size_v<T> == 0, int> Z>
inline void LogMessage<0,T,Z>::DoDumbPrint() const {
	printf("LogMessage with no placeholders: [%.*s]\n", gsl::narrow_cast<int>(lt.TokenLength(0)), lt.Token(0));
}

template<size_t n, typename...Args>
constexpr auto make_lm(const LogMessageFormatTemplate& _lt, Args&&... args) {
	static_assert(std::tuple_size_v<std::tuple<Args...>> == n,
		"Arguments received do not match number of placeholders");
	return LogMessage<n, std::tuple<Args...>>(_lt, std::forward_as_tuple(args...));
}

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
		{static constexpr LogMessageFormatTemplate lt1("HELLO {blah} {bleh}");
		//constexpr LogMessageFormatTemplate lt1("{first}SECOND{second} THIRD {}{fourth} FIFTH {fifth} SIXTH {sixth}");
		//constexpr LogMessageFormatTemplate lt1("Blah");
		static_assert(lt1.IsValid(), "Invalid string (1)");
		constexpr auto lm1 = make_lm<lt1.PlaceholderCount()>(lt1, 1, 2);
		lm1.DoDumbPrint();}

		{static constexpr LogMessageFormatTemplate lt2("Goodbye, nothing to say");
		static_assert(lt2.IsValid(), "Invalid string (2)");
		constexpr auto lm2 = make_lm<lt2.PlaceholderCount()>(lt2);
		lm2.DoDumbPrint();}

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

